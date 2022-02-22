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

#ifndef CSC369_E2FS_H
#define CSC369_E2FS_H

#include "ext2.h"
#include <string.h>
#include <stdio.h>

/**
 * TODO: add in here prototypes for any helpers you might need.
 * Implement the helpers in e2fs.c
 */

 // .....
unsigned int target_path(char *path);
struct ext2_dir_entry* get_dir_entry(struct ext2_inode *inode, unsigned int inode_num, char* dir_name, unsigned char file_type);
int init_inode_block(unsigned char file_type);
void set_bit_inuse(unsigned char *map, int index);
int get_free_block();
int get_free_bit(unsigned char *map,int size);
unsigned short to_filemode(unsigned char f);
void set_bit_unuse(unsigned char *map, int index);
struct ext2_dir_entry* new_dir_entry(struct ext2_inode *inode, char* name, unsigned int inode_num, unsigned char file_type);
struct ext2_dir_entry *add_dir_entry(int block_num,char* name, unsigned int inode_num,unsigned char file_tp);
int copy_file(struct ext2_inode *inode,FILE *fp);
void remove_data(int inode_num);
int remove_inode(struct ext2_inode *inode, char* dir_name, unsigned char file_tp);
int overwrite_file(struct ext2_inode *inode, FILE *fp);
#endif