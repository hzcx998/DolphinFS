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

#endif
