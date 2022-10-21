#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG

#ifdef DEBUG
#define printD(format, ...) printf("DEBUG: " format "\n", __VA_ARGS__)
#else
#define printD(...) ;
#endif

char bfr[256] = {0};

cmd_err_code_t load_cmd(FILE *from)
{
    char *args[2] = {0};
    char *cmd;
    void *vargs;

    bzero(bfr, 256);
    fgets(bfr, 255, from);
    bfr[strcspn(bfr, "\r\n")] = 0; // delete newline character
    printD("bfr=%s", bfr);

    if(!(cmd = strtok(bfr, " "))) return CMD_UNKNOWN; // no command -> continue
    args[0] = strtok(NULL, " "); // first argument, can be string or NULL
    args[1] = strtok(NULL, " "); // second argument, can be string or NULL
    printD("cmd=%s, arg1=%s, arg2=%s", cmd, args[0]?args[0]:"NULL",args[1]?args[1]:"NULL");

    if(!args[0]) vargs = NULL;                  //first arg not specified -> no args -> pass NULL
    else if(!args[1]) vargs = (void *)args[0];  //first arg OK, second missing -> pass first only
    else vargs = (void *)args;                  //both args specified -> pass arg array

    return cmd_exec(cmd, vargs);
}

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
        color_print(ANSI_GREEN);
        cmd_pwd(NULL);
        color_print(ANSI_BLUE);
        fputs(" $: \n", stdout);
        color_print(ANSI_RST);

        err = load_cmd(stdin);
    }
}