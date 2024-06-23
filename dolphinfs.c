#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern char generic_io_block[BLOCK_SIZE];

extern struct super_block dolphin_sb;

int dolphin_mkfs(char *disk)
{
    struct blkdev *bdev = search_blkdev(disk);
    if (!bdev) {
        return -1;
    }

    if (open_blkdev(bdev, 0)) {
        return -1;
    }

    printf("mkfs on blkdev %s\n", bdev->name);

    /* init sb info */
    init_sb(&dolphin_sb, get_capacity(bdev), BLOCK_SIZE);
    dolphin_sb.blkdev = bdev;

    dump_sb(&dolphin_sb);

    /* init man info */
    init_man(&dolphin_sb);

    /* init file info */
    init_file_info(&dolphin_sb, MAX_FILES);

    /* write sb info to disk */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    memcpy(generic_io_block, &dolphin_sb, sizeof(dolphin_sb));
    write_block(bdev, dolphin_sb.block_off[BLOCK_AREA_SB], 0, generic_io_block, sizeof(generic_io_block));

    dump_sb(&dolphin_sb);

    close_blkdev(bdev);

    return 0;
}

int dolphin_mount(char *disk, struct super_block *sb)
{
    struct blkdev *bdev = search_blkdev(disk);
    if (!bdev) {
        return -1;
    }

    if (open_blkdev(bdev, 0)) {
        return -1;
    }

    printf("mount on blkdev %s\n", bdev->name);

    sb->blkdev = bdev;
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(bdev, 0, 0, generic_io_block, sizeof(generic_io_block));
    memcpy(sb, generic_io_block, sizeof(struct super_block));
    /* 再次读取设备 */
    sb->blkdev = bdev;
    dump_sb(sb);

    if (sb->magic != SUPER_BLOCK_MAGIC) {
        return -1;
    }

    return 0;
}

int dolphin_unmount(struct super_block *sb)
{
    /* sync all to disk */

    close_blkdev(sb->blkdev);
    sb->blkdev = NULL;
    return 0;
}
