//
// Created by Isaac Quebec on 2021/6/8.
//

#include "ext2.h"

Ext2::Ext2(BlockDevice* block_device, u32 total_blocks,  u32 inode_bitmap_blocks) : block_device(block_device) {
        // {{{2 计算块数划分, 创建超级块
        u32 inode_area_blocks = ceiling(inode_bitmap_blocks*512*8, INODE_NUM_PER_BLOCK); // bug:一开始忘了*8, 512 bytes对应的是4096个bits
        u32 inode_blocks = inode_bitmap_blocks+inode_area_blocks;
        u32 remaining = total_blocks-1-inode_blocks; // bug:zq忘了-1
        u32 data_bitmap_blocks=remaining/(512*8+1); // 不应该往上取整, 应该往下取整
        u32 data_area_blocks=remaining-data_bitmap_blocks;
        *(SuperBlock*)block_device->get_block_cache(0)=SuperBlock(total_blocks,inode_bitmap_blocks,inode_area_blocks, data_bitmap_blocks, data_area_blocks);
        // {{{2 创建位图
        inode_bitmap = new Bitmap(inode_bitmap_blocks, 1, block_device);
        data_bitmap = new Bitmap(data_bitmap_blocks, 1+inode_blocks, block_device);

        // {{{2 初始化root, 创建相应的目录
        u32 root_inode_id = inode_bitmap->alloc();
        assert(root_inode_id==0);
        this->root = root+root_inode_id;
        root->initialize(FileType::DIR);
}
Inode* Ext2::get_inode_from_id(const u32 inode_id) const {
  return root+inode_id;
}