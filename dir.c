#include <dir.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct fs_dir_open open_dirs[MAX_OPEN_DIR] = {0};

struct fs_dir_open *alloc_open_dir(void)
{
    int i;
    struct fs_dir_open *od;
    for (i = 0; i < MAX_OPEN_DIR; i++) {
        od = &open_dirs[i];
        if (od->sb == NULL) {
            return od;
        }
    }
    return NULL;
}


int free_open_dir(struct fs_dir_open *od)
{
    if (!od) {
        return -1;
    }
    od->sb = NULL;
    return 0;
}

#define odir_idx(od) ((od) - &open_dirs[0])
#define idx_odir(idx) (&open_dirs[(idx)])

/**
 * @brief 清理路径，移除其中的 "."、".." 和多余的分隔符。
 * @param input_path 待清洗的原始路径字符串
 * @param clean_result 存储清洗后结果的字符数组
 * @param result_size clean_result数组的大小
 */
void clean_path(const char* input_path, char* clean_result, size_t result_size) {
    char* components[256]; // 栈，用于存储路径组件
    int depth = -1; // 栈深度指针

    // 复制路径，因为strtok会修改原字符串
    char* path_copy = strdup(input_path);
    char* token = strtok(path_copy, "/");

    // 1. 词法分析：分解路径并处理特殊组件
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || strlen(token) == 0) {
            // 忽略当前目录标记和空字符串
        } else if (strcmp(token, "..") == 0) {
            // 处理上级目录：如果栈内有组件，则弹出（回退）
            if (depth >= 0) {
                depth--;
            }
            // 如果栈为空，在根目录下忽略".."
        } else {
            // 普通路径组件，压入栈
            if (depth < 255) {
                components[++depth] = token;
            }
        }
        token = strtok(NULL, "/");
    }

    // 2. 构建标准化路径
    clean_result[0] = '\0';
    // 处理绝对路径：如果原路径以'/'开始
    if (input_path[0] == '/') {
        strncat(clean_result, "/", result_size - 1);
    }
    // 连接栈中的组件
    for (int i = 0; i <= depth; i++) {
        strncat(clean_result, components[i], result_size - strlen(clean_result) - 1);
        if (i < depth) {
            strncat(clean_result, "/", result_size - strlen(clean_result) - 1);
        }
    }
    // 如果结果为空，设为当前目录
    if (strlen(clean_result) == 0) {
        strncpy(clean_result, ".", result_size);
    }
    clean_result[result_size - 1] = '\0'; // 确保终止

    free(path_copy); // 释放复制的字符串
}

/**
 * @brief 从路径中提取下一个组件
 * @param path 输入路径
 * @param component 输出参数，存储提取的组件
 * @param max_len 组件缓冲区最大长度
 * @return 返回下一个组件的起始位置，NULL表示结束
 */
const char* extract_next_component(const char* path, char* component, size_t max_len) {
    static const char* current_pos = NULL;
    const char* start;
    size_t len = 0;
    
    // 初始化或继续上次的位置
    if (path != NULL) {
        current_pos = path;
    }
    
    if (current_pos == NULL || *current_pos == '\0') {
        return NULL;
    }
    
    // 跳过开头的分隔符
    while (*current_pos == '/') {
        current_pos++;
    }
    
    if (*current_pos == '\0') {
        return NULL;
    }
    
    // 找到组件的开始和结束
    start = current_pos;
    while (*current_pos != '\0' && *current_pos != '/') {
        len++;
        current_pos++;
    }
    
    // 复制组件（确保不溢出）
    if (len >= max_len) {
        len = max_len - 1;
    }
    
    strncpy(component, start, len);
    component[len] = '\0';
    
    return current_pos;
}

/**
 * @brief 检查路径组件是否存在且为目录
 * @param base_path 基础路径
 * @param component 要检查的组件名
 * @param full_path 输出参数，存储完整路径
 * @param max_path_len 路径缓冲区大小
 * @return 0:存在且为目录, -1:不存在, -2:存在但不是目录
 */
