#dd if=/dev/zero of=~/diskfile bs=512 count=8096

export wd=$MHOME/Playground/ext2/

out() {
  cp -f $MHOME/Playground/ext2/build/fuse ~
}

fuse_run() {
  cd ~
  fusermount -u fuse_test
  rm -rf fuse_test
  mkdir fuse_test
  ./fuse -s -d fuse_test
}

fuse_rr() {
  cd ~
  fusermount -u fuse_test
  rm -rf fuse_test
  mkdir fuse_test
  ./fuse fuse_test
}

fuse_gdb() {
  cd ~
  fusermount -u fuse_test
  rm -rf fuse_test
  mkdir fuse_test
  gdb --args ./fuse -s -d fuse_test
}

fuse_fi() {
  cd ~
  fusermount -u fuse_test
}

submit() {
  mv -r ../ext2 ~/Desktop/
  cd ~/Desktop/
  rm -rf diskfile log log.md obj/ .gitignore .idea/ cmake-build-debug/ build/ gdb_history.txt gcc_error.txt tags .DS_Store
}

export -f out
export -f fuse_run
export -f fuse_fi
