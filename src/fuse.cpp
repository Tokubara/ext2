#include <fuse.h>
#include "mylib.h"
#include "inode.h"
#include "ext2.h"
#include "fuse_ext2.h"

static struct fuse_operations file_operations = {
        .getattr    = ext2_getattr,
        .readdir    = ext2_readdir,
        .mkdir      = ext2_mkdir,
        .rmdir      = ext2_rmdir,
        .open       = ext2_open,
        .create     = ext2_create,
        .read       = ext2_read,
        .write      = ext2_write,
        .unlink     = ext2_unlink,
        .truncate = ext2_truncate,
        .rename = ext2_rename,
        .link = ext2_link,
};

void init();

int main(int argc, char *argv[]) {
  BlockDevice block_device{"diskfile"};
//        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  init();
  return fuse_main(argc, argv, &file_operations, NULL);
}

void init() {
  block_device = new BlockDevice{"diskfile"};
  ext2 = new Ext2;
  ext2->create(block_device, block_device->block_num);
}