int check_component_exists(struct super_block *sb, const char* base_path, const char* component, 
                          char* full_path, size_t max_path_len) {
    struct file_stat stat_buf;
    
    // 构建完整路径
    if (strcmp(base_path, "/") == 0) {
        snprintf(full_path, max_path_len, "/%s", component);
    } else {
        snprintf(full_path, max_path_len, "%s/%s", base_path, component);
    }
    
    // 检查路径是否存在
    printf("check_component_exists: %s\n", full_path);
    if (stat_file(sb, full_path, &stat_buf) != 0) {
        return -1; // 路径不存在
    }
    
    // 检查是否为目录
    if (!(stat_buf.mode & FF_DIR)) {
        return -2; // 存在但不是目录
    }

    return 0; // 存在且是目录
}

static int load_dir_head(struct fs_dir_head *head, int fd)
{
    /* seek to head */
    if (seek_file(fd, 0, FP_SET) != 0) {
        return -1;
    }

    if (read_file(fd, head, sizeof(struct fs_dir_head)) != sizeof(struct fs_dir_head)) {
        return -1;
    }

    return 0;
}

static int sync_dir_head(struct fs_dir_head *head, int fd)
{
    /* seek to head */
    if (seek_file(fd, 0, FP_SET) != 0) {
        return -1;
    }

    if (write_file(fd, head, sizeof(struct fs_dir_head)) != sizeof(struct fs_dir_head)) {
        return -1;
    }

    return 0;
}

static int touch_dir_file(struct super_block *sb, const char *path)
{
    int fd;
    int fill_head = 0;
    struct file_stat st;

    /* fill dir head */
    if (stat_file(sb, path, &st)) {
        fill_head = 1;
    }

    /* create file with dir attr on system */
    fd = open_file(sb, path, FF_DIR | FF_CRATE | FF_RDWR);
    if (fd == -1) {
        return -1;
    }

    // fill head info if file not exist
    if (fill_head) {
        struct fs_dir_head head;
        printf("[INFO] touch dir, fill head info of %s\n", path);
    
        head.count = 0;
        memset(head.bitmap, 0, sizeof(head.bitmap));

        if (sync_dir_head(&head, fd)) {
            close_file(fd);
            return -1;
        }
    }

    close_file(fd);
    
    return 0;
}

static int touch_normal_file(struct super_block *sb, const char *path, int mode)
{
    int fd;

    /* create file with dir attr on system */
    fd = open_file(sb, path, FF_CRATE | mode);
    if (fd == -1) {
        return -1;
    }

    close_file(fd);
    
    return 0;
}

static int rm_dir_file(struct super_block *sb, const char *path)
{
    return delete_file(sb, path);    
}

static int alloc_dir_bitmap(struct fs_dir_head *head)
{
    int i, j;
    unsigned int *bit;

    for (i = 0; i < DIR_BITMAP_SIZE; i++) {
        bit = &head->bitmap[i];
        for (j = 0; j < DIR_BITMAP_ITEM_SIZE; j++) {
            if (!(*bit & (1 << j))) {
                *bit |= (1 << j);
                return i * DIR_BITMAP_ITEM_SIZE + j;
            }
        }
    }
    return -1;
}

static int find_dir_bitmap_next(struct fs_dir_head *head, int start)
{
    int i, j;
    unsigned int *bit;

    i = start / DIR_BITMAP_ITEM_SIZE;
    j = start % DIR_BITMAP_ITEM_SIZE;

    for (; i < DIR_BITMAP_SIZE; i++) {
        bit = &head->bitmap[i];
        for (; j < DIR_BITMAP_ITEM_SIZE; j++) {
            if ((*bit & (1 << j)) != 0) {
                return i * DIR_BITMAP_ITEM_SIZE + j;
            }
        }
    }
    return -1;
}

