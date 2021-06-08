//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_BITMAP_H
#define EXT2_BITMAP_H
#include "common.h"
#include "BlockDevice.h"

struct Bitmap {
    u32 block_num; // 有几块
    u32 start_block_id;
    BlockDevice* block_device;
    u8* bitmap_block; // 这就是它指向的内存区域
    u32 alloc() const;
    void dealloc(u32 bit_id);
    Bitmap(u32 blocks, u32 start_block_id, BlockDevice* block_device);
};

#endif //EXT2_BITMAP_H
