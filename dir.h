#ifndef _DIR_H
#define _DIR_H

#include "fsconfig.h"
#include "file.h"
#include "fsblk.h"

// 更简单点，一个目录就是一个文件，有多少个子目录。就有多少个文件。
// 目录名字为256，所以每次读取都是按照一个目录格式读取。

struct fs_dir_data {
    int num; /* 文件编号 */
};

struct fs_dir_entry {
    struct file_name fname;
    int num;
    unsigned char type; // dir or file?
};

struct fs_dir_head {
    int count; // dir count
    int max_index;
};
struct fs_dir_body {
#define DIR_BITMAP_ITEM_SIZE ((sizeof(unsigned int) * 8))
#define DIR_BITMAP_SIZE (MAX_FILES / DIR_BITMAP_ITEM_SIZE)
    unsigned int bitmap[DIR_BITMAP_SIZE];
};

struct fs_dir_meta {
    struct fs_dir_head head;
    struct fs_dir_body body;
};

struct fs_dir_open {
    int fd;
    long offset; // seek pos
    struct super_block *sb;
};

int create_dir(struct super_block *sb, const char *path);
int delete_dir(struct super_block *sb, const char *path);

int open_dir(struct super_block *sb, const char *path);
int seek_dir(int dir, int pos);
int read_dir(int dir, struct fs_dir_entry *de);
int close_dir(int dir);

int list_dir(struct super_block *sb, const char *path);

void init_dir_open(void);

#endif
