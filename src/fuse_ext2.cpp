//
// Created by Isaac Quebec on 2021/6/9.
//

#include "fuse_ext2.h"
#include "ext2.h"
#include "inode.h"
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cerrno>

Ext2* ext2;

int ext2_get_attr(const char *path, struct stat *statbuf) {
  log_enter;
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
  log_trace("path: %s", path);
  (void)mode;
  // 先找到父目录
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  auto basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[basename-path]='\0';
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
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
  log_trace("path: %s", path);
  (void) offset;
  (void) fi;

  Inode inode = ext2->find_inode_by_full_path(path);
  if (!inode.is_self_valid()) {
    log_error("entry_inode not found for %s\n", path);
    return -ENOENT;
  } else if(!inode.is_dir()) {
    return -ENOTDIR;
  }
    std::queue<std::string> entries_str = inode.ls();

   while(!entries_str.empty()) {
     filler(buf, entries_str.front().c_str(), NULL, 0); // 目录已经包含了.和..
     entries_str.pop();
   }

  return 0;
}

int nxfs_rmdir(const char *path)
{
  // 除最后一行以外, 都与mkdir相同
  log_trace("path: %s", path);
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  auto basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[basename-path]='\0';
  basename++; // 之前指向的是/, 现在指向下一个
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }
  return parent_inode.unlink(basename);
}

/**
 * 无权限检查
 * */
static int turbo_open(const char *path, struct fuse_file_info *fi)
{
  log_trace("path: %s", path);
  Inode inode = ext2->find_inode_by_full_path(path);
  if(inode.is_self_valid()) {
    return -ENOENT;
  } else if(!inode.is_reg()) { // 不知道这里对symlink要不要单独处理
    return -EISDIR; // TODO 根据man 2 open, 目录也不一定会出错, 只有pathname refers to a directory and the access requested  involved  writing  (that  is, O_WRONLY or O_RDWR is set).
  }
  // 存入fi
  auto* fh = new FileHandle{.size=inode.disk_inode->size, .inode_number=inode.disk_inode->inode_number};
  fi->fh = (u64)fh;
  return 0;
}

static int turbo_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi) {
  FileHandle* fh = (FileHandle*)fi->fh;
  log_trace("inode_number: %u", fh->inode_number);
  assert(fh->inode_number>0);
  Inode inode = ext2->get_inode_from_id(fh->inode_number);
  if(offset>=fh->size) {
    return 0;
  }
  u32 read_len = std::min((u64)(fh->size-offset),size); //? 不知道这里为啥报错
  inode.read_at(offset,read_len,(u8*)buf);
  return read_len;
}

static int turbo_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi) {
  FileHandle* fh = (FileHandle*)fi->fh;
  log_trace("inode_number: %u", fh->inode_number);
  assert(fh->inode_number>0);
  Inode inode = ext2->get_inode_from_id(fh->inode_number);
  if(size==0) return 0; // 虽然按理说这样write_at也不会出错
  inode.write_at(offset,size,(u8*)buf);
  return size;
}

static int turbo_create(const char *path, mode_t mode,struct fuse_file_info *fi) {
  log_trace("path: %s", path);
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  auto basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[basename-path]='\0';
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
//  assert(parent_inode.is_self_valid()); // valid
  if(!parent_inode.is_self_valid()) {
    return -ENOENT;
  }
  basename++; // 之前指向的是/, 现在指向下一个
  Inode new_inode =  parent_inode.create(basename, FileType::REG); // 创建的一定是文件, 不是目录, 因为目录应该是mkdir
  if(!new_inode.is_self_valid()) {
    return -EEXIST;
  }
  // 写fh
  assert(new_inode.disk_inode->size==0);
  auto* fh = new FileHandle{.size=new_inode.disk_inode->size, .inode_number=new_inode.disk_inode->inode_number};
  fi->fh = (u64)fh;
  return 0;
}

int nxfs_unlink(const char *path) { // 这里的实现是把rmdir的实现拷贝过来的
  log_trace("path: %s", path);
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  auto basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[basename-path]='\0';
  basename++; // 之前指向的是/, 现在指向下一个
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }
  return parent_inode.unlink(basename);
}

//int nxfs_statfs(const char *path, struct statvfs *statInfo) {
//


//需要处理的情况有:
// |old_path不存在
//        |old_path是目录
//|new_path的父目录不存在
//        |new_path父目录不是目录
//new_path的base部分已存在(在ext2中的link处理的)
int nxfs_link (const char *old_path, const char *new_path) {
  // {{{2 处理old_path可能的问题
  Inode inode = ext2->find_inode_by_full_path(old_path);
  if(!inode.is_self_valid()) {
    return -ENOENT;
  }
  if(inode.is_dir()) { // 是目录
    return -EPERM;
  }
  char* path_copy = (char*)malloc(sizeof(new_path)+1);
  strcpy(path_copy,new_path);
  auto basename = strrchr(new_path,'/'); // 现在指向的还有/
  path_copy[basename-new_path]='\0';
  basename++; // 之前指向的是/, 现在指向下一个
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
  if(!parent_inode.is_self_valid()) {
    return -ENOENT;
  } else if(!parent_inode.is_dir()) {
    return -ENOTDIR;
  }
  return parent_inode.link(basename, &inode);
}

// rename
int nxfs_rename(const char *path, const char *newpath) {

}

// truncate

