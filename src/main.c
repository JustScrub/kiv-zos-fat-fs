#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

char bfr[256] = {0};

void sigint_handler(int signum)
{
    color_print(ANSI_RED);
    printf("\nSingal SIGINT caught. Exitting...\n");
    color_print(ANSI_RST);
    exit(130); // 130 = SIGINT
}


void main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fputs("Usage: <program name> <path/to/FS_file>\nFile System file not specified.\n", stderr);
        exit(1);
    }

    signal(SIGINT, sigint_handler);

    fat_info_t fat_info = {
        .fs_file = argv[1]
    };

    switch (fat_load_info(argv[1], &fat_info))
    {
    case FAT_BAD_FORMAT:
        fputs("Warning: The input file is not formatted properly or does not exist."
              "Please, use the format command.\n", stdout);
        break;

    default:
        break;
    }

    set_fat_info(&fat_info);
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
}