#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern char generic_io_block[BLOCK_SIZE];

extern struct super_block dolphin_sb;

int dolphin_mkfs(char *disk)
{
    /* init sb info */
    init_sb(&dolphin_sb, get_capacity(), BLOCK_SIZE);

    dump_sb(&dolphin_sb);

    /* init man info */
    init_man(&dolphin_sb);

    /* init file info */
    init_file_info(&dolphin_sb, MAX_FILES);

    /* write sb info to disk */
    memset(generic_io_block, 0, sizeof(generic_io_block));
    memcpy(generic_io_block, &dolphin_sb, sizeof(dolphin_sb));
    write_block(dolphin_sb.block_off[BLOCK_AREA_SB], 0, generic_io_block, sizeof(generic_io_block));

    dump_sb(&dolphin_sb);

    return 0;
}

int dolphin_mount(char *disk, struct super_block *sb)
{
    memset(generic_io_block, 0, sizeof(generic_io_block));
    read_block(0, 0, generic_io_block, sizeof(generic_io_block));
    memcpy(sb, generic_io_block, sizeof(struct super_block));

    dump_sb(sb);

    return 0;
}
