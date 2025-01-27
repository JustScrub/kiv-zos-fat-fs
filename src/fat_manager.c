#include "include/fat_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* TODO: 

    cd command - write correct path to fat_info_t
*/
const int DIR_LEN = DDIR_LEN; // to calc it only once

fat_manag_err_code_t fat_load_info(char *fat_file, fat_info_t *info)
{
    FILE *fs = fopen(fat_file, CLUSTER_READ);
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
    FILE *fs = fopen(fat_file.fs_file, CLUSTER_WRITE);
    if(!fs)
    {
        return FAT_FILE_404;
    }
    fseek(fs, 0, SEEK_SET);

    printD("fs_file=%s,ftell=%ld",fat_file.fs_file, ftell(fs));

    // write metadata
    void *data_seq[] = {MAGIC_VAL, &fat_file.block_size, &fat_file.data_blocks, &fat_file.fat_size, &fat_file.first_block_offset, &fat_file.last_read, &fat_file.last_write};
    int data_lens[] = {strlen(MAGIC_VAL), sizeof(int), sizeof(fat_file.data_blocks), sizeof(fat_file.fat_size), sizeof(fat_file.first_block_offset), sizeof(time_t), sizeof(time_t)};
    unsigned long size = 0;

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
    printD("fat_copen: opening=%s,cluster=%d",info->fs_file,cluster);
    if(c) fseek(c, info->first_block_offset + cluster * info->block_size, SEEK_SET);
    else{
        printD("fat_copen: error,c=%p",c);
        perror("fat_copen");
        return NULL;
    }
    return c;
}

fat_manag_err_code_t fat_cseek(fat_info_t *info, FILE *fs, dblock_idx_t cluster)
{
    if(!fs) return FAT_FILE_404;
    printD("fat_cseek: cluster=%d",cluster);
    if(!fseek(fs, info->first_block_offset + cluster * info->block_size, SEEK_SET))
        return FAT_OK;
    return FAT_FPTR_ERR;
}

fat_manag_err_code_t fat_mkdir(fat_info_t *info, fat_dir_t *root, char *dname)
{
    if(root && root->fnum >= DIR_LEN)
    {
        return FAT_NO_MEM;
    }

    if(root)
    for(int i=0;i<root->fnum;i++)
    {
        if(!strncmp(dname, root->files[i].name,FILENAME_SIZE))
        {
            return FAT_EXIST;
        }
    }

    fat_file_info_t finfo = {0};
    dblock_idx_t cluster = root? fat_get_free_cluster(info): 0;
    dblock_idx_t parent = root? root->idx : 0;
    printD("fat_mkdir: free_cluster=%d", cluster);
    printD("fat_mkdir: parent_cluster=%d", parent);

    if(cluster==FAT_ERR) return FAT_NO_MEM;
    info->FAT[cluster] = FAT_EOF; // mark the cluster as used. Also the end of the dir

    FILE *c = fat_copen(info, cluster, CLUSTER_WRITE);
    if(!c) return FAT_FPTR_ERR;

    // create the new directory
        int fnum = 2;
        fwrite(&fnum, sizeof(int), 1, c);

        // write the "." entry
        finfo.name[0] = '.';
        finfo.type = FTYPE_DIR;
        finfo.start = cluster;
        if(fwrite(&finfo,FINFO_SIZE, 1, c) != 1)
        {
            fclose(c);
            perror("fat_mkdir write .");
            return FAT_ERR_CRITICAL;
        }

        // write the ".." entry
        finfo.name[1] = '.';
        finfo.start = parent;
        if(fwrite(&finfo,FINFO_SIZE, 1, c) != 1)
        {
            fclose(c);
            perror("fat_mkdir write ..");
            return FAT_ERR_CRITICAL;
        }
        printD("fat_mkdir: new_dir_ftell=%ld",ftell(c));

    // add the new dir to the parent dir, if not creating root dir
    if(root)
    {
        /* write to parent directory */
        if(fat_cseek(info, c, root->idx) != FAT_OK)
        {
            fclose(c);
            return FAT_FPTR_ERR;
        }
        fnum = ++root->fnum; // adding new entry to the dir, increase the number of files
        fwrite(&fnum, sizeof(int), 1, c);
        fseek(c, (root->fnum-1)*FINFO_SIZE, SEEK_CUR); // jump after the *written* directories. Not behind the one to be written!

        strncpy(finfo.name, dname, 11); // if stlen(dname)<11, it will be padded with 0s, if >11, it will be trimmed
        finfo.start = cluster;
        if(fwrite(&finfo,FINFO_SIZE, 1, c) != 1)
        {
            fclose(c);
            return FAT_ERR_CRITICAL;
        }
        printD("fat_mkdir: root_ftell=%ld",ftell(c));
    }
    fclose(c);

    if(root)
    {
        fat_load_dir_info(info, root, parent,NULL);
    }

    return FAT_OK;
}

void consume_path_part(char **path, char *bfr)
{
     while(**path&&**path-'/') 
    *(bfr++) = *((*path)++);
}

