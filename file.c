#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern char generic_io_block[BLOCK_SIZE];
extern char allocator_io_block[BLOCK_SIZE];

struct ofile open_files[MAX_OPEN_FILE];

long free_file_block(struct file *f, unsigned long off);
long walk_file_block(struct file *f, unsigned long off);

long alloc_file_num(struct super_block *sb)
{
    /* walk all man block */

    unsigned long start, end, next;
    unsigned long file_num;

    start = sb->file_bitmap_start;
    end = sb->file_bitmap_end;
    
    next = start;

    for (; next < end; next++) {
        memset(allocator_io_block, 0, sizeof(allocator_io_block));
        read_block(sb->blkdev, next, 0, allocator_io_block, sizeof(allocator_io_block));
        
        file_num = scan_free_bits(allocator_io_block, sb->block_size);
        /* 扫描成功 */
        if (sb->block_size * 8 != file_num) {
            // printf("---> get block id:%ld\n", data_block_id);
            /* 如果不是第一个块，就乘以块偏移 */
            if ((next - start) != 0) 
                file_num += (next - start) * (sb->block_size * 8); /* 一个块标记4096*8个位 */

            write_block(sb->blkdev, next, 0, allocator_io_block, sizeof(allocator_io_block));

            // printf("---> alloc block:%ld\n", data_block_id + sb->block_off[BLOCK_AREA_DATA]);
            if (file_num < sb->file_count) {
                return file_num;
            } else {
                return -1;
            }
        }
    }

    printf("!!!WARN no free file\n");
    return -1;
}

int free_file_num(struct super_block *sb, long file_num)
{
    unsigned long bmap_block, byte_off, bits_off;;
    unsigned long data_block_id, data_block_byte;

    // printf("---> free block:%ld\n", blk);

    if (file_num < 0 || file_num >= sb->file_count)
        return -1;
    
    /* 获取位偏移 */
    bits_off = file_num % 8;

    data_block_byte = file_num / 8;

    /* 获取块内字节偏移 */
    byte_off = data_block_byte % sb->block_size;
    /* 获取块偏移 */
    bmap_block = data_block_byte / sb->block_size;

    /* 转换出逻辑块 */
    bmap_block += sb->file_bitmap_start;

    memset(allocator_io_block, 0, sizeof(allocator_io_block));
    read_block(sb->blkdev, bmap_block, 0, allocator_io_block, sizeof(allocator_io_block));

    allocator_io_block[byte_off] &= ~(1 << bits_off);
    /* 修改块 */
    write_block(sb->blkdev, bmap_block, 0, allocator_io_block, sizeof(allocator_io_block));

    return 0;
}

int load_file_info(struct super_block *sb, struct file *file, long num)
{
    struct file *file_table;
    unsigned long file_info_block, block_off;
    unsigned long file_count_in_block;

    file_count_in_block = sb->block_size / sizeof(struct file);

    block_off = num % file_count_in_block;
    file_info_block = num / file_count_in_block;
    file_info_block += sb->file_info_start;

    /* 通过file_info_block读取文件所在的块 */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, file_info_block, 0, generic_io_block, sizeof(generic_io_block));

    /* 通过block_off定位某个文件 */
    file_table = (struct file *)generic_io_block;
    *file = file_table[block_off];
    return 0;
}

static int sync_file_name(struct super_block *sb, unsigned long num, char *name)
{
    struct file_name *file_name;
    unsigned long file_name_block, block_off;
    unsigned long file_count_in_block;

    file_count_in_block = sb->block_size / sizeof(struct file_name);

    block_off = num % file_count_in_block;
    file_name_block = num / file_count_in_block;
    file_name_block += sb->file_name_start;

    /* 通过file_info_block读取文件所在的块 */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, file_name_block, 0, generic_io_block, sizeof(generic_io_block));

    /* 通过block_off定位某个文件 */
    file_name = (struct file_name *)generic_io_block;
    strncpy(file_name[block_off].buf, name, FILE_NAME_LEN);

    write_block(sb->blkdev, file_name_block, 0, generic_io_block, sizeof(generic_io_block));
}

static int load_file_name(struct super_block *sb, unsigned long num, char *name, int len)
{
    struct file_name *file_name;
    unsigned long file_name_block, block_off;
    unsigned long file_count_in_block;

    file_count_in_block = sb->block_size / sizeof(struct file_name);

    block_off = num % file_count_in_block;
    file_name_block = num / file_count_in_block;
    file_name_block += sb->file_name_start;

    /* 通过file_info_block读取文件所在的块 */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, file_name_block, 0, generic_io_block, sizeof(generic_io_block));

    /* 通过block_off定位某个文件 */
    file_name = (struct file_name *)generic_io_block;
    strncpy(name, file_name[block_off].buf, min(len, FILE_NAME_LEN));

    return 0;
}

long walk_file_name(struct super_block *sb, long file_num, void *buf, long len)
{
    /* 遍历到最大值或者最小值则返回-1，表示遍历完成 */
    if (file_num < 0 || file_num >= MAX_FILES)
        return -1;
    
    /* 检查file_num是否已经被分配 */

    load_file_name(sb, file_num, buf, len);

    /* 返回下一个可以检查的文件号 */
    return file_num + 1;
}

