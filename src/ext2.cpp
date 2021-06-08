//
// Created by Isaac Quebec on 2021/6/8.
//

#include "ext2.h"

Ext2 Ext2::create(BlockDevice* block_device, u32 total_blocks,  u32 inode_bitmap_blocks)  {
        // {{{2 计算块数划分, 创建超级块
        Ext2 ext2;
        ext2.block_device=block_device;
        u32 inode_area_blocks = ceiling(inode_bitmap_blocks*512*8, INODE_NUM_PER_BLOCK); // bug:一开始忘了*8, 512 bytes对应的是4096个bits
        u32 inode_blocks = inode_bitmap_blocks+inode_area_blocks;
        u32 remaining = total_blocks-1-inode_blocks; // bug:zq忘了-1
        u32 data_bitmap_blocks=remaining/(512*8+1); // 不应该往上取整, 应该往下取整
        u32 data_area_blocks=remaining-data_bitmap_blocks;
        *(SuperBlock*)block_device->get_block_cache(0)=SuperBlock(total_blocks,inode_bitmap_blocks,inode_area_blocks, data_bitmap_blocks, data_area_blocks);
        ext2.inode_area_start_block=1;
        ext2.data_area_start_block=1+inode_blocks;
        // {{{2 创建位图
        ext2.inode_bitmap = new Bitmap(inode_bitmap_blocks, 1, block_device);
        ext2.data_bitmap = new Bitmap(data_bitmap_blocks, 1+inode_blocks, block_device);

        // {{{2 初始化root, 创建相应的目录
        u32 root_inode_id = ext2.inode_bitmap->alloc();
        assert(root_inode_id==0);
        ext2.root = (Inode*)ext2.block_device->get_block_cache(1);
        ext2.root->initialize_dir(root_inode_id);
        return ext2;
}
Inode* Ext2::get_inode_from_id(const u32 inode_id) const {
  return root+inode_id;
}

Ext2 Ext2::open(BlockDevice* block_device) {
        auto* sb = (SuperBlock*)block_device->get_block_cache(0);
        assert(sb->magic==EFS_MAGIC);
        Ext2 ext2;
        ext2.block_device=block_device;
        ext2.inode_bitmap=new Bitmap(sb->inode_bitmap_blocks, 1, block_device);
        u32 inode_blocks = sb->inode_bitmap_blocks+sb->inode_area_blocks;
        ext2.data_bitmap=new Bitmap(sb->inode_bitmap_blocks, 1+inode_blocks, block_device);
        ext2.inode_area_start_block = 1;
        ext2.data_area_start_block = 1+inode_blocks;
        ext2.root = ext2.get_inode_from_id(0);
        return ext2;
//        return Ext2(block_device, )
}