#ifndef _FILE_H
#define _FILE_H

/**
 * 支持的文件数量
 */
#define MAX_FILES 256
#define MAX_OPEN_FILE 32

#define FILE_NAME_LEN 256

/* file mode */
#define FM_READ 0X01
#define FM_WRITE 0X02
#define FM_RDWR (FM_READ | FM_WRITE)

/* file flags */
#define FF_READ 0X01
#define FF_WRITE 0X02
#define FF_RDWR (FF_READ | FF_WRITE)
#define FF_CRATE 0X10

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
    int oflags; /* open flags */
    long off;
};

void dump_all_file(void);
int delete_file(char *path);
int open_file(char *path, int flags);
int close_file(int file);
long write_file(int file, void *buf, long len);
long read_file(int file, void *buf, long len);
int seek_file(int file, int off, int pos);

long walk_file_name(long file_num, void *buf, long len);
void list_files(void);

int rename_file(char *src_name, char *dest_name);
int stat_file(char *path, struct file_stat *stat);

long alloc_file_num(void);
int free_file_num(long file_num);

#endif