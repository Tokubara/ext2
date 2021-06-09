//#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include "mylib.h"
#include "inode.h"
#include "ext2.h"

int main() {
  i32 ret;
  BlockDevice block_device{"diskfile"};
//  Bitmap bitmap{3,1,&block_device};
//  u32 ret;
//  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
//    ret = bitmap.alloc();
//    Assert(ret==i, "alloc:%u, should:%u", ret, i);
//  }
//  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
//    bitmap.dealloc(i);
//  }
//  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
//    ret = bitmap.alloc();
//    Assert(ret==i, "alloc:%u, should:%u", ret, i);
//  }
//  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
//    bitmap.dealloc(i);
//  }
//  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
//    ret = bitmap.alloc();
//    Assert(ret==0, "alloc:%u, should:%u", ret, 0);
//    bitmap.dealloc(0);
//  }
//  Ext2 ext2{};
//  std::cout << "Hello, World!" << std::endl;
// {{{2 对get_block_num_by_size的测试
//  assert(Inode::get_block_num_by_size(0)==0);
//assert(Inode::get_block_num_by_size(3)==1);
//  assert(Inode::get_block_num_by_size(513)==2);
//  assert(Inode::get_block_num_by_size(13824)==27); // 13824=27*512
//  assert(Inode::get_block_num_by_size(13825)==29); // 13824=27*512
//  assert(Inode::get_block_num_by_size(79360)==27+128+1); // 79360=(27+128)*512
//  assert(Inode::get_block_num_by_size(79361)==27+128+1+ 1+1+1);
  Ext2 ext2_;
  ext2_.create(&block_device, block_device.block_num);
//  ext2.root->ls();
//  Ext2 ext2;
//  ext2.open(&block_device);
//  ext2_.root->ls();
  Inode file0 = ext2_.root->create("file0", FileType::DIR);
  auto file0_content = "I hate you";
  file0.write_at(0, strlen(file0_content),(u8*)file0_content);
  u8 buf[100];
  file0.read_at(1,10,buf);
  for(u32 i = 0; i<10; i++) {
    putchar(buf[i]);
  }
  Inode dir0 = ext2_.root->create("dir0", FileType::DIR);
  dir0.ls();
  Inode file1 = dir0.create("file1", FileType::REG);
  auto file1_content = "I still hate you";
  file1.write_at(0, strlen(file1_content),(u8*)file1_content);
  file1.read_at(0,10,buf);
  for(u32 i = 0; i<10; i++) {
    putchar(buf[i]);
  }
  Inode tmp = ext2_.find_inode_by_full_path("/dir0/file1", &ret);
  assert(tmp.disk_inode->file_type==FileType::REG);
  tmp = ext2_.find_inode_by_full_path("file3", &ret);
  assert(ret==-1);
//  ext2_.root->ls();
//  log_trace("%u",ext2.data_area_start_block);
//  log_trace("%u",ext2_.data_area_start_block);
  return 0;
}
