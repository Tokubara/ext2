//
// Created by Isaac Quebec on 2021/6/9.
//

#include "fuse_ext2.h"
#include "common.h"
#include <cstring>

int nxfs_get_attr(const char *path, struct stat *statbuf) {
memset(statbuf, 0, sizeof(struct stat));

u32 len = strlen(path);
char path_copy[len + 1];
strncpy(path_copy, path, len);
path_copy[len] = 0;

u32 entry_inode = lookup_entry_inode(path_copy, ROOT_INO);
if (entry_inode == 0)
{
printf("entry_inode not found for %s\n", path);
return -ENOENT;
}

struct s_inode *dir_inode = read_inode(entry_inode);

statbuf->st_mode = dir_inode->i_mode;
statbuf->st_nlink = dir_inode->i_links_count;
statbuf->st_size = dir_inode->i_size;

statbuf->st_uid = dir_inode->i_uid;
statbuf->st_gid = dir_inode->i_gid;

statbuf->st_blocks = dir_inode->i_blocks;

if (print_info)
printf("success\n");
free(dir_inode);
return 0;
}
