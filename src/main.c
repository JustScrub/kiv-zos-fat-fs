#include "include/commands.h"
#include <stdio.h>

extern const fat_shell_cmd_t command_arr[];

void main()
{
    if(!command_arr[0].callback(NULL))
        printf("success");
    else
        printf("moron");
}