#define FUSE_USE_VERSION 32
#include <fuse.h>
#include "mylib.h"
#include "inode.h"
#include "ext2.h"
#include "fuse_ext2.h"

Ext2 *ext2;
BlockDevice* block_device;

static struct fuse_operations file_operations = {
        .getattr    = ext2_getattr,
        .mkdir      = ext2_mkdir,
        .unlink     = ext2_unlink,
        .rmdir      = ext2_rmdir,
        .rename     = ext2_rename,
        .link       = ext2_link,
        .truncate   = ext2_truncate,
        .open       = ext2_open,
        .read       = ext2_read,
        .write      = ext2_write,
        .fsync      = ext2_fsync,
        .readdir    = ext2_readdir,
        .create     = ext2_create,
};

void init();

int main(int argc, char *argv[]) {
//        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  init();
  return fuse_main(argc, argv, &file_operations, NULL);
}

void init() {
  block_device = new BlockDevice{"diskfile"};
  ext2 = new Ext2;
  ext2->create(block_device, block_device->block_num);
}

