#ifndef _BLKDEV_H
#define _BLKDEV_H

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

#define __IO

#define BLOCK_DATA_SIZE (16 * 1024 * 1024) // 16 MB
#define MAX_BLOCK_NR (BLOCK_DATA_SIZE / BLOCK_SIZE)

__IO long write_block(unsigned long blk, unsigned long off, void *buf, long len);
__IO long read_block(unsigned long blk, unsigned long off, void *buf, long len);

#define SECTOR_SIZE 512

#endif /* _BLKDEV_H */