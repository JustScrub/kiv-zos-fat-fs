#include "include/commands.h"
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


cmd_err_code_t cmd_cd(void *arg){
    char *path = (char *)arg;
    if(!path) return CMD_PATH_404;
    
    printf("cd %s", path);
    return CMD_OK;
}

cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    for(size_t i=0; command_arr[i].id; i++)
        if(!strcmp(cmd_id, command_arr[i].id))
            return command_arr[i].callback(args);

    return CMD_UNKNOWN;
}