#include <dolphinfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// mkdir
#include <sys/stat.h>
#include <sys/types.h>

// stat
#include <unistd.h>
#include <dirent.h>

#define CLI_DISK_DEV "disk"

// Function declarations
int cmd_mkfs(void);
int cmd_put(const char* src_path, const char* dest_path);
int cmd_get(const char* src_path, const char* dest_path);
int cmd_list(const char* dir_path);
int cmd_delete_file(const char* file_path);
int cmd_copy(const char* src_path, const char* dest_path);
int cmd_move(const char* src_path, const char* dest_path);
int cmd_mkdir(const char* dir_path);
int cmd_rmdir(const char* dir_path);
int cmd_cat(const char* path);
int cmd_test(void);
int cmd_test_dir(void);

// Function implementations
int cmd_mkfs(void) {
    // Implement file system creation operation
    printf("Creating file system\n");

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (!dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Filesystem exist, can not format!\n");
        dolphin_unmount(&dolphin_sb);
        return -1;
    }

    /* 创建文件系统 */
    dolphin_mkfs(CLI_DISK_DEV, &dolphin_sb);
    /* 再次挂载 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("mount dolphinfs failed!\n");
        return -1;
    }

    /* create root dir */
    if (create_dir(&dolphin_sb, "/")) {
        printf("create root failed!\n");
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("Create filesystem success.\n");

    return 0; // Return 0 to indicate success
}

static int put_one_file(struct super_block *sb, const char* src_path, const char* dest_path)
{
    int ret = 0;

    FILE *src_file;
    int dest_file;
    
    char buffer[1024];
    size_t bytes_read;
    
    printf("[PUT] put file from %s to %s\n", src_path, dest_path);

    // Open the source file in the current directory
    src_file = fopen(src_path, "rb");
    if (src_file == NULL) {
        perror("Error opening source file");
        ret = -1;
        goto end;
    }

    // Open the destination file in the file system
    if (new_file(sb, dest_path, FF_RDWR)) {
        perror("Error create destination file");
        fclose(src_file);
        ret = -1;
        goto end;
    }

    // NOTE: This example uses fopen, but you should use your file system's API
    dest_file = open_file(sb, dest_path, FF_WRITE);
    if (dest_file == -1) {
        perror("Error opening destination file");
        fclose(src_file);
        ret = -1;
        goto end;
    }

    // Copy the file contents
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (write_file(dest_file, buffer, bytes_read) != bytes_read) {
            perror("Error writing to destination file");
            fclose(src_file);
            close_file(dest_file);
            ret = -1;
            goto end;
        }
    }

    if (ferror(src_file)) {
        perror("Error reading source file");
        fclose(src_file);
        close_file(dest_file);
        ret = -1;
        goto end;
    }

    // Close both files
    fclose(src_file);
    close_file(dest_file);
end:
    // printf("File %s put to %s %s\n", src_path, dest_path, ret == 0 ? "successfully" : "failed");
    return ret;
}

static int put_one_dir(struct super_block *sb, const char* src_path, const char* dest_path)
{
    char src_buf[256];
    char dst_buf[256];
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if ((dir = opendir(src_path)) == NULL) {
        printf("open dir %s failed!\n", src_path);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // build path
        memset(src_buf, 0, sizeof(src_buf));
        snprintf(src_buf, sizeof(src_buf), "%s/%s", src_path, entry->d_name);

        memset(dst_buf, 0, sizeof(dst_buf));
        snprintf(dst_buf, sizeof(dst_buf), "%s/%s", dest_path, entry->d_name);

        if (stat(src_buf, &statbuf) == -1) {
            printf("stat file %s failed!\n", src_buf);
            continue; // 获取信息失败，跳过该条目
        }

        if (S_ISDIR(statbuf.st_mode)) { // dir
            
            if (create_dir(sb, dst_buf)) {
                printf("mkdir dest %s faild!\n", dst_buf);
                return -1;
            }

            if (put_one_dir(sb, src_buf, dst_buf)) {
                printf("put dir src %s dest %s faild!\n", src_buf, dst_buf);
                return -1;
            }

        } else {
            // copy file
            if (put_one_file(sb, src_buf, dst_buf)) {
                printf("put file src %s dest %s faild!\n", src_buf, dst_buf);
                return -1;
            }
        }
    }

    closedir(dir);

    return 0;
}

