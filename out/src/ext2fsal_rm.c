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
#include <sys/mman.h>
#include <pthread.h>

int32_t ext2_fsal_rm(const char *path)
{
    /**
     * TODO: implement the ext2_rm command here ...
     * the argument 'path' is the path to the file to be removed.
     */

     /* This is just to avoid compilation warnings, remove this line when you're done. */
    if(strcmp(path,"/") == 0){
        
        return ENOENT;
    }
    if(path[0] != '/'){
       
        return ENOENT;
    }
    char *query = strdup(path);
    char *query2 = strdup(path);
    char *pathname = dirname(query);
    char *base_name = basename(query2);
    unsigned int curr_inode_num = target_path(pathname);
    //if the path is not valid, return ENOENT
    if (curr_inode_num == 0){
        return ENOENT;
    }
    //if we want to remove a directory
    if (path[strlen(path)-1] == '/'){
        pthread_mutex_lock(&inode_table_lock[curr_inode_num-1]);
        int attempt = remove_inode(&inode_table[curr_inode_num-1], base_name, EXT2_FT_DIR);
        pthread_mutex_unlock(&inode_table_lock[curr_inode_num-1]);
        if(attempt == -1){
            return ENOENT;
        }
        pthread_mutex_lock(&inode_table_lock[attempt-1]);
        remove_data(attempt);
        pthread_mutex_unlock(&inode_table_lock[attempt-1]);
    }
    //if we want to remove a regular file
    else{
        pthread_mutex_lock(&inode_table_lock[curr_inode_num-1]);
        int attempt = remove_inode(&inode_table[curr_inode_num-1], base_name, EXT2_FT_REG_FILE);
        pthread_mutex_unlock(&inode_table_lock[curr_inode_num-1]);
        if (attempt != -1){
            pthread_mutex_lock(&inode_table_lock[attempt-1]);
            remove_data(attempt);
            pthread_mutex_unlock(&inode_table_lock[attempt-1]);
            return 0;
        }

        pthread_mutex_lock(&inode_table_lock[curr_inode_num-1]);
        attempt = remove_inode(&inode_table[curr_inode_num-1], base_name, EXT2_FT_SYMLINK);
        pthread_mutex_unlock(&inode_table_lock[curr_inode_num-1]);
        if (attempt != -1){
            pthread_mutex_lock(&inode_table_lock[attempt-1]);
            remove_data(attempt);
            pthread_mutex_unlock(&inode_table_lock[attempt-1]);
            return 0;
        }
        return EISDIR;
    }
    return 0;
}