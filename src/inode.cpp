//
// Created by Isaac Quebec on 2021/6/8.
//

#include "inode.h"
#include "ext2.h"
#include <cstring>
#include <cerrno>


// 此函数没有加锁, 因为它会在find和create中调用
Inode::Inode(Ext2 *ext2, DiskInode *disk_inode, const u32 inode_number) : disk_inode(disk_inode), fs(ext2) {
  if (ext2 == nullptr) { assert(disk_inode == nullptr && inode_number == INVALID_INODE_NO); }
  if (disk_inode == nullptr) { assert(ext2 == nullptr && inode_number == INVALID_INODE_NO); }
//  this->fs->wrlock();
  if (disk_inode != nullptr) this->disk_inode->inode_number = inode_number;
//  this->fs->unlock();
}

/** 此函数在文件长度可能增大的时候调用, 如果new_size<当前size, 啥也不做就返回了
 * i32是因为可能失败, 但是目前不做处理
 * 此函数不需要锁, 因为它会在write_at中调用, write_at会加锁
 * */
i32 Inode::_increase_size(u32 new_size) {
  // log_trace("enter, new_size: %u, cur_size: %u", new_size, this->disk_inode->size);
//  this->fs->wrlock();
  if (new_size <= this->disk_inode->size) {
//    this->fs->unlock();
    return 0;
  }
  // {{{2 虽然大小增大, 但是需要的块数并没有增多
  u32 need = get_block_num_by_size(new_size) - get_block_num_by_size(disk_inode->size);
  assert(need >= 0);
  u32 cur_data_block_num = ceiling(disk_inode->size, BLOCK_SIZE); // 这个变量会不断修改, 直到==need_block_num
  // 现在对disk_inode->size的变量使用完了, 可以设置它了
  disk_inode->size = new_size;
  if (need == 0) {
//    goto return_part;
//    this->fs->unlock();
    return 0;
  }


  // {{{2 分配需要的块数
  std::queue<u32> block_id;
  assert(fs != nullptr); // TODO fs
  for (u32 i = 0; i < need; i++) {
    block_id.push(fs->_alloc_data());
//    log_debug("alloc %u", block_id.back());
  }


//  u32 need_data_block_num = ceiling(new_size,BLOCK_SIZE);
//  assert(need_data_block_num>=cur_data_block_num);

  // {{{2 写直接块
  while (cur_data_block_num < INODE_DIRECT_NUM && !block_id.empty()) {
    this->disk_inode->direct[cur_data_block_num++] = block_id.front();
    block_id.pop();
  }
  if (block_id.empty()) {
//    this->fs->unlock();
    return 0;
  }
  // {{{2 准备一级块
  if (cur_data_block_num == INODE_DIRECT_NUM) { // 需要分配一级块, 否则一级块一定有了
    this->disk_inode->indirect1 = block_id.front();
    block_id.pop();
    assert(!block_id.empty()); // 有一级块肯定还有别的
  } else {
    assert(this->disk_inode->indirect1 != 0);
  }
  u32 *indirect1_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect1);
  cur_data_block_num -= INODE_DIRECT_NUM;
  while (cur_data_block_num < INDEX_NO_PER_BLOCK && !block_id.empty()) {
    indirect1_array[cur_data_block_num++] = block_id.front();
    block_id.pop();
  }
  if (block_id.empty()) {
//    this->fs->unlock();
    return 0;
  }

  // 准备二级块
  cur_data_block_num -= INDEX_NO_PER_BLOCK;
  u32 a0 = cur_data_block_num / INDEX_NO_PER_BLOCK; // a0存的是目标块号
  u32 b0 = cur_data_block_num % INDEX_NO_PER_BLOCK;
  u32 tar_data_block_num = ceiling(new_size, BLOCK_SIZE) - INODE_DIRECT_NUM - INDEX_NO_PER_BLOCK; // tmp
  u32 a1 = tar_data_block_num / INDEX_NO_PER_BLOCK;
  u32 b1 = tar_data_block_num % INDEX_NO_PER_BLOCK;

  u32 *indirect2_array = nullptr;
  indirect1_array = nullptr;
  if (cur_data_block_num == 0) {
    this->disk_inode->indirect2 = block_id.front();
    block_id.pop();
    assert(!block_id.empty()); // 有一级块肯定还有别的
    indirect2_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect2);
  } else {
    assert(this->disk_inode->indirect2 != 0);
    indirect2_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect2);
    assert(indirect2_array[a0] > 0);
    if (b0 != 0) {
      indirect1_array = (u32 *) fs->block_device->get_block_cache(indirect2_array[a0]);
    }
  }
  while (a0 < a1 || (a0 == a1 && b0 < b1)) {
    if (b0 == 0) {
      indirect2_array[a0] = block_id.front();// 是么?
      block_id.pop();
      indirect1_array = (u32 *) fs->block_device->get_block_cache(indirect2_array[a0]);
    }
    indirect1_array[b0++] = block_id.front();
    block_id.pop();
    if (b0 == INDEX_NO_PER_BLOCK) {
      a0++;
      b0 = 0;
    }
  }
