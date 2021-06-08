//
// Created by Isaac Quebec on 2021/6/8.
//

#include "bitmap.h"
#include <cstring>

u32 Bitmap::alloc() const {
  u32 total_bytes_num = block_num*BLOCK_SIZE;
  for(u32 i = 0; i < total_bytes_num; i++) {
    if(bitmap_block[i]!=0xFF) {
      u32 in_byte_offset = trailing_ones(bitmap_block[i]);
      bitmap_block[i]|=(1u<<in_byte_offset);
//          assert( ((bitmap_block[i]>>in_byte_offset)&1u) >0);
      return i*8+in_byte_offset;
    }
  }
  panic("cannot alloc, no more");
//      return 0; // 为0的结果是不能用的
}

void Bitmap::dealloc(u32 bit_id) {
  u32 block_id = bit_id/8;
  u32 in_byte_offset = bit_id%8;
  bitmap_block[block_id]&=~(1u<<in_byte_offset);
}

Bitmap::Bitmap(u32 blocks, u32 start_block_id, BlockDevice* block_device):block_num(blocks), start_block_id(start_block_id), block_device(block_device) {
  bitmap_block = block_device->get_block_cache(start_block_id);
  memset(bitmap_block,0,block_num*BLOCK_SIZE);
}