int sync_file_info(struct super_block *sb, struct file *file, long num)
{
    struct file *file_table;
    unsigned long file_info_block, block_off;
    unsigned long file_count_in_block;

    file_count_in_block = sb->block_size / sizeof(struct file);

    block_off = num % file_count_in_block;
    file_info_block = num / file_count_in_block;
    file_info_block += sb->file_info_start;

    /* 通过file_info_block读取文件所在的块 */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, file_info_block, 0, generic_io_block, sizeof(generic_io_block));

    /* 通过block_off定位某个文件 */
    file_table = (struct file *)generic_io_block;
    file_table[block_off] = *file;

    write_block(sb->blkdev, file_info_block, 0, generic_io_block, sizeof(generic_io_block));

    return 0;
}

/**
 * 分配一个文件号，并分配内存保存数据
 */
struct file *alloc_file(struct super_block *sb)
{
    long num = alloc_file_num(sb);
    if (num < 0) {
        return NULL;
    }
    /* load file num from disk */
    struct file *f = malloc(sizeof(struct file));
    f->num = num;
    f->sb = sb;
    return f;
}

/**
 * 释放文件号，并释放内存
 */
void free_file(struct file *file, int sync)
{
    if (sync) {
        struct file empty_file;
        memset(&empty_file, 0, sizeof(struct file));
        sync_file_info(file->sb, &empty_file, file->num);
        
        /* clear name */
        struct file_name file_name = {0};
        sync_file_name(file->sb, file->num, file_name.buf);
    }
    free_file_num(file->sb, file->num);

    free(file);
}

void dump_all_file(struct super_block *sb)
{
    int i;
    struct file *f;
    
    printf("==== dump files ====\n");
    struct file tmp_file;
    for (i = 0; i < sb->file_count; i++) {
        load_file_info(sb, &tmp_file, i);
        f = &tmp_file;
        if (f->hash != 0) {
            printf("  [%d] hash:%p, table:%lld, size:%lld, ref:%d, mode:%x\n",
                i, f->hash, f->data_table, f->file_size, f->ref, f->mode);
        }
    } 
}

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

static struct file *create_file(struct super_block *sb, char *path, int mode)
{
    if (!path || !strlen(path) || !mode)
        return NULL;

    struct file *f = alloc_file(sb);
    if (!f)
        return NULL;
    
    f->hash = str2hash(path);
    f->data_table = alloc_data_block(sb);
    f->ref = 0;
    f->mode = mode;
    f->file_size = 0;
    if (f->data_table < 0) {
        free_file(f, 0);
        return NULL;
    }

    /* NOTICE: clear data table */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    write_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    /* sync file to disk */
    sync_file_info(sb, f, f->num);

    /* sync file name to disk */
    sync_file_name(sb, f->num, path);

    return f;
}

static struct file *search_file(struct super_block *sb, char *path)
{
    if (!path || !strlen(path))
        return NULL;

    int i;
    struct file *f = malloc(sizeof(struct file));
    if (!f) {
        return NULL;
    }
    for (i = 0; i < sb->file_count; i++) {
        load_file_info(sb, f, i);
        if (f->hash == str2hash(path)) {
            f->sb = sb;
            return f;
        }
    }

    free(f);
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

int delete_file(struct super_block *sb, char *path)
{
    if (!path || !strlen(path))
        return -1;
    
    struct file *f = search_file(sb, path);
    if (!f) {
        printf("delete: err: file %s not found\n", path);
        return -1;
    }

    /* 1. 删除文件数据 */
    if (f->file_size > 0) {
        unsigned long blocks;
        blocks = DIV_ROUND_UP(f->file_size, sb->block_size);
        int i;

        for (i = 0; i < blocks; i++) {
            long ret = walk_file_block(f, i * sb->block_size);
            assert(ret > 0);
        }
        for (i = 0; i < blocks; i++) {
            long ret = free_file_block(f, i * sb->block_size);
            if (ret != 0) {
                printf("[ERR] delete_file: free block %d failed with %d\n", i, ret);
            }
        }
    }
    
    assert(f->data_table > 0);

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    assert(check_empty_u32(generic_io_block, sizeof(generic_io_block) / sizeof(unsigned int)));

    /* free data table */
    free_block(sb, f->data_table);

    /* 2. 删除文件结构 */
    free_file(f, 1);
    return 0;
}

int open_file(struct super_block *sb, char *path, int flags)
{
    if (!path || !strlen(path) || !flags)
        return -1;
    
    struct file *f = search_file(sb, path);
    if (!f) {
        /* file not found, create new */
        if (flags & FF_CRATE) {
            f = create_file(sb, path, flags & FF_RDWR);
            if (!f) {
                return -1;
            }
        } else {
            return -1;
        }
    }
    struct ofile *of = alloc_ofile();
    if (!of)
        return -1;

    of->f = f;
    of->oflags = flags;
    of->off = 0;
    of->sb = sb;

    f->ref++;
    sync_file_info(sb, f, f->num);

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
    sync_file_info(of->f->sb, of->f, of->f->num);
    /* 释放文件信息内存 */
    free(of->f);
    of->f = NULL;

    return 0;
}

long get_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2;
    unsigned int l1_val, l2_val;
    struct super_block *sb = f->sb;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    assert(l1 != NULL);

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // alloc BMD block
        l1_val = alloc_data_block(sb);
        l1[GET_BGD_OFF(off)] = l1_val;
        // sync block
        write_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