//  return_part:
  assert(block_id.empty());
//  this->disk_inode->size = new_size;
//  this->fs->unlock();
  return 0;
}

i32 Inode::_write_at(const u32 offset, u32 len, const u8 *buffer) {
  _increase_size(offset + len);
  u32 in_offset = offset % BLOCK_SIZE;
  u32 logic_block_id = offset / BLOCK_SIZE;
  u32 cur_len = 0;
  while (cur_len < len) {
    u32 phy_block_id = _logic_to_phy_block_id(logic_block_id);
    u8 *file_block = this->fs->block_device->get_block_cache(phy_block_id);
    file_block += in_offset;
    memcpy(file_block, buffer + cur_len, std::min(len - cur_len, BLOCK_SIZE - in_offset));
    in_offset = 0;
    cur_len += std::min(len - cur_len, BLOCK_SIZE - in_offset);
    logic_block_id++;
  }
  return 0;
}

// create中调用
i32 Inode::_initialize_dir(u32 parent_inode_number = 0) {
  this->disk_inode->initialize(FileType::DIR);
  this->_write_dirent(".",  this->disk_inode->inode_number);
  this->_write_dirent("..",  parent_inode_number);
//  log_trace("first data block id:%u", this->disk_inode->direct[0]);
  return 0;
}

/**
 * 根据文件大小, 计算需要的索引块和数据块数的总和, 已测试
 * 不需要锁
 * */
u32 Inode::get_block_num_by_size(const u32 size) {
  u32 data_block_num = ceiling(size, BLOCK_SIZE);
  assert(data_block_num <= FILE_MAX_BLOCK_NUM);
  u32 total = data_block_num;
  if (data_block_num > INODE_DIRECT_NUM) {
    total += 1; // 一级索引块
    data_block_num -= INODE_DIRECT_NUM;
    if (data_block_num > INDEX_NO_PER_BLOCK) { // 需要二级索引块
      total += 1;
      data_block_num -= INDEX_NO_PER_BLOCK;
      total += ceiling(data_block_num, INDEX_NO_PER_BLOCK);
    }
  }
  return total;
}

// _read_at, write_at中调用
u32 Inode::_logic_to_phy_block_id(u32 logic_id) const {
  u32 phy_id = 0;
  assert(logic_id < FILE_MAX_BLOCK_NUM);
  if (logic_id < INODE_DIRECT_NUM) {
    phy_id = this->disk_inode->direct[logic_id];
  } else if (logic_id < INODE_DIRECT_NUM + INDEX_NO_PER_BLOCK) {
    assert(this->disk_inode->indirect1 > 0);
    u32 *indirect1_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect1);
    phy_id = indirect1_array[logic_id - INODE_DIRECT_NUM];
  } else {
    assert(this->disk_inode->indirect2 > 0);
    u32 tmp = logic_id - (INODE_DIRECT_NUM + INDEX_NO_PER_BLOCK);
    u32 *indirect2_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect2);
    u32 indirect1_index = indirect2_array[tmp / INDEX_NO_PER_BLOCK];
    u32 *indirect1_array = (u32 *) fs->block_device->get_block_cache(indirect2_array[indirect1_index]);
    phy_id = indirect1_array[tmp % INDEX_NO_PER_BLOCK];
  }
  return phy_id;
}

