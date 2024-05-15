#ifndef _FSBLK_H
#define _FSBLK_H

#define BLOCK_AREA_SB 0
#define BLOCK_AREA_MAN 1
#define BLOCK_AREA_DATA 2
#define BLOCK_AREA_MAX (BLOCK_AREA_DATA + 1)

/**
 * 块分区：
 * 总块：G
 * 超级块：1， S_OFF = 0
 * 管理块：N = G / 8 / BLOCK_SIZE， N_OFF = S_OFF + 1
 * 数据块：M = G - N - 1， M_OFF = N_OFF + N
 */
struct super_block
{
    unsigned long capacity;
    unsigned long block_size;

    unsigned long block_nr[BLOCK_AREA_MAX];
    unsigned long block_off[BLOCK_AREA_MAX];

    /**
     * 下一个可用空闲管理块
     */
    unsigned long next_free_man_block;
};

long alloc_block(void);
int free_block(long blk);
#define alloc_data_block alloc_block

void init_sb(struct super_block *sb, unsigned long capacity, unsigned long block_size);
void dump_sb(struct super_block *sb);
void init_man(struct super_block *sb);

#endif
