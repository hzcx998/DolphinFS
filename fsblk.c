#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct super_block dolphin_sb;

char generic_io_block[BLOCK_SIZE];
char allocator_io_block[BLOCK_SIZE];

unsigned long scan_free_bits(unsigned char *buf, unsigned long size)
{
    int i, j;
    for (i = 0; i < size; i++) {
        if (buf[i] != 0xff) {
            for (j = 0; j < 8; j++) {
                if (!(buf[i] & (1 << j))) {
                    buf[i] |= (1 << j);
                    return i * 8 + j;
                }
            }
        }
    }
    return size * 8;
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
        memset(allocator_io_block, 0, sizeof(allocator_io_block));
        read_block(next, 0, allocator_io_block, sizeof(allocator_io_block));
        
        data_block_id = scan_free_bits(allocator_io_block, sb->block_size);
        /* 扫描成功 */
        if (sb->block_size * 8 != data_block_id) {
            // printf("---> get block id:%ld\n", data_block_id);
            /* 如果不是第一个块，就乘以块偏移 */
            if ((next - start) != 0) 
                data_block_id += (next - start) * sb->block_size * 8;  /* 一个块标记4096*8个位 */
            /* 标记下一个空闲块 */
            sb->next_free_man_block = next;
            
            write_block(next, 0, allocator_io_block, sizeof(allocator_io_block));

            // printf("---> alloc block:%ld\n", data_block_id + sb->block_off[BLOCK_AREA_DATA]);
            if (data_block_id < sb->block_nr[BLOCK_AREA_DATA]) {

                /* 更新超级块 */
                memset(allocator_io_block, 0, sizeof(allocator_io_block));
                memcpy(allocator_io_block, sb, sizeof(*sb));
                write_block(sb->block_off[BLOCK_AREA_SB], 0, allocator_io_block, sizeof(allocator_io_block));

                return data_block_id + sb->block_off[BLOCK_AREA_DATA]; // 返回的块是绝对块地址
            } else {
                return 0;
            }
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

    memset(allocator_io_block, 0, sizeof(allocator_io_block));
    read_block(man_block, 0, allocator_io_block, sizeof(allocator_io_block));

    allocator_io_block[byte_off] &= ~(1 << bits_off);
    /* 修改块 */
    write_block(man_block, 0, allocator_io_block, sizeof(allocator_io_block));

    return 0;
}

void init_sb(struct super_block *sb, unsigned long capacity, unsigned long block_size)
{
    sb->magic = SUPER_BLOCK_MAGIC;
    sb->capacity = capacity;
    sb->block_size = block_size;
    
    sb->block_nr[BLOCK_AREA_SB] = 1;
    sb->block_nr[BLOCK_AREA_MAN] = DIV_ROUND_UP(DIV_ROUND_UP(capacity, 8), block_size);
    sb->block_nr[BLOCK_AREA_DATA] = capacity  - sb->block_nr[BLOCK_AREA_MAN] - sb->block_nr[BLOCK_AREA_SB];

    sb->block_off[BLOCK_AREA_SB] = 0;
    sb->block_off[BLOCK_AREA_MAN] = sb->block_off[BLOCK_AREA_SB] + sb->block_nr[BLOCK_AREA_SB];
    sb->block_off[BLOCK_AREA_DATA] = sb->block_off[BLOCK_AREA_MAN] + sb->block_nr[BLOCK_AREA_MAN];

    sb->next_free_man_block = 0; /* invalid */
}

void dump_sb(struct super_block *sb)
{
    printf("==== sb info ====\n");
    printf("magic: %x\n", sb->magic);
    printf("capacity: %ld\n", sb->capacity);
    printf("block_size: %ld\n", sb->block_size);
    printf("block_nr: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_nr[BLOCK_AREA_SB], sb->block_nr[BLOCK_AREA_MAN], sb->block_nr[BLOCK_AREA_DATA]);
    printf("block_off: SB %ld, MAN %ld, DATA %ld\n", 
        sb->block_off[BLOCK_AREA_SB], sb->block_off[BLOCK_AREA_MAN], sb->block_off[BLOCK_AREA_DATA]);

    printf("next_free_man_block: %ld\n", sb->next_free_man_block);
    printf("file_bitmap_start: %ld, file_bitmap_end: %ld\n", sb->file_bitmap_start, sb->file_bitmap_end);
    printf("file_info_start: %ld, file_info_end: %ld\n", sb->file_info_start, sb->file_info_end);
    printf("file_name_start: %ld, file_name_end: %ld\n", sb->file_name_start, sb->file_name_end);
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

void init_file_info(struct super_block *sb, unsigned long file_count)
{
    /* 
    根据文件数量创建文件节点，并通过位图来管理文件节点的分配状态。
    也就是需要要创建2组内容，一组是管理文件分配的位图块。一组是管理文件内容的数据块。
     */

    /**
     * 计算文件信息占用的空间
     */
    unsigned long file_info_blocks = DIV_ROUND_UP(file_count * sizeof(struct file), BLOCK_SIZE);
    assert(file_info_blocks > 0);

    /**
     * 计算文件分配位图
     */
    unsigned long file_bmap_blocks = DIV_ROUND_UP(DIV_ROUND_UP(file_info_blocks, 8), BLOCK_SIZE);
    assert(file_bmap_blocks > 0);

    /**
     * 计算文件名块数量
     */
    unsigned long file_name_blocks = DIV_ROUND_UP(file_count * sizeof(struct file_name), BLOCK_SIZE);
    assert(file_name_blocks > 0);

    sb->file_count = file_count;
    /**
     * 分配文件位块
     */
    long block;
    int i;

    sb->file_bitmap_start = 0;
    for (i = 0; i < file_bmap_blocks; i++) {
        block = alloc_data_block();
        assert(block > 0);
        if (!sb->file_bitmap_start) {
            sb->file_bitmap_start = block;
        }
    }
    sb->file_bitmap_end = sb->file_bitmap_start + file_bmap_blocks;
    
    memset(generic_io_block, 0, sizeof(generic_io_block));
    for (i = 0; i < file_bmap_blocks; i++) {
        write_block(sb->file_bitmap_start + i, 0, generic_io_block, sizeof(generic_io_block));
    }

    /**
     * 分配文件信息块
     */
    sb->file_info_start = 0;
    for (i = 0; i < file_info_blocks; i++) {
        block = alloc_data_block();
        assert(block > 0);
        if (!sb->file_info_start) {
            sb->file_info_start = block;
        }
    }
    sb->file_info_end = sb->file_info_start + file_info_blocks;
    for (i = 0; i < file_info_blocks; i++) {
        write_block(sb->file_info_start + i, 0, generic_io_block, sizeof(generic_io_block));
    }

    /**
     * 分配文件名字块
     */
    sb->file_name_start = 0;
    for (i = 0; i < file_name_blocks; i++) {
        block = alloc_data_block();
        assert(block > 0);
        if (!sb->file_name_start) {
            sb->file_name_start = block;
        }
    }
    sb->file_name_end = sb->file_name_start + file_name_blocks;
    for (i = 0; i < file_name_blocks; i++) {
        write_block(sb->file_name_start + i, 0, generic_io_block, sizeof(generic_io_block));
    }
}