void DiskInode::initialize(FileType type) {
  assert(sizeof(DiskInode) == 128);
  this->nlinks = 1;
  if (type == FileType::DIR) this->nlinks++;
  this->size = 0;
  memset(this->direct, 0, sizeof(DiskInode) * INODE_DIRECT_NUM);
  this->indirect1 = 0;
  this->indirect2 = 0;
  this->file_type = type;
}

/**
 * 不会处理offset+len>size的情况, 调用者保证
 * */
i32 Inode::_read_at(u32 offset, u32 len, u8 *buffer) const {
  assert(offset + len <= this->disk_inode->size);
  u32 in_offset = offset % BLOCK_SIZE;
  u32 logic_block_id = offset / BLOCK_SIZE;
  u32 cur_len = 0;
  while (cur_len < len) {
    u32 phy_block_id = _logic_to_phy_block_id(logic_block_id);
    u8 *file_block = this->fs->block_device->get_block_cache(phy_block_id);
    file_block += in_offset;
    memcpy(buffer + cur_len, file_block, std::min(len - cur_len, BLOCK_SIZE - in_offset));
    in_offset = 0;
    cur_len += std::min(len - cur_len, BLOCK_SIZE - in_offset);
    logic_block_id++;
  }
  return 0;
}

// 底层
std::queue<std::string> Inode::_ls() const {
  assert(this->disk_inode->file_type == FileType::DIR);
  std::queue<std::string> entries_str;
  u32 dir_num = this->disk_inode->size / sizeof(DirEntry);
  auto *dir_entries = new DirEntry[dir_num];
  this->_read_at(0, this->disk_inode->size, (u8 *) dir_entries);
  for (u32 i = 0; i < dir_num; i++) {
    if (Inode::is_valid(dir_entries[i].inode_number)) {
      entries_str.push(dir_entries[i].name);
    }
//    printf("%s\n", dir_entries[i].name);
  }
  return entries_str;
}

// 顶层函数, 需要加锁
Inode Inode::create(const char *name, FileType type) {
  assert(strlen(name) <= NAME_LENGTH_LIMIT);
  assert(name != nullptr);
  this->fs->wrlock();
  // {{{2 判断是否有同名的
  Inode tmp_inode = this->_find(name, nullptr);
  if (tmp_inode.is_self_valid()) {
    log_error("create fail: %s has exists", name);
    this->fs->unlock();
    return Inode::invalid_inode();
  }
  // {{{2 创建
  const u32 new_inode_number = this->fs->_alloc_inode();
//  assert(this->fs->_alloc_inode()==new_inode_number+1);
  DiskInode *new_disk_inode = this->fs->_get_disk_inode_from_id(new_inode_number);
  Inode inode{this->fs, new_disk_inode, new_inode_number};
  log_trace("new inode number: %u, its parent inode number: %u", new_inode_number, this->disk_inode->inode_number);
  if (type == FileType::DIR) {
    inode._initialize_dir(this->disk_inode->inode_number);
  } else if (type == FileType::REG) {
    inode._initialize_regfile();
  } else {
    TODO();
  }
  this->_write_dirent(name, new_inode_number);
  this->fs->unlock();
  return inode;
}

void Inode::_initialize_regfile() const {
  this->disk_inode->initialize(FileType::REG);
}

// 与ls的实现差不多
/** 如果需要它在目录中的位置, entry_index指针不为空, 否则为空, 不过此字段只有在Inode有效的情况下才有意义(注意它是u32)
 * */
Inode Inode::_find(const std::string &name, u32 *entry_index) const {
  assert(this->disk_inode->file_type == FileType::DIR);
//  log_debug("name:%s",name.c_str());
  u32 dir_num = this->disk_inode->size / sizeof(DirEntry);
  auto *dir_entries = new DirEntry[dir_num];
  this->_read_at(0, this->disk_inode->size, (u8 *) dir_entries);
  for (u32 i = 0; i < dir_num; i++) {
    if (dir_entries[i].name == name && is_valid(dir_entries[i].inode_number)) {
      if (entry_index != nullptr) *entry_index = i;
      return Inode{this->fs, this->fs->_get_disk_inode_from_id(dir_entries[i].inode_number),
                   dir_entries[i].inode_number};
    }
  }
  return Inode::invalid_inode();
}

