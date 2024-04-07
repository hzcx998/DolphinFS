#ifndef _DOLPHINFS_H
#define _DOLPHINFS_H


/*
数据页表格式：
32位格式：10，10，12
*/
#define BLOCK_SHIFT 12 
#define BLOCK_SIZE (1 << BLOCK_SHIFT) /* 4kb block */
#define BLOCK_MASK (BLOCK_SIZE - 1)
#define GET_BLOCK_OFF(addr) (((addr) & BLOCK_MASK)) 

#define BMD_SHIFT (BLOCK_SHIFT + 10)
#define BMD_SIZE (1 << BMD_SHIFT)
#define BMD_MASK (BMD_SIZE - 1)
#define GET_BMD_OFF(addr) (((addr) & BMD_MASK) >> BLOCK_SHIFT) 

#define BGD_SHIFT (BMD_SHIFT + 10)
#define BGD_SIZE (1 << BGD_SHIFT)
#define BGD_MASK (BGD_SIZE - 1)
#define GET_BGD_OFF(addr) (((addr) & BGD_MASK) >> BMD_SHIFT) 

#define MAX_FILES 256
#define MAX_OPEN_FILE 32

/* file mode */
#define FM_READ 0X01
#define FM_WRITE 0X02
#define FM_RDWR (FM_READ | FM_WRITE)

/* file flags */
#define FF_READ 0X01
#define FF_WRITE 0X02
#define FF_RDWR (FF_READ | FF_WRITE)

#define FP_SET 1
#define FP_CUR 2
#define FP_END 3

#define __IO

#define BLOCK_DATA_SIZE (16 * 1024 * 1024) // 16 MB
#define MAX_BLOCK_NR (BLOCK_DATA_SIZE / BLOCK_SIZE)

#define min(x, y) (x) < (y) ? (x) : (y)
#define max(x, y) (x) > (y) ? (x) : (y)

/*
目录结构，只是名字带有目录，将文件名以目录的方式组合在一起。
可以提供更高一层的api取实现目录。
*/

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

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

__IO long write_block(unsigned long blk, unsigned long off, void *buf, long len);
__IO long read_block(unsigned long blk, unsigned long off, void *buf, long len);

#endif
