#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


const static fat_shell_cmd_t command_arr[] = {
    {
        .id = "cd",
        .callback = cmd_cd
    },
    {
        .id = "pwd",
        .callback = cmd_pwd
    },
    {
        .id = "load",
        .callback = cmd_load
    },
    {
        .id = "clear",
        .callback = cmd_clear
    },

    {
        .id = NULL,
        .callback = NULL
    }
};


fat_info_t fat_file;

void set_fat_info(fat_info_t *the_fat)
{
    memcpy(&fat_file, the_fat, sizeof(fat_info_t));
}

void color_print(ansi_color_t color){
    printf("\e[%im",color);
}

cmd_err_code_t cmd_cd(void *arg){
    char *path = ((char **)arg)[0];
    if(!path) return CMD_PATH_404;
    int pwdlen = strlen(fat_file.pwd);

    if(!strcmp(path,".."))
    {
        if(!strlen(fat_file.pwd)) return CMD_OK;
        while(fat_file.pwd[pwdlen-1] != '/')
        {
            fat_file.pwd[pwdlen-1] = 0;
            pwdlen--;
        }
        fat_file.pwd[pwdlen-1] = 0;
        return CMD_OK;
    }

    if(pwdlen >= PWD_MAX_LEN)
    {
        return CMD_NO_MEM;
    }

    fat_file.pwd[pwdlen] = '/';
    strncpy(fat_file.pwd+pwdlen+1, path, PWD_MAX_LEN - pwdlen);
    return CMD_OK;
}

cmd_err_code_t cmd_pwd(void *args)
{
    printf(fat_file.pwd);
    return CMD_OK;
}

cmd_err_code_t cmd_load(void *args)
{
    char *batch_path = ((char **)args)[0];
    char *bfr = NULL;
    FILE *batchf = fopen(batch_path, "r");
    if(!batchf)
    {
        return CMD_PATH_404;
    }

    bfr = alloca(256);
    cmd_err_code_t err = CMD_OK;

    while(!feof(batchf))
    {
        bzero(bfr,256);
        err = load_cmd(batchf,bfr,256);
        if(err) break;
    }
    fclose(batchf);
    return err;
}

cmd_err_code_t cmd_clear(void* args)
{
    system("clear");
    return CMD_OK;
}

cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    for(size_t i=0; command_arr[i].id; i++)
        if(!strcmp(cmd_id, command_arr[i].id))
            return command_arr[i].callback(args);

    return CMD_UNKNOWN;
}

