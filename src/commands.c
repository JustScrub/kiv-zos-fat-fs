#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


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
    char *path = (char *)arg;
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
    char *batch_path = (char *)args;
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








// utility functions
char *cmd_err_msgs[] = {
    "Success.",
    "File not found.",
    "Path does not exist.",
    "Already exists.",
    "Directory not empty.",
    "Format failed.",
    "Out of memory.",
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
    void *vargs;
    cmd_err_code_t ret = CMD_OK;

    fgets(bfr, bfr_len-1, from);
    bfr[strcspn(bfr, "\r\n")] = 0; // delete newline character
    printD("bfr=%s", bfr);

    if(!(cmd = strtok(bfr, " "))) return CMD_OK; // no command (empty line) -> ignore
    args[0] = strtok(NULL, " "); // first argument, can be string or NULL
    args[1] = strtok(NULL, " "); // second argument, can be string or NULL
    printD("cmd=%s, arg1=%s, arg2=%s", cmd, args[0]?args[0]:"NULL",args[1]?args[1]:"NULL");

    if(!args[0]) vargs = NULL;                  //first arg not specified -> no args -> pass NULL
    else if(!args[1]) vargs = (void *)args[0];  //first arg OK, second missing -> pass first only
    else vargs = (void *)args;                  //both args specified -> pass arg array

    ret = cmd_exec(cmd, vargs);
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