bool Inode::is_valid(u32 number) {
  return number != INVALID_INODE_NO;
}

Inode Inode::invalid_inode() {
  return Inode(nullptr, nullptr, INVALID_INODE_NO);
}

// 是顶层函数, 但本来就不需要锁
bool Inode::is_self_valid() const {
  if (this->fs != nullptr) {
    assert(this->disk_inode != nullptr && this->disk_inode->inode_number != INVALID_INODE_NO);
    return true;
  } else {
    assert(this->disk_inode == nullptr);
    return false;
  }
}

bool Inode::is_dir() const {
  return this->disk_inode->file_type == FileType::DIR;
}

/** 处理了两种错误情况, name找不到, 或者是非空目录
 * */
i32 Inode::unlink(const char *name) {
  this->fs->wrlock();
  assert(this->is_dir());
  u32 entry_index;
  Inode inode = this->_find(name, &entry_index);
  if (!inode.is_self_valid()) {
    log_error("%s not exists", name);
    this->fs->unlock();
    return -ENOENT;
  }
  if (inode.is_dir()) { // 非空目录不能删除
    auto ret = inode._ls().size();
    assert(ret >= 2);
    if (ret > 2) {
      log_error("%s is not empty", name);
      this->fs->unlock();
      return -ENOTEMPTY;
    }
  }
  // 可以删除的情况
  // {{{2 清除在父目录中的项, 置为INVALID_INODE_NO
  this->_rm_direntry(entry_index);

  inode.disk_inode->nlinks -= 1;
  // {{{2 是否clear
  if (inode.is_dir() || (inode.is_reg() && inode.disk_inode->nlinks == 0)) { // 之前已经减了
    inode._clear(); // 回收这些数据块
    this->fs->_dealloc_inode(inode.disk_inode->inode_number); // 回收inode号
  }
  this->fs->unlock();
  return 0;
}

bool Inode::is_reg() const {
  return this->disk_inode->file_type == FileType::REG;
}

std::queue<u32> Inode::_get_data_block_id_in_use() const {
  // 清除此inode的数据块
  std::queue<u32> clear_inodes_id;
  u32 cur_block_num = this->get_block_num_by_size(this->disk_inode->size); // 会不断变化
  u32 old_block_num = cur_block_num;
  u32 data_block_num = ceiling(this->disk_inode->size, BLOCK_SIZE);
  u32 tmp = std::min(cur_block_num, INODE_DIRECT_NUM); // 一开始是存min, 但也可能改成别的
  for (u32 i = 0; i < tmp; i++) {
    clear_inodes_id.push(this->disk_inode->direct[i]);
  }
  cur_block_num -= tmp;
  if (cur_block_num > 0) {
    // 处理一级块部分
    u32 *indirect1_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect1);
    clear_inodes_id.push(this->disk_inode->indirect1); // 回收一级块
    cur_block_num--;
    tmp = std::min(INDEX_NO_PER_BLOCK, cur_block_num);
    assert(cur_block_num > 0);
    for (u32 i = 0; i < tmp; i++) {
      clear_inodes_id.push(indirect1_array[i]);
    }
    cur_block_num -= tmp;
    if (cur_block_num > 0) { // 用到了二级块
      clear_inodes_id.push(this->disk_inode->indirect2); // 回收二级块
      cur_block_num--; // 二级块
      u32 *indirect2_array = (u32 *) fs->block_device->get_block_cache(this->disk_inode->indirect2);
      data_block_num -= (INDEX_NO_PER_BLOCK + INODE_DIRECT_NUM);
      u32 indirect1_num = ceiling(data_block_num, INDEX_NO_PER_BLOCK);
      for (u32 i = 0; i < indirect1_num; i++) {
        clear_inodes_id.push(indirect2_array[i]); // 回收一级块
        cur_block_num--;
        u32 *cur_indirect1 = (u32 *) fs->block_device->get_block_cache(indirect2_array[i]);
        tmp = (i == indirect1_num - 1) ? data_block_num % INDEX_NO_PER_BLOCK : INDEX_NO_PER_BLOCK;
        for (u32 j = 0; j < tmp; j++) {
          clear_inodes_id.push(cur_indirect1[j]); // 回收数据块
        }
        cur_block_num -= tmp;
      }
    }
  }
  assert(cur_block_num == 0);
  assert(old_block_num == clear_inodes_id.size());
  return clear_inodes_id;
}

