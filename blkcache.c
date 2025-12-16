#include <blkcache.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * 全局缓存实例
 */
static struct blk_cache cache;

/**
 * 查找缓存项
 */
static struct blk_cache_entry *find_cache_entry(struct blkdev *bdev, unsigned long block_num)
{
    struct blk_cache_entry *entry = cache.head;
    while (entry) {
        if (entry->valid && entry->bdev == bdev && entry->block_num == block_num) {
            // 更新访问时间
            entry->access_time = ++cache.timestamp;
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * 将缓存项移到LRU链表头部
 */
static void move_to_head(struct blk_cache_entry *entry)
{
    if (entry == cache.head) {
        return; // 已经在头部
    }
    
    // 从当前位置移除
    struct blk_cache_entry *prev = cache.head;
    while (prev && prev->next != entry) {
        prev = prev->next;
    }
    
    if (prev) {
        prev->next = entry->next;
    }
    
    // 添加到头部
    entry->next = cache.head;
    cache.head = entry;
}

/**
 * 分配新的缓存项
 */
static struct blk_cache_entry *allocate_cache_entry(void)
{
    struct blk_cache_entry *entry = NULL;
    
    // 如果有未使用的项，直接分配
    if (cache.entry_count < BLK_CACHE_SIZE) {
        entry = &cache.entries[cache.entry_count];
        entry->data = malloc(BLOCK_SIZE);
        if (!entry->data) {
            return NULL;
        }
        cache.entry_count++;
        memset(entry->data, 0, BLOCK_SIZE);
    } else {
        // 查找最久未访问的项（LRU）
        struct blk_cache_entry *lru_entry = cache.head;
        struct blk_cache_entry *prev_lru = NULL;
        struct blk_cache_entry *prev = NULL;
        struct blk_cache_entry *curr = cache.head;
        
        while (curr) {
            if (curr->valid && (!lru_entry || curr->access_time < lru_entry->access_time)) {
                lru_entry = curr;
                prev_lru = prev;
            }
            prev = curr;
            curr = curr->next;
        }
        
        if (!lru_entry) {
            return NULL;
        }
        
        // 如果是脏的，写回磁盘
        if (lru_entry->dirty) {
            write_block(lru_entry->bdev, lru_entry->block_num, 0, lru_entry->data, BLOCK_SIZE);
            lru_entry->dirty = 0;
        }
        
        // 重置该缓存项
        lru_entry->valid = 0;
        lru_entry->dirty = 0;
        entry = lru_entry;
        
        // 从链表中移除
        if (prev_lru) {
            prev_lru->next = lru_entry->next;
        } else {
            cache.head = lru_entry->next;
        }
    }
    
    // 初始化新项
    entry->valid = 1;
    entry->dirty = 0;
    entry->access_time = ++cache.timestamp;
    entry->bdev = NULL;
    entry->block_num = 0;
    entry->next = cache.head;
    cache.head = entry;
    
    return entry;
}

/**
 * 初始化块缓存
 */
int blk_cache_init(void)
{
    cache.entries = malloc(sizeof(struct blk_cache_entry) * BLK_CACHE_SIZE);
    if (!cache.entries) {
        return -1;
    }
    
    memset(cache.entries, 0, sizeof(struct blk_cache_entry) * BLK_CACHE_SIZE);
    cache.head = NULL;
    cache.entry_count = 0;
    cache.timestamp = 0;
    
    return 0;
}

/**
 * 清理块缓存
 */
void blk_cache_exit(void)
{
    // 同步所有脏块
    blk_cache_sync(NULL);
    
    // 释放所有内存
    for (int i = 0; i < cache.entry_count; i++) {
        if (cache.entries[i].data) {
            free(cache.entries[i].data);
        }
    }
    
    free(cache.entries);
    memset(&cache, 0, sizeof(cache));
}

/**
 * 使用缓存读取块
 */
int blk_cache_read(struct blkdev *bdev, unsigned long block_num, void *buf, long len)
{
    if (!bdev || !buf || len <= 0 || len > BLOCK_SIZE) {
        return -1;
    }
    
    // 查找缓存项
    struct blk_cache_entry *entry = find_cache_entry(bdev, block_num);
    if (entry) {
        // 缓存命中
        memcpy(buf, entry->data, len);
        move_to_head(entry);
        return len;
    }
    
    // 缓存未命中，分配新项
    entry = allocate_cache_entry();
    if (!entry) {
        // 缓存分配失败，直接从磁盘读取
        return read_block(bdev, block_num, 0, buf, len);
    }
    
    // 从磁盘读取数据到缓存
    if (read_block(bdev, block_num, 0, entry->data, BLOCK_SIZE) != BLOCK_SIZE) {
        // 读取失败，标记为无效
        entry->valid = 0;
        return -1;
    }
    
    // 设置缓存项
    entry->bdev = bdev;
    entry->block_num = block_num;
    
    // 复制数据到用户缓冲区
    memcpy(buf, entry->data, len);
    
    return len;
}

/**
 * 使用缓存写入块
 */
int blk_cache_write(struct blkdev *bdev, unsigned long block_num, void *buf, long len)
{
    if (!bdev || !buf || len <= 0 || len > BLOCK_SIZE) {
        return -1;
    }
    
    // 查找缓存项
    struct blk_cache_entry *entry = find_cache_entry(bdev, block_num);
    if (entry) {
        // 缓存命中
        memcpy(entry->data, buf, len);
        entry->dirty = 1;
        move_to_head(entry);
        return len;
    }
    
    // 缓存未命中，分配新项
    entry = allocate_cache_entry();
    if (!entry) {
        // 缓存分配失败，直接写入磁盘
        return write_block(bdev, block_num, 0, buf, len);
    }
    
    // 设置缓存项
    entry->bdev = bdev;
    entry->block_num = block_num;
    
    // 先读取整个块（保持一致性）
    if (read_block(bdev, block_num, 0, entry->data, BLOCK_SIZE) != BLOCK_SIZE) {
        entry->valid = 0;
        return -1;
    }
    
    // 写入新数据
    memcpy(entry->data, buf, len);
    entry->dirty = 1;
    
    return len;
}

/**
 * 同步所有脏块
 */
int blk_cache_sync(struct blkdev *bdev)
{
    int sync_count = 0;
    
    for (int i = 0; i < cache.entry_count; i++) {
        struct blk_cache_entry *entry = &cache.entries[i];
        if (entry->valid && entry->dirty) {
            // 如果指定了设备，只同步该设备的块
            if (bdev && entry->bdev != bdev) {
                continue;
            }

            // 写入磁盘
            if (write_block(entry->bdev, entry->block_num, 0, entry->data, BLOCK_SIZE) == BLOCK_SIZE) {
                entry->dirty = 0;
                sync_count++;
            }
        }
    }
    
    return sync_count;
}
