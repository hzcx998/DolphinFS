#ifndef _FILE_H
#define _FILE_H

/**
 * 支持的文件数量
 */
#define MAX_FILES 256
#define MAX_OPEN_FILE 32

/* file mode */
#define FM_READ 0X01
#define FM_WRITE 0X02
#define FM_RDWR (FM_READ | FM_WRITE)

/* file flags */
#define FF_READ 0X01
#define FF_WRITE 0X02
#define FF_RDWR (FF_READ | FF_WRITE)

#define FP_SET 1
#define FP_CUR 2
#define FP_END 3

struct file {
    unsigned long hash; /* name hash */
    long data_table;
    unsigned long file_size;
    int ref;
    int mode; /* r,w,rw */
};

/* open file */
struct ofile {
    struct file *f;
    int oflags; /* open flags */
    long off;
};

void dump_all_file(void);
int create_file(char *path, int mode);
int delete_file(char *path);
int open_file(char *path, int flags);
int close_file(int file);
long write_file(int file, void *buf, long len);
long read_file(int file, void *buf, long len);
int seek_file(int file, int off, int pos);


#endif