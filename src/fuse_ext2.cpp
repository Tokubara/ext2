//
// Created by Isaac Quebec on 2021/6/9.
//

#include "fuse_ext2.h"
#include "ext2.h"
#include "inode.h"
#include "common.h"
#include <cstring>
#include <sys/stat.h>

Ext2* ext2;

int ext2_get_attr(const char *path, struct stat *statbuf) {
memset(statbuf, 0, sizeof(struct stat));

u32 len = strlen(path);
char path_copy[len + 1];
strncpy(path_copy, path, len);
path_copy[len] = 0;

//i32 ret;

Inode inode = ext2->find_inode_by_full_path(path_copy);
if (!inode.is_self_valid())
{
log_error("entry_inode not found for %s\n", path);
return -ENOENT;
}

//statbuf->st_mode = dir_inode->i_mode; // ? 没有访问模式
//  statbuf->st_mode = inode.disk_inode->file_type==FileType::REG?():(S_IFDIR | 0777);
if(inode.disk_inode->file_type==FileType::REG) {
  statbuf->st_mode = S_IFREG | 0777;
} else if (inode.disk_inode->file_type==FileType::DIR) {
  statbuf->st_mode = S_IFDIR | 0777;
}
statbuf->st_nlink = inode.disk_inode->nlinks;
statbuf->st_size = inode.disk_inode->size;

//statbuf->st_uid = dir_inode->i_uid;
//statbuf->st_gid = dir_inode->i_gid;

//statbuf->st_blocks = dir_inode->i_blocks;
return 0;
}

int nxfs_mkdir(const char *path, mode_t mode)
{
  (void)mode;
  // 先找到父目录
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  u64 pos = strrchr(path,'/')-path;
  path_copy[pos]='\0';
  Inode inode = ext2->find_inode_by_full_path(path_copy);
  if(!inode.is_self_valid() || !inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }
  return 0;
}

