#ifndef _BLKDEV_H
#define _BLKDEV_H

/**
 * 最大支持的块设备数量
 */
#define MAX_BLOCK_DEV_NR 2

/*
数据页表格式：
32位格式：10，10，12
*/
#define BLOCK_SHIFT 12 
#define BLOCK_SIZE (1 << BLOCK_SHIFT) /* 4kb block */
#define BLOCK_MASK (BLOCK_SIZE - 1)
#define GET_BLOCK_OFF(addr) (((addr) & BLOCK_MASK) >> 0) 

#define BMD_SHIFT (BLOCK_SHIFT + 10)
#define BMD_SIZE (1 << BMD_SHIFT)
#define BMD_MASK (BMD_SIZE - 1)
#define GET_BMD_OFF(addr) (((addr) & BMD_MASK) >> BLOCK_SHIFT) 

#define BGD_SHIFT (BMD_SHIFT + 10)
#define BGD_SIZE (1 << BGD_SHIFT)
#define BGD_MASK (BGD_SIZE - 1)
#define GET_BGD_OFF(addr) (((addr) & BGD_MASK) >> BMD_SHIFT) 

#define __IO

#define BLOCK_DATA_SIZE (128 * 1024 * 1024) // 128 MB
#define MAX_BLOCK_NR (BLOCK_DATA_SIZE / BLOCK_SIZE)

#define SECTOR_SIZE 512

struct blkdev {
    char *name;
    unsigned long blksz;
    unsigned long blkcnt;
    int (*open)(struct blkdev *bdev, int flags);
    int (*close)(struct blkdev *bdev);
    int (*read)(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors);
    int (*write)(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors);
    void *data;
    int ref;
};

extern int add_blkdev(struct blkdev *bdev);
extern int del_blkdev(struct blkdev *bdev);

extern int open_blkdev(struct blkdev *bdev, int flags);
extern int close_blkdev(struct blkdev *bdev);
extern __IO long write_block(struct blkdev *bdev, unsigned long blk, unsigned long off, void *buf, long len);
extern __IO long read_block(struct blkdev *bdev, unsigned long blk, unsigned long off, void *buf, long len);
extern long get_capacity(struct blkdev *bdev);
extern struct blkdev *search_blkdev(char *name);

void init_blkdev(void);
void exit_blkdev(void);
void list_blkdev(void);

#endif /* _BLKDEV_H */