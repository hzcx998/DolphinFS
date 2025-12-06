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
* 添加交互接口，通过命令行进行格式化，文件读写
* 加入目录层拓展功能

## TODO

* 将文件系统接入windows/linux，可以直接在上面创建文件系统
* 测试文件读写性能
* 添加块缓存
* 添加tlb缓存，提高文件命中率
* 基于buddy进行块管理
* 优化目录索引，根据目录数量拓展位图。

## 框架

- 文件 API
    * open_file, close_file, read_file, write_file, seek_file, delete_file,
    * (Opt) rename_file, stat_file
    * (Opt) open_dir, close_dir, rewind_dir, read_dir
- 块 API
    * open_blkdev, close_blkdev, read_block, write_block，get_capacity
    * (Opt) sync_block

## 使用

使用run脚本运行，包含编译，生成磁盘，文件读写测试，清除文件。

```bash
./run.sh
```

## 目录

目录结构，特殊格式保存目录信息，包括file num，用于快速找到文件。
目录也是一个普通文件，只不过存在映射关系。而读取目录文件，就可以找到子文件。
当然，删除目录，也就是删除文件映射关系，以及子文件。
