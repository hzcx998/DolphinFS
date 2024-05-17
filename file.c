#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern struct super_block dolphin_sb;

extern char generic_io_block[BLOCK_SIZE];

int next_file = 0;

struct file file_table[MAX_FILES];
struct ofile open_files[MAX_OPEN_FILE];

long free_file_block(struct file *f, unsigned long off);
long walk_file_block(struct file *f, unsigned long off);

struct file *alloc_file(void)
{
    return &file_table[next_file++];
}

void free_file(struct file *file)
{
    file->data_table = -1;
    file->hash = 0;
    /* TODO: free file solt */
}

void dump_all_file(void)
{
    int i;
    struct file *f;
    
    printf("==== dump files ====\n");
    for (i = 0; i < MAX_FILES; i++) {
        f = &file_table[i];
        if (f->hash != 0) {
            printf("  [%d] hash:%p, table:%lld, size:%lld, ref:%d, mode:%x\n",
                i, f->hash, f->data_table, f->file_size, f->ref, f->mode);
        }
    }
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
        free_file(f);
        return -1;
    }
    return file_idx(f);
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

static int check_empty_u32(unsigned int *table, unsigned long size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (table[i] != 0) {
            return 0;
        }
    }
    return 1;
}

int delete_file(char *path)
{
    if (!path || !strlen(path))
        return -1;
    
    struct file *f = search_file(path);
    if (!f) {
        return -1;
    }

    /* 1. 删除文件数据 */
    if (f->file_size > 0) {
        unsigned long blocks;
        blocks = DIV_ROUND_UP(f->file_size, dolphin_sb.block_size);
        int i;

        for (i = 0; i < blocks; i++) {
            long ret = walk_file_block(f, i * dolphin_sb.block_size);
            assert(ret > 0);
        }
        for (i = 0; i < blocks; i++) {
            long ret = free_file_block(f, i * dolphin_sb.block_size);
            if (ret != 0) {
                printf("[ERR] delete_file: free block %d failed with %d\n", i, ret);
            }
        }
    }
    
    assert(f->data_table > 0);

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    assert(check_empty_u32(generic_io_block, sizeof(generic_io_block) / sizeof(unsigned int)));

    /* free data table */
    free_block(f->data_table);

    /* 2. 删除文件结构 */
    free_file(f);
    return 0;
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

long get_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2;
    unsigned int l1_val, l2_val;

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

        // clear new block   
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // alloc data block
        l2_val = alloc_data_block();
        l2[GET_BMD_OFF(off)] = l2_val;
        // sync block
        write_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

        // clear new block   
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(l2_val, 0, generic_io_block, sizeof(generic_io_block));
    }

    return l2_val;
}

long walk_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2;
    unsigned int l1_val, l2_val;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    assert(l1 != NULL);

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // alloc BMD block
        printf("ERR off:%d, l1_val:%d\n", off, l1_val);
        return 0;
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;

    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // alloc data block
        printf("ERR off:%d->%d, l2_val:%d\n", off, GET_BMD_OFF(off), l2_val);
        return 0;
    }

    return l2_val;
}

long free_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2;
    unsigned int l1_val, l2_val;
    int i;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    if (l1 == NULL) {
        return -1;
    }

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // none BMD block
        return -2;
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // none data block
        return -3;
    }

    /* free data block */
    free_data_block(l2_val);

    l2[GET_BMD_OFF(off)] = 0; /* clear data block in l2 */

    // sync block
    write_block(l1_val, 0, generic_io_block, sizeof(generic_io_block));

    /* check l2 empty? */
    if (!check_empty_u32(generic_io_block, sizeof(generic_io_block) / sizeof(unsigned int))) {
        return 0;
    }

    /* l2 empty !*/

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;

    /* free l2 block */
    free_block(l1_val);

    // printf("%s:%d: free l2 block: %d at %d\n", __func__, __LINE__, l1_val, l2_val);

    l1[GET_BGD_OFF(off)] = 0; /* clear l2 block in l1 */

    // sync block
    write_block(f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    return 0;
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

        /* 将数据读取块 */
        memset(generic_io_block, 0, sizeof(generic_io_block));
        ret = read_block(phy_blk, 0, generic_io_block, sizeof(generic_io_block));
        if (ret != sizeof(generic_io_block)) { // 写IO失败
            return -1;
        }

        /* 拷贝数据 */
        memcpy(generic_io_block + blk_off, pbuf, chunk);

        /* 将数据写入块 */
        ret = write_block(phy_blk, 0, generic_io_block, sizeof(generic_io_block));
        if (ret != sizeof(generic_io_block)) { // 写IO失败
            return -1;
        }

        /* 只有在偏移+chunk大小超过文件大小的时候，才去更新文件大小 */
        if (next_off + chunk > f->file_size) {
            f->file_size = next_off + chunk;
        }

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
        memset(generic_io_block, 0, sizeof(generic_io_block));
        ret = read_block(phy_blk, 0, generic_io_block, sizeof(generic_io_block));
        if (ret != sizeof(generic_io_block)) { // 写IO失败
            return -1;
        }

        /* 拷贝数据 */
        memcpy(pbuf, generic_io_block + blk_off, chunk);

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
