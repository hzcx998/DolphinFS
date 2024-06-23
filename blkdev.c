#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CONFIG_USE_DISK 1

struct ram_dev_data {
    unsigned char *block_ram;
};

struct ram_dev_data ram_data;

struct raw_file_disk_data {
    FILE *disk_fp;
};
struct raw_file_disk_data raw_file_data;

#define CONFIG_STATIC_RAM 0

#if CONFIG_STATIC_RAM == 1
unsigned char block_ram[BLOCK_DATA_SIZE] = {0};
#else
unsigned char *block_ram;
#endif

int open_ram(struct blkdev *bdev, int flags)
{
    struct ram_dev_data *data = (struct ram_dev_data *)bdev->data;

#if CONFIG_STATIC_RAM == 0
    data->block_ram = malloc(BLOCK_DATA_SIZE);
    if (!data->block_ram) {
        printf("alloc block data failed!\n");
        return -1;
    }
    memset(data->block_ram, 0, BLOCK_DATA_SIZE);
#else
    data->block_ram = block_ram;
#endif
    return 0;
}

int open_disk(struct blkdev *bdev, int flags)
{
    struct raw_file_disk_data *data = (struct raw_file_disk_data *)bdev->data;

    data->disk_fp = fopen("disk.vhd", "r+b");
    if (!data->disk_fp) {
        printf("disk file 'disk.vhd' not found!\n");
        return -1;
    }

    return 0;
}

int close_ram(struct blkdev *bdev)
{
    struct ram_dev_data *data = (struct ram_dev_data *)bdev->data;

#if CONFIG_STATIC_RAM == 0
    free(data->block_ram);
#endif
    return 0;
}

int close_disk(struct blkdev *bdev)
{    
    struct raw_file_disk_data *data = (struct raw_file_disk_data *)bdev->data;

    fclose(data->disk_fp);
    data->disk_fp = NULL;
    return 0;
}

int read_ram(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors)
{
    struct ram_dev_data *data = (struct ram_dev_data *)bdev->data;

    unsigned char *pblk = (unsigned char *)&data->block_ram[lba * bdev->blksz];

    memcpy(buf, pblk, bdev->blksz);

    return 0;
}

int write_ram(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors)
{    
    struct ram_dev_data *data = (struct ram_dev_data *)bdev->data;

    unsigned char *pblk = (unsigned char *)&data->block_ram[lba * bdev->blksz];

    memcpy(pblk, buf, bdev->blksz);

    return 0;
}

int read_disk(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors)
{
    struct raw_file_disk_data *data = (struct raw_file_disk_data *)bdev->data;

    fseek(data->disk_fp, lba * bdev->blksz, SEEK_SET);
    fread(buf, 1, bdev->blksz, data->disk_fp);

    return 0;
}

int write_disk(struct blkdev *bdev, unsigned long lba, void *buf, unsigned long sectors)
{    
    struct raw_file_disk_data *data = (struct raw_file_disk_data *)bdev->data;

    fseek(data->disk_fp, lba * bdev->blksz, SEEK_SET);
    fwrite(buf, 1, bdev->blksz, data->disk_fp);

    return 0;
}

long get_capacity(struct blkdev *bdev)
{
    return bdev->blkcnt;
}

__IO long write_block(struct blkdev *bdev, unsigned long blk, unsigned long off, void *buf, long len)
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
        bdev->write(bdev, blk * nsectors + i, sec_buf, 1);
        p += SECTOR_SIZE;
    }

    return len;
}

__IO long read_block(struct blkdev *bdev, unsigned long blk, unsigned long off, void *buf, long len)
{
    int i;
    unsigned char *p = buf;
    if (blk >= MAX_BLOCK_NR)
        return -1;

    unsigned char sec_buf[SECTOR_SIZE];
    memset(sec_buf, 0, SECTOR_SIZE);

    int nsectors = BLOCK_SIZE / SECTOR_SIZE;

    for (i = 0; i < nsectors; i++) {
        bdev->read(bdev, blk * nsectors + i, sec_buf, 1);
        memcpy(p, sec_buf, SECTOR_SIZE);
        p += SECTOR_SIZE;
    }

    return len;
}

static struct blkdev ram_bdev = {
    .name = "ram",
    .blksz = SECTOR_SIZE,
    .blkcnt = MAX_BLOCK_NR,
    .data = &ram_data,
    .open = open_ram,
    .close = close_ram,
    .read = read_ram,
    .write = write_ram,
};

static struct blkdev raw_file_disk_bdev = {
    .name = "disk",
    .blksz = SECTOR_SIZE,
    .blkcnt = MAX_BLOCK_NR,
    .data = &raw_file_data,
    .open = open_disk,
    .close = close_disk,
    .read = read_disk,
    .write = write_disk,
};

static struct blkdev *blk_dev[MAX_BLOCK_DEV_NR] = {NULL,};

int add_blkdev(struct blkdev *bdev)
{
    int i;
    for (i = 0; i < MAX_BLOCK_DEV_NR; i++) {
        if (blk_dev[i] == NULL) {
            bdev->ref = 0;
            blk_dev[i] = bdev;
            return 0;
        }
    }
    return -1;
}

int del_blkdev(struct blkdev *bdev)
{
    int i;
    for (i = 0; i < MAX_BLOCK_DEV_NR; i++) {
        if (blk_dev[i] == bdev) {
            blk_dev[i] = NULL;
            return 0;
        }
    }
    return -1;
}

void list_blkdev(void)
{
    int i;
    struct blkdev *bdev;
    for (i = 0; i < MAX_BLOCK_DEV_NR; i++) {
        bdev = blk_dev[i];
        if (bdev != NULL) {
            printf("dev name:%s capacity:%d, blksz:%d read:%s, write:%s\n",
                bdev->name, bdev->blkcnt, bdev->blksz, bdev->read ? "Y" : "N", bdev->write ? "Y" : "N");
        }
    }
    return -1;
}

struct blkdev *search_blkdev(char *name)
{
    int i;
    struct blkdev *bdev;
    for (i = 0; i < MAX_BLOCK_DEV_NR; i++) {
        bdev = blk_dev[i];
        if (bdev != NULL) {
            if (!strcmp(bdev->name, name)) {
                return bdev;
            }
        }
    }
    return NULL;
}

void init_blkdev(void)
{
    add_blkdev(&ram_bdev);
    add_blkdev(&raw_file_disk_bdev);
}

void exit_blkdev(void)
{
    del_blkdev(&ram_bdev);
    del_blkdev(&raw_file_disk_bdev);
}

int open_blkdev(struct blkdev *bdev, int flags)
{
    int ret;
    if (!bdev) {
        return -1;
    }

    if (bdev->ref > 0) {
        bdev->ref++;
        return 0;
    }

    if (bdev->open) {    
        ret = bdev->open(bdev, flags);
        if (ret) {
            return ret;
        }
    }
    
    bdev->ref++;
    return 0;
}

int close_blkdev(struct blkdev *bdev)
{
    int ret;

    if (!bdev) {
        return -1;
    }

    if (bdev->ref <= 0) {
        return -1;
    }

    if (bdev->close && bdev->ref == 1) {
        ret = bdev->close(bdev);
        if (ret) {
            return ret;
        }
    }
    
    bdev->ref--;

    return 0;
}
