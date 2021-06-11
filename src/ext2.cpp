//
// Created by Isaac Quebec on 2021/6/8.
//

#include "ext2.h"
#include <queue>
#include <cstring>
#include <cerrno>

i32 Ext2::create(BlockDevice* block_device, u32 total_blocks, u32 inode_bitmap_blocks)  {
        // 格式化 TODO
//        for(u32 i = 0; i < total_blocks; i++) {
//                memset(block_device->get_block_cache(i), 0, BLOCK_SIZE);
//        }
        // {{{2 计算块数划分, 创建超级块
        this->block_device=block_device;
        u32 inode_area_blocks = ceiling(inode_bitmap_blocks*512*8, INODE_NUM_PER_BLOCK); // bug:一开始忘了*8, 512 bytes对应的是4096个bits
        u32 inode_blocks = inode_bitmap_blocks+inode_area_blocks;
        u32 remaining = total_blocks-1-inode_blocks; // bug:zq忘了-1
        u32 data_bitmap_blocks=remaining/(512*8+1); // 不应该往上取整, 应该往下取整
        u32 data_area_blocks=remaining-data_bitmap_blocks;
        *(SuperBlock*)block_device->get_block_cache(0)=SuperBlock(total_blocks,inode_bitmap_blocks,inode_area_blocks, data_bitmap_blocks, data_area_blocks);
        this->inode_area_start_block=1;
        this->data_area_start_block=1+inode_blocks+data_bitmap_blocks;
//        log_debug("data_area_start_block: %u", this->data_area_start_block);
        // {{{2 创建位图
        this->inode_bitmap = new Bitmap(inode_bitmap_blocks, 1, block_device);
        this->inode_bitmap->initialize();
        this->data_bitmap = new Bitmap(data_bitmap_blocks, 1+inode_blocks, block_device);
        this->inode_bitmap->initialize();
        // {{{2 初始化root, 创建相应的目录
        u32 root_inode_id = this->_alloc_inode();
        assert(root_inode_id==0);
//        assert(this->_alloc_inode()==1);

        this->disk_inode_start = (DiskInode*)this->block_device->get_block_cache(1+inode_bitmap_blocks);
        this->root = new Inode(this, this->disk_inode_start, root_inode_id);
        this->root->_initialize_dir(root_inode_id);

        // 初始化锁
        pthread_rwlock_init(&this->lock, nullptr);
        return 0;
}
DiskInode* Ext2::_get_disk_inode_from_id(const u32 inode_id) const {
  return this->disk_inode_start+inode_id;
}

// 用于打开一个文件系统, 而不是创建
void Ext2::open(BlockDevice* block_device) {
        auto* sb = (SuperBlock*)block_device->get_block_cache(0);
        assert(sb->magic==EFS_MAGIC);
        this->block_device=block_device;
        this->inode_bitmap=new Bitmap(sb->inode_bitmap_blocks, 1, block_device);
        u32 inode_blocks = sb->inode_bitmap_blocks+sb->inode_area_blocks;
        this->data_bitmap=new Bitmap(sb->data_bitmap_blocks, 1+inode_blocks, block_device);
        this->inode_area_start_block = 1;
        this->data_area_start_block = 1+inode_blocks+sb->data_bitmap_blocks;
        this->disk_inode_start = (DiskInode*)this->block_device->get_block_cache(1+sb->inode_bitmap_blocks);
        this->root = new Inode(this, this->disk_inode_start, 0);
        log_trace("inode_bitmap_blocks:%u, inode_blocks:%u, data_bitmap_blocks:%u", sb->inode_bitmap_blocks, inode_blocks, sb->data_bitmap_blocks);
        pthread_rwlock_init(&this->lock, nullptr);
//        return Ext2(block_device, )
}

// increase_size中调用
u32 Ext2::_alloc_data() {
        return this->data_bitmap->alloc()+this->data_area_start_block;
}

// inode create中调用
u32 Ext2::_alloc_inode() {
        return this->inode_bitmap->alloc(); // 分配inode号与alloc_data并不对称, 本来就是从0开始
}

/**
 * 需要维护ret, 此函数需要上锁, inode不调用它, 但是fuse.cpp中会调用它
 * 顶层函数, 需要锁
 * */