/**
 * 没有清空要回收的数据块, 也没有把indirect这些置为0, 不过改了size, 这只是与increase_size, 感觉不改也行
 * 此函数应该保序, 越是后面的块, 越会被回收(truncate性质如此), 因此, 比如如果是一级索引块, 那么应该push一级索引块, 再比如而级索引块, 应该先push二级索引块, 再push一级索引块, 然后才是数据块
 * */
void Inode::_clear() const {
  std::queue<u32> clear_inodes_id = this->_get_data_block_id_in_use();
  this->disk_inode->size = 0;
  while (!clear_inodes_id.empty()) {
    this->fs->_dealloc_data(clear_inodes_id.front());
    clear_inodes_id.pop();
  }
}

/** 文件的新大小是new_size, 需要维护size
 * 顶层
 * */
void Inode::truncate(const u64 new_size) {
  this->fs->wrlock();
  assert(this->is_reg());
  if(new_size==this->disk_inode->size) {
    this->fs->unlock();
    return;
  }
  // {{{2 如果文件大小增大
  if(new_size>this->disk_inode->size) {
    u32 len = new_size-this->disk_inode->size; // 要写这么多字节
    u8* buf = (u8*)malloc(len);
    memset(buf,0,len);
    this->_write_at(this->disk_inode->size, len, buf);
    free(buf);
    this->disk_inode->size = new_size;
    this->fs->unlock();
    return;
  }
  // {{{2 如果文件大小变小
  u32 new_block_num = this->get_block_num_by_size(new_size);
  std::queue<u32> block_in_use = this->_get_data_block_id_in_use();
  for(u32 i = 0; i< new_block_num; i++) {
    block_in_use.pop();
  }
  // 剩下的就是要被清除的了
  while(!block_in_use.empty()) {
    this->fs->_dealloc_data(block_in_use.front());
    block_in_use.pop();
  }
  this->disk_inode->size = new_size;
  this->fs->unlock();
}

/**
 * 在当前目录下(this是一个目录), 创建一个目录项,
 * */
i32 Inode::link(const char* name, Inode* inode) {
  this->fs->wrlock();
  assert(this->is_dir());
  assert(inode->is_self_valid());
  assert(this->fs->inode_bitmap->test_exist(inode->disk_inode->inode_number)); // 感觉有点多余
  assert(inode->is_reg());
  // {{{2 判断是否有同名的
  Inode tmp_inode = this->_find(name, nullptr);
  if (tmp_inode.is_self_valid()) {
    log_error("link fail: %s has exists", name);
    this->fs->unlock();
    return -EEXIST;
  }
  // {{{2 设置nlinks
  inode->disk_inode->nlinks++;
  // {{{2 写目录项
  this->_write_dirent(name, inode->disk_inode->inode_number);
  this->fs->unlock();
  return 0;
}

void Inode::_write_dirent(const char *name, u32 inode_number, u32 index) {
  assert(sizeof(DirEntry) == 32);
  assert(this->is_dir());
  DirEntry dir_entry;
  strcpy(dir_entry.name, name);
  dir_entry.inode_number = inode_number;
  u32 offset = index<U32_MAX?index*sizeof(DirEntry):this->disk_inode->size;
  this->_write_at(offset, sizeof(DirEntry), (u8 *) &dir_entry);
}

void Inode::_rm_direntry(u32 dirent_index) {
  this->_write_dirent("",INVALID_INODE_NO,dirent_index);
}

i32 Inode::read_at(u32 offset, u32 len, u8 *buffer) const {
  this->fs->rdlock();
  i32 ret = this->_read_at(offset, len, buffer);
  this->fs->unlock();
  return ret;
}

i32 Inode::write_at(const u32 offset, u32 len, const u8 *buffer) {
  this->fs->wrlock();
  i32 ret = this->_write_at(offset, len, buffer);
  this->fs->unlock();
  return ret;
}

std::queue<std::string> Inode::ls() const {
  this->fs->rdlock();
  auto ret = this->_ls();
  this->fs->unlock();
  return ret;
}