static int find_dir_bitmap_by_name(struct fs_dir_head *head, char *name, int fd)
{
    struct fs_dir_entry de;
    int idx;

    // seek to head
    if (seek_file(fd, sizeof(struct fs_dir_head), FP_SET) != sizeof(struct fs_dir_head)) {
        return -1;
    }
    
    idx = 0;
    while (read_file(fd, &de, sizeof(struct fs_dir_entry)) == sizeof(struct fs_dir_entry)) {
        if (!strcmp(de.fname.buf, name)) {
            return idx;
        }
        idx++;
    }

    return -1;
}

static int free_dir_bitmap(struct fs_dir_head *head, int idx)
{
    int i, j;
    
    i = idx / DIR_BITMAP_ITEM_SIZE;
    j = idx % DIR_BITMAP_ITEM_SIZE;
    
    head->bitmap[i] &= ~(1 << j);
    return 0;
}

static int test_dir_bitmap(struct fs_dir_head *head, int idx)
{
    int i, j;
    
    i = idx / DIR_BITMAP_ITEM_SIZE;
    j = idx % DIR_BITMAP_ITEM_SIZE;
    
    return (head->bitmap[i] & (1 << j)) != 0;
}

static int mod_dir_entry(struct fs_dir_head *head, int fd, struct fs_dir_entry *de, int bit)
{
    long off;

    // calc file offset
    off = sizeof(struct fs_dir_head) + bit * sizeof(struct fs_dir_entry);

    if (seek_file(fd, off, FP_SET) != off) {
        return -1;
    }
    
    if (write_file(fd, de, sizeof(struct fs_dir_entry)) != sizeof(struct fs_dir_entry)) {
        return -1;
    }
    
    return 0;
}

static int add_dir_entry(struct fs_dir_head *head, int fd, struct fs_dir_entry *de)
{
    int bit;
    long off;
    // find a free dir entry to store
    bit = alloc_dir_bitmap(head);
    if (bit == -1) {
        return -1;
    }

    if (mod_dir_entry(head, fd, de, bit)) {
        free_dir_bitmap(head, bit);
        return -1;
    }

    head->count++;

    if (sync_dir_head(head, fd)) {
        free_dir_bitmap(head, bit);
        return -1;
    }

    return 0;
}

static int del_dir_entry(struct fs_dir_head *head, int fd, const char *dir_name)
{
    int bit;
    long off;
    struct fs_dir_entry de;

    /* 遍历读取所有文件，匹配文件名，找对匹配的，并给出索引 */
    bit = find_dir_bitmap_by_name(head, dir_name, fd);
    if (bit == -1) {
        return -1;
    }

    /* 清理目录项 */
    memset(&de, 0, sizeof(de));
    if (mod_dir_entry(head, fd, &de, bit)) {
        return -1;
    }

    /* 释放位图 */
    if (free_dir_bitmap(head, bit)) {
        return -1;
    }

    head->count--;

    if (sync_dir_head(head, fd)) {
        return -1;
    }

    return 0;
}

static int create_dir_entry(struct super_block *sb, const char *path, const char *current_path, const char *name, int type)
{
    int fd;
    struct fs_dir_entry de;
    struct file_stat st;
    struct fs_dir_head head;
    
    if (!strlen(name)) {
        return 0; // if no name, just return
    }

    /* write dir entry info into parent dir */
    if (stat_file(sb, path, &st)) {
        return -1;
    }

    /* fill dir entry */
    strcpy(de.fname.buf, name);
    de.num = st.num;
    de.type = type;

    /* write dir entry into parent dir */
    fd = open_file(sb, current_path, FF_RDWR);
    if (fd == -1) {
        return -1;
    }
    
    if (load_dir_head(&head, fd)) {
        close_file(fd);
        return -1;
    }
    
    if (add_dir_entry(&head, fd, &de)) {
        close_file(fd);
        return -1;
    }
    
    close_file(fd);
    return 0;
}