        // clear new block   
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // alloc data block
        l2_val = alloc_data_block(sb);
        l2[GET_BMD_OFF(off)] = l2_val;
        // sync block
        write_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));

        // clear new block   
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(sb->blkdev, l2_val, 0, generic_io_block, sizeof(generic_io_block));
    }

    return l2_val;
}

long walk_file_block(struct file *f, unsigned long off)
{
    unsigned int *l1, *l2;
    unsigned int l1_val, l2_val;
    struct super_block *sb = f->sb;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    assert(l1 != NULL);

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // alloc BMD block
        printf("ERR off:%d, l1_val:%d\n", off, l1_val);
        return 0;
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));

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
    struct super_block *sb = f->sb;

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;
    if (l1 == NULL) {
        return -1;
    }

    l1_val = l1[GET_BGD_OFF(off)];
    if (!l1_val) { // none BMD block
        return -2;
    }

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));

    l2 = (unsigned int *)generic_io_block;
    
    l2_val = l2[GET_BMD_OFF(off)];
    if (!l2_val) { // none data block
        return -3;
    }

    /* free data block */
    free_data_block(sb, l2_val);

    l2[GET_BMD_OFF(off)] = 0; /* clear data block in l2 */

    // sync block
    write_block(sb->blkdev, l1_val, 0, generic_io_block, sizeof(generic_io_block));

    /* check l2 empty? */
    if (!check_empty_u32(generic_io_block, sizeof(generic_io_block) / sizeof(unsigned int))) {
        return 0;
    }

    /* l2 empty !*/

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

    l1 = (unsigned int *)generic_io_block;

    /* free l2 block */
    free_block(sb, l1_val);

    // printf("%s:%d: free l2 block: %d at %d\n", __func__, __LINE__, l1_val, l2_val);

    l1[GET_BGD_OFF(off)] = 0; /* clear l2 block in l1 */

    // sync block
    write_block(sb->blkdev, f->data_table, 0, generic_io_block, sizeof(generic_io_block));

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
    struct super_block *sb = f->sb;

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
        ret = read_block(sb->blkdev, phy_blk, 0, generic_io_block, sizeof(generic_io_block));
        if (ret != sizeof(generic_io_block)) { // 写IO失败
            return -1;
        }

        /* 拷贝数据 */
        memcpy(generic_io_block + blk_off, pbuf, chunk);

        /* 将数据写入块 */
        ret = write_block(sb->blkdev, phy_blk, 0, generic_io_block, sizeof(generic_io_block));
        if (ret != sizeof(generic_io_block)) { // 写IO失败
            return -1;
        }

        /* 只有在偏移+chunk大小超过文件大小的时候，才去更新文件大小 */
        if (next_off + chunk > f->file_size) {
            f->file_size = next_off + chunk;
            sync_file_info(sb, f, f->num);
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
    struct super_block *sb = f->sb;

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
        ret = read_block(sb->blkdev, phy_blk, 0, generic_io_block, sizeof(generic_io_block));
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

long tell_file(int file)
{
    long ret;
    if (file < 0 || file >= MAX_OPEN_FILE)
        return -1;
    
    struct ofile *of = idx_ofile(file);
    struct file *f;
    if (!of->f)
        return -1;
    
    return of->off;
}

void list_files(struct super_block *sb)
{
    long idx, next;
    char name[FILE_NAME_LEN];

    idx = 0;

    while (idx >= 0) {
        memset(name, 0, sizeof(name));
        next = walk_file_name(sb, idx, name, FILE_NAME_LEN);
        if (name[0] != 0) {
            struct file_stat stat;
            stat_file(sb, name, &stat);
            printf("[%d] name:%s num: %d size: %d mode:%x\n", idx, name, stat.num, stat.file_size, stat.mode);
        }
        idx = next;
    }
}

int rename_file(struct super_block *sb, char *src_name, char *dest_name)
{
    struct file *f;
    
    if (!src_name || !dest_name) {
        return -1;
    }
    f = search_file(sb, src_name);
    if (!f) {
        return -1;
    }

    /* 修改名字 */
    sync_file_name(f->sb, f->num, dest_name);

    /* 修改名字hash */
    f->hash = str2hash(dest_name);
    sync_file_info(sb, f, f->num);

    return 0;
}

int stat_file(struct super_block *sb, char *path, struct file_stat *stat)
{
    struct file *f;
    
    if (!path) {
        return -1;
    }
    f = search_file(sb, path);
    if (!f) {
        return -1;
    }

    stat->file_size = f->file_size;
    stat->num = f->num;
    stat->mode = f->mode;
    
    return 0;
}
