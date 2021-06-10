#define FUSE_USE_VERSION 32
//
// Created by Isaac Quebec on 2021/6/9.
//

#ifndef EXT2_FUSE_EXT2_H
#define EXT2_FUSE_EXT2_H

#include "fuse.h"
#include "common.h"
#include "ext2.h"

struct FileHandle {
    u32 size;
    u32 inode_number;
};
extern Ext2 *ext2;
extern BlockDevice* block_device;

int ext2_truncate(const char *path,off_t new_size,struct fuse_file_info *fi);
int ext2_rename(const char *oldpath,const char *newpath, unsigned int flags);
int ext2_link(const char *old_path,const char *new_path);
int ext2_unlink(const char *path);
int ext2_create(const char *path,mode_t mode,struct fuse_file_info *fi);
int ext2_write(const char *path,const char *buf,size_t size,off_t offset,struct fuse_file_info *fi);
int ext2_read(const char *path,char *buf,size_t size,off_t offset,struct fuse_file_info *fi);
int ext2_rmdir(const char *path);
int ext2_readdir(const char *path,void *buf,fuse_fill_dir_t filler,off_t offset,struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int ext2_mkdir(const char *path,mode_t mode);
int ext2_getattr(const char *path,struct stat *statbuf, fuse_file_info* fi);
int ext2_open(const char *path, struct fuse_file_info *fi);

#endif //EXT2_FUSE_EXT2_H
