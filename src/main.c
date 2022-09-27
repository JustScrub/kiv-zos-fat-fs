#include "include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char bfr[256] = {0};

void main()
{
    char *args[2] = {0};
    char *cmd;
    void *vargs;
    cmd_err_code_t err;

    for(;;)
    {
        fgets(bfr, 256, stdin);
        bfr[strcspn(bfr, "\r\n")] = 0; // delete newline character
        printf("DEBUG: bfr=%s\n", bfr);

        if(!(cmd = strtok(bfr, " "))) continue; // no command -> continue
        args[0] = strtok(NULL, " "); // first argument, can be string or NULL
        args[1] = strtok(NULL, " "); // second argument, can be string or NULL

        printf("DEBUG: cmd=%s, arg1=%s, arg2=%s\n", cmd, args[0]?args[0]:"NULL",args[1]?args[1]:"NULL");

        if(!args[0]) vargs = NULL;                  //first arg not specified -> no args -> pass NULL
        else if(!args[1]) vargs = (void *)args[0];  //first arg OK, second missing -> pass first only
        else vargs = (void *)args;                  //both args specified -> pass arg array

        err = cmd_exec(cmd, vargs);
        if(err) printf("Error: %x\n", err);
    }
}