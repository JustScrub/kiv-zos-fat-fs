#include "include/fat_manager.h"
#include <stdio.h>
#include <string.h>

fat_manag_err_code_t fat_load_info(char *fat_file, fat_info_t *info)
{
    FILE *fs = fopen(fat_file, "rb");
    if(!fs)
    {
        return FAT_BAD_FORMAT;
    }

    char magic[9] = {0};
    fread(magic, 1, strlen(MAGIC_VAL), fs);
    if(strcmp(magic,MAGIC_VAL))
    {
        fclose(fs);
        return FAT_BAD_FORMAT;
    }

    void *fill[] = {&info->block_size, &info->data_blocks, &info->fat_size, &info->first_block_offset, &info->last_read, &info->last_write};
    int data_sizes[] = {sizeof(int),sizeof(int),sizeof(int),sizeof(int),sizeof(time_t),sizeof(time_t)};

    for(int i=0;i<6;i++)
    {
        fread(fill[i],data_sizes[i],1,fs);
    }
    printD("BS=%d,DB=%d,FS=%d,OFF=0x%X", info->block_size,info->data_blocks,info->fat_size,info->first_block_offset);

    if(info->FAT) free(info->FAT);
    info->FAT = malloc(info->fat_size);
    fread(info->FAT, info->fat_size, 1, fs);

    fclose(fs);
    memset(info->pwd, 0, PWD_MAX_LEN);
    info->fs_file = fat_file;
    return FAT_OK;
}

fat_manag_err_code_t fat_write_info(fat_info_t *info)
{
    fat_info_t fat_file = *info;
    FILE *fs = fopen(fat_file.fs_file, "ab");
    if(!fs)
    {
        return FAT_FILE_404;
    }
    fseek(fs, 0, SEEK_SET);

    printD("fs_file=%s",fat_file.fs_file);

    // write metadata
    void *data_seq[] = {MAGIC_VAL, &fat_file.block_size, &fat_file.data_blocks, &fat_file.fat_size, &fat_file.first_block_offset, &fat_file.last_read, &fat_file.last_write};
    int data_lens[] = {strlen(MAGIC_VAL), sizeof(int), sizeof(fat_file.data_blocks), sizeof(fat_file.fat_size), sizeof(fat_file.first_block_offset), sizeof(time_t), sizeof(time_t)};
    int size = 0;

    for(int j=0; j < 7; j++)
    {
        fwrite(data_seq[j], data_lens[j],1,fs);
        size += data_lens[j];
    }

    printD("metadata len=%ld,ftell=0x%lX", size,ftell(fs));

    // write FAT table
    fwrite(fat_file.FAT, fat_file.fat_size, 1, fs);
    printD("FAT write ftell=0x%lX", ftell(fs));

    if(fat_file.first_block_offset != ftell(fs))
    {
        fclose(fs);
        *info = fat_file;
        return FAT_ERR_CRITICAL;
    }

    fclose(fs);
    *info = fat_file;
    return FAT_OK;
}

fat_manag_err_code_t fat_info_create(fat_info_t *info, unsigned long size)
{
    fat_info_t fat_file = *info;

    fat_file.block_size = BLOCK_SIZE;
    fat_file.data_blocks = size/BLOCK_SIZE;
    fat_file.data_blocks += !!(size%BLOCK_SIZE); // to round up any remainder, efectively ceil()
        /*
        example: 
        size = 1000B, BLOCK_SIZE = 1024B
        1000 / 1024 = 0
        1000 % 1024 = 1000
        !!1000 = 1 (convert to bool, true iff there is remainder)
        ==> data_blocks = 0+1
        --------
        size = 4096B, BLOCK_SIZE = 1024B
        4096 / 1024 = 4
        4096 % 1024 = 0
        !!0 = 0
        ==> data_blocks = 4+0
        */

    fat_file.fat_size = sizeof(dblock_idx_t)*fat_file.data_blocks;

    fat_file.first_block_offset = fat_file.fat_size + METADATA_SIZE;
    fat_file.pwd[0] = 0;
    
    if(fat_file.FAT) free(fat_file.FAT);
    fat_file.FAT = malloc(fat_file.fat_size);
    memset(fat_file.FAT, FAT_FREE, fat_file.fat_size);
    fat_file.FAT[0] = FAT_EOF; // first block is used by root dir

    fat_file.last_read = fat_file.last_write = time(NULL);

    *info = fat_file;
    return FAT_OK;
}
