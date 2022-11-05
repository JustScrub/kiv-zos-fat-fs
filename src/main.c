#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

char bfr[256] = {0};

void sigint_handler(int signum)
{
    save_fat_info();

    color_print(ANSI_RED);
    printf("\nSingal SIGINT caught. Exitting...\n");
    color_print(ANSI_RST);
    exit(130); // 130 = SIGINT
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fputs("Usage: <program name> <path/to/FS_file>\nFile System file not specified.\n", stderr);
        exit(1);
    }

    signal(SIGINT, sigint_handler);

    if(set_fat_info(argv[1]))
    {
        fputs("Warning: The input file is not formatted properly or does not exist."
              "Please, use the format command.\n", stdout);
    }

    cmd_err_code_t err;

    for(;;)
    {
        color_print(ANSI_GREEN);
        printf("\n%s", argv[1]);
        cmd_pwd(NULL);
        color_print(ANSI_BLUE);
        fputs(" $: \n", stdout);
        color_print(ANSI_RST);

        bzero(bfr, 256);
        err = load_cmd(stdin,bfr,256);
    }
    err--;
    return 0;
}