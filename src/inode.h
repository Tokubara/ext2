//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_INODE_H
#define EXT2_INODE_H
#include "common.h"
#include <string>

struct Ext2;

struct DirEntry {
    char name[NAME_LENGTH_LIMIT + 1];
    u32 inode_number;
};

struct Inode {
    u32 find(std::string path);
    i32 increase_size(u32 need);
    i32 read_at(u32 offset, u32 len, u8* buffer);
/**
 * 根据文件大小, 计算需要的索引块和数据块数的总和, 已测试
 * */
    static  u32 get_block_num_by_size(const u32 size);

    i32 write_at(u32 offset, u32 len, const u8* buffer);
/**
 * 初始化目录
 * @return
 */
    i32 initialize(FileType file_type);
public:
    struct DiskInode { // disk上也有的
        u32 size; // 文件的字节数大小
    } diskInode;
    Ext2* fs;

};
#endif //EXT2_INODE_H
