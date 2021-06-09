//
// Created by Isaac Quebec on 2021/6/9.
//

#include "fuse_ext2.h"
#include "ext2.h"
#include "inode.h"
#include "common.h"
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cerrno>

Ext2* ext2;

int ext2_get_attr(const char *path, struct stat *statbuf) {
memset(statbuf, 0, sizeof(struct stat));

Inode inode = ext2->find_inode_by_full_path(path);
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
  auto basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[basename-path]='\0';
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }

  basename++; // 之前指向的是/, 现在指向下一个
  Inode new_inode = parent_inode.create(basename, FileType::DIR);
  if(!new_inode.is_self_valid()) {
    return -EEXIST;
  }

  return 0;
}

static int turbo_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

  (void) offset;
  (void) fi;

  Inode inode = ext2->find_inode_by_full_path(path);
  if (!inode.is_self_valid()) {
    log_error("entry_inode not found for %s\n", path);
    return -ENOENT;
  } else if(!inode.is_dir()) {
    return -ENOTDIR;
  }

    dirent *temp;

    std::queue<std::string> entries_str = inode.ls();

   while(!entries_str.empty()) {
     filler(buf, entries_str.front().c_str(), NULL, 0); // 目录已经包含了.和..
     entries_str.pop();
   }

  return 0;
}

int nxfs_rmdir(const char *path)
{
  printf("rmdir %s\n", path);
  char path_copy[strlen(path)+1];
  strncpy(path_copy, path, strlen(path));
  path_copy[strlen(path)] = 0;
  uint32 parent_inode_number = lookup_entry_inode(path_copy,ROOT_INO);

  struct s_inode* parent_inode = read_inode(parent_inode_number);
  struct s_dir_entry2* last_entry = find_last_entry(*parent_inode);
  if(strcmp(last_entry->name, "..") == 0){
    nxfs_unlink(path);
    return 0;
  }else
    return -EPERM;
}