static int create_new_dir(struct super_block *sb, const char *path, const char *current_path, const char *name)
{
    /* create dir entry file */
    if (touch_dir_file(sb, path)) {
        return -1;
    }

    if (create_dir_entry(sb, path, current_path, name, 1)) {
        rm_dir_file(sb, path);
        return -1;
    }
    return 0;
}

static int create_new_file(struct super_block *sb, const char *path, const char *current_path, const char *name, int mode)
{
    /* create normal file */
    if (touch_normal_file(sb, path, mode)) {
        return -1;
    }

    if (create_dir_entry(sb, path, current_path, name, 0)) {
        rm_dir_file(sb, path);
        return -1;
    }
    return 0;
}

static struct fs_dir_open *load_dir_info(struct super_block *sb, const char *path)
{
    struct fs_dir_open *od;

    od = alloc_open_dir();

    if (!od) {
        return NULL;
    }

    od->sb = sb;
    od->fd = open_file(sb, path, FF_RDWR);
    if (od->fd == -1) {
        free_open_dir(od);
        return NULL;
    }
    od->offset = sizeof(struct fs_dir_head);
    
    return od;
}

static int release_dir_info(struct fs_dir_open *od)
{
    if (!od) {
        return -1;
    }

    if (od->fd == -1) {
        return -1;
    }

    od->offset = 0;

    if (close_file(od->fd)) {
        return -1;
    }
    od->fd = -1;

    return free_open_dir(od);    
}

#define DIR_WALK_CREATE_NOT_EXIST 0x01
#define DIR_WALK_LOAD_INFO 0x02
#define DIR_WALK_CREATE_FILE_NOT_EXIST 0x04

/**
 * @brief 逐级解析并检查路径组件
 * @param full_path 完整路径（如 "/abc/def/123"）
 * @return 0:所有组件都存在, -1:路径格式错误, -2:某个组件不存在
 */
int walk_dir(struct super_block *sb, const char *path, int flags, int mode, struct fs_dir_open **od)
{
    char current_path[1024] = "/";  // 从根目录开始构建
    char full_path[1024] = {0};  // 从根目录开始构建
    char component[256] = {0};
    const char* pos;
    int level = 0;
    char new_path[1024] = {0};
    
    if (!strlen(path)) {
        return -1;
    }

    // printf("开始解析路径: %s\n", path);
    
    clean_path(path, full_path, sizeof(full_path));

    printf("clean path: %s\n", full_path);
    
    pos = full_path;
    // 初始化组件提取
    pos = extract_next_component(full_path, component, sizeof(component));
    // printf("pos: %p, component:%s\n", pos, component);
    if (pos == NULL && component[0] != '\0') {
        // 只有一个组件的情况
        pos = extract_next_component(NULL, component, sizeof(component));
        // printf("pos: %p, component:%s\n", pos, component);
    }

    /* 检查是根目录的情况 */
    if (pos == NULL && full_path[0] == '/' && full_path[1] == '\0') {
        component[0] = '\0';
    }

    do {
        int result;
        
        if (pos != NULL && pos[0] != '\0') {
            // printf("解析下一个组件，相对路径：%s\n", pos);
        }
        // printf("第%d级 - 检查组件: '%s'\n", level, component);
        // printf("当前路径: %s\n", current_path);
        
        // 检查当前组件是否存在
        result = check_component_exists(sb, current_path, component, new_path, sizeof(new_path));
        
        switch (result) {
            case 0:
                printf("  ✓ 组件存在且是目录: %s\n", new_path);
                strcpy(current_path, new_path); // 更新当前路径

                // 如果是最后一个组件，并且是创建目录，说明已经存在，不能再创建了，就返回报错。
                if ((flags & DIR_WALK_CREATE_NOT_EXIST) && !(pos != NULL && pos[0] != '\0')) {
                    printf("[ERROR] 创建目录，但已经存在，不允许创建: %s\n", new_path);
                    return -1;
                }
                if ((flags & DIR_WALK_CREATE_FILE_NOT_EXIST) && !(pos != NULL && pos[0] != '\0')) {
                    printf("[ERROR] 创建文件，但已经存在，不允许创建: %s\n", new_path);
                    return -1;
                }
                break;
                
            case -1:
                printf("  ✗ 组件不存在: %s, 全路径：%s\n", component, new_path);
                printf("在层级 %d 处停止: %s 不存在\n", level, component);

                // CREATE DIR hear
                if (flags & DIR_WALK_CREATE_NOT_EXIST) {
                    printf("---> 打开目录文件：%s，写入组件信息：%s\n", current_path, component);
                    if (create_new_dir(sb, new_path, current_path, component)) {
                        return -1;
                    }

                    printf("  ✓ 组件不存在但创建了新目录: %s\n", new_path);
                    strcpy(current_path, new_path); // 更新当前路径
                } else if ((flags & DIR_WALK_CREATE_FILE_NOT_EXIST) && !(pos != NULL && pos[0] != '\0')) {
                    printf("---> 文件不存在创建新文件：%s\n", new_path);
                    struct file *f;
                    if (create_new_file(sb, new_path, current_path, component, mode)) {
                        return -1;
                    }
                } else {
                    return -2;
                }
                break;
            case -2:
                printf("  ✗ 路径存在但不是目录: %s\n", new_path);
                printf("在层级 %d 处停止: %s 不是目录\n", level, component);
                return -2;
        }
        level++;

        // 获取下一个组件
        if (pos != NULL && pos[0] != '\0') {
            // printf("解析下一个组件，相对路径：%s\n", pos);
            pos = extract_next_component(pos, component, sizeof(component));
        } else {
            // 加载目录信息
            if ((DIR_WALK_LOAD_INFO & flags) && od != NULL) {
                *od = load_dir_info(sb, new_path);
                if (*od == NULL) {
                    return -1;
                }
            }

            component[0] = '\0'; // 结束循环

        }
        
        // printf("---\n");
    } while (component[0] != '\0');
    
    // printf("✓ 路径解析完成: 所有 %d 个组件都存在\n", level);
    return 0;
}

int create_dir(struct super_block *sb, const char *path)
{
    return walk_dir(sb, path, DIR_WALK_CREATE_NOT_EXIST, 0, NULL);
}

int new_file(struct super_block *sb, const char *path, int mode)
{
    return walk_dir(sb, path, DIR_WALK_CREATE_FILE_NOT_EXIST, mode, NULL);
}

static int split_dir_path(const char *path, char *parent_name, char *dir_name)
{
    char *p;
    int len;

    p = strrchr(path, '/');
    if (p == NULL) {
        return -1;
    }

    len = p - path;

    if (!len) {
        strcpy(parent_name, "/");
    } else {
        strncpy(parent_name, path, len);
    }
    strcpy(dir_name, (const char *)(p + 1));
    return 0;
}

static int remove_parent_dir_entry(struct super_block *sb, const char *path)
{
    int fd;
    struct fs_dir_head head;
    char parent_dir[FILE_NAME_LEN] = {0};
    char dir_name[FILE_NAME_LEN] = {0};

    if (split_dir_path(path, parent_dir, dir_name)) {
        return -1;
    }

    fd = open_file(sb, parent_dir, FF_RDWR);
    if (fd == -1) {
        return -1;
    }
    
    if (load_dir_head(&head, fd)) {
        close_file(fd);
        return -1;
    }
    
    if (del_dir_entry(&head, fd, dir_name)) {
        close_file(fd);
        return -1;
    }
    
    close_file(fd);
    return 0;
}

static int check_dir_empty(struct super_block *sb, const char *path)
{
    int fd;
    struct fs_dir_head head;
    char parent_dir[FILE_NAME_LEN] = {0};
    char dir_name[FILE_NAME_LEN] = {0};

    fd = open_file(sb, path, FF_RDWR);
    if (fd == -1) {
        return -1;
    }
    
    if (load_dir_head(&head, fd)) {
        close_file(fd);
        return -1;
    }
    
    close_file(fd);

    return !head.count;
}

