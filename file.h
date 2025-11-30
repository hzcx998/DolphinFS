#ifndef _FILE_H
#define _FILE_H

#include "fsconfig.h"

/* file mode */
#define FM_READ 0X01
#define FM_WRITE 0X02
#define FM_RDWR (FM_READ | FM_WRITE)

/* file flags */
#define FF_READ 0X01
#define FF_WRITE 0X02
#define FF_RDWR (FF_READ | FF_WRITE)
#define FF_CRATE 0X10
#define FF_DIR 0X20

#define FP_SET 1
#define FP_CUR 2
#define FP_END 3

struct file {
    unsigned long hash; /* name hash */
    long data_table;
    unsigned long file_size;
    int ref;
    int mode; /* r,w,rw */
    int num; /* 文件编号 */
    struct super_block *sb;
};

struct file_name {
    char buf[FILE_NAME_LEN];
};

struct file_stat {
    unsigned long file_size;
    int mode; /* r,w,rw */
    int num; /* 文件编号 */
};

/* open file */
struct ofile {
    struct file *f;
    struct super_block *sb;
    int oflags; /* open flags */
    long off;
};

void dump_all_file(struct super_block *sb);
int delete_file(struct super_block *sb, char *path);
int open_file(struct super_block *sb, char *path, int flags);
int close_file(int file);
long write_file(int file, void *buf, long len);
long read_file(int file, void *buf, long len);
int seek_file(int file, int off, int pos);
long tell_file(int file);

long walk_file_name(struct super_block *sb, long file_num, void *buf, long len);
void list_files(struct super_block *sb);

int rename_file(struct super_block *sb, char *src_name, char *dest_name);
int stat_file(struct super_block *sb, char *path, struct file_stat *stat);

long alloc_file_num(struct super_block *sb);
int free_file_num(struct super_block *sb, long file_num);

/* internal */
struct file *create_file(struct super_block *sb, char *path, int mode);

int new_file(struct super_block *sb, const char *path, int mode);
int del_file(struct super_block *sb, const char *path);

#endif