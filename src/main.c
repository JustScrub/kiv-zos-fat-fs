#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG


char bfr[256] = {0};


void main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fputs("Usage: <program name> <path/to/FS_file>\nFile System file not specified.\n", stderr);
        exit(1);
    }

    fat_info_t fat_info = {0};
    sprintf(fat_info.pwd,"/%s",argv[1]);

    FILE *the_fs = fopen(argv[1], "wb+");

    switch (fat_load_info(the_fs, &fat_info))
    {
    case FAT_BAD_FORMAT:
        fputs("Warning: The input file is not formatted properly (newly created or corrupted)"
              "Please, use the format command.\n", stdout);
        break;
    
    case FAT_TABLE_DISAG:
        fat_restore_table_disag(&fat_info);
        break;

    default:
        break;
    }

    set_fat_info(&fat_info);
    cmd_err_code_t err;

    for(;;)
    {
        printf("\n");
        color_print(ANSI_GREEN);
        cmd_pwd(NULL);
        color_print(ANSI_BLUE);
        fputs(" $: \n", stdout);
        color_print(ANSI_RST);

        bzero(bfr, 256);
        err = load_cmd(stdin,bfr,256);
    }
}