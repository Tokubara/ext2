//
// Created by Isaac Quebec on 2021/6/8.
//

#ifndef EXT2_INODE_H
#define EXT2_INODE_H
#include "common.h"
#include <string>
#include <queue>

struct Ext2;

struct DiskInode { // disk上也有的
    u32 size; // 文件的字节数大小
    u32 indirect1;
    u32 indirect2;
    u32 nlinks;
    u32 inode_number;
    FileType file_type;
    u32 direct[INODE_DIRECT_NUM]; // 其它有5个字段
//        u32 inode_number;
    void initialize(FileType type);

};

struct DirEntry {
    char name[NAME_LENGTH_LIMIT + 1];
    u32 inode_number;
};

struct Inode {
    Inode(const Ext2* ext2, DiskInode* disk_inode, const u32 inode_number);
    Inode find(const std::string& name, u32* entry_index) const;
    i32 increase_size(u32 need);
    std::queue<std::string> ls() const;
    i32 read_at(u32 offset, u32 len, u8* buffer) const;
/**
 * 根据文件大小, 计算需要的索引块和数据块数的总和, 已测试
 * */
    static  u32 get_block_num_by_size(const u32 size);

    i32 write_at(u32 offset, u32 len, const u8* buffer);
/**
 * 初始化目录
 * @return
 */
    i32 initialize_dir(u32 parent_inode_number);

    void rm_direntry(u32 dirent_index);

public:
    DiskInode* disk_inode;
    const Ext2* fs;

    u32 logic_to_phy_block_id(u32 logic_id) const;

    Inode create(const char *string, FileType type);
    i32 unlink(const char *string);
    i32 link(const char* name, Inode* inode);
    static Inode invalid_inode();

    void initialize_regfile() const;
    bool is_self_valid() const;
    bool is_dir() const;

    static bool is_valid(u32 number) ;

    bool is_reg() const;

    void clear();

//private:
    void _write_dirent(const char *name, u32 inode_number, u32 index=U32_MAX);
};
#endif //EXT2_INODE_H
