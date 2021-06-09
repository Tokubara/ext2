//
// Created by Isaac Quebec on 2021/6/8.
//

#include "inode.h"
#include "ext2.h"
#include <queue>
#include <cstring>

Inode::Inode(Ext2* ext2, DiskInode* disk_inode):disk_inode(disk_inode), fs(ext2) {}

/** 此函数在文件长度可能增大的时候调用, 如果new_size<当前size, 啥也不做就返回了
 * i32是因为可能失败, 但是目前不做处理
 * */
i32 Inode::increase_size(u32 new_size) {
  // TODO
//  assert(sizeof this->disk_inode==128);
  if(new_size<=this->disk_inode->size) return 0;
  // {{{2 虽然大小增大, 但是需要的块数并没有增多
  u32 need = get_block_num_by_size(new_size)-get_block_num_by_size(disk_inode->size);
  assert(need>=0);
  if(need==0) return 0;
  // {{{2 分配需要的块数
  std::queue<u32> block_id;
  assert(fs!=nullptr); // TODO fs
  for(u32 i = 0; i < need; i++) {
    block_id.push(fs->alloc_data());
  }

  // 为方便, 计算数据块数
  u32 cur_data_block_num = ceiling(disk_inode->size,BLOCK_SIZE); // 这个变量会不断修改, 直到==need_block_num
//  u32 need_data_block_num = ceiling(new_size,BLOCK_SIZE);
//  assert(need_data_block_num>=cur_data_block_num);

  // {{{2 写直接块
  while(cur_data_block_num < INODE_DIRECT_NUM && !block_id.empty()) {
    this->disk_inode->direct[cur_data_block_num++]=block_id.front();
    block_id.pop();
  }
  if(block_id.empty()) {
    return 0;
  }
  // {{{2 准备一级块
  if(cur_data_block_num==INODE_DIRECT_NUM) { // 需要分配一级块, 否则一级块一定有了
    this->disk_inode->indirect1=block_id.front();
    block_id.pop();
    assert(!block_id.empty()); // 有一级块肯定还有别的
  } else {
    assert(this->disk_inode->indirect1!=0);
  }
  u32* indirect1_array = (u32*)fs->block_device->get_block_cache(this->disk_inode->indirect1);
  cur_data_block_num-=INODE_DIRECT_NUM;
  while(cur_data_block_num<INDEX_NO_PER_BLOCK && !block_id.empty()) {
    indirect1_array[cur_data_block_num++]=block_id.front();
    block_id.pop();
  }
  if(block_id.empty()) {
    return 0;
  }

  // 准备二级块
  cur_data_block_num-=INDEX_NO_PER_BLOCK;
  u32 a0 = cur_data_block_num/INDEX_NO_PER_BLOCK; // a0存的是目标块号
  u32 b0 = cur_data_block_num%INDEX_NO_PER_BLOCK;
  u32 tar_data_block_num = ceiling(new_size, BLOCK_SIZE)-INODE_DIRECT_NUM-INDEX_NO_PER_BLOCK; // tmp
  u32 a1 = tar_data_block_num/INDEX_NO_PER_BLOCK;
  u32 b1 = tar_data_block_num%INDEX_NO_PER_BLOCK;

  u32* indirect2_array = nullptr;
  indirect1_array = nullptr;
  if(cur_data_block_num==0) {
    this->disk_inode->indirect2=block_id.front();
    block_id.pop();
    assert(!block_id.empty()); // 有一级块肯定还有别的
    indirect2_array = (u32*)fs->block_device->get_block_cache(this->disk_inode->indirect2);
  } else {
    assert(this->disk_inode->indirect2!=0);
    indirect2_array = (u32*)fs->block_device->get_block_cache(this->disk_inode->indirect2);
    assert(indirect2_array[a0]>0);
    if(b0!=0) {
      indirect1_array=(u32*)fs->block_device->get_block_cache(indirect2_array[a0]);
    }
  }
  while(a0<a1||(a0==a1 && b0<b1)) {
    if(b0==0) {
      indirect2_array[a0]=block_id.front();// 是么?
      block_id.pop();
      indirect1_array=(u32*)fs->block_device->get_block_cache(indirect2_array[a0]);
    }
    indirect1_array[b0++]=block_id.front();
    block_id.pop();
    if(b0==INDEX_NO_PER_BLOCK) {
      a0++;
      b0=0;
    }
  }
  assert(block_id.empty());
  this->disk_inode->size = new_size;
  return 0;
}

i32 Inode::write_at(const u32 offset, u32 len, const u8* buffer) {
  increase_size(offset+len);
  u32 in_offset = offset%BLOCK_SIZE;
  u32 logic_block_id = offset/BLOCK_SIZE;
  u32 cur_len = 0;
  while(cur_len<len) {
    u32 phy_block_id = logic_to_phy_block_id(logic_block_id);
    u8* file_block = this->fs->block_device->get_block_cache(phy_block_id);
    file_block+=in_offset;
    memcpy(file_block,buffer+cur_len,std::min(len-cur_len,BLOCK_SIZE-in_offset));
    in_offset=0;
    cur_len+=std::min(len-cur_len,BLOCK_SIZE-in_offset);
    logic_block_id++;
  }
  return 0;
}

i32 Inode::initialize_dir(u32 self_inode_number) {
      DirEntry dir[2];
      assert(sizeof(DirEntry)==32);
      if(self_inode_number==0) {
        dir[0].inode_number=0;
        memcpy(dir[0].name, "." , sizeof("."));
        dir[1].inode_number=0;
        memcpy(dir[1].name, ".." , sizeof(".."));
      } else {
        TODO();
      }
      this->write_at(0,sizeof(DirEntry)*2, (u8*)dir);
      this->disk_inode->file_type=FileType::DIR;
      log_trace("first data block id:%u",this->disk_inode->direct[0]);
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

u32 Inode::logic_to_phy_block_id(u32 logic_id) {
  u32 phy_id = 0;
  assert(logic_id<FILE_MAX_BLOCK_NUM);
  if(logic_id<INODE_DIRECT_NUM) {
    phy_id = this->disk_inode->direct[logic_id];
  } else if(logic_id<INODE_DIRECT_NUM+INDEX_NO_PER_BLOCK) {
    assert(this->disk_inode->indirect1>0);
    u32* indirect1_array = (u32*)fs->block_device->get_block_cache(this->disk_inode->indirect1);
    phy_id=indirect1_array[logic_id-INODE_DIRECT_NUM];
  } else {
    assert(this->disk_inode->indirect2>0);
    u32 tmp = logic_id - (INODE_DIRECT_NUM+INDEX_NO_PER_BLOCK);
    u32* indirect2_array = (u32*)fs->block_device->get_block_cache(this->disk_inode->indirect2);
    u32 indirect1_index = indirect2_array[tmp/INDEX_NO_PER_BLOCK];
    u32* indirect1_array=(u32*)fs->block_device->get_block_cache(indirect2_array[indirect1_index]);
    phy_id = indirect1_array[tmp%INDEX_NO_PER_BLOCK];
  }
  return phy_id;
}
