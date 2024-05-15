#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct super_block dolphin_sb;

char generic_io_block[BLOCK_SIZE];

unsigned long scan_free_bits(unsigned long *buf, unsigned long size)
{
    int i, j;
    for (i = 0; i < size / sizeof(unsigned long); i++) {
        if (buf[i] != 0xffffffffffffffff) {
            for (j = 0; j < sizeof(unsigned long); j++) {
                if (!(buf[i] & (1 << j))) {
                    buf[i] |= (1 << j);
                    return i * sizeof(unsigned long) + j;
                }
            }
        }
    }
    return size;
}

long alloc_block(void)
{
    /* walk all man block */

    unsigned long start, end, next;
    unsigned long data_block_id;
    int scaned = 0;

    struct super_block *sb = &dolphin_sb;
    
    start = sb->block_off[BLOCK_AREA_MAN];
    end = start + sb->block_nr[BLOCK_AREA_MAN];
    
    next = start;
    /* fix next */
    if (sb->next_free_man_block != 0 && sb->next_free_man_block > start && sb->next_free_man_block < end)
        next = sb->next_free_man_block;

retry:
    for (; next < end; next++) {
        memset(generic_io_block, 0, sizeof(generic_io_block));
        read_block(next, 0, generic_io_block, sizeof(generic_io_block));
        
        data_block_id = scan_free_bits(generic_io_block, sb->block_size);
        /* 扫描成功 */
        if (sb->block_size != data_block_id) {
            // printf("---> get block id:%ld\n", data_block_id);
            /* 如果不是第一个块，就乘以块偏移 */
            if ((next - start) != 0) 
                data_block_id *= (next - start) * sb->block_size;
            /* 标记下一个空闲块 */
            sb->next_free_man_block = next;
            
            write_block(next, 0, generic_io_block, sizeof(generic_io_block));

            // printf("---> alloc block:%ld\n", data_block_id + sb->block_off[BLOCK_AREA_DATA]);
            return data_block_id + sb->block_off[BLOCK_AREA_DATA]; // 返回的块是绝对块地址
        }
    }

    /* 把后面的块都扫描完了，尝试从起始块开始扫描。 */
    next = start;

    if (!scaned) {
        scaned = 1;
        goto retry;
    }

    printf("!!!WARN no free block\n");
    /* 没有空闲块，返回0，由于0是超级快，可以表示无效 */
    return 0;
}

int free_block(long blk)
{
    unsigned long man_block, byte_off, bits_off;;
    unsigned long data_block_id, data_block_byte;
    struct super_block *sb = &dolphin_sb;
    
    // printf("---> free block:%ld\n", blk);

    if (blk < sb->block_off[BLOCK_AREA_DATA] || blk >= sb->block_off[BLOCK_AREA_DATA] + sb->block_nr[BLOCK_AREA_DATA])
        return -1;
    
    /* 获取blk对应的索引 */
    data_block_id = blk - sb->block_off[BLOCK_AREA_DATA];

    /* 获取位偏移 */
    bits_off = data_block_id % 8;

    data_block_byte = data_block_id / 8;

    /* 获取块内字节偏移 */
    byte_off = data_block_byte % sb->block_size;
    /* 获取块偏移 */
    man_block = data_block_byte / sb->block_size;

    /* 转换出逻辑块 */
    man_block += sb->block_off[BLOCK_AREA_MAN];

    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(man_block, 0, generic_io_block, sizeof(generic_io_block));

    generic_io_block[byte_off] &= ~(1 << bits_off);
    /* 修改块 */
    write_block(man_block, 0, generic_io_block, sizeof(generic_io_block));

    return 0;
}

void init_sb(struct super_block *sb, unsigned long capacity, unsigned long block_size)
{
    sb->capacity = capacity;
    sb->block_size = block_size;
    
    sb->block_nr[BLOCK_AREA_SB] = 1;
    sb->block_nr[BLOCK_AREA_MAN] = DIV_ROUND_UP(capacity / 8, block_size);
    sb->block_nr[BLOCK_AREA_DATA] = capacity  - sb->block_nr[BLOCK_AREA_MAN] - sb->block_nr[BLOCK_AREA_SB];

    sb->block_off[BLOCK_AREA_SB] = 0;
    sb->block_off[BLOCK_AREA_MAN] = sb->block_off[BLOCK_AREA_SB] + sb->block_nr[BLOCK_AREA_SB];
    sb->block_off[BLOCK_AREA_DATA] = sb->block_off[BLOCK_AREA_MAN] + sb->block_nr[BLOCK_AREA_MAN];

    sb->next_free_man_block = 0; /* invalid */
}

void dump_sb(struct super_block *sb)
{
    printf("==== sb info ====\n");
    printf("capacity: %ld\n", sb->capacity);
    printf("block_size: %ld\n", sb->block_size);
    printf("block_nr: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_nr[BLOCK_AREA_SB], sb->block_nr[BLOCK_AREA_MAN], sb->block_nr[BLOCK_AREA_DATA]);
    printf("block_off: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_off[BLOCK_AREA_SB], sb->block_off[BLOCK_AREA_MAN], sb->block_off[BLOCK_AREA_DATA]);

    printf("next_free_man_block: %ld\n", sb->next_free_man_block);
}

void init_man(struct super_block *sb)
{
    /* clear all man */
    unsigned long start, end;
    start = sb->block_off[BLOCK_AREA_MAN];
    end = start + sb->block_nr[BLOCK_AREA_MAN];
    for (; start < end; start++) {
        memset(generic_io_block, 0, sizeof(generic_io_block));
        write_block(start, 0, generic_io_block, sizeof(generic_io_block));
    }
}