cmd_err_code_t cmd_format(void *args)
{
    char *arg = ((char **)arg)[0];
    char units[3] = {0}, *valid_units[] = {"", "B", "KB", "MB", "GB"};
    unsigned long size;

    size = strtoul(arg, &units, 10);
    if(size <=0 )
    {
        return CMD_INV_ARG;
    }

    int i;
    for(i=0; i<4 && strcmp(units, valid_units[i]);i++) ;
    if(i > 3) return CMD_INV_ARG;
    if(units[0]) size <<= (10*(i-1));

    size -= (size%BLOCK_SIZE); // decrease by any remainder since blocks must be of same size, efectively floor()

    // fill the structure with new metadata
    fat_file.block_size = BLOCK_SIZE;
    fat_file.data_blocks = size/BLOCK_SIZE;

    fat_file.fat_size = sizeof(dblock_idx_t)*fat_file.data_blocks;
    fat_file.fat_blocks = (fat_file.fat_size)/BLOCK_SIZE; // num of data blocks for FAT
    fat_file.fat_blocks += !!(fat_file.fat_size%BLOCK_SIZE); // to round up any remainder, efectively ceil()
    /*
        example: 
        data_blocks = 3, BLOCK_SIZE = 1024, sizeof(dblock_idx_t) = 4
        3*4 / 1024 = 0
        3*4 % 1024 = 12
        !!12 = 1 (convert to bool)
        ==> fat_blocks = 0+1 (need one block for fat)
        --------
        data_blocks = 1024, BLOCK_SIZE = 1024, sizeof(dblock_idx_t) = 4
        1024*4 / 1024 = 4
        1024*4 % 1024 = 0
        !!0 = 0
        ==> fat_blocks = 4+0 (need 4 fat blocks)
    */
    fat_file.first_block_offset = fat_file.fat_blocks*BLOCK_SIZE + BLOCK_SIZE;
    fat_file.pwd[0] = 0;

    if(fat_file.FAT) free(fat_file.FAT);
    fat_file.FAT = malloc(fat_file.fat_size);
    memset(fat_file.FAT, FAT_FREE, fat_file.fat_size);
    fat_file.FAT[0] = FAT_EOF; // first block is used by root dir

    size = BLOCK_SIZE;
    time_t secs = time(NULL);
    FILE *fs = fopen(fat_file.fs_file, "wb");

    // write metadata
    void *data_seq[] = {"MLADY_FS", &size, &fat_file.data_blocks, &fat_file.fat_blocks, &fat_file.fat_size, &fat_file.first_block_offset, &secs, &secs};
    int data_lens[] = {strlen("MLADY_FS"), sizeof(size), sizeof(fat_file.data_blocks), sizeof(fat_file.fat_blocks), sizeof(fat_file.fat_size), sizeof(fat_file.first_block_offset), sizeof(secs), sizeof(secs)};
    i = 0;

    for(int j=0; j < sizeof(data_seq); j++)
    {
        fwrite(data_seq[j], data_lens[j],1,fs);
        i += data_lens[j];
    }

    for(;i < BLOCK_SIZE; i++)
    {
        fputc(0,fs); // pad metadata with 0s
    }

    // write FAT table
    fwrite(fat_file.FAT, sizeof(dblock_idx_t), fat_file.fat_size, fs);
    for(size = fat_file.fat_size; size<fat_file.fat_blocks*BLOCK_SIZE; size++)
    {
        fputc(0,fs); // pad FAT with zeros - valid since we know the length of FAT from metadata
    }

    // pad the rest of the FS with zeros (only data blocks are remaining now)
    for(i = 0; i < fat_file.data_blocks * BLOCK_SIZE; i++)
    {
        fputc(0, fs);
    }

    fclose(fs);
    return CMD_OK;
}





// utility functions
char *cmd_err_msgs[] = {
    "Success.",
    "File not found.",
    "Path does not exist.",
    "Already exists.",
    "Directory not empty.",
    "Format failed.",
    "Out of memory.",
    "Invalid argument",
    "Unknown command."
};
void pcmderr(cmd_err_code_t err)
{
    printf("%s\n",cmd_err_msgs[err]);
}

cmd_err_code_t load_cmd(FILE *from, char *bfr, int bfr_len)
{
    if(!bfr)
    {
        bfr = alloca(256);
        bzero(bfr,256);
        bfr_len = 256;
    }
    char *args[2] = {0};
    char *cmd;
    cmd_err_code_t ret = CMD_OK;

    fgets(bfr, bfr_len-1, from);
    bfr[strcspn(bfr, "\r\n")] = 0; // delete newline character
    printD("bfr=%s", bfr);

    if(!(cmd = strtok(bfr, " "))) return CMD_OK; // no command (empty line) -> ignore
    args[0] = strtok(NULL, " "); // first argument, can be string or NULL
    args[1] = strtok(NULL, " "); // second argument, can be string or NULL
    printD("cmd=%s, arg1=%s, arg2=%s", cmd, args[0]?args[0]:"NULL",args[1]?args[1]:"NULL");

    ret = cmd_exec(cmd, (void*) args);
    if(ret)
    {
        printf("%s: ",cmd);
        pcmderr(ret);
    }
    return ret;
}

bool file_exists(char *path)
{
    return true;
}