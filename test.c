#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void test_fs(char *device)
{
    printf("\n############ test fs on device: %s\n", device);
    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(device, &dolphin_sb)) {
        /* 创建文件系统 */
        dolphin_mkfs(device, &dolphin_sb);
        /* 再次挂载 */
        if (dolphin_mount(device, &dolphin_sb)) {
            printf("mount dolphinfs failed!\n");
            return -1;
        }
    }

    int fd = open_file(&dolphin_sb, "test", FF_RDWR | FF_CRATE);
    printf("open test: %d\n", fd);
    
    printf("close f: %d\n", close_file(fd));

    fd = open_file(&dolphin_sb, "test_dir", FF_RDWR);
    printf("open test_dir: %d\n", fd);
    printf("close f: %d\n", close_file(fd));
    
    fd = open_file(&dolphin_sb, "test_dir/", FF_RDWR | FF_CRATE);
    printf("open test_dir/: %d\n", fd);
    printf("close f: %d\n", close_file(fd));

    fd = open_file(&dolphin_sb, "test_dir/a", FF_RDWR | FF_CRATE);
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
    
    fd = open_file(&dolphin_sb, "test_dir/", FF_RDWR);
    printf("open test_dir/: %d\n", fd);
    
    printf("seek: %d\n", seek_file(fd, 1, FP_SET));
    int j;
    for (j = 0; j < 10; j++) {
        write_file(fd, buf, 4096);
    }

    printf("seek: %d\n", seek_file(fd, 0, FP_END));
    printf("close f: %d\n", close_file(fd));

    /* list files */
    list_files(&dolphin_sb);

    rename_file(&dolphin_sb, "test_dir/a", "test_dir/abc");

    list_files(&dolphin_sb);

    rename_file(&dolphin_sb, "test_dir/abc", "test_dir/a");

    list_files(&dolphin_sb);

    dump_all_file(&dolphin_sb);
    delete_file(&dolphin_sb, "test_dir/a");
    dump_all_file(&dolphin_sb);
    delete_file(&dolphin_sb, "test_dir/");
    dump_all_file(&dolphin_sb);
    delete_file(&dolphin_sb, "test");
    dump_all_file(&dolphin_sb);

    dump_sb(&dolphin_sb);

    // long num = alloc_file_num();
    // printf("alloc num: %ld\n", num);
    // printf("free num: %ld\n", free_file_num(num));
    // num = alloc_file_num();
    // printf("alloc num: %ld\n", num);
    // printf("free num: %ld\n", free_file_num(num));
    // num = alloc_file_num();
    // printf("alloc num: %ld\n", num);
    // printf("free num: %ld\n", free_file_num(num));

    // long num_table[MAX_FILES + 1];
    // for (j = 0; j < MAX_FILES + 1; j++) {
    //     num_table[j] = alloc_file_num();
    //     printf("alloc num: %ld\n", num_table[j]);
    // }
    char buffer[1024];
#if 1
    fd = open_file(&dolphin_sb, "dolphinfs.h", FF_RDWR | FF_CRATE);
    printf("open dolphinfs.h: %d\n", fd);

    /* 打开文件，并将本地文件显示出来 */

    FILE *fp;
    int num_read;

    fp = fopen("dolphinfs.h", "r+");
    if (!fp) {
        printf("disk file 'dolphinfs.h' not found!\n");
        return -1;
    }
    // 读取文件内容到缓冲区，并打印
    while ((num_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        // 写入文件系统
        printf("IN %d\n", num_read);
        write_file(fd, buffer, num_read);
    }

    // 关闭文件
    fclose(fp);
    close_file(fd);
#endif
    /* 读取文件 */
    fd = open_file(&dolphin_sb, "dolphinfs.h", FF_RDWR | FF_CRATE);
    printf("open dolphinfs.h: %d\n", fd);

    memset(buffer, 0, 1024);
    printf("read: %d\n", read_file(fd, buffer, 1024));
    printf("file context:\n%s\n", buffer);

    close_file(fd);

    /* umount fs */
    dolphin_unmount(&dolphin_sb);

}

#define TEST_RAM_DEV "ram"
#define TEST_DISK_DEV "disk"

int cmd_test(void)
{
    /* init disk */
    dolphin_init();

    test_fs(TEST_RAM_DEV);
    test_fs(TEST_DISK_DEV);

    dolphin_exit();
    return 0;
}
