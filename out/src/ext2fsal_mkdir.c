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

int32_t ext2_fsal_mkdir(const char *path)
{
    /**
     * TODO: implement the ext2_mkdir command here ...
     * the argument path is the path to the directory that is to be created.
     */

     /* This is just to avoid compilation warnings, remove this line when you're done. */
    if(strcmp(path,"/") == 0){
        
        return ENOENT;
    }
    if(path[0] != '/'){
       
        return ENOENT;
    }
    //Get the inode for root
    char *query = strdup(path);
    char *storeToken;
    char *tokens = strtok_r(query, "/", &storeToken);
    struct ext2_inode *curr_inode = &inode_table[EXT2_ROOT_INO-1];
    unsigned int curr_inode_num = EXT2_ROOT_INO;
    struct ext2_dir_entry *curr_dir = get_dir_entry(curr_inode, curr_inode_num, tokens,EXT2_FT_DIR);
    while(tokens != NULL){
        char *next_token = strtok_r(storeToken, "/", &storeToken);
        //if we can not find a directory with current token name, then there are two cases
        //1. We reached the one we want to create
        //2. The path is invalid
        if(curr_dir == NULL){
            if (strlen(tokens) > 255){
                return ENAMETOOLONG;
            }
            else if(next_token == NULL){
                struct ext2_dir_entry *new_entry = new_dir_entry(curr_inode, tokens, curr_inode_num, EXT2_FT_DIR);
                //if we can not get enough space to create.
                if (new_entry == NULL){
                    return ENOSPC;
                }
                int inode_num = init_inode_block(EXT2_FT_DIR);
                
                new_entry->inode = inode_num + 1;
                if(new_dir_entry(&inode_table[inode_num],".",inode_num+1,EXT2_FT_DIR) == NULL){
                    return ENOSPC;
                }

                pthread_mutex_lock(&inode_table_lock[inode_num]);
                if(new_dir_entry(&inode_table[inode_num],"..",curr_inode_num,EXT2_FT_DIR) == NULL){
                    return ENOSPC;
                }
                pthread_mutex_unlock(&inode_table_lock[inode_num]);

                pthread_mutex_lock(&gd_lock);
                gd->bg_used_dirs_count++;
                pthread_mutex_unlock(&gd_lock);
                return 0;
            }
            else{
                return ENOENT;
            }
        }
        tokens = next_token;
        if(tokens != NULL){
            curr_inode_num = curr_dir->inode;
            pthread_mutex_lock(&inode_table_lock[curr_inode_num - 1]);
            curr_inode = &inode_table[curr_inode_num - 1];
            pthread_mutex_unlock(&inode_table_lock[curr_inode_num - 1]);
            curr_dir = get_dir_entry(curr_inode, curr_inode_num, tokens,EXT2_FT_DIR);
        }
    }
    
    //if the file name exist and it is a regular file or a soft link
    if(curr_dir->file_type != EXT2_FT_DIR){
        return ENOENT;
    }
    return EEXIST;
}