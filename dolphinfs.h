#ifndef _DOLPHINFS_H
#define _DOLPHINFS_H

#include <blkdev.h>
#include <fsblk.h>
#include <file.h>
#include <blkdev.h>

#define min(x, y) (x) < (y) ? (x) : (y)
#define max(x, y) (x) > (y) ? (x) : (y)

/*
目录结构，只是名字带有目录，将文件名以目录的方式组合在一起。
可以提供更高一层的api取实现目录。
*/

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

int dolphin_mkfs(char *disk);
int dolphin_mount(char *disk, struct super_block *sb);

#endif
