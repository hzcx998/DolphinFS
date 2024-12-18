#ifndef _FSBLK_H
#define _FSBLK_H

#define BLOCK_AREA_SB 0
#define BLOCK_AREA_MAN 1
#define BLOCK_AREA_DATA 2
#define BLOCK_AREA_MAX (BLOCK_AREA_DATA + 1)

#define SUPER_BLOCK_MAGIC 0x1a5b7c9d

struct blkdev;

/**
 * 块分区：
 * 总块：G
 * 超级块：1， S_OFF = 0
 * 管理块：N = G / 8 / BLOCK_SIZE， N_OFF = S_OFF + 1
 * 数据块：M = G - N - 1， M_OFF = N_OFF + N
 */
struct super_block
{
    unsigned int magic;
    unsigned long capacity;
    unsigned long block_size;

    unsigned long block_nr[BLOCK_AREA_MAX];
    unsigned long block_off[BLOCK_AREA_MAX];

    /**
     * 下一个可用空闲管理块
     */
    unsigned long next_free_man_block;
    /**
     * 支持的文件数量
     */
    unsigned long file_count;
    /**
     * 文件管理信息和位图
     */
    unsigned long file_bitmap_start;
    unsigned long file_bitmap_end;

    unsigned long file_info_start;
    unsigned long file_info_end;

    unsigned long file_name_start;
    unsigned long file_name_end;

    /* 以下是内存中的数据，在磁盘上无意义 */
    struct blkdev *blkdev;
};

long alloc_block(struct super_block *sb);
int free_block(struct super_block *sb, long blk);
#define alloc_data_block alloc_block
#define free_data_block free_block

void init_sb(struct super_block *sb, unsigned long capacity, unsigned long block_size);
void dump_sb(struct super_block *sb);
void init_man(struct super_block *sb);
void init_file_info(struct super_block *sb, unsigned long file_count);

#endif
