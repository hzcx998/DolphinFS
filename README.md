# DolphinFS

DolphinFS是一个基于key-value键值作为目录索引，基于类mmu页表映射作为数据索引，基于buddy作为块分配管理的文件系统，可用于大块文件存储。比如在嵌入式领域替代FatFS，做音频，视频，图形相关存储。

## 功能

* 基于kv的文件管理
* 基于mmu模型的文件数据管理
* 基于bitmap的块管理
* 基于ram的块管理
* 基础的文件open，close，read，write操作
* 文件删除功能
* 将文件表纳入块管理
* 基于磁盘块的功能
* 添加格式化功能

## TODO

* 添加交互接口，通过命令行进行格式化，文件读写
* 将文件系统接入windows/linux，可以直接在上面创建文件系统
* 加入目录层拓展功能
* 测试文件读写性能
* 添加块缓存
* 添加tlb缓存，提高文件命中率
* 基于buddy进行块管理

## 框架

- 文件 API
    * open_file, close_file, read_file, write_file, seek_file, delete_file,
    * (Opt) rename_file, stat_file
    * (Opt) open_dir, close_dir, rewind_dir, read_dir
- 块 API
    * open_blkdev, close_blkdev, read_block, write_block，get_capacity
    * (Opt) sync_block

## 使用

1. 安装构建程序

本项目使用xmake进行构建，需要先安装xmake，参考这个链接自行安装下载 [xmake](https://xmake.io/mirror/zh-cn/guide/installation.html)

2. 创建虚拟磁盘

```bash
./gendisk.sh
```

3. 编译源码并运行

```bash
./run.sh
```
