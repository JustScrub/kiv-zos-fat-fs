#include "include/fat_manager.h"
#include <stdio.h>
#include <string.h>

/* TODO: 
    fat_un/load_dir_info
    fat_get_free_cluster

    fat_goto_dir
*/

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

FILE *fat_copen(fat_info_t *info, dblock_idx_t cluster, char *mode)
{
    FILE *c = fopen(info->fs_file, mode);
    if(c) fseek(c, info->first_block_offset + cluster * info->block_size, SEEK_SET);
    return c;
}

fat_manag_err_code_t fat_cseek(fat_info_t *info, FILE *fs, dblock_idx_t cluster)
{
    if(!fs) return FAT_FILE_404;
    if(!fseek(fs, info->first_block_offset + cluster * info->block_size, SEEK_SET))
        return FAT_OK;
    return FAT_FPTR_ERR;
}

int write_dir_info(char *name, long size, dblock_idx_t start, char type, FILE *cp)
{
    int i = 0;
    i += fwrite(name, 1, 12, cp);
    i += fwrite(&size, sizeof(size), 1, cp);
    i += fwrite(&start, sizeof(start), 1, cp);
    i += fwrite(&type, sizeof(type), 1, cp);
    return i;
}

fat_manag_err_code_t fat_mkdir(fat_info_t *info, fat_dir_t *root, char *dname)
{
    if(root && root->fnum >= DIR_LEN)
    {
        return FAT_NO_MEM;
    }

    dblock_idx_t cluster = root? fat_get_free_cluster(info): 0;
    FILE *c = fat_copen(info, cluster, CLUSTER_WRITE);
    if(!c) return FAT_FPTR_ERR;

    unsigned long size = 2;
    if(!root) // write fnum to the root directory
    {
        fwrite(&size, sizeof(unsigned long), 1, c);
    }

    char name[12] = {0};
    name[0] = '.';
    size = DIR_SIZE(2); // will contain only '.' and '..' dirs
    char type = FTYPE_DIR;
    if(write_dir_info(name, size, cluster, type, c) != FINFO_SIZE)
    {
        fclose(c);
        return FAT_ERR_CRITICAL;
    }

    int i = 0;
    name[1] = '.';
    size = root? (unsigned long)(DIR_SIZE(root->fnum)) : DIR_SIZE(2); // if root dir, '..' points to itself
    dblock_idx_t parent = root? root->idx : 0;
    if(write_dir_info(name, size, parent, type, c) != FINFO_SIZE)
    {
        fclose(c);
        return FAT_ERR_CRITICAL;
    }

    if(root)
    {
        /* write to parent directory */
        if(fat_cseek(info, c, root->idx) != FAT_OK)
        {
            fclose(c);
            return FAT_FPTR_ERR;
        }
        size = ++root->fnum; // new "file" in the root dir
        fwrite(&size, sizeof(unsigned long), 1, c);
        fseek(c, root->fnum*FINFO_SIZE, SEEK_CUR); // jump to the end of the dir data
        strncpy(name, dname, 11);
        size = DIR_SIZE(2);
        if(write_dir_info(name, size, cluster, type, c) != FINFO_SIZE)
        {
            fclose(c);
            return FAT_ERR_CRITICAL;
        }
    }
    fclose(c);

    if(root)
    {
        fat_unload_dir_info(root);
        fat_load_dir_info(info, root, parent);
    }

    return FAT_OK;
}

int read_dir_info(fat_file_info_t *file, FILE *cp)
{
    int i = 0;
    i += fread(file->name, 1, 12, cp);
    i += fread(&file->size, sizeof(unsigned long), 1, cp);
    i += fread(&file->start, sizeof(dblock_idx_t), 1, cp);
    i += fread(&file->type, sizeof(char), 1, cp);
    return i;
}

fat_manag_err_code_t fat_load_dir_info(fat_info_t *info, fat_dir_t *dir, dblock_idx_t *dir_idx)
{
    FILE *c = fat_copen(info, dir_idx, CLUSTER_READ);
    unsigned long size = 0;
    fread(&size, sizeof(unsigned long), 1, c);

}

fat_manag_err_code_t fat_unload_dir_info(fat_dir_t *dir);
