//
// Created by Isaac Quebec on 2021/6/9.
//

#ifndef EXT2_FUSE_EXT2_H
#define EXT2_FUSE_EXT2_H

#include "fuse.h"
#include "common.h"

struct FileHandle {
    u32 size;
    u32 inode_number;
};


#endif //EXT2_FUSE_EXT2_H
