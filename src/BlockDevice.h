//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_BLOCKDEVICE_H
#define EXT2_BLOCKDEVICE_H
#include "common.h"
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

struct BlockDevice {
    u8* block;
    i32 fd;
    u32 file_size;
    u32 block_num;
    BlockDevice(std::string file_path);
    ~BlockDevice();
    void sync();
    u8* get_block_cache(u32 block_id);
};

#endif //EXT2_BLOCKDEVICE_H