fat_manag_err_code_t fat_goto_dir(fat_info_t *info, fat_dir_t *root, char *path)
{
    printD("goto_dir: cluster=%d",(!root || *path == '/')? 0 : root->idx);
    if(*path == '/'){
        path++;
        fat_load_dir_info(info, root, 0, NULL);
    }
    char bfr[FILENAME_SIZE+1];
    fat_file_info_t *explored;

    traverse:
    bzero(bfr, FILENAME_SIZE+1);
    consume_path_part(&path, bfr); // updates path
    printD("goto_dir: next=%s,rest=%s",bfr,path);
    if(*bfr)
    {
        for(int i=0;i<root->fnum;i++)
        {
            explored = &root->files[i];
            printD("goto_dir: testing=%s",explored->name);
            if(strncmp(explored->name, bfr, FILENAME_SIZE))
            {
                continue;
            }
            if(explored->type != FTYPE_DIR)
            {
                return FAT_PATH_404; // the file is not a directory, wrong path
            }
            // found next dir to move into, set root to the new dir
            fat_load_dir_info(info, root, explored->start, NULL); // this also fseeks to the start of new root
            printD("goto_dir: new_root=%s/%s,cluster=%d",bfr,explored->name, explored->start);
            if(*path) path++; //consume the '/'. If path points to null terminator, do nothing (otherwise dangling ptr)
            else goto end; // no remaining path -> we're there
            goto traverse;
        }
        return FAT_PATH_404; // no file with specified name in root
    }
    end:
    return FAT_OK;
}

fat_manag_err_code_t fat_load_dir_info(fat_info_t *info, fat_dir_t *dir, dblock_idx_t dir_idx, FILE* fs)
{
    FILE *c = fs? fs : fat_copen(info, dir_idx, CLUSTER_READ);
    if(fs) fat_cseek(info, c, dir_idx);
    fread(&dir->fnum, sizeof(int), 1, c);
    fread(&dir->files, FINFO_SIZE, dir->fnum, c);
    dir->idx = dir_idx;
    printD("fat_load_dir_info: fnum=%i,idx=%i,ftell=0x%lX",dir->fnum, dir_idx, ftell(c));
    if(!fs) fclose(c);
    return FAT_OK;
}

fat_manag_err_code_t fat_cat_into(fat_info_t *info, fat_dir_t *cwd, char *inf_name, FILE *cpyf)
{
    dblock_idx_t idx = FAT_ERR;
    unsigned long fsize;

    for(int i=0; i<cwd->fnum; i++)
    {
        if(!strcmp(inf_name,cwd->files[i].name))
        {
            idx = cwd->files[i].start;
            fsize = cwd->files[i].size;
            break;
        }
    }
    if(idx == FAT_ERR)
    {
        return FAT_FILE_404;
    }

    
    FILE *fs = fat_copen(info, idx, CLUSTER_READ);
    char *bfr = malloc(BLOCK_SIZE);

    while(1)
    {
        bzero(bfr, BLOCK_SIZE);
        fread(bfr, 1, BLOCK_SIZE, fs);
        fwrite(bfr, 1, (fsize/BLOCK_SIZE)? BLOCK_SIZE : (fsize%BLOCK_SIZE), cpyf );
        fsize -= BLOCK_SIZE;
        idx = info->FAT[idx];
        if((long)fsize <= 0 || idx == FAT_EOF) break;
        fat_cseek(info, fs, idx);
    }

    fclose(fs);
    free(bfr);
    return FAT_OK;
}

fat_manag_err_code_t fat_remove_file(fat_info_t *info, fat_dir_t *cwd, char *rmdir_name, unsigned char type)
{
    FILE *c = NULL;
    int i;
    printD("fat_rmf: name=%s, type=%c", rmdir_name, type==FTYPE_DIR? 'D':'F');
    for(i=0;i<cwd->fnum;i++)
    {
        if(!strncmp(rmdir_name, cwd->files[i].name,FILENAME_SIZE))
        {
            if(cwd->files[i].type == type)
                goto removal;
            else break;
        }
    }
    return FAT_FILE_404;

    removal:
    c = fat_copen(info, cwd->files[i].start, CLUSTER_WRITE);
    if(cwd->files[i].type == FTYPE_DIR)
    {
        // first, check if not empty
        printD("fat_rm: rm_idx=%d,rm_ftell=0x%lX",cwd->files[i].start, ftell(c));
        int rmfnum;
        fread(&rmfnum, sizeof(int), 1, c);
        if(rmfnum > 2)
        {
            fclose(c);
            return FAT_NOT_EMPTY;
        }
    }

    // now remove
    fat_cseek(info, c, cwd->idx);
    printD("fat_rm: parent_idx=%d,parent_ftell=0x%lX,parent_fnum=%d",cwd->idx, ftell(c),cwd->fnum);
    cwd->fnum--;
    fwrite(&(cwd->fnum), sizeof(int), 1, c);
    fseek(c,FINFO_SIZE*i, SEEK_CUR); // goto FINFO of the deleted dir
    fwrite(&cwd->files[cwd->fnum],FINFO_SIZE, 1, c); // overwrite with the last entry (fnum is already decremented!)

    dblock_idx_t idx = cwd->files[i].start; // cwd is not updated
    for(;;)
    {
        i = info->FAT[idx];
        info->FAT[idx] = FAT_FREE;
        idx = i;
        if(idx == FAT_EOF) break;
    }

    fclose(c);
    return FAT_OK;
}

dblock_idx_t fat_get_free_cluster(fat_info_t *info)
{
    if(!info) return FAT_FREE; // the largest number
    for(dblock_idx_t i=1; i<info->data_blocks; i++)
    {
        if(info->FAT[i]==FAT_FREE) return i;
    }
    return FAT_ERR; // no free blocks
}