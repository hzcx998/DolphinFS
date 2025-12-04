#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void test_fs_dir(char *device)
{
    printf("\n############ test fs dir on device: %s\n", device);
    struct super_block dolphin_sb;
    int ret;

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

    ret = create_dir(&dolphin_sb, "/");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/abc");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/abc");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/123");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/123");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/xyz");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/abc/def");
    if (ret)
    {
        printf("create root dir failed!\n");
    }

    ret = create_dir(&dolphin_sb, "/abc/def");
    if (ret)
    {
        printf("create root dir failed!\n");
    }
    
    new_file(&dolphin_sb, "/abc/111", FF_RDWR);
    new_file(&dolphin_sb, "/abc", FF_RDWR);
    new_file(&dolphin_sb, "/456", FF_RDWR);

    list_dir(&dolphin_sb, "/");
    list_dir(&dolphin_sb, "/abc");

    list_files(&dolphin_sb);
    
    ret = delete_dir(&dolphin_sb, "/");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/abc");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/123");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/xyz");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/qwer");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/abc/def");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/abc");
    printf("del dir %d\n", ret);

    ret = delete_dir(&dolphin_sb, "/123");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/xyz");
    printf("del dir %d\n", ret);
    ret = delete_dir(&dolphin_sb, "/abc/def");
    printf("del dir %d\n", ret);

    ret = del_file(&dolphin_sb, "/abc/111");
    printf("del file %d\n", ret);

    int fd = open_file(&dolphin_sb, "/456", FF_RDWR);
    if (fd >= 0) {
        printf("write %d\n", write_file(fd, "hello", 5));
        seek_file(fd, 0, FP_SET);
        char buf[32] = {0};
        printf("read %d\n", read_file(fd, buf, 5));
        printf("read data %s\n", buf);
        
        close_file(fd);
    }

    ret = delete_dir(&dolphin_sb, "/abc");
    printf("del dir %d\n", ret);



    /* umount fs */
    dolphin_unmount(&dolphin_sb);

}

#define TEST_RAM_DEV "ram"
#define TEST_DISK_DEV "disk"

int cmd_test_dir(void)
{
    /* init disk */
    init_blkdev();
    list_blkdev();

    test_fs_dir(TEST_RAM_DEV);
    test_fs_dir(TEST_DISK_DEV);

    exit_blkdev();
    return 0;
}
