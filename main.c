#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern unsigned char *block_ram;
extern struct super_block dolphin_sb;

int main(int argc, char *argv[])
{
    printf("hello, DolphinFS\n");

    /* init disk */
    open_ram();

    /* 创建文件系统 */
    dolphin_mkfs(block_ram);

    /* 挂载文件系统 */
    dolphin_mount(block_ram, &dolphin_sb);

    printf("create test:%d\n", create_file("test", FM_RDWR));
    printf("create test_dir/:%d\n", create_file("test_dir/", FM_RDWR));
    printf("create test_dir/a:%d\n", create_file("test_dir/a", FM_RDWR));

    int fd = open_file("test", FF_RDWR);
    printf("open test: %d\n", fd);
    
    printf("close f: %d\n", close_file(fd));

    fd = open_file("test_dir", FF_RDWR);
    printf("open test_dir: %d\n", fd);
    printf("close f: %d\n", close_file(fd));
    
    fd = open_file("test_dir/", FF_RDWR);
    printf("open test_dir/: %d\n", fd);
    printf("close f: %d\n", close_file(fd));

    fd = open_file("test_dir/a", FF_RDWR);
    printf("open test_dir/a: %d\n", fd);

    printf("write: %d\n", write_file(fd, "hello", 5));
    printf("seek: %d\n", seek_file(fd, 0, FP_END));
    printf("seek: %d\n", seek_file(fd, 0, FP_SET));
    char buf[32] = {0};
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    memset(buf, 0, 32);
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    printf("seek: %d\n", seek_file(fd, 0, FP_SET));
    printf("read: %d\n", read_file(fd, buf, 32));
    printf("read data: %s\n", buf);

    printf("close f: %d\n", close_file(fd));
    
    fd = open_file("test_dir/a", FF_RDWR);
    printf("open test_dir/a: %d\n", fd);
    
    printf("seek: %d\n", seek_file(fd, 1, FP_SET));
    int j;
    for (j = 0; j < 10; j++) {
        write_file(fd, buf, 4096);
    }

    printf("seek: %d\n", seek_file(fd, 0, FP_END));
    printf("close f: %d\n", close_file(fd));

    dump_all_file();
    delete_file("test_dir/a");
    dump_all_file();
    delete_file("test_dir/");
    dump_all_file();
    delete_file("test");
    dump_all_file();

    dump_sb(&dolphin_sb);

    close_ram();

    return 0;
}
