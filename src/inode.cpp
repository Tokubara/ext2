//
// Created by Isaac Quebec on 2021/6/8.
//

#include "inode.h"
#include "ext2.h"

i32 Inode::increase_size(u32 need) {
  std::vector<u32> block_id;
  for(u32 i = 0; i < need; i++) {
    assert(fs!=nullptr);
    block_id.push_back(fs->data_bitmap->alloc());
  }
  return 0;
}

i32 Inode::write_at(u32 offset, u32 len, const u8* buffer) {
  u32 cur_block_num = get_block_num_by_size(diskInode.size); // TODO: 为了过编
  u32 need_block_num = get_block_num_by_size(offset+len);
  if(offset+len>cur_block_num) {
    increase_size(need_block_num-cur_block_num);
  }
  return cur_block_num;
}

i32 Inode::initialize(FileType file_type) {
  switch(file_type) {
    case FileType::REG : {
      TODO();
      break;
    }
    case FileType::DIR : {
    }
  }
  return 0;
}

/**
 * 根据文件大小, 计算需要的索引块和数据块数的总和, 已测试
 * */
u32 Inode::get_block_num_by_size(const u32 size)  {
  u32 data_block_num = ceiling(size, BLOCK_SIZE);
  assert(data_block_num<=FILE_MAX_BLOCK_NUM);
  u32 total=data_block_num;
  if(data_block_num>INODE_DIRECT_NUM) {
    total+=1; // 一级索引块
    data_block_num-=INODE_DIRECT_NUM;
    if(data_block_num>INDEX_NO_PER_BLOCK) { // 需要二级索引块
      total+=1;
      data_block_num-=INDEX_NO_PER_BLOCK;
      total+=ceiling(data_block_num, INDEX_NO_PER_BLOCK);
    }
  }
  return total;
}