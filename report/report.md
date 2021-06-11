#### 运行方式

```
# 已装了libfuse后
make fuse # 不是make, 默认make是make main
# 生成的可执行文件是build/fuse
dd if=/dev/zero of=diskfile count=8096 # 这一部分是写死的文件名, 只能是diskfile
mkdir fuse_test
build/fuse fuse_test
fusermount -u build/fuse # 结束运行

# 或者可以
source tool.sh
out # 将可执行文件./build/fuse拷贝到~下
fuse_rr # 运行 
fuse_fi # 结束运行
```

#### 实现

文件系统模仿ext2实现. block device(由文件模拟, 一个块的大小是512 bytes)由5部分组成: 超级块, inode位图, inode数据块, 数据块位图, 数据块. 支持二级间接索引块.

主要分为4层, BlockDevice, Ext2, Inode, fuse.

##### BlockDevice层

对应BlockDevice.h, BlockDevice.cpp.

构造函数是对给定文件名进行mmap, 析构是munmap.

这一层对上一层提供的接口只有一个: `u8* BlockDevice::get_block_cache(u32 block_id)`, 给物理块号, 返回指向这个块的指针.

对fysnc的实现也是对这一层调用msync.

##### Ext2层

这一层管理块设备, 对Inode层提供了这些支持: 分配,回收inode和数据块, 以及提供锁的支持, Ext2有一个字段是读写锁`pthread_rwlock_t`, 因此有rdlock, wrlock, unlock操作.

对fuse层提供了这些支持: create, 初始化文件系统, 具体来说是写超级块, 初始化位图(清空), 创建root Inode. 以及open, 打开一个已经被初始化过的文件系统.

以及与绝对路径相关的操作: 最重要的是`Inode Ext2::find_inode_by_full_path(const char *path)`, 参数是绝对路径, 返回Inode, 如果不存在, 返回的是无效Inode. `get_parent_inode_and_basename`是对`find_inode_by_full_path`做了封装, 由于mkdir, rmdir等操作经常需要找到父目录对应的Inode而做了这个封装.

以及rename接口, `int Ext2::rename(const char *oldpath, const char *newpath)`.

##### Inode层

文件操作的核心层. 核心的函数包括: `_increase_size`, 文件大小如果需要增大, 会调用这个函数, 涉及到写直接索引, 间接索引块. `u32 Inode::_logic_to_phy_block_id(u32 logic_id)`, 文件的逻辑块号转换为物理块号, 也需要处理直接索引, 间接索引块等. 在实现这两个函数之后, `_write_at`和`_read_at`的实现就比较容易了. 还有`create`(如果Inode是目录, 创建子节点), `truncate`(改变文件大小), `link`(创建hard link)等方法, 它们是建立在`_read_at`, `_write_at`等方法基础上的. 实现中比如`_read_at`, 还有`read_at`函数, 类似有`_write_at`和`write_at`, 参数相同, 名字只差一个下划线, 原因在后面叙述困难时解释.

##### fuse层

实现fuse的fuse_operations结构体的各个函数. 实现了以下函数:

```
static struct fuse_operations file_operations = {
        .getattr    = ext2_getattr,
        .mkdir      = ext2_mkdir,
        .unlink     = ext2_unlink,
        .rmdir      = ext2_rmdir,
        .rename     = ext2_rename,
        .link       = ext2_link,
        .truncate   = ext2_truncate,
        .open       = ext2_open,
        .read       = ext2_read,
        .write      = ext2_write,
        .fsync      = ext2_fsync,
        .readdir    = ext2_readdir,
        .create     = ext2_create,
};
```

 这一层就是使用Inode层和Ext层提供的各种接口, 再加上一些错误判断.

#### 实现中遇到的困难

防止死锁是需要考虑的问题, 最开始接口调用混乱. 比如, `read_at`这个方法, 会在fuse层(顶层)调用, 也会在Inode其它方法中调用, 如果`read_at`不加锁, fuse层调用会没有锁保护, 如果`read_at`. 于是, 把Inode和ext2类方法分为2类, 顶层函数, 被顶层fuse调用,  加锁. 底层函数, 会被顶层方法调用, 因此不加锁, 否则死锁. 为了区分, 底层方法名以`_`开始. 如果有方法既需要被fuse层调用, 又被Inode, Ext2类的其它方法调用, 比如`read_at`, 那就再创建`_read_at`方法, 前者封装后者且加锁, 后者不加锁.