#ifndef _BLKCACHE_H
#define _BLKCACHE_H

#include "blkdev.h"
#include "fsconfig.h"

/**
 * 简单的块缓存项结构
 */
struct blk_cache_entry {
    struct blkdev *bdev;          // 块设备
    unsigned long block_num;      // 块号
    unsigned char *data;          // 块数据
    int dirty;                    // 脏标志
    int valid;                    // 有效标志
    unsigned long access_time;     // 访问时间，用于LRU
    struct blk_cache_entry *next; // 单向链表
};

/**
 * 简单的块缓存控制结构
 */
struct blk_cache {
    struct blk_cache_entry *entries; // 缓存项数组
    struct blk_cache_entry *head;     // LRU链表头
    int entry_count;                  // 已使用项数
    unsigned long timestamp;          // 时间戳计数器
};

/**
 * 缓存操作函数
 */
extern int blk_cache_init(void);
extern void blk_cache_exit(void);
extern int blk_cache_read(struct blkdev *bdev, unsigned long block_num, void *buf, long len);
extern int blk_cache_write(struct blkdev *bdev, unsigned long block_num, void *buf, long len);
extern int blk_cache_sync(struct blkdev *bdev);

#endif /* _BLKCACHE_H */