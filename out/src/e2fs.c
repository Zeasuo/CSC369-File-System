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

/**
 * TODO: Make sure to add all necessary includes here
 */
 

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
#include "ext2fsal.h"
#include "e2fs.h"

#define INODE_COUNT 32
#define BLOCK_COUNT 128


 /**
  * TODO: Add any helper implementations here
  */
/* Lock order should be 
    block bitmap
    inode bitmap
    group descriptor
    superblock

    Inodetable_lock[i] should be added depending on situation but mostly before
    block bitmap
*/
//initilize the an inode with file_type
char get_inode_type(struct ext2_inode *inode) {
    if (inode->i_mode & EXT2_S_IFREG) {
        return 'f';
    } else if (inode->i_mode & EXT2_S_IFDIR) {
        return 'd';
    } else if (inode->i_mode & EXT2_S_IFLNK) {
        return 'l';
    }

    return -1;

}

char get_dir_type(unsigned char type) {
    switch (type) {
        case EXT2_FT_REG_FILE: return 'f';
        case EXT2_FT_DIR: return 'd';
        case EXT2_FT_SYMLINK: return 'l';
        default: return -1;
    }
}
unsigned short to_filemode(unsigned char f){
    if(f == EXT2_FT_DIR){
        return EXT2_S_IFDIR;
    }
    else if(f == EXT2_FT_REG_FILE){
        return EXT2_S_IFREG;
    }
    else if(f == EXT2_FT_SYMLINK){
        return EXT2_S_IFLNK;
    }
    else{
        return 0;
    }
}
int init_inode_block(unsigned char file_type){
    pthread_mutex_lock(&sb_lock);
    if(sb->s_free_inodes_count == 0){
        pthread_mutex_unlock(&sb_lock);
        return -1;
    }
    pthread_mutex_unlock(&sb_lock);

    pthread_mutex_lock(&inode_bitmap_lock);
    int free_inode = get_free_bit(inode_bitmap,(sb->s_inodes_count/8));
    pthread_mutex_lock(&inode_table_lock[free_inode]);
    struct ext2_inode *inode = &inode_table[free_inode];
    memset(inode,0,sizeof(struct ext2_inode));
    set_bit_inuse(inode_bitmap, free_inode);
    inode->i_mode |= to_filemode(file_type);
    inode->i_size = 1024;
    inode->i_blocks = 0;
    pthread_mutex_unlock(&inode_bitmap_lock);
    pthread_mutex_unlock(&inode_table_lock[free_inode]);

    pthread_mutex_lock(&gd_lock);
    pthread_mutex_lock(&sb_lock);
    sb->s_free_inodes_count--;
    gd->bg_free_inodes_count--;
    pthread_mutex_unlock(&gd_lock);
    pthread_mutex_unlock(&sb_lock);
    return free_inode;
}
int get_free_block(){
    pthread_mutex_lock(&block_bitmap_lock);
    pthread_mutex_lock(&gd_lock);
    pthread_mutex_lock(&sb_lock);
    int next_free_block;
    if (gd->bg_free_blocks_count > 1){
        next_free_block = get_free_bit(block_bitmap,(sb->s_blocks_count/8));
        set_bit_inuse(block_bitmap,next_free_block);
        sb->s_free_blocks_count--;
        gd->bg_free_blocks_count--;
    }
    else{
        return -1;
    }
    pthread_mutex_unlock(&block_bitmap_lock);
    pthread_mutex_unlock(&gd_lock);
    pthread_mutex_unlock(&sb_lock);
    return next_free_block +1;
}
int get_free_bit(unsigned char *map,int size){
    for (int byte = 0; byte < size; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            unsigned char check = (map[byte] & (1 << bit));
            if(!(check)){
                return 8*byte+bit;
            }
        }
    }
    return -1;
}
void set_bit_inuse(unsigned char *map, int index){
    int byte_num = index / 8;
    int bit_num = index % 8;
    map[byte_num] |= 1 << bit_num;
    return;

}
void set_bit_unuse(unsigned char *map, int index){
    int byte_num = index / 8;
    int bit_num = index % 8;
    map[byte_num] = map[byte_num] & (~(1 << bit_num));
    return;

}
unsigned int target_path(char *path){
    char *storePoint;
    char *tokens = strtok_r(path, "/", &storePoint); //This breaks the path into series of token based on "/". (Similar to python's str.split())
    pthread_mutex_lock(&inode_table_lock[EXT2_ROOT_INO-1]);
    struct ext2_inode *curr_inode = &inode_table[EXT2_ROOT_INO-1];
    unsigned int curr_inode_num = EXT2_ROOT_INO; //Starting from root directory, we need to find the first directory in the tokens.
    
    //This means the path in the root directory
    if (tokens == NULL){
        pthread_mutex_unlock(&inode_table_lock[EXT2_ROOT_INO-1]);
        return curr_inode_num;
    }
    pthread_mutex_unlock(&inode_table_lock[EXT2_ROOT_INO-1]);
    struct ext2_dir_entry *curr_dir_entry = get_dir_entry(curr_inode, EXT2_ROOT_INO, tokens, EXT2_FT_DIR);
    if (curr_dir_entry == NULL){
        return 0;
    }
    while(tokens != NULL){
        if (curr_dir_entry == NULL){
            return 0;
        }
        curr_inode_num = curr_dir_entry->inode;
        pthread_mutex_lock(&inode_table_lock[curr_inode_num-1]);
        curr_inode = &inode_table[curr_inode_num-1];
        tokens = strtok_r(storePoint, "/", &storePoint);
        if(tokens != NULL){
            pthread_mutex_unlock(&inode_table_lock[curr_inode_num-1]);
            curr_dir_entry = get_dir_entry(curr_inode, curr_inode_num, tokens, EXT2_FT_DIR);
            pthread_mutex_lock(&inode_table_lock[curr_inode_num-1]);
        }
        pthread_mutex_unlock(&inode_table_lock[curr_inode_num-1]);
        if(curr_dir_entry == NULL){
            return 0;
        }
        
    }
    return curr_inode_num;

}

