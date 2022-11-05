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
        .modifying = CMD_MODIFYING,
        .callback = cmd_format
    },
    {
        .id = "clear",
        .callback = cmd_clear
    },
    {
        .id = "mkdir",
        .modifying = CMD_MODIFYING,
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

#define trim_slash(path, len) do{path[len-1] = path[len-1] == '/'? 0 : path[len-1]; len--;}while(0)

fat_info_t fat_file;
fat_dir_t  *curr_dir = NULL,  //cache the cwd and the root directory
           *root_dir = NULL;

void init_dir_cache()
{
    if(curr_dir) free(curr_dir);
    if(root_dir) free(root_dir);
    curr_dir = malloc(sizeof(fat_dir_t));
    root_dir = malloc(sizeof(fat_dir_t));
    fat_load_dir_info(&fat_file, curr_dir, 0, NULL);
    fat_load_dir_info(&fat_file, root_dir, 0, NULL);
}
void update_cache()
{
    fat_load_dir_info(&fat_file, curr_dir, curr_dir->idx, NULL);
    fat_load_dir_info(&fat_file, root_dir, 0, NULL);
}

char set_fat_info(char *fat_file_path)
{
    fat_manag_err_code_t err;
    fat_file.fs_file = fat_file_path;
    if((err = fat_load_info(fat_file_path, &fat_file)) != FAT_OK)
    {
        return err;
    }
    init_dir_cache();
    return 0;
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
    trim_slash(path,pwdlen);

    fat_goto_dir(&fat_file, curr_dir, path);
    strcpy(fat_file.pwd+1, curr_dir->path);
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
    printD("demanded size: %lu B", size);

    // fill the structure with new metadata
    fat_info_create(&fat_file, size);
    printD("actual size: %d B", fat_file.data_blocks*BLOCK_SIZE);
    printD("BS=%d,DB=%d,FS=%d,OFF=0x%X,fs=%s", fat_file.block_size,fat_file.data_blocks,fat_file.fat_size,fat_file.first_block_offset,fat_file.fs_file);

    // trim the file to 0 or create it
    FILE *fs = fopen(fat_file.fs_file, "wb");
    fclose(fs);

    // write the info
    switch (fat_write_info(&fat_file)){
        case FAT_FILE_404: return CMD_FILE_404;
        case FAT_ERR_CRITICAL: return CMD_CANNOT_CREATE_FILE;
        default: break;
    }

    // pad the rest of the FS with zeros (only data blocks are remaining now)
    fs = fopen(fat_file.fs_file, "ab");
    for(unsigned long i = 0; i < fat_file.data_blocks * BLOCK_SIZE; i++)
    {
        fputc(0, fs);
    }
    printD("datablocks write ftell=0x%lX", ftell(fs));
    printD("FS size: %ld",fat_file.first_block_offset + (long)fat_file.data_blocks*BLOCK_SIZE);
    fclose(fs);

    // make root dir empty
    if(fat_mkdir(&fat_file, NULL, NULL) != FAT_OK)
    {
        return CMD_CANNOT_CREATE_FILE;
    }
    init_dir_cache();

    return CMD_OK;
}

cmd_err_code_t cmd_mkdir(void *args)
{
    char *newdir_path = ((char **)args)[0];
    int i = strlen(newdir_path);

    //trim optional ending '/'
    trim_slash(newdir_path, i);

    char *newdir_name = strrchr(newdir_path,'/'); // mkdir in cwd -> this returns NULL (there is no path, only dir name)
    if(newdir_name){
        *newdir_name = 0; // split into two strings
        newdir_name++;
    }
    else newdir_name = newdir_path;
    // length of the name does not matter rn. It will be handeled in fat_mkdir
    printD("cmd_mkdir: path=%s,dname=%s",newdir_path,newdir_name);

    fat_dir_t *cwd = malloc(sizeof(fat_dir_t));// dir struct for traversing
    memcpy(cwd, curr_dir, sizeof(fat_dir_t)); 

    if(newdir_name != newdir_path) //there is path and the name
    if(fat_goto_dir(&fat_file, cwd, newdir_path) != FAT_OK) // -> traverse to that path to make the dir there
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
    free(cwd);
    return i;
}

cmd_err_code_t cmd_lw(void *null)
{
    time_t t = fat_file.last_write;
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}
cmd_err_code_t cmd_lr(void *null)
{
    time_t t = fat_file.last_read;
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}




// utility functions
cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    cmd_err_code_t err;
    const fat_shell_cmd_t *cmd;
    for(cmd = command_arr; cmd->id; cmd++)
        if(!strcmp(cmd_id, cmd->id))
        {
            err = cmd->callback(args);
            if(cmd->modifying)
            {
                update_cache();
                fat_write_info(&fat_file);
                printD("cmd_exec: modifying=%s",cmd->id);
            }
            return err;
        }

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