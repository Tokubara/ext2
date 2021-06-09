//
// Created by Isaac Quebec on 2021/6/8.
//

#include "ext2.h"
#include <queue>

i32 Ext2::create(BlockDevice* block_device, u32 total_blocks, u32 inode_bitmap_blocks)  {
        // {{{2 计算块数划分, 创建超级块
        this->block_device=block_device;
        u32 inode_area_blocks = ceiling(inode_bitmap_blocks*512*8, INODE_NUM_PER_BLOCK); // bug:一开始忘了*8, 512 bytes对应的是4096个bits
        u32 inode_blocks = inode_bitmap_blocks+inode_area_blocks;
        u32 remaining = total_blocks-1-inode_blocks; // bug:zq忘了-1
        u32 data_bitmap_blocks=remaining/(512*8+1); // 不应该往上取整, 应该往下取整
        u32 data_area_blocks=remaining-data_bitmap_blocks;
        *(SuperBlock*)block_device->get_block_cache(0)=SuperBlock(total_blocks,inode_bitmap_blocks,inode_area_blocks, data_bitmap_blocks, data_area_blocks);
        this->inode_area_start_block=1;
        this->data_area_start_block=1+inode_blocks;
//        log_debug("data_area_start_block: %u", this->data_area_start_block);
        // {{{2 创建位图
        this->inode_bitmap = new Bitmap(inode_bitmap_blocks, 1, block_device);
        this->inode_bitmap->initialize();
        this->data_bitmap = new Bitmap(data_bitmap_blocks, 1+inode_blocks, block_device);
        this->inode_bitmap->initialize();
        // {{{2 初始化root, 创建相应的目录
        u32 root_inode_id = this->alloc_inode();
        assert(root_inode_id==0);

        this->disk_inode_start = (DiskInode*)this->block_device->get_block_cache(1);
        this->root = new Inode(this, this->disk_inode_start, root_inode_id);
        this->root->initialize_dir(root_inode_id);
        return 0;
}
DiskInode* Ext2::get_disk_inode_from_id(const u32 inode_id) const {
  return this->disk_inode_start+inode_id;
}

void Ext2::open(BlockDevice* block_device) {
        auto* sb = (SuperBlock*)block_device->get_block_cache(0);
        assert(sb->magic==EFS_MAGIC);
        this->block_device=block_device;
        this->inode_bitmap=new Bitmap(sb->inode_bitmap_blocks, 1, block_device);
        u32 inode_blocks = sb->inode_bitmap_blocks+sb->inode_area_blocks;
        this->data_bitmap=new Bitmap(sb->inode_bitmap_blocks, 1+inode_blocks, block_device);
        this->inode_area_start_block = 1;
        this->data_area_start_block = 1+inode_blocks;
        this->disk_inode_start = (DiskInode*)this->block_device->get_block_cache(1);
        this->root = new Inode(this, this->disk_inode_start, 0);
//        return Ext2(block_device, )
}

u32 Ext2::alloc_data() const {
        return this->data_bitmap->alloc()+this->data_area_start_block;
}

u32 Ext2::alloc_inode() const {
        return this->inode_bitmap->alloc(); // 分配inode号与alloc_data并不对称, 本来就是从0开始
}



/**
 * 需要维护ret
 *
 * */
Inode Ext2::find_inode_by_full_path(const char *path,  i32* ret) const {
        *ret=-1;
        if(path[0]!='/') {
                log_error("invalid path");
                return Inode{nullptr, nullptr,0};
        }
        std::queue<std::string> path_tokens_str = split_path(path);
        std::string name;
        Inode* dir = this->root;
        while(!path_tokens_str.empty()) {
                name = path_tokens_str.front();
                path_tokens_str.pop();
                if(dir->disk_inode->file_type!=FileType::DIR) {
                        return Inode{nullptr, nullptr,0};
                }
                *dir = dir->find(name, ret);
                if(*ret<0) return Inode{nullptr, nullptr,0};
        }
        *ret=0;

        return *dir;
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
