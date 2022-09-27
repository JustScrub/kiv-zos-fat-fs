#include "include/commands.h"
#include <stdio.h>


const fat_shell_cmd_t command_arr[] = {
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

    return CMD_OK;
}
