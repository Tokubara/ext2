//
// Created by Isaac Quebec on 2021/6/8.
//

#include "ext2.h"

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
        // {{{2 创建位图
        this->inode_bitmap = new Bitmap(inode_bitmap_blocks, 1, block_device);
        this->data_bitmap = new Bitmap(data_bitmap_blocks, 1+inode_blocks, block_device);

        // {{{2 初始化root, 创建相应的目录
        u32 root_inode_id = this->alloc_inode();
        assert(root_inode_id==0);

        this->root = new Inode(this, (DiskInode*)this->block_device->get_block_cache(1));
        this->root->initialize_dir(root_inode_id);
        return 0;
}
Inode* Ext2::get_disk_inode_from_id(const u32 inode_id) const {
  return root+inode_id;
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
        this->root = this->get_disk_inode_from_id(0);
//        return Ext2(block_device, )
}

u32 Ext2::alloc_data() {
        return this->data_bitmap->alloc()+this->data_area_start_block;
}

u32 Ext2::alloc_inode() {
        return this->inode_bitmap->alloc(); // 分配inode号与alloc_data并不对称, 本来就是从0开始
}
