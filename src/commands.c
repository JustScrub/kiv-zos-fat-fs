#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <string.h>


const static fat_shell_cmd_t command_arr[] = {
    {
        .id = "cd",
        .callback = cmd_cd
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

cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    for(size_t i=0; command_arr[i].id; i++)
        if(!strcmp(cmd_id, command_arr[i].id))
            return command_arr[i].callback(args);

    return CMD_UNKNOWN;
}