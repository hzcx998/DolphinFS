#ifndef _FSCONFIG_H
#define _FSCONFIG_H

#define OPT_FS_DIR 1

/**
 * 支持的文件数量
 */
#define MAX_FILES 16384
#define MAX_OPEN_FILE 32

#define MAX_OPEN_DIR 32

#define FILES_PER_DIR 512

#define FILE_NAME_LEN 256

/**
 * 块缓存配置
 */
#define BLK_CACHE_SIZE 128

#endif
