#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct file {
    unsigned long hash; /* name hash */
    long data_table;
    unsigned long file_size;
    int ref;
    int mode; /* r,w,rw */
};

/* open file */
struct ofile {
    struct file *f;
    int oflags; /* open flags */
    long off;
};

static struct super_block dolphin_sb;

int next_file = 0;

struct file file_table[MAX_FILES];
struct ofile open_files[MAX_OPEN_FILE];

/* 用于记录块id和块数据的关系数组，数组索引就是块id，数据就是块数据 */
long *block_ids[MAX_BLOCK_NR];
#ifdef CONFIG_BLOCK_RAM
char *block_ram;
#endif

struct file *alloc_file(void)
{
    return &file_table[next_file++];
}

#define file_idx(f) ((f) - &file_table[0])

struct ofile *alloc_ofile(void)
{
    int i;
    struct ofile *of;
    for (i = 0; i < MAX_OPEN_FILE; i++) {
        of = &open_files[i];
        if (of->f == NULL) {
            return of;
        }
    }
    return NULL;
}

#define ofile_idx(of) ((of) - &open_files[0])
#define idx_ofile(idx) (&open_files[(idx)])

unsigned long get_area_off(int area)
{
    return dolphin_sb.block_off[area];
}

unsigned long get_area_nr(int area)
{
    return dolphin_sb.block_nr[area];
}

long alloc_block(int area)
{
    int i;

    for (i = get_area_off(area); i < get_area_nr(area); i++) {
        if (block_ids[i] == NULL) {
#ifdef CONFIG_BLOCK_RAM
            block_ids[i] = (long *)&block_ram[i * BLOCK_SIZE];
#else
            block_ids[i] = malloc(BLOCK_SIZE);
#endif
            return i;
        }
    }
    return -1;
}

long alloc_data_block(void)
{
    return alloc_block(BLOCK_AREA_DATA);
}

long alloc_sb_block(void)
{
    return alloc_block(BLOCK_AREA_SB);
}

int free_block(long blk)
{
    long i = blk;
    if (block_ids[i] == NULL) {
        return -1;
    }
#ifdef CONFIG_BLOCK_RAM
    memset((char *)&block_ram[i * BLOCK_SIZE], 0, BLOCK_SIZE);
#else
    free(block_ids[i]);
#endif
    block_ids[i] = NULL;
    return 0;
}

#define blk2data(blk) block_ids[blk]

/**
 * 1. path -> file
 * 2. file -> block table
 * 3. block table -> block
 */
unsigned long str2hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

int create_file(char *path, int mode)
{
    if (!path || !strlen(path) || !mode)
        return -1;

    struct file *f = alloc_file();
    if (!f)
        return -1;
    
    f->hash = str2hash(path);
    f->data_table = alloc_data_block();
    f->ref = 0;
    f->mode = mode;
    f->file_size = 0;
    if (f->data_table < 0) {
        // TODO: free file
        return -1;
    }
    return file_idx(f);
}

int delete_file(char *path)
{
    /* 1. 删除文件数据 */

    /* 2. 删除文件结构 */

    /*  */
}

struct file *search_file(char *path)
{
    if (!path || !strlen(path))
        return NULL;

    int i;
    struct file *f;
    for (i = 0; i < next_file; i++) {
        f = &file_table[i];
        if (f->hash == str2hash(path)) {
            return f;
        }
    }
    return NULL;
}

int open_file(char *path, int flags)
{
    if (!path || !strlen(path) || !flags)
        return -1;
    
    struct file *f = search_file(path);
    if (!f) {
        return -1;
    }
    struct ofile *of = alloc_ofile();
    if (!of)
        return -1;

    of->f = f;
    of->oflags = flags;
    of->off = 0;

    f->ref++;

    return ofile_idx(of);
}

int close_file(int file)
{
    if (file < 0 || file >= MAX_OPEN_FILE)
        return -1;
    
    struct ofile *of = idx_ofile(file);
    if (!of->f)
        return -1;

    of->f->ref--;

    of->oflags = 0;
    of->off = 0;
    of->f = NULL;

    return 0;
}

long get_capacity(void)
{
    return MAX_BLOCK_NR;
}

__IO long write_block(long blk, long off, void *buf, long len)
{
    if (blk >= MAX_BLOCK_NR)
        return -1;
    
    char *pblk = (char *)blk2data(blk);

    memcpy(pblk + off, buf, len);

    return len;
}

__IO long read_block(long blk, long off, void *buf, long len)
{
    if (blk >= MAX_BLOCK_NR)
        return -1;

    char *pblk = (char *)blk2data(blk);

    memcpy(buf, pblk + off, len);

    return len;
}

long get_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2, *l3;
    unsigned int l1_val, l2_val, l3_val;

    l1 = (unsigned int *)blk2data(f->data_table);
    assert(l1 != NULL);

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // alloc BMD block
        l1_val = alloc_data_block();
        l1[GET_BGD_OFF(off)] = l1_val;
        // sync block
    }

    l2 = (unsigned int *)blk2data(l1_val);
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // alloc data block
        l2_val = alloc_data_block();
        l2[GET_BGD_OFF(off)] = l2_val;
        // sync block
    }

    return l2_val;
}

/**
 * 按照页面的方式寻址，并写入数据。
 */
long do_write_file(struct file *f, long off, void *buf, long len)
{
    long vir_blk, phy_blk, blk_off, chunk;
    long next_off;
    long left_len; // 必须是有符号数
    long write_len;
    char *pbuf;
    long ret;

    next_off = off;
    left_len = len;
    write_len = 0;
    pbuf = buf;

    while (left_len > 0) {
        /* 计算出需要访问的位置，以及大小 */
        vir_blk = next_off / BLOCK_SIZE;
        blk_off = next_off % BLOCK_SIZE;
        chunk = min(left_len, BLOCK_SIZE - blk_off);

        /* 通过文件表获取物理块位置 */
        ret = get_file_block(f, vir_blk * BLOCK_SIZE);
        if (ret < 0) { // 没有可用块
            return -1;
        }
        phy_blk = ret;

        /* 将数据写入块 */
        ret = write_block(phy_blk, blk_off, pbuf, chunk);
        if (ret != chunk) { // 写IO失败
            return -1;
        }
        f->file_size += chunk;
        write_len += chunk;
        pbuf += chunk;
        next_off += chunk;
        left_len -= chunk;
    }
    return write_len;
}

long write_file(int file, void *buf, long len)
{
    long ret;
    if (file < 0 || file >= MAX_OPEN_FILE)
        return -1;
    
    struct ofile *of = idx_ofile(file);
    if (!of->f)
        return -1;
    
    ret = do_write_file(of->f, of->off, buf, len);
    if (ret > 0)
        of->off += ret;
    
    return ret;
}

long do_read_file(struct file *f, long off, void *buf, long len)
{
    long vir_blk, phy_blk, blk_off, chunk;
    long next_off;
    long left_len; // 必须是有符号数
    long read_len;
    char *pbuf;
    long ret;
    long file_size;
    
    file_size = f->file_size;
    next_off = off;
    left_len = len;
    read_len = 0;
    pbuf = buf;

    if (off >= file_size)
        return 0;

    while (left_len > 0 && file_size > 0) {
        /* 计算出需要访问的位置，以及大小 */
        vir_blk = next_off / BLOCK_SIZE;
        blk_off = next_off % BLOCK_SIZE;
        /* 读取buf大小，直到文件结束 */
        chunk = min(left_len, BLOCK_SIZE - blk_off);
        chunk = min(file_size, chunk);

        /* 通过文件表获取物理块位置 */
        ret = get_file_block(f, vir_blk * BLOCK_SIZE);
        if (ret < 0) { // 没有可用块
            return -1;
        }
        phy_blk = ret;

        /* 将数据读取块 */
        ret = read_block(phy_blk, blk_off, pbuf, chunk);
        if (ret != chunk) { // 写IO失败
            return -1;
        }
        read_len += chunk;
        pbuf += chunk;
        next_off += chunk;
        left_len -= chunk;
        file_size -= chunk;
    }
    return read_len;
}

long read_file(int file, void *buf, long len)
{
    long ret;
    if (file < 0 || file >= MAX_OPEN_FILE)
        return -1;
    
    struct ofile *of = idx_ofile(file);
    if (!of->f)
        return -1;
    
    ret = do_read_file(of->f, of->off, buf, len);
    if (ret > 0)
        of->off += ret;
    
    return ret;
}

int seek_file(int file, int off, int pos)
{
    long ret;
    if (file < 0 || file >= MAX_OPEN_FILE)
        return -1;
    
    struct ofile *of = idx_ofile(file);
    struct file *f;
    if (!of->f)
        return -1;
    
    ret = of->off;

    switch (pos) {
    case FP_SET:
        of->off = off;
        break;
    case FP_CUR:
        of->off += off;
        break;
    case FP_END:
        f = of->f;
        of->off = f->file_size + off;
        if (of->off > f->file_size)
            of->off = f->file_size;
        break;
    default:
        break;
    }
    return of->off;
}

void init_sb(struct super_block *sb, unsigned long capacity, unsigned long block_size)
{
    sb->capacity = capacity;
    sb->block_size = block_size;
    
    sb->block_nr[BLOCK_AREA_SB] = 1;
    sb->block_nr[BLOCK_AREA_MAN] = DIV_ROUND_UP(capacity / 8, block_size);
    sb->block_nr[BLOCK_AREA_DATA] = capacity  - sb->block_nr[BLOCK_AREA_MAN] - sb->block_nr[BLOCK_AREA_SB];

    sb->block_off[BLOCK_AREA_SB] = 0;
    sb->block_off[BLOCK_AREA_MAN] = sb->block_off[BLOCK_AREA_SB] + sb->block_nr[BLOCK_AREA_SB];
    sb->block_off[BLOCK_AREA_DATA] = sb->block_off[BLOCK_AREA_MAN] + sb->block_nr[BLOCK_AREA_MAN];
}

void dump_sb(struct super_block *sb)
{
    printf("==== sb info ====\n");
    printf("capacity: %ld\n", sb->capacity);
    printf("block_size: %ld\n", sb->block_size);
    printf("block_nr: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_nr[BLOCK_AREA_SB], sb->block_nr[BLOCK_AREA_MAN], sb->block_nr[BLOCK_AREA_DATA]);
    printf("block_off: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_off[BLOCK_AREA_SB], sb->block_off[BLOCK_AREA_MAN], sb->block_off[BLOCK_AREA_DATA]);
}


int main(int argc, char *argv[])
{
    printf("hello, DolphinFS\n");

    block_ram = malloc(BLOCK_DATA_SIZE);
    if (!block_ram) {
        printf("alloc block data failed!\n");
        return -1;
    }

    /* init sb info */
    init_sb(&dolphin_sb, get_capacity(), BLOCK_SIZE);

    dump_sb(&dolphin_sb);

    assert(dolphin_sb.block_off[BLOCK_AREA_SB] == alloc_sb_block());
    /* write sb info to disk */
    write_block(dolphin_sb.block_off[BLOCK_AREA_SB], 0, &dolphin_sb, sizeof(dolphin_sb));

    /* init man info */

    printf("create test:%d\n", create_file("test", FM_RDWR));
    printf("create test_dir/:%d\n", create_file("test_dir/", FM_RDWR));
    printf("create test_dir/a:%d\n", create_file("test_dir/a", FM_RDWR));

    int fd = open_file("test", FF_RDWR);
    printf("open test: %d\n", fd);
    
    printf("close f: %d\n", close_file(fd));

    fd = open_file("test_dir", FF_RDWR);
    printf("open test_dir: %d\n", fd);
    printf("close f: %d\n", close_file(fd));
    
    fd = open_file("test_dir/", FF_RDWR);
    printf("open test_dir/: %d\n", fd);
    printf("close f: %d\n", close_file(fd));

    fd = open_file("test_dir/a", FF_RDWR);
    printf("open test_dir/a: %d\n", fd);

    printf("write: %d\n", write_file(fd, "hello", 5));
    printf("seek: %d\n", seek_file(fd, 0, FP_SET));
    printf("seek: %d\n", seek_file(fd, 0, FP_SET));
    char buf[32] = {0};
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    memset(buf, 0, 32);
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    printf("seek: %d\n", seek_file(fd, 0, FP_SET));
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    printf("close f: %d\n", close_file(fd));
    
    fd = open_file("test_dir/a", FF_RDWR);
    printf("open test_dir/a: %d\n", fd);

    printf("seek: %d\n", seek_file(fd, 0, FP_END));
    printf("close f: %d\n", close_file(fd));

    return 0;
}
