#include <iostream>
#include <unistd.h>
#include <cstring>
#include "mylib.h"
//#include <sys/stat.h>
//#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

const u32 BLOCK_SIZE = 512;
const u32 EFS_MAGIC = 0x3b800001;
const u32 INODE_BITMAP_BLOCKS = 1; // 这样的话, 总共能创建512*8个文件和目录
const u32 INODE_NUM_PER_BLOCK = 4;

inline int trailing_ones( uint32_t in ) {
  return ~ in == 0? 32 : __builtin_ctz( ~ in );
}

struct Inode {
  u32 find(std::string path);
  i32 increase_size(u32 block_num);
  i32 read_at(u32 offset, u32 len, u8* buffer);
  i32 write_at(u32 offset, u32 len, const u8* buffer);
/**
 * 初始化目录
 * @return
 */
    i32 initialize_dir() {

      return 0;
    }
};

struct BlockDevice {
    u8* block;
    i32 fd;
    u32 file_size;
    u32 block_num;
    BlockDevice(std::string file_path) {
      fd = open(file_path.c_str(), O_RDWR); // 这里是提前创建好的文件
      assert(fd>0);
      struct stat stat;
      fstat(fd, &stat);
      file_size = stat.st_size;
      block_num = file_size/BLOCK_SIZE;
      log_trace("file size: %lu, block num: %d", file_size, block_num);
      block = (u8*)mmap(nullptr, file_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    }
    ~BlockDevice() {
      munmap(block, file_size);
      close(fd);
    }
    u8* get_block_cache(u32 block_id) {
      assert(block_id<file_size/BLOCK_SIZE);
      return block+BLOCK_SIZE*block_id;
    }
};

struct Bitmap {
    u32 block_num; // 有几块
    u32 start_block_id;
    BlockDevice* block_device;
    u8* bitmap_block; // 这就是它指向的内存区域
    u32 alloc() const {
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
    void dealloc(u32 bit_id) {
      u32 block_id = bit_id/8;
      u32 in_byte_offset = bit_id%8;
      bitmap_block[block_id]&=~(1u<<in_byte_offset);
    }
    Bitmap(u32 blocks, u32 start_block_id, BlockDevice* block_device):block_num(blocks), start_block_id(start_block_id), block_device(block_device) {
      bitmap_block = block_device->get_block_cache(start_block_id);
      memset(bitmap_block,0,block_num*BLOCK_SIZE);
    }
};

/**
 * ceiling(x/y)
 * @param x
 * @param y
 * @return
 */
inline u32 ceiling(u32 x, u32 y) {
return (x+y-1)/y;
}

struct SuperBlock {
    u32 magic;
    u32 total_blocks;
    u32 inode_bitmap_blocks;
    u32 inode_area_blocks;
    u32 data_bitmap_blocks;
    u32 data_area_blocks;
    SuperBlock(u32 total_blocks, u32 inode_bitmap_blocks, u32 inode_area_blocks, u32 data_bitmap_blocks, u32 data_area_blocks):magic(EFS_MAGIC), total_blocks(total_blocks), inode_bitmap_blocks(inode_bitmap_blocks), inode_area_blocks(inode_area_blocks), data_bitmap_blocks(data_bitmap_blocks), data_area_blocks(data_area_blocks){}

};

struct Ext2 {
    Ext2(BlockDevice* block_device, u32 total_blocks,  u32 inode_bitmap_blocks = INODE_BITMAP_BLOCKS) : block_device(block_device) {
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
      root->initialize_dir();
    }
    Inode* get_inode_from_id(const u32 inode_id) const {
      return root+inode_id;
    }

   Inode* root;
    BlockDevice* block_device; // 这里存引用真的靠谱么
    Bitmap* inode_bitmap;
    Bitmap* data_bitmap;
};

int main() {
  BlockDevice block_device{"diskfile"};
  Bitmap bitmap{3,1,&block_device};
  u32 ret;
  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
    ret = bitmap.alloc();
    Assert(ret==i, "alloc:%u, should:%u", ret, i);
  }
  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
    bitmap.dealloc(i);
  }
  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
    ret = bitmap.alloc();
    Assert(ret==i, "alloc:%u, should:%u", ret, i);
  }
  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
    bitmap.dealloc(i);
  }
  for(u32 i = 0; i<3*8*BLOCK_SIZE; i++) {
    ret = bitmap.alloc();
    Assert(ret==0, "alloc:%u, should:%u", ret, 0);
    bitmap.dealloc(0);
  }
//  Ext2 ext2{};
//  std::cout << "Hello, World!" << std::endl;
  return 0;
}
