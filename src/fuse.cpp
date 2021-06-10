#include <fuse.h>
#include "mylib.h"
#include "inode.h"
#include "ext2.h"

static struct fuse_operations file_operations = {
        .getattr    = ext2_getattr,
        .readdir    = ext2_readdir,
        .mkdir      = ext2_mkdir,
        .rmdir      = ext2_rmdir,
        .open       = ext2_open,
        .create     = ext2_create,
        .read       = ext2_read,
        .write      = ext2_write,
        .unlink     = ext2_rm,
};

int main(int argc, char *argv[])
{
        BlockDevice block_device{"diskfile"};
//        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
        Ext2 ext2;
        ext2.create(&block_device, block_device.block_num);
        return fuse_main(argc, argv, &file_operations, NULL);
}

//? 
static int ext2_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi)
{


        int res = 0;

        char *path2;
        path2 = (char *)malloc(sizeof(path));
        strcpy(path2, path);

        //buffer to get the attributes
        memset(stbuf, 0, sizeof(struct stat));

        int ino;
        //printf(GRN BOLD "getattr calling path_to_inode\n" RESET);
        path_to_inode(path, &ino); //find inode using the path



        //printf("before isDir path - %s\n", path2);
        int directory_flag = isDir(path2); //chek if it is a directory

        if (directory_flag == 1)
        {
                //printf("getattr - Directory\n");
                stbuf->st_mode = S_IFDIR | 0777;
                stbuf->st_nlink = 2; // 目录的nlink必然是2
        }

        else if (directory_flag == 0)
        {
                inode *temp_ino = inodes + (ino * sizeof(inode));
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1; // 很显然没有支持hard link
                stbuf->st_size = temp_ino -> size;
        }

        else
        {
                res = -ENOENT;
        }
        return res;
}

static int ext2_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{

        (void) offset;
        (void) fi;


        int ino; //inode index in the array
        //printf(GRN BOLD "readdir calling path_to_inode\n" RESET);
        path_to_inode(path, &ino);// read the path to find the inode

        //printf("ino returned for %s - %d\n", path, ino);

        //if inode not found
        if (ino == -1)
        {
                return -ENOENT;
        }

        else // 这才是真正要处理的情况
        {
                dirent *temp;
                filler(buf, ".", NULL, 0); // 一定要填2项
                filler(buf, "..", NULL, 0);
                //printf("filled some stuff\n");

                //if directory is root
                if(strcmp(path, "/") == 0)
                {
                        temp = root_directory;
                }

                        //get the dirent using inode found at inode number
                else
                {
                        inode *temp_ino = inodes + (ino * sizeof(inode));
                        temp = (dirent *)(datablks + ((temp_ino -> data) * block_size));

                }

                //read all files/entries in the DIR in a loop
                while((strcmp(temp -> filename, "") != 0))
                {
                        //printf("I'm here\n");
                        filler(buf, temp -> filename, NULL, 0);
                        temp++;
                        //temp = (dirent *)temp
                }
        }

        return 0;
}


static int ext2_mkdir(const char *path, mode_t mode)
{
        //ALLOCATE AND SET new directory

        //find free inode -> set inode parameters at index
        int ino = return_first_unused_inode(inode_map);
        allocate_inode(path, &ino, true);

        //access the inode
        inode *temp_ino = inodes + (ino * sizeof(inodes));
        print_inode(temp_ino);
        //printf("inode address for %s - %u\n", path, ino);


        // go through the path
        char *token = strtok(path, "/");
        dirent *temp;
        temp = root_directory;

        while (token != NULL)
        {
                //printf("token = %s\n", token);
                //printf("temp -> filename = %s\n", temp->filename);

                while((strcmp(temp -> filename, "") != 0) && (strcmp((temp -> filename), token) != 0))
                {
                        temp++;
                }

                if((strcmp(temp -> filename, "") == 0))
                {
                        // Add the new directory to temp
                        //printf("HERE\n");
                        temp = (dirent *)temp;
                        //printf("temp = %u\n temp -> filename = %u\n", temp, temp -> filename);
                        //(temp -> filename) = (char *)malloc(15);
                        strcpy((temp -> filename), token);
                        temp -> file_inode = ino;

                        printf("\n\ndirectory created on disk as well\n\n");
                        lseek(ext2_file, 0, SEEK_SET);
                        write(ext2_file, fs, FS_SIZE);
                        return 0;
                }
                else
                {
                        temp_ino = (inodes + ((temp -> file_inode) * sizeof(inode)));
                        if(temp_ino -> directory)
                        {
                                temp = (dirent *)(datablks + ((temp_ino -> data) * block_size));
                        }
                }
                token = strtok(NULL, "/");
        }


}

//remove a directory only if the directory is empty
static int ext2_rmdir(const char *path)
{

        int i;
        //start from back and get the last directory
        i = strlen(path);
        while(path[i] != '/')
        {
                i--;
        }


        char *subpath;
        subpath = (char *)malloc(strlen(path)+ 1);

        //basically not including the non directory part
        strncpy(subpath, path, i);
        subpath[i] = '/';
        subpath[i+1] = '\0';


        int ino;
        dirent *temp;
        dirent *temp_data;
        inode *temp_ino;

        //if root 
        if(strcmp(subpath, "/") == 0)
        {
                temp = root_directory;
        }

                //get inode number using the original path given -> and thus the dirent
        else
        {
                printf(GRN BOLD "rmdir calling path_to_inode\n" RESET);
                path_to_inode(subpath, &ino);
                //printf("Inode for the path - %s - %d\n", path, ino);

                temp_ino = inodes + (ino * sizeof(inode)); //get inode at offset

                temp = datablks + ((temp_ino->data) * block_size); // get dirent using the inode
        }


        //do similar for the subpath = to check if the directory is empty or not

        //printf("ino -> data = %u\n", temp);
        //printf("root_directory = %u\n", root_directory);

        while(((strcmp(temp -> filename, "") != 0) || (temp -> file_inode) != 0) && strcmp(path + (i+1), temp -> filename) != 0)
        {
                temp ++;
        }

        if((strcmp(temp -> filename, "") != 0))
        {
                //printf("I'm here!!");
                //printf("temp -> file_inode = %d\n", temp -> file_inode);

                if(temp -> file_inode != 0)
                {
                        temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
                        temp_data = ((dirent *)(datablks + ((temp_ino -> data) * block_size)));

                        //directory has stuff
                        if(strcmp((temp_data -> filename), "") != 0)
                        {

#ifdef DEBUG
                                printf("Directory isnt empty .. so cannot rmdir!!\n");
#endif

                                return -1;
                        }


                        //if it is empty -> free the bitmaps(inode and the databitmap)

                        strcpy(temp ->filename, "");
                        data_map[(temp_ino -> data)] = 1;
                        inode_map[(temp -> file_inode)] = 0;

                }
        }


        lseek(ext2_file, 0, SEEK_SET);
        write(ext2_file, fs, FS_SIZE);

        return 0;
}


// create new file -> for touch
static int ext2_create(const char *path, mode_t mode,struct fuse_file_info *fi)
{

        //find free inode and set the params for inode
        int ino = return_first_unused_inode(inode_map);
        allocate_inode(path, &ino, false);


        //start at the root
        char *token = strtok(path, "/");
        dirent *temp;
        temp = root_directory;



        char *file = (char *)malloc(15);
        while (token != NULL)
        {

                //loop till the end of the path
                while((strcmp(temp -> filename, "") != 0) && (strcmp(temp -> filename, token) != 0))
                {
                        temp++;
                }

                //if it is end of path , attach the inode and set dirent
                if((strcmp(temp -> filename, "") != 0))
                {
                        inode *temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
                        if(temp_ino -> directory)
                        {
                                temp = datablks + ((temp_ino -> data) * block_size);
                        }
                        else
                        {
                                return -1;
                        }
                }

                strcpy(file, token);
                token = strtok(NULL, "/");
        }



        strcpy(temp -> filename, file);
        temp -> file_inode = ino;
        printf("\n\ncreating on disk!!\n\n");
        lseek(ext2_file, 0, SEEK_SET);
        printf("%d\n", write(ext2_file, fs, FS_SIZE));

        return 0;

}


//just go through the path and find the inode number
//it us basically the fd for the file
static int ext2_open(const char *path, struct fuse_file_info *fi)
{
        int ino;
        printf(GRN BOLD "open calling path_to_inode\n" RESET);
        path_to_inode(path, &ino);
        //printf("inode returned = %u\n", ino);

        if(ino == -1)
        {
                return -1;
        }

        return 0;
}


static int ext2_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
        int ino;
        printf(GRN BOLD "read calling path_to_inode\n" RESET);
        path_to_inode(path, &ino);
        size_t len;
        (void) fi;

        if(ino == -1)
                return -ENOENT;

        inode *temp_ino = (inodes + (ino * sizeof(inode)));
        len = temp_ino->size;

        if (offset < len)
        {
                if (offset + size > len)
                        size = len - offset;
                char *temp_data = datablks + ((temp_ino -> data) * block_size);
                memcpy(buf, temp_data + offset, size);
        }

        else
                size = 0;

        return size;
}


static int ext2_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi)
{

        //printf("Write called!!\n");

        int ino;
        printf(GRN BOLD "write calling path_to_inode\n" RESET);
        path_to_inode(path, &ino);
        inode *temp_ino = inodes + (ino * sizeof(inode));
        memcpy(((datablks + ((temp_ino -> data) * block_size)) + offset), (buf), size);
        temp_ino -> size = (temp_ino -> size) +  size;

        printf("\n\nwriting to disk\n\n");
        lseek(ext2_file, 0, SEEK_SET);
        printf("%d\n", write(ext2_file, fs, FS_SIZE));

        return 0;

}



//same as rm_dir but no need to check if empty or not
static int ext2_rm(const char *path)
{

        int i;
        i = strlen(path);
        while(path[i] != '/')
        {
                i--;
        }

        char *subpath;
        subpath = (char *)malloc(strlen(path)+ 1);
        strncpy(subpath, path, i);
        subpath[i] = '/';
        subpath[i+1] = '\0';

        int ino;
        dirent *temp;
        dirent *temp_data;
        inode *temp_ino;

        //if root
        if(strcmp(subpath, "/") == 0)
        {
                temp = root_directory;
        }

                //original path
        else
        {
                printf(GRN BOLD "unlink calling path_to_inode\n" RESET);
                path_to_inode(subpath, &ino);
                // printf("Inode for the path - %s - %d\n", path, ino);

                temp_ino = inodes + (ino * sizeof(inode));

                temp = datablks + ((temp_ino->data) * block_size);
        }

        //subpath

        //printf("ino -> data = %u\n", temp);
        //printf("root_directory = %u\n", root_directory);

        while(((strcmp(temp -> filename, "") != 0) || (temp -> file_inode) != 0) && strcmp(path + (i+1), temp -> filename) != 0){
                temp ++;
        }


        if((strcmp(temp -> filename, "") != 0))
        {
                //printf("temp -> file_inode = %d\n", temp -> file_inode);
                if(temp -> file_inode != 0)
                {
                        temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
                        temp_data = ((dirent *)(datablks + ((temp_ino -> data) * block_size)));
                        strcpy(temp ->filename, "");
                        //data_map[(temp_ino -> data)] = 1;
                        inode_map[(temp -> file_inode)] = 0;
                }
        }


        printf("\n\nfile deleted from disk\n\n");
        lseek(ext2_file, 0, SEEK_SET);
        write(ext2_file, fs, FS_SIZE);
        printf("____________________________________________________________\n");
        return 0;

}
