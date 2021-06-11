//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_EXT2_H
#define EXT2_EXT2_H

#include <queue>
#include "common.h"
#include "BlockDevice.h"
#include "bitmap.h"
#include "inode.h"
#include <pthread.h>

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
    i32 create(BlockDevice* block_device, u32 total_blocks, u32 inode_bitmap_blocks = INODE_BITMAP_BLOCKS);
    DiskInode* _get_disk_inode_from_id(const u32 inode_id) const;
    Inode get_inode_from_id(u32 inode_id);
    int rename(const char *oldpath, const char *newpath);
    void open(BlockDevice*);
    u32 _alloc_data();
    u32 _alloc_inode();
    u32 _dealloc_data(u32 data_block_id);
    u32 _dealloc_inode(u32 inode_number);
    Inode _find_inode_by_full_path(const char *path);
    Inode find_inode_by_full_path(const char *path);
    static std::queue<std::string> split_path(const char *path);
    Inode _get_parent_inode_and_basename(const char* path, const char** basename);
    Inode get_parent_inode_and_basename(const char* path, const char** basename);
    void rdlock();
    void wrlock();
    void unlock();

    Inode* root; // 好像不能有
    DiskInode* disk_inode_start;
    BlockDevice* block_device; // 这里存引用真的靠谱么
    Bitmap* inode_bitmap;
    Bitmap* data_bitmap;
    u32 inode_area_start_block;
    u32 data_area_start_block;
    pthread_rwlock_t lock;
};

#endif //EXT2_EXT2_H
