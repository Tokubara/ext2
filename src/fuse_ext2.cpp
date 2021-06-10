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
#include <queue>

extern Ext2 *ext2;
extern BlockDevice* block_device;

static Inode get_parent_inode_and_basename(const char* path, const char** basename) {
  char* path_copy = (char*)malloc(sizeof(path)+1);
  strcpy(path_copy,path);
  *basename = strrchr(path,'/'); // 现在指向的还有/
  path_copy[*basename-path]='\0';
  (*basename)+=1; // 之前指向的是/, 现在指向下一个
  // bug: 未处理父目录本来就是根目录的情况, 比如/a, 那么这样的话, 之前的实现中, path_copy会为空串, 这种情况下, 将它修改为'/'
  if(strlen(path_copy)==0) {
    path_copy[0]='/';
    path_copy[1]='\0';
  }
  Inode parent_inode = ext2->find_inode_by_full_path(path_copy);
  free(path_copy);
  return parent_inode;
}

int ext2_getattr(const char *path, struct stat *statbuf, fuse_file_info* fi) {
  log_trace("path:%s", path);
  (void)fi;
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

int ext2_mkdir(const char *path, mode_t mode)
{
  log_trace("path: %s", path);
  (void)mode;
  const char* basename;
  Inode parent_inode = get_parent_inode_and_basename(path, &basename);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }

  Inode new_inode = parent_inode.create(basename, FileType::DIR);
  if(!new_inode.is_self_valid()) {
    return -EEXIST;
  }

  // DEBUG
  log_debug("ls");
  auto parent_ls = parent_inode.ls();
  while(!parent_ls.empty()) {
    printf("%s\n", parent_ls.front().c_str());
    parent_ls.pop();
  }

  return 0;
}

int ext2_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
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
     filler(buf, entries_str.front().c_str(), NULL, 0, (fuse_fill_dir_flags)0); // 目录已经包含了.和..
     entries_str.pop();
   }

  return 0;
}

int ext2_rmdir(const char *path)
{
  // 除最后一行以外, 都与mkdir相同
  log_trace("path: %s", path);
  const char* basename;
  Inode parent_inode = get_parent_inode_and_basename(path, &basename);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }
  return parent_inode.unlink(basename);
}

/**
 * 无权限检查
 * */
int ext2_open(const char *path, struct fuse_file_info *fi)
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

int ext2_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi) {
  log_trace("read:%s", path);
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

int ext2_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi) {
  log_trace("read:%s", path);
  FileHandle* fh = (FileHandle*)fi->fh;
  log_trace("inode_number: %u", fh->inode_number);
  assert(fh->inode_number>0);
  Inode inode = ext2->get_inode_from_id(fh->inode_number);
  if(size==0) return 0; // 虽然按理说这样write_at也不会出错
  inode.write_at(offset,size,(u8*)buf);
  return size;
}

int ext2_create(const char *path, mode_t mode,struct fuse_file_info *fi) {
  log_trace("path: %s", path);
  const char* basename;
  Inode parent_inode = get_parent_inode_and_basename(path, &basename);
//  assert(parent_inode.is_self_valid()); // valid
  if(!parent_inode.is_self_valid()) {
    return -ENOENT;
  }
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

int ext2_unlink(const char *path) { // 这里的实现是把rmdir的实现拷贝过来的
  log_trace("path: %s", path);
  const char* basename;
  Inode parent_inode = get_parent_inode_and_basename(path, &basename);
  if(!parent_inode.is_self_valid() || !parent_inode.is_dir()) {
    log_error("cannot find parent directory");
    return -ENOENT;
  }
  return parent_inode.unlink(basename);
}

//int ext2_statfs(const char *path, struct statvfs *statInfo) {
//


//需要处理的情况有:
// |old_path不存在
//        |old_path是目录
//|new_path的父目录不存在
//        |new_path父目录不是目录
//new_path的base部分已存在(在ext2中的link处理的)
int ext2_link (const char *old_path, const char *new_path) {
  log_trace("old_path:%s, new_path:%s", old_path, new_path);
  // {{{2 处理old_path可能的问题
  Inode inode = ext2->find_inode_by_full_path(old_path);
  if(!inode.is_self_valid()) {
    return -ENOENT;
  }
  if(inode.is_dir()) { // 是目录
    return -EPERM;
  }
  const char* basename;
  Inode parent_inode = get_parent_inode_and_basename(new_path, &basename);
  if(!parent_inode.is_self_valid()) {
    return -ENOENT;
  } else if(!parent_inode.is_dir()) {
    return -ENOTDIR;
  }
  return parent_inode.link(basename, &inode);
}

// rename
int ext2_rename(const char *oldpath, const char *newpath, unsigned int flags) {
  log_trace("old_path:%s, new_path:%s", oldpath, newpath);
  const char* basename;
  Inode old_parent_inode = get_parent_inode_and_basename(oldpath, &basename);
  if(!old_parent_inode.is_self_valid()) {
    return -ENOENT;
  } else if(!old_parent_inode.is_dir()) {
    return -ENOTDIR;
  }
  u32 dirent_index;
  Inode inode = old_parent_inode.find(basename, &dirent_index);
  if(!inode.is_self_valid()) {
    return -ENOENT;
  }
  // 去除这个目录项
  old_parent_inode.rm_direntry(dirent_index);
  Inode new_parent_inode = get_parent_inode_and_basename(newpath, &basename);
  if(!new_parent_inode.is_self_valid()) {
    return -ENOENT;
  } else if(!new_parent_inode.is_dir()) {
    return -ENOTDIR;
  }
  if(new_parent_inode.find(basename, nullptr).is_self_valid()) {
    return -EEXIST;
  }
  new_parent_inode._write_dirent(basename,inode.disk_inode->inode_number);
  return 0;
}

// truncate
int ext2_truncate (const char * path, off_t new_size, struct fuse_file_info *fi) {
  log_trace("path:%s, new_size:%u", path, new_size);
  Inode inode = ext2->find_inode_by_full_path(path);
  if (!inode.is_self_valid())
  {
    log_error("entry_inode not found for %s\n", path);
    return -ENOENT;
  }
  if (!inode.is_reg()) {
    log_error("%s is not a dir", path);
    return -EISDIR;
  }
  inode.truncate(new_size);
  return 0;
}