//Return a dir entry with the name dir_name and file type. 
struct ext2_dir_entry* get_dir_entry(struct ext2_inode *inode, unsigned int inode_num, char* dir_name, unsigned char file_type){
    if (dir_name == NULL){
        return NULL;
    }
    pthread_mutex_lock(&inode_table_lock[inode_num-1]);
    for (int i = 0; i < 12; i++){
        int i_block = inode->i_block[i];
        if(i_block != 0){
            int curr_len = 0;
            while (curr_len < EXT2_BLOCK_SIZE){
                struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * i_block + curr_len);
                if ((entry->file_type == file_type) && (entry->name_len == strlen(dir_name)) && (strncmp(dir_name,entry->name,entry->name_len) == 0)){
                    pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
                    return entry;
                }
                curr_len += entry->rec_len;
            }
        }
    }
    pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
    return NULL;
}

struct ext2_dir_entry* new_dir_entry(struct ext2_inode *inode, char* name, unsigned int inode_num, unsigned char file_type){
    pthread_mutex_lock(&inode_table_lock[inode_num-1]);
    if(file_type == EXT2_FT_DIR){
        inode->i_links_count++;
    }
    struct ext2_dir_entry *entry = NULL;
    for(int i = 0; i<12; i++){
        if(inode->i_block[i] != 0){
            entry = add_dir_entry(inode->i_block[i],name,inode_num,file_type);
            if(entry != NULL){
                pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
                return entry;
            }
        }
    }
    //no space in blocks in inode so create a new block
    for(int i = 0; i<12; i++){
        if(inode->i_block[i] == 0){
            int index = get_free_block();
            //no free block available
            if(index == -1){
                pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
                return NULL;
            }
            struct ext2_dir_entry *new_dir = (struct ext2_dir_entry*)(disk + 1024*(index));
            strncpy(new_dir->name,name,strlen(name));
            new_dir->rec_len = 1024;
            new_dir->file_type = file_type;
            new_dir->name_len = strlen(name);
            new_dir ->inode = inode_num;
            inode->i_block[i] = index;
            inode->i_blocks += 2;
            pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
            return new_dir;
        }
    }
    pthread_mutex_unlock(&inode_table_lock[inode_num-1]);
    return NULL;
}
//No locks here because this function can only be access by new_dir_entry(). Locks are acquired inside
//that function
struct ext2_dir_entry *add_dir_entry(int block_num,char* name, unsigned int inode_num, unsigned char file_tp){
    struct ext2_dir_entry *entry = (struct ext2_dir_entry*)(disk + 1024*block_num);
    int count = entry->rec_len;
    while(count<EXT2_BLOCK_SIZE){
        entry = (struct ext2_dir_entry*)((char*)entry + entry->rec_len);
        count += entry->rec_len;
    }
    int length = sizeof(struct ext2_dir_entry) + entry->name_len;
    int x = length % 4;
    length = (x == 0)?length:length-x+4;
    int padding = entry->rec_len - length;
    int check = sizeof(struct ext2_dir_entry) + strlen(name);
    if(check <= padding){
        // can add dir
        count = count - entry->rec_len + length;
        entry->rec_len = length;
        struct ext2_dir_entry *new_dir = (struct ext2_dir_entry*)(disk + 1024*block_num + count);
        strncpy(new_dir->name,name,strlen(name));
        new_dir->rec_len = EXT2_BLOCK_SIZE-count;
        new_dir->file_type = file_tp;
        new_dir->name_len = strlen(name);
        new_dir ->inode = inode_num;
        return new_dir;
    }
    return NULL;

}
int copy_file(struct ext2_inode *inode, FILE *fp){
    unsigned char buf[EXT2_BLOCK_SIZE];
    inode->i_size = 0;
    int bytes;
    int counter = 0;
    while((counter < 12) && ((bytes = fread(buf,1,1024,fp)) > 0)){
        int next_free = get_free_block();
        if(next_free == -1){
            return -1;
        }
        unsigned char *new_block = (disk + 1024*(next_free));
        memcpy(new_block, buf, bytes);
        inode->i_block[counter] = next_free;
        inode->i_size += bytes;
        inode->i_blocks += 2;
        counter++;
    }
    if(bytes > 0){ 
        int next_free = get_free_block();
        if(next_free == -1){
            return -1;
        }
        inode->i_block[counter] = next_free;
        inode->i_blocks += 2;
        unsigned int *indirect_block = (unsigned int *)(disk + 1024*(next_free));
        int index = 0;
        while(((bytes = fread(buf,1,1024,fp)) > 0)){
            next_free = get_free_block();
            if(next_free == -1){
                return -1;
            }
            indirect_block[index] = next_free;
            unsigned char *new_block = (disk + 1024*(next_free));
            memcpy(new_block, buf, bytes);
            inode->i_size += bytes;
            inode->i_blocks += 2;
            index++;
        }
    }
    return 0;

}
int overwrite_file(struct ext2_inode *inode, FILE *fp){
    unsigned char buf[EXT2_BLOCK_SIZE];
    inode->i_size = 0;
    int bytes;
    int counter = 0;
    while((counter < inode->i_blocks/2) && ((bytes = fread(buf,1,1024,fp)) > 0)){
        int next_block = inode->i_block[counter];
        unsigned char *new_block = (disk + 1024*(next_block));
        memcpy(new_block, buf, bytes);
        inode->i_size += bytes;
        counter++;
    }
    //if the size of target file is smaller than the original file
    if(counter < inode->i_blocks/2){
        pthread_mutex_lock(&block_bitmap_lock);
        pthread_mutex_lock(&gd_lock);
        pthread_mutex_lock(&sb_lock);
        while((counter < inode->i_blocks/2)){
            set_bit_unuse(block_bitmap, inode->i_block[counter]-1);
            sb->s_free_blocks_count++;
            gd->bg_free_blocks_count++;
            counter++;
        }
        if(inode->i_block[12] != 0){
            unsigned int *indirect_block = (unsigned int *)(disk + 1024*(inode->i_block[12]));
            int count = 0;
            while(indirect_block[count] != 0){
                set_bit_unuse(block_bitmap,indirect_block[count]-1);
                sb->s_free_blocks_count++;
                gd->bg_free_blocks_count++;
                count++;
            }
            set_bit_unuse(block_bitmap,inode->i_block[12] -1);
            sb->s_free_blocks_count++;
            gd->bg_free_blocks_count++;
        }
        pthread_mutex_unlock(&block_bitmap_lock);
        pthread_mutex_unlock(&gd_lock);
        pthread_mutex_unlock(&sb_lock);
    }
    //if the size of target file is larger than the original file
    if(bytes > 0 && counter < 12){
        while((counter < 12) && ((bytes = fread(buf,1,1024,fp)) > 0)){
            int next_free = get_free_block();
            if(next_free == -1){
                return -1;
            }
            unsigned char *new_block = (disk + 1024*(next_free));
            memcpy(new_block, buf, bytes);
            inode->i_block[counter] = next_free;
            inode->i_size += bytes;
            inode->i_blocks += 2;
            counter++;
        }
    }
    if(bytes > 0){ 
        int next_free = get_free_block();
        if(next_free == -1){
            return -1;
        }
        inode->i_block[counter] = next_free;
        inode->i_blocks += 2;
        unsigned int *indirect_block = (unsigned int *)(disk + 1024*(next_free));
        int index = 0;
        while(((bytes = fread(buf,1,1024,fp)) > 0)){
            next_free = get_free_block();
            if(next_free == -1){
                return -1;
            }
            indirect_block[index] = next_free;
            unsigned char *new_block = (disk + 1024*(next_free));
            memcpy(new_block, buf, bytes);
            inode->i_size += bytes;
            inode->i_blocks += 2;
            index++;
        }
    }
    return 0;
}
int remove_inode(struct ext2_inode *inode, char* dir_name, unsigned char file_type){
    if (dir_name == NULL){
        return -1;
    }
    
    for(int i = 0; i<12; i++){
        if(inode->i_block[i] != 0){
            int block_num = inode->i_block[i];
            struct ext2_dir_entry *entry = (struct ext2_dir_entry*)(disk + 1024*block_num);
            if((file_type == entry->file_type) && (entry->name_len == strlen(dir_name)) && (strncmp(dir_name,entry->name,entry->name_len) == 0)){
                int t = entry->inode;
                return t;
            }
            int prev = 0; 
            int curr_len = entry->rec_len;
            while(curr_len<EXT2_BLOCK_SIZE){
                entry = (struct ext2_dir_entry*)((char*)entry + entry->rec_len);
                //if we found the dir_entry, and it is not at the beginning, then we need add its rec len to its prev entry to compromise the removal
                if((file_type == entry->file_type) && (entry->name_len == strlen(dir_name)) && (strncmp(dir_name,entry->name,entry->name_len) == 0)){
                    struct ext2_dir_entry *prev_entry = (struct ext2_dir_entry*)(disk + 1024*block_num +prev);
                    prev_entry->rec_len += entry->rec_len;
                    return entry->inode;
                }
                prev = curr_len;
                curr_len += entry->rec_len;
            }

        }
    }
    return -1; // does not find the dir entry with same dir_name and filetype

}
//Remove data of a file
void remove_data(int inode_num){
    pthread_mutex_lock(&gd_lock);
    pthread_mutex_lock(&sb_lock);
    struct ext2_inode *inode = &inode_table[inode_num-1];
    inode -> i_links_count--;
    if(inode->i_links_count == 0){
        set_bit_unuse(inode_bitmap,inode_num-1);
        sb->s_free_inodes_count++;
        gd->bg_free_inodes_count++;
        for(int i = 0; i < 12; i ++){
            if(inode->i_block[i] != 0){
                set_bit_unuse(block_bitmap,inode->i_block[i]-1);
                sb->s_free_blocks_count++;
                gd->bg_free_blocks_count++;
            }
        }
        if(inode->i_block[12] != 0){
            unsigned int *indirect_block = (unsigned int *)(disk + 1024*(inode->i_block[12]));
            int count = 0;
            while(indirect_block[count] != 0){
                set_bit_unuse(block_bitmap,indirect_block[count]-1);
                sb->s_free_blocks_count++;
                gd->bg_free_blocks_count++;
                count++;
            }
            set_bit_unuse(block_bitmap,inode->i_block[12] -1);
            sb->s_free_blocks_count++;
            gd->bg_free_blocks_count++;
        }
    }
    pthread_mutex_unlock(&gd_lock);
    pthread_mutex_unlock(&sb_lock);
    return;
}
