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
        .id = "format",
        .callback = cmd_format
    },
    {
        .id = "clear",
        .callback = cmd_clear
    },
    {
        .id = "mkdir",
        .callback = cmd_mkdir
    },
    {
        .id = "lr",
        .callback = cmd_lr
    },
    {
        .id = "lw",
        .callback = cmd_lw
    },

    {
        .id = NULL,
        .callback = NULL
    }
};


fat_info_t fat_file;
fat_dir_t  *curr_dir,  //cache the cwd and the root directory
           *root_dir;

void set_fat_info(fat_info_t *the_fat)
{
    memcpy(&fat_file, the_fat, sizeof(fat_info_t));
}
void save_fat_info()
{
    fat_write_info(&fat_file);
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

cmd_err_code_t cmd_format(void *args)
{
    char *arg = ((char **)args)[0];
    char *units, *valid_units[] = {"", "B", "KB", "MB", "GB"};
    unsigned long size;

    size = strtoul(arg, &units, 10);
    printD("size=%lu,units=%s", size,units);
    if(size <=0 )
    {
        return CMD_INV_ARG;
    }

    int i;
    for(i=0; i<4 && strcmp(units, valid_units[i]);i++) ;
    if(i > 4) return CMD_INV_ARG;
    if(units[0]) size <<= (10*(i-1));
    printD("demanded size: %d B", size);

    // fill the structure with new metadata
    fat_info_create(&fat_file, size);
    printD("actual size: %d B", fat_file.data_blocks*BLOCK_SIZE);
    printD("BS=%d,DB=%d,FS=%d,OFF=0x%X", fat_file.block_size,fat_file.data_blocks,fat_file.fat_size,fat_file.first_block_offset);

    // trim the file to 0 or create it
    FILE *fs = fopen(fat_file.fs_file, "wb");
    close(fs);

    // write the info
    switch (fat_write_info(&fat_file)){
        case FAT_FILE_404: return CMD_FILE_404;
        case FAT_ERR_CRITICAL: return CMD_CANNOT_CREATE_FILE;
    }

    // pad the rest of the FS with zeros (only data blocks are remaining now)
    fs = fopen(fat_file.fs_file, "ab");
    for(int i = 0; i < fat_file.data_blocks * BLOCK_SIZE; i++)
    {
        fputc(0, fs);
    }
    printD("datablocks write ftell=0x%lX", ftell(fs));
    printD("FS size: %ld",fat_file.first_block_offset + fat_file.data_blocks*BLOCK_SIZE);

    // make root dir empty
    if(fat_mkdir(&fat_file, NULL, NULL) != FAT_OK)
    {
        return CMD_CANNOT_CREATE_FILE;
    }

    curr_dir = malloc(sizeof(fat_dir_t));
    root_dir = malloc(sizeof(fat_dir_t));
    fat_load_dir_info(&fat_file, curr_dir, 0, NULL);
    fat_load_dir_info(&fat_file, root_dir, 0, NULL);

    return CMD_OK;
}

cmd_err_code_t cmd_mkdir(void *args)
{
    char *newdir_path = ((char **)args)[0];
    int i = strlen(newdir_path);

    //trim optional ending '/'
    newdir_path[i-1] = newdir_path[i-1]=='/'? 0 : newdir_path[i-1];

    char *newdir_name = strrchr(newdir_path,'/');
    *newdir_name = 0; // split into two strings
    // length of the name does not matter rn. It will be handeled in fat_mkdir

    fat_dir_t *cwd = malloc(sizeof(fat_dir_t));// dir struct for traversing
    memcpy(cwd, curr_dir, sizeof(fat_dir_t)); 
    if(!fat_goto_dir(&fat_file, &cwd, newdir_path) == FAT_OK)
    {
        free(cwd);
        return CMD_PATH_404;
    }

    for(i=0;i<cwd->fnum;i++)
    {
        if(!strncmp(newdir_name, cwd->files[i].name,FILENAME_SIZE))
        {
            return CMD_EXIST;
        }
    }

    i=fat_mkdir(&fat_file,cwd,newdir_name);
    switch(i)
    {
        case FAT_NO_MEM: free(cwd); return CMD_NO_MEM;
        case FAT_OK: break;
        default: free(cwd); return CMD_FAT_ERR;
    }

    free(cwd);
    return CMD_OK;
}

cmd_err_code_t cmd_lw(char *null)
{
    time_t t = fat_file.last_write;
    struct tm *tm = localtime(&t);
    char s[64];
    size_t ret = strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}
cmd_err_code_t cmd_lr(char *null)
{
    time_t t = fat_file.last_read;
    struct tm *tm = localtime(&t);
    char s[64];
    size_t ret = strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}




// utility functions
cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    for(size_t i=0; command_arr[i].id; i++)
        if(!strcmp(cmd_id, command_arr[i].id))
            return command_arr[i].callback(args);

    return CMD_UNKNOWN;
}


char *cmd_err_msgs[] = {
    "Success.",
    "File not found.",
    "Path does not exist.",
    "Already exists.",
    "Directory not empty.",
    "Format failed.",
    "Out of memory.",
    "Invalid argument",
    "Filesystem error.",
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