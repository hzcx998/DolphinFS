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

static char generic_io_block[BLOCK_SIZE];

int next_file = 0;

struct file file_table[MAX_FILES];
struct ofile open_files[MAX_OPEN_FILE];

char *block_ram;

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

unsigned long scan_free_bits(unsigned long *buf, unsigned long size)
{
    int i, j;
    for (i = 0; i < size / sizeof(unsigned long); i++) {
        if (buf[i] != 0xffffffffffffffff) {
            for (j = 0; j < sizeof(unsigned long); j++) {
                if (!(buf[i] & (1 << j))) {
                    buf[i] |= (1 << j);
                    return i * sizeof(unsigned long) + j;
                }
            }
        }
    }
    return size;
}

long alloc_block(void)
{
    /* walk all man block */

    unsigned long start, end, next;
    unsigned long data_block_id;
    int scaned = 0;

    struct super_block *sb = &dolphin_sb;
    
    start = sb->block_off[BLOCK_AREA_MAN];
    end = start + sb->block_nr[BLOCK_AREA_MAN];
    
    next = start;
    /* fix next */
    if (sb->next_free_man_block != 0 && sb->next_free_man_block > start && sb->next_free_man_block < end)
        next = sb->next_free_man_block;

retry:
    for (; next < end; next++) {
        memset(generic_io_block, 0, sizeof(generic_io_block));
        read_block(next, 0, generic_io_block, sizeof(generic_io_block));
        
        data_block_id = scan_free_bits(generic_io_block, sb->block_size);
        /* 扫描成功 */
        if (sb->block_size != data_block_id) {
            // printf("---> get block id:%ld\n", data_block_id);
            /* 如果不是第一个块，就乘以块偏移 */
            if ((next - start) != 0) 
                data_block_id *= (next - start) * sb->block_size;
            /* 标记下一个空闲块 */
            sb->next_free_man_block = next;
            
            write_block(next, 0, generic_io_block, sizeof(generic_io_block));

            // printf("---> alloc block:%ld\n", data_block_id + sb->block_off[BLOCK_AREA_DATA]);
            return data_block_id + sb->block_off[BLOCK_AREA_DATA]; // 返回的块是绝对块地址
        }
    }

    /* 把后面的块都扫描完了，尝试从起始块开始扫描。 */
    next = start;

    if (!scaned) {
        scaned = 1;
        goto retry;
    }

    printf("!!!WARN no free block\n");
    /* 没有空闲块，返回0，由于0是超级快，可以表示无效 */
    return 0;
}

#define alloc_data_block alloc_block

int free_block(long blk)
{
    unsigned long man_block, byte_off, bits_off;;
    unsigned long data_block_id, data_block_byte;
    struct super_block *sb = &dolphin_sb;
    
    // printf("---> free block:%ld\n", blk);

    if (blk < sb->block_off[BLOCK_AREA_DATA] || blk >= sb->block_off[BLOCK_AREA_DATA] + sb->block_nr[BLOCK_AREA_DATA])
        return -1;
    
    /* 获取blk对应的索引 */
    data_block_id = blk - sb->block_off[BLOCK_AREA_DATA];

    /* 获取位偏移 */
    bits_off = data_block_id % 8;

    data_block_byte = data_block_id / 8;

    /* 获取块内字节偏移 */
    byte_off = data_block_byte % sb->block_size;
    /* 获取块偏移 */
    man_block = data_block_byte / sb->block_size;

    /* 转换出逻辑块 */
    man_block += sb->block_off[BLOCK_AREA_MAN];

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(man_block, 0, generic_io_block, sizeof(generic_io_block));

    generic_io_block[byte_off] &= ~(1 << bits_off);
    /* 修改块 */
    write_block(man_block, 0, generic_io_block, sizeof(generic_io_block));

    return 0;
}

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

__IO long write_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    if (blk >= MAX_BLOCK_NR)
        return -1;
    
    char *pblk = (char *)&block_ram[blk * BLOCK_SIZE];

    memcpy(pblk + off, buf, len);

    return len;
}

__IO long read_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    if (blk >= MAX_BLOCK_NR)
        return -1;

    char *pblk = (char *)&block_ram[blk * BLOCK_SIZE];

    memcpy(buf, pblk + off, len);

    return len;
}

long get_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2, *l3;
    unsigned int l1_val, l2_val, l3_val;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    assert(l1 != NULL);

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // alloc BMD block
        l1_val = alloc_data_block();
        l1[GET_BGD_OFF(off)] = l1_val;
        // sync block
        write_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // alloc data block
        l2_val = alloc_data_block();
        l2[GET_BGD_OFF(off)] = l2_val;
        // sync block
        write_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));
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

    sb->next_free_man_block = 0; /* invalid */
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

    printf("next_free_man_block: %ld\n", sb->next_free_man_block);
}

void init_man(struct super_block *sb)
{
    /* clear all man */
    unsigned long start, end;
    start = sb->block_off[BLOCK_AREA_MAN];
    end = start + sb->block_nr[BLOCK_AREA_MAN];
    for (; start < end; start++) {
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(start, 0, generic_io_block, sizeof(generic_io_block));
    }
}

int main(int argc, char *argv[])
{
    printf("hello, DolphinFS\n");

    /* init disk */
    block_ram = malloc(BLOCK_DATA_SIZE);
    if (!block_ram) {
        printf("alloc block data failed!\n");
        return -1;
    }

    /* init sb info */
    init_sb(&dolphin_sb, get_capacity(), BLOCK_SIZE);

    dump_sb(&dolphin_sb);

    /* write sb info to disk */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    memcpy(generic_io_block, &dolphin_sb, sizeof(dolphin_sb));
    write_block(dolphin_sb.block_off[BLOCK_AREA_SB], 0, generic_io_block, sizeof(generic_io_block));

    /* init man info */
    init_man(&dolphin_sb);

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

    dump_sb(&dolphin_sb);

    return 0;
}
