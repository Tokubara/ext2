//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_COMMON_H
#define EXT2_COMMON_H

#include "mylib.h"
//#include <vector>

const u32 BLOCK_SIZE = 512;
const u32 EFS_MAGIC = 0x3b800001;
const u32 INODE_BITMAP_BLOCKS = 1; // 这样的话, 总共能创建512*8个文件和目录
const u32 INODE_NUM_PER_BLOCK = 4;
const u32 INDEX_NO_PER_BLOCK = BLOCK_SIZE/sizeof(u32);
const u32 NAME_LENGTH_LIMIT = 27;
const u32 INODE_DIRECT_NUM = 26;
const u32 FILE_MAX_BLOCK_NUM = INODE_DIRECT_NUM+INDEX_NO_PER_BLOCK+INDEX_NO_PER_BLOCK*INDEX_NO_PER_BLOCK;
const u32 INVALID_INODE_NO = 0xffffffff;

/**
 * ceiling(x/y)
 */
static inline u32 ceiling(u32 x, u32 y) {
  return (x+y-1)/y;
}

static inline int trailing_ones( uint32_t in ) {
  return ~ in == 0? 32 : __builtin_ctz( ~ in );
}

enum class FileType {
    REG,
    DIR
};

#endif //EXT2_COMMON_H
