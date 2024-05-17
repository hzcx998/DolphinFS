# DolphinFS

DolphinFS是一个基于key-value键值作为目录索引，基于类mmu页表映射作为数据索引，基于buddy作为块分配管理的文件系统，可用于大块文件存储。

## 功能

* 基于kv的文件管理
* 基于mmu模型的文件数据管理
* 基于bitmap的块管理
* 基于ram的块管理
* 基础的文件open，close，read，write操作
* 文件删除功能

## TODO

* 基于磁盘块的功能
* 将文件表纳入块管理
* 添加格式化功能
* 添加交互接口，通过命令行进行格式化，文件读写
* 将文件系统接入windows/linux，可以直接在上面创建文件系统
* 加入目录层拓展功能
* 测试文件读写性能
* 添加块缓存
* 添加tlb缓存，提高文件命中率

## 框架

- 文件 API
    * FS_OpenFile, FS_CloseFile, FS_ReadFile, FS_WriteFile, FS_SeekFile, FS_CreateFile, FS_RemoveFile,
    * (Opt) FS_RenameFile, FS_CopyFile
    * (Opt) FS_OpenDir, FS_CloseDir, FS_RewindDir, FS_ReadDir
- 块 API（后续考虑和DeviceIO兼容）
    * IO_OpenBlock, IO_CloseBlock, IO_ReadBlock, IO_WriteBlock
    * (Opt) IO_SyncBlock