int delete_dir(struct super_block *sb, const char *path)
{
    // check dir empty
    struct file_stat stat_buf;
    struct fs_dir_head head;
    int fd;
    
    if (stat_file(sb, path, &stat_buf) != 0) {
        return -1;
    }

    if (!(stat_buf.mode & FF_DIR)) {
        return -1;
    }

    // can't delete root dir
    if (!strcmp(path, "/")) {
        printf("[ERROR] can not delete root dir!\n");
        return -1;        
    }

    /* check dir is empty */
    if (!check_dir_empty(sb, path)) {
        printf("[ERROR] dir %s not empty!\n", path);
        return -1;
    }

    printf("[ERROR] ready delete dir %s.\n", path);

    /* remove dir entry from parent dir */
    if (remove_parent_dir_entry(sb, path)) {
        return -1;
    }
    
    return delete_file(sb, path);
}

int del_file(struct super_block *sb, const char *path)
{
    // check dir empty
    struct file_stat stat_buf;
    struct fs_dir_head head;
    int fd;
    
    if (stat_file(sb, path, &stat_buf) != 0) {
        return -1;
    }

    if ((stat_buf.mode & FF_DIR)) {
        return -1;
    }

    // can't delete root dir
    if (!strcmp(path, "/")) {
        printf("[ERROR] can not delete root dir!\n");
        return -1;        
    }

    printf("[ERROR] ready delete file %s.\n", path);

    /* remove dir entry from parent dir */
    if (remove_parent_dir_entry(sb, path)) {
        return -1;
    }
    
    return delete_file(sb, path);
}

int open_dir(struct super_block *sb, const char *path)
{
    struct fs_dir_open *od = NULL;
    // 先遍历目录，确认文件存在。
    if (walk_dir(sb, path, DIR_WALK_LOAD_INFO, 0, &od)) {
        return -1;
    }
    return odir_idx(od);
}

int close_dir(int dir)
{
    struct fs_dir_open *od;
    od = idx_odir(dir);

    return release_dir_info(od);
}

int read_dir(int dir, struct fs_dir_entry *de)
{
    struct fs_dir_open *od;
    struct fs_dir_head head;
    int idx;

    od = idx_odir(dir);
    long len;

    struct fs_dir_entry tmp;

    /* get head info */
    if (load_dir_head(&head, od->fd)) {
        return -1;
    }
    
    if (!head.count) {
        return -1;
    }

    /* check offset has entry */
    idx = (od->offset - sizeof(struct fs_dir_head)) / sizeof(struct fs_dir_entry);
    if (!test_dir_bitmap(&head, idx)) {
        // find next bitmap
        idx = find_dir_bitmap_next(&head, idx);
        if (idx == -1) {
            return -1; // no direnty
        }
        od->offset = sizeof(struct fs_dir_head) + idx * sizeof(struct fs_dir_entry);
    }

    seek_file(od->fd, od->offset, FP_SET);
    len = read_file(od->fd, &tmp, sizeof(tmp));
    if (len <= 0) {
        return -1;
    }

    *de = tmp;

    od->offset += sizeof(tmp);
    return 0;
}

int seek_dir(int dir, int pos)
{
    struct fs_dir_open *od;
    od = idx_odir(dir);
    long offset;
    
    offset = sizeof(struct fs_dir_head) + pos * sizeof(struct fs_dir_entry);

    if (offset < 0) {
        offset = 0;
    }

    od->offset = offset;
    return 0;
}

int list_dir(struct super_block *sb, const char *path)
{
    int dir = open_dir(sb, path);
    if (dir < 0)
    {
        printf("open dir %s failed!\n", path);
        return -1;
    }

    struct fs_dir_entry de;
    
    printf("PATH: %s\n", path);
    printf("NUM     TYPE     NAME\n", de.fname.buf, de.type, de.num);
    while (!read_dir(dir, &de))
    {
        printf("%8d %8d %s\n", de.num, de.type, de.fname.buf);
    }

    close_dir(dir);

    return 0;
}