int cmd_put(const char* src_path, const char* dest_path) {
    // Implement the operation to copy a file from the current directory to the file system
    printf("[++++++++] Copying %s to %s in the file system\n", src_path, dest_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;
    int ret = 0;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    // 如果是文件，就直接推送，如果是目录，则遍历推送。

    struct stat st;
    if (stat(src_path, &st) == -1) {
        printf("file %s not exist!\n", src_path);
        goto end;
    }

    if ((S_ISDIR(st.st_mode))) {
        struct file_stat dst_st;
        if (stat_file(&dolphin_sb, dest_path, &dst_st)) {
            if (create_dir(&dolphin_sb, dest_path)) {
                printf("mkdir dest %s faild!\n", dest_path);
                goto end;
            }
        }

        ret = put_one_dir(&dolphin_sb, src_path, dest_path);
        
        printf("Dir %s copied to %s %s\n", src_path, dest_path, 
            (ret == 0) ? "successfully" : "failed");
    } else {
        ret = put_one_file(&dolphin_sb, src_path, dest_path);

        printf("File %s copied to %s %s\n", src_path, dest_path, 
            (ret == 0) ? "successfully" : "failed");
    }
end:
    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("[++++++++] File %s put to %s %s\n", src_path, dest_path, ret == 0 ? "successfully" : "failed");
    return ret; // Return 0 to indicate success
}

static int get_one_file(struct super_block *sb, const char* src_path, const char* dest_path)
{
    int src_file;
    FILE *dest_file;
    char buffer[1024];
    size_t bytes_read;
    int ret = 0;

    printf("[GET] get file from %s to %s\n", src_path, dest_path);

    // Open the source file in the file system
    // NOTE: This example uses fopen, but you should use your file system's API
    src_file = open_file(sb, src_path, FF_READ);
    if (src_file == -1) {
        perror("Error opening source file");
        ret = -1;
        goto end;
    }

    // Open the destination file in the current directory
    dest_file = fopen(dest_path, "wb");
    if (dest_file == NULL) {
        perror("Error opening destination file");
        close_file(src_file);
        ret = -1;
        goto end;
    }

    // Copy the file contents
    while ((bytes_read = read_file(src_file, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            perror("Error writing to destination file");
            close_file(src_file);
            fclose(dest_file);
            ret = -1;
            goto end;
        }
    }

    // check file finished
    long cur_pos = tell_file(src_file);
    long end_pos = seek_file(src_file, 0, FP_END);
    
    if (cur_pos != end_pos) {
        printf("Error reading source file: %d, %d\n", cur_pos, end_pos);
        close_file(src_file);
        fclose(dest_file);
        ret = -1;
        goto end;
    }

    // Close both files
    close_file(src_file);
    fclose(dest_file);

end:
    // printf("File %s get to %s %s\n", src_path, dest_path, ret == 0 ? "successfully" : "failed");
    return ret;
}

static int get_one_dir(struct super_block *sb, const char* src_path, const char* dest_path)
{
    char src_buf[256] = {0};
    char dst_buf[256] = {0};

    int dir = open_dir(sb, src_path);
    if (dir < 0)
    {
        printf("open dir %s failed!\n", src_path);
        return -1;
    }

    struct fs_dir_entry de;
    
    while (read_dir(dir, &de) != -1)
    {
        // printf("---> %8d %8d %s\n", de.num, de.type, de.fname.buf);
        // build path
        memset(src_buf, 0, sizeof(src_buf));
        snprintf(src_buf, sizeof(src_buf), "%s/%s", src_path, de.fname.buf);

        memset(dst_buf, 0, sizeof(dst_buf));
        snprintf(dst_buf, sizeof(dst_buf), "%s/%s", dest_path, de.fname.buf);

        if (de.type == 1) { // dir
            mode_t mode = 0755;
            if (mkdir(dst_buf, mode)) {
                printf("mkdir dest %s faild!\n", dst_buf);
                return -1;
            }

            if (get_one_dir(sb, src_buf, dst_buf)) {
                printf("get dir src %s dest %s faild!\n", src_buf, dst_buf);
                return -1;
            }

        } else {
            // copy file
            if (get_one_file(sb, src_buf, dst_buf)) {
                printf("get file src %s dest %s faild!\n", src_buf, dst_buf);
                return -1;
            }
        }
    }

    close_dir(dir);

    return 0;
}

int cmd_get(const char* src_path, const char* dest_path) {
    // Implement the operation to copy a file from the file system to the current directory
    printf("[--------] Get %s from the file system to %s\n", src_path, dest_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;
    int ret = 0;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    // 如果是文件，就直接获取，如果是目录，则遍历获取。
    struct file_stat st;

    if (stat_file(&dolphin_sb, src_path, &st)) {
        printf("file %s not exist!\n", src_path);
        goto end;
    }

    if ((st.mode & FF_DIR)) {
        // create dest dir if not exist
        struct stat dst_st;
        if (stat(dest_path, &dst_st) == -1) {
            mode_t mode = 0755;
            if (mkdir(dest_path, mode)) {
                printf("mkdir dest %s faild!\n", dest_path);
                goto end;
            }
        }
        ret = get_one_dir(&dolphin_sb, src_path, dest_path);
        
        printf("Dir %s copied to %s %s\n", src_path, dest_path, 
            (ret == 0) ? "successfully" : "failed");
    } else {
        ret = get_one_file(&dolphin_sb, src_path, dest_path);

        printf("File %s copied to %s %s\n", src_path, dest_path, 
            (ret == 0) ? "successfully" : "failed");
    }

end:
    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("[--------] File %s get to %s %s\n", src_path, dest_path, ret == 0 ? "successfully" : "failed");
    return 0; // Return 0 to indicate success
}

int cmd_list(const char* dir_path) {
    // Implement the operation to list files in a directory of the file system
    printf("Listing %s in the file system\n", dir_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    if (list_dir(&dolphin_sb, dir_path)) {
        printf("List dir %s failed!\n", dir_path);
        return -1;
    }

    list_files(&dolphin_sb);

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("List filesystem success.\n");

    return 0; // Return 0 to indicate success
}

int cmd_cat(const char* path) {
    // Implement the operation to list files in a directory of the file system
    printf("Cat %s in the file system\n", path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    int fd = open_file(&dolphin_sb, path, FF_READ);
    if (fd < 0) {
        printf("open file %s failed!\n", path);
    } else {
        char buf[256];
        int len;
        
        do {
            memset(buf, 0, 256);
            len = read_file(fd, buf, 256);
            printf("%s", buf);
        } while (len > 0);

        printf("\n");
        close_file(fd);
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("Cat %s done.\n", path);

    return 0; // Return 0 to indicate success
}

int cmd_delete_file(const char* file_path) {
    // Implement the operation to delete a file in the file system
    printf("Deleting %s in the file system\n", file_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
            
        /* 挂载成功 */
        dolphin_unmount(&dolphin_sb);

        exit_blkdev();
        return -1;
    }

    if (!del_file(&dolphin_sb, file_path)) {    
        printf("Delete file %s success.\n", file_path);
    } else {
        printf("Delete file %s failed!\n", file_path);
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    return 0; // Return 0 to indicate success
}

int cmd_copy(const char* src_path, const char* dest_path) {
    // Implement the operation to copy a file within the file system
    printf("Copying %s to %s in the file system\n", src_path, dest_path);

    /* init disk */
    init_blkdev();
    list_blkdev();
    int ret = 0;

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    int src_file, dest_file;
    char buffer[1024];
    size_t bytes_read;

    // Open the source file in the file system
    src_file = open_file(&dolphin_sb, src_path, FF_READ);
    if (src_file == -1) {
        perror("Error opening source file");
        ret = 1;
        goto end;
    }

    // Open the destination file in the file system
    if (new_file(&dolphin_sb, dest_path, FF_RDWR)) {
        perror("Error create destination file");
        fclose(src_file);
        ret = 1;
        goto end;
    }
    dest_file = open_file(&dolphin_sb, dest_path, FF_WRITE);
    if (dest_file == -1) {
        perror("Error opening destination file");
        close_file(src_file);
        ret = 1;
        goto end;
    }

    // Copy the file contents
    while ((bytes_read = read_file(src_file, buffer, sizeof(buffer))) > 0) {
        if (write_file(dest_file, buffer,bytes_read) != bytes_read) {
            perror("Error writing to destination file");
            close_file(src_file);
            close_file(dest_file);
            ret = 1;
        goto end;
        }
    }

    if (tell_file(src_file) != seek_file(src_file, 0, FP_END)) {
        perror("Error reading source file");
        close_file(src_file);
        close_file(dest_file);
        ret = 1;
        goto end;
    }

    // Close both files
    close_file(src_file);
    close_file(dest_file);
end:
    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("File %s copied to %s %s\n", src_path, dest_path, !ret ? "successfully" : "failed");
    return 0; // Return 0 to indicate success
}

int cmd_move(const char* src_path, const char* dest_path) {
    // Implement the operation to move a file within the file system
    printf("Moving %s to %s in the file system\n", src_path, dest_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    // use dir rename
    if (!rename_file(&dolphin_sb, src_path, dest_path)) {    
        printf("Move file %s to %s success.\n", src_path, dest_path);
    } else {
        printf("Move file%s to %s failed!\n", src_path, dest_path);
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    return 0; // Return 0 to indicate success
}

int cmd_mkdir(const char* dir_path) {
    // Implement the operation to create a directory in the file system
    printf("Creating directory %s.\n", dir_path);
    
    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    if (create_dir(&dolphin_sb, dir_path)) {
        printf("Create dir %s failed!\n", dir_path);
        dolphin_unmount(&dolphin_sb);
        exit_blkdev();
        return -1;
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("Creating directory %s success.\n", dir_path);

    return 0; // Return 0 to indicate success
}

int cmd_rmdir(const char* dir_path) {
    // Implement the operation to create a directory in the file system
    printf("Remove directory %s.\n", dir_path);

    /* init disk */
    init_blkdev();
    list_blkdev();

    struct super_block dolphin_sb;

    /* 挂载文件系统 */
    if (dolphin_mount(CLI_DISK_DEV, &dolphin_sb)) {
        printf("Mount filesystem failed!\n");
        return -1;
    }

    if (delete_dir(&dolphin_sb, dir_path)) {
        printf("Remove dir %s failed!\n", dir_path);
        dolphin_unmount(&dolphin_sb);
        exit_blkdev();
        return -1;
    }

    /* 挂载成功 */
    dolphin_unmount(&dolphin_sb);

    exit_blkdev();

    printf("Remove directory %s success.\n", dir_path);

    return 0; // Return 0 to indicate success
}

int main(int argc, char* argv[]) {
    // Check if the number of arguments is less than 2
    if (argc < 2) {
        printf("Usage: %s <command> [<args>]\n", argv[0]);
        printf("Commands:\n");
        printf("  mkfs\n");
        printf("  put <src> <dest>\n");
        printf("  get <src> <dest>\n");
        printf("  list <dir>\n");
        printf("  delete <file>\n");
        printf("  copy <src> <dest>\n");
        printf("  move <src> <dest>\n");
        printf("  mkdir <dir>\n");
        printf("  rmdir <dir>\n");
        printf("  cat <file>\n");
        printf("  test\n");
        return 1;
    }

    // Process commands using if-else statements
    if (strcmp(argv[1], "mkfs") == 0) {
        return cmd_mkfs();
    } else if (strcmp(argv[1], "put") == 0) {
        if (argc != 4) {
            printf("Usage: %s put <src> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_put(argv[2], argv[3]);
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc != 4) {
            printf("Usage: %s get <src> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_get(argv[2], argv[3]);
    } else if (strcmp(argv[1], "list") == 0) {
        if (argc != 3) {
            printf("Usage: %s list <dir>\n", argv[0]);
            return 1;
        }
        return cmd_list(argv[2]);
    } else if (strcmp(argv[1], "cat") == 0) {
        if (argc != 3) {
            printf("Usage: %s cat <dir>\n", argv[0]);
            return 1;
        }
        return cmd_cat(argv[2]);
    } else if (strcmp(argv[1], "delete") == 0) {
        if (argc != 3) {
            printf("Usage: %s delete <file>\n", argv[0]);
            return 1;
        }
        return cmd_delete_file(argv[2]);
    } else if (strcmp(argv[1], "copy") == 0) {
        if (argc != 4) {
            printf("Usage: %s copy <src> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_copy(argv[2], argv[3]);
    } else if (strcmp(argv[1], "move") == 0) {
        if (argc != 4) {
            printf("Usage: %s move <src> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_move(argv[2], argv[3]);
    } else if (strcmp(argv[1], "mkdir") == 0) {
        if (argc != 3) {
            printf("Usage: %s mkdir <dir>\n", argv[0]);
            return 1;
        }
        return cmd_mkdir(argv[2]);
    } else if (strcmp(argv[1], "rmdir") == 0) {
        if (argc != 3) {
            printf("Usage: %s rmdir <dir>\n", argv[0]);
            return 1;
        }
        return cmd_rmdir(argv[2]);
    } else if (strcmp(argv[1], "test") == 0) {
        if (argc != 2) {
            printf("Usage: %s test\n", argv[0]);
            return 1;
        }
        return cmd_test();
    } else if (strcmp(argv[1], "test_dir") == 0) {
        if (argc != 2) {
            printf("Usage: %s test\n", argv[0]);
            return 1;
        }
        return cmd_test_dir();
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
