#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CONFIG_USE_DISK 1

#define CONFIG_STATIC_RAM 0

#if CONFIG_STATIC_RAM == 1
unsigned char block_ram[BLOCK_DATA_SIZE] = {0};
#else
unsigned char *block_ram;
#endif

int open_ram()
{
#if CONFIG_STATIC_RAM == 0
    block_ram = malloc(BLOCK_DATA_SIZE);
    if (!block_ram) {
        printf("alloc block data failed!\n");
        return -1;
    }
    memset(block_ram, 0, BLOCK_DATA_SIZE);
#endif
    return 0;
}

FILE *disk_fp;

int open_disk()
{
    disk_fp = fopen("disk.vhd", "r+b");
    if (!disk_fp) {
        printf("disk file 'disk.vhd' not found!\n");
        return -1;
    }

    return 0;
}

int close_ram()
{
#if CONFIG_STATIC_RAM == 0
    free(block_ram);
#endif
    return 0;
}

int close_disk()
{
    fclose(disk_fp);
    return 0;
}

int get_ram_sector_size(void)
{
    return SECTOR_SIZE;
}

int read_ram(unsigned long lba, void *buf, unsigned long sectors)
{
    unsigned char *pblk = (unsigned char *)&block_ram[lba * get_ram_sector_size()];

    memcpy(buf, pblk, get_ram_sector_size());

    return 0;
}

int write_ram(unsigned long lba, void *buf, unsigned long sectors)
{    
    unsigned char *pblk = (unsigned char *)&block_ram[lba * get_ram_sector_size()];

    memcpy(pblk, buf, get_ram_sector_size());

    return 0;
}

int read_disk(unsigned long lba, void *buf, unsigned long sectors)
{
    fseek(disk_fp, lba * get_ram_sector_size(), SEEK_SET);
    fread(buf, 1, get_ram_sector_size(), disk_fp);

    return 0;
}

int write_disk(unsigned long lba, void *buf, unsigned long sectors)
{    
    fseek(disk_fp, lba * get_ram_sector_size(), SEEK_SET);
    fwrite(buf, 1, get_ram_sector_size(), disk_fp);

    return 0;
}

long get_capacity(void)
{
    return MAX_BLOCK_NR;
}

__IO long write_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    int i;
    unsigned char *p = buf;

    if (blk >= MAX_BLOCK_NR)
        return -1;

    unsigned char sec_buf[SECTOR_SIZE];
    memset(sec_buf, 0, SECTOR_SIZE);

    int nsectors = BLOCK_SIZE / SECTOR_SIZE;

    for (i = 0; i < nsectors; i++) {
        memcpy(sec_buf, p, SECTOR_SIZE);
#if CONFIG_USE_DISK == 1
        write_disk(blk * nsectors + i, sec_buf, 1);
#else
        write_ram(blk * nsectors + i, sec_buf, 1);
#endif
        p += SECTOR_SIZE;
    }

    return len;
}

__IO long read_block(unsigned long blk, unsigned long off, void *buf, long len)
{
    int i;
    unsigned char *p = buf;
    if (blk >= MAX_BLOCK_NR)
        return -1;

    unsigned char sec_buf[SECTOR_SIZE];
    memset(sec_buf, 0, SECTOR_SIZE);

    int nsectors = BLOCK_SIZE / SECTOR_SIZE;

    for (i = 0; i < nsectors; i++) {
#if CONFIG_USE_DISK == 1
        read_disk(blk * nsectors + i, sec_buf, 1);
#else
        read_ram(blk * nsectors + i, sec_buf, 1);
#endif
        memcpy(p, sec_buf, SECTOR_SIZE);
        p += SECTOR_SIZE;
    }

    return len;
}

int open_blkdev()
{
#if CONFIG_USE_DISK == 1
    open_disk();
#else
    open_ram();
#endif
}

int close_blkdev()
{
#if CONFIG_USE_DISK == 1
    close_disk();
#else
    close_ram();
#endif
}