Inode Ext2::_find_inode_by_full_path(const char *path) {
        if(path[0]!='/') {
                log_error("invalid path");
                return Inode::invalid_inode();
        }
        std::queue<std::string> path_tokens_str = split_path(path);
        std::string name;
        Inode dir = *this->root;
        while(!path_tokens_str.empty()) {
                name = path_tokens_str.front();
                path_tokens_str.pop();
                if(dir.disk_inode->file_type!=FileType::DIR) {
                        return Inode::invalid_inode();
                }
                dir = dir._find(name, nullptr); // 这里用的就是底层_find
                if(!dir.is_self_valid()) return Inode::invalid_inode();
        }
        return dir;
}

std::queue<std::string> Ext2::split_path(const char *path) {
        std::string path_str(path);
        std::queue<std::string> path_tokens_str;
        u64 pos;
        std::string s;
        while ((pos = path_str.find('/')) != std::string::npos) {
                s = path_str.substr(0, pos);
                if (s.length() != 0) {
                        path_tokens_str.push(std::move(s));
                }
                path_str.erase(0, pos + 1);
        }
        if (path_str.length() > 0) {
                path_tokens_str.push(path_str);
        }
        return path_tokens_str;
}

u32 Ext2::_dealloc_inode(u32 inode_number) {
        assert(inode_number!=0); // 根目录节点必须有
        this->inode_bitmap->dealloc(inode_number);
        return 0;
}

u32 Ext2::_dealloc_data(u32 data_block_id) {
        this->inode_bitmap->dealloc(data_block_id-this->data_area_start_block);
        return 0;
}

// 此函数需要上锁, 调用它不是在inode中, 而是作为顶层函数在fuse_ext2中用fh时调用过,
Inode Ext2::get_inode_from_id(u32 inode_id) {
        assert(this->inode_bitmap->test_exist(inode_id));
        return Inode(this, _get_disk_inode_from_id(inode_id), inode_id);
}

void Ext2::rdlock() {
        pthread_rwlock_rdlock(&this->lock);
}

void Ext2::wrlock() {
        pthread_rwlock_wrlock(&this->lock);
}

void Ext2::unlock() {
        pthread_rwlock_unlock(&this->lock);
}

// 对底层_find_inode_by_full_path的包装
Inode Ext2::_get_parent_inode_and_basename(const char *path, const char **basename) {
        char* path_copy = (char*)malloc(sizeof(path)+1);
        strcpy(path_copy,path);
        *basename = strrchr(path,'/'); // 现在指向的还有/
        path_copy[*basename-path]='\0';
        (*basename)+=1; // 之前指向的是/, 现在指向下一个
        // bug: 未处理父目录本来就是根目录的情况, 比如/a, 那么这样的话, 之前的实现中, path_copy会为空串, 这种情况下, 将它修改为'/'
        if(strlen(path_copy)==0) {
                path_copy[0]='/';
                path_copy[1]='\0';
        }
        Inode parent_inode = this->_find_inode_by_full_path(path_copy);
        free(path_copy);
        return parent_inode;
}

int Ext2::rename(const char *oldpath, const char *newpath) {
        this->wrlock(); // 好吧, 其实这里可以前面用读锁, 后面用写锁, 但是没必要
        const char* basename;
        Inode old_parent_inode = _get_parent_inode_and_basename(oldpath, &basename);
        if(!old_parent_inode.is_self_valid()) {
                this->unlock();
                return -ENOENT;
        } else if(!old_parent_inode.is_dir()) {
                this->unlock();
                return -ENOTDIR;
        }
        u32 dirent_index;
        Inode inode = old_parent_inode._find(basename, &dirent_index);
        if(!inode.is_self_valid()) {
                this->unlock();
                return -ENOENT;
        }

        Inode new_parent_inode = _get_parent_inode_and_basename(newpath, &basename);
        if(!new_parent_inode.is_self_valid()) {
                this->unlock();
                return -ENOENT;
        } else if(!new_parent_inode.is_dir()) {
                this->unlock();
                return -ENOTDIR;
        }
        if(new_parent_inode._find(basename, nullptr).is_self_valid()) {
                this->unlock();
                return -EEXIST;
        }
        // 去除这个目录项
        old_parent_inode._rm_direntry(dirent_index);
        new_parent_inode._write_dirent(basename,inode.disk_inode->inode_number);
        this->unlock();
        return 0;
}

Inode Ext2::find_inode_by_full_path(const char *path) {
        this->rdlock();
        Inode ret =  _find_inode_by_full_path(path);
        this->unlock();
        return ret;
}

Inode Ext2::get_parent_inode_and_basename(const char *path, const char **basename) {
        this->rdlock();
        Inode ret =  this->_get_parent_inode_and_basename(path, basename);
        this->unlock();
        return ret;
}
