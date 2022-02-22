/*
 *------------
 * This code is provided solely for the personal and private use of
 * students taking the CSC369H5 course at the University of Toronto.
 * Copying for purposes other than this use is expressly prohibited.
 * All forms of distribution of this code, whether as given or with
 * any changes, are expressly prohibited.
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 MCS @ UTM
 * -------------
 */

#include "ext2fsal.h"
#include "e2fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/mman.h>



//Check if the path is a regular file in the native file system
int file_exist(const char *p) {
    struct stat stater;
    stat(p, &stater);
    return S_ISREG(stater.st_mode);
}

int32_t ext2_fsal_cp(const char *src,
                     const char *dst)
{
    if (dst[0] != '/'){
        return ENOENT;
    }
    
    char *query = strdup(dst);
    char *query2 = strdup(dst);
    char *p = strdup(src);
    struct stat s;
    stat(src, &s);
    int size = s.st_size;
    FILE *fp = fopen(src, "rb");
    if(fp == NULL){
        return ENOENT;
    }
    
    char *pathname = dirname(query);
    char *base_name = basename(query2);
    char *src_base = basename(p);
    pthread_mutex_lock(&gd_lock);
    if(size > (gd->bg_free_blocks_count*1024)){
        pthread_mutex_unlock(&gd_lock);
        fclose(fp);
        return ENOSPC;
    }
    pthread_mutex_unlock(&gd_lock);
    //Find the inode for the directory
    //if the file is too big to fit into the FS.
    
    unsigned int dir_inode_num = target_path(pathname);
    //the parent path is not valid, either foo or bar does not exist, or either foo or bar is a file
    if(dir_inode_num == 0){
        fclose(fp);
        return ENOENT;
    }
    //if the filename exists and it is a soft link, then return EEXIST
    if(get_dir_entry(&inode_table[dir_inode_num-1], dir_inode_num, base_name, EXT2_FT_SYMLINK)){
        fclose(fp);
        return EEXIST;
    }
    //If the filename exists and it is a regular file, then we need to overwrite
    struct ext2_dir_entry* entry = get_dir_entry(&inode_table[dir_inode_num-1], dir_inode_num, base_name, EXT2_FT_REG_FILE);
    if(entry != NULL){
        int inode_num = entry->inode;
        pthread_mutex_lock(&inode_table_lock[inode_num-1]);
        struct ext2_inode *inode = &inode_table[inode_num-1];
        inode->i_ctime = time(NULL);
        overwrite_file(inode, fp);
        pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
        fclose(fp);   
        return 0;
    }
    //if the basename is a directory, then we need to create a file with the name of src_base
    entry = get_dir_entry(&inode_table[dir_inode_num-1], dir_inode_num, base_name, EXT2_FT_DIR);
    if (entry != NULL){
        int inode_num = init_inode_block(EXT2_FT_REG_FILE);
        if (inode_num == -1){
            fclose(fp);
            return ENOSPC;
        }
        pthread_mutex_lock(&inode_table_lock[inode_num]);
        struct ext2_inode *inode = &inode_table[inode_num];
        inode->i_ctime = time(NULL);
        if (copy_file(inode, fp) == -1){
            pthread_mutex_unlock(&inode_table_lock[inode_num]);
            fclose(fp);
            return ENOSPC;
        }
        pthread_mutex_unlock(&inode_table_lock[inode_num]);

        pthread_mutex_lock(&inode_table_lock[entry->inode-1]);
        new_dir_entry(&inode_table[entry->inode-1], src_base, inode_num+1, EXT2_FT_REG_FILE);
        pthread_mutex_unlock(&inode_table_lock[entry->inode-1]);

        pthread_mutex_lock(&inode_table_lock[inode_num]);
        inode->i_links_count++;
        pthread_mutex_unlock(&inode_table_lock[inode_num]);
        fclose(fp);
    }
    //the basename does not exist in the parent directory, then we create a file named base_name and copy src into it
    else{
        if (strlen(base_name) > 255){ 
            fclose(fp);
            return ENAMETOOLONG;
        }
        int inode_num = init_inode_block(EXT2_FT_REG_FILE);
        if (inode_num == -1){
            fclose(fp);
            return ENOSPC;
        }
        pthread_mutex_lock(&inode_table_lock[inode_num]);
        struct ext2_inode *inode = &inode_table[inode_num];
        inode->i_ctime = time(NULL);
        if (copy_file(inode, fp) == -1){
            pthread_mutex_unlock(&inode_table_lock[inode_num]);
            fclose(fp);
            return ENOSPC;
        }
        pthread_mutex_unlock(&inode_table_lock[inode_num]);

        pthread_mutex_lock(&inode_table_lock[dir_inode_num-1]);
        new_dir_entry(&inode_table[dir_inode_num-1], base_name, inode_num+1, EXT2_FT_REG_FILE);
        pthread_mutex_unlock(&inode_table_lock[dir_inode_num-1]);

        pthread_mutex_lock(&inode_table_lock[inode_num]);
        inode->i_links_count++;
        pthread_mutex_unlock(&inode_table_lock[inode_num]);
        fclose(fp);
    }
    return 0;
}