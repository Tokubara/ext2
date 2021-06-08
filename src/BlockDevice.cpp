//
// Created by Isaac Quebec on 2021/6/8.
//

#include "BlockDevice.h"

BlockDevice::BlockDevice(std::string file_path) {
fd = open(file_path.c_str(), O_RDWR); // 这里是提前创建好的文件
assert(fd>0);
struct stat stat;
fstat(fd, &stat);
file_size = stat.st_size;
block_num = file_size/BLOCK_SIZE;
log_trace("file size: %lu, block num: %d", file_size, block_num);
block = (u8*)mmap(nullptr, file_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
}

BlockDevice::~BlockDevice() {
  munmap(block, file_size);
  close(fd);
}

u8* BlockDevice::get_block_cache(u32 block_id) {
  assert(block_id<file_size/BLOCK_SIZE);
  return block+BLOCK_SIZE*block_id;
}
