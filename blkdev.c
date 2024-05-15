#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char *block_ram;

int open_ram()
{
    block_ram = malloc(BLOCK_DATA_SIZE);
    if (!block_ram) {
        printf("alloc block data failed!\n");
        return -1;
    }
    return 0;
}

int close_ram()
{
    free(block_ram);
    return 0;
}

int get_ram_sector_size(void)
{
    return SECTOR_SIZE;
}

int read_ram(unsigned long lba, void *buf, unsigned long sectors)
{
    char *pblk = (char *)&block_ram[lba * get_ram_sector_size()];

    memcpy(buf, pblk, get_ram_sector_size());

    return 0;
}

int write_ram(unsigned long lba, void *buf, unsigned long sectors)
{    
    char *pblk = (char *)&block_ram[lba * get_ram_sector_size()];

    memcpy(pblk, buf, get_ram_sector_size());

    return 0;
}

long get_capacity(void)
{
    return MAX_BLOCK_NR;
}

__IO long write_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    int i;
    char *p = buf;
    if (blk >= MAX_BLOCK_NR)
        return -1;

    char sec_buf[SECTOR_SIZE];

    int nsectors = BLOCK_SIZE / SECTOR_SIZE;

    for (i = 0; i < nsectors; i++) {
        memcpy(sec_buf, p, SECTOR_SIZE);
        write_ram(blk * nsectors + i, sec_buf, 1);
        p += SECTOR_SIZE;
    }

    return len;
}

__IO long read_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    int i;
    char *p = buf;
    if (blk >= MAX_BLOCK_NR)
        return -1;

    char sec_buf[SECTOR_SIZE];

    int nsectors = BLOCK_SIZE / SECTOR_SIZE;

    for (i = 0; i < nsectors; i++) {
        read_ram(blk * nsectors + i, sec_buf, 1);
        memcpy(p, sec_buf, SECTOR_SIZE);
        p += SECTOR_SIZE;
    }

    return len;
}
