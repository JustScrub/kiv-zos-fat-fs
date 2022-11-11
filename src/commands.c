#include "include/commands.h"
#include "include/fat_manager.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


const static fat_shell_cmd_t command_arr[] = {
    {
        .id = "cd",
        .callback = cmd_cd
    },
    {
        .id = "ls",
        .callback = cmd_ls
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
        .id = "format",
        .modifying = CMD_MODIFYING,
        .callback = cmd_format
    },
    {
        .id = "clear",
        .callback = cmd_clear
    },
    {
        .id = "mkdir",
        .modifying = CMD_MODIFYING,
        .callback = cmd_mkdir
    },
    {
        .id = "rmdir",
        .modifying = CMD_MODIFYING,
        .callback = cmd_rmdir
    },
    {
        .id = "incp",
        .modifying = CMD_MODIFYING,
        .callback = cmd_incp
    },
    {
        .id = "outcp",
        .callback = cmd_outcp
    },
    {
        .id = "lr",
        .callback = cmd_lr
    },
    {
        .id = "lw",
        .callback = cmd_lw
    },

    {
        .id = NULL,
        .callback = NULL
    }
};

const static cmd_err_code_t fat_to_cmd_err[] = {
    [FAT_OK] = CMD_OK,
    [FAT_FILE_404] = CMD_FILE_404,
    [FAT_PATH_404] = CMD_PATH_404,
    [FAT_NOT_EMPTY] = CMD_NOT_EMPTY,
    [FAT_EXIST] = CMD_EXIST,
    [FAT_NO_MEM] = CMD_NO_MEM,
    [FAT_FPTR_ERR] = CMD_FAT_ERR,
    [FAT_ERR_CRITICAL] = CMD_FAT_ERR
};

#define trim_slash(path, len) do{path[len-1] = path[len-1] == '/'? 0 : path[len-1]; len--;}while(0)

fat_info_t fat_file = {0};
fat_dir_t  *curr_dir = NULL,  //cache the cwd and the root directory
           *root_dir = NULL;

void init_dir_cache()
{
    if(curr_dir) free(curr_dir);
    if(root_dir) free(root_dir);
    curr_dir = malloc(sizeof(fat_dir_t));
    root_dir = malloc(sizeof(fat_dir_t));
    fat_load_dir_info(&fat_file, curr_dir, 0, NULL);
    fat_load_dir_info(&fat_file, root_dir, 0, NULL);
    bzero(fat_file.pwd,PWD_MAX_LEN);
}
void update_cache()
{
    fat_load_dir_info(&fat_file, curr_dir, curr_dir->idx, NULL);
    fat_load_dir_info(&fat_file, root_dir, 0, NULL);
}

char set_fat_info(char *fat_file_path)
{
    fat_manag_err_code_t err;
    fat_file.fs_file = fat_file_path;
    if((err = fat_load_info(fat_file_path, &fat_file)) != FAT_OK)
    {
        return err;
    }
    init_dir_cache();
    return 0;
}

void save_fat_info()
{
    fat_write_info(&fat_file);
}


void color_print(ansi_color_t color){
    printf("\e[%im",color);
}

/**
 * @brief Resolve the path starting from \c cwd . Does not check if those paths exist.
 * If \c path starts with '/', \c cwd is cleared (since we start at the root directory)
 * @param cwd will get updated. MUST BEGIN WITH '/' OR BE EMPTY, else it does nothing.
 * @param path 
 */
void resolve_path(char *cwd, char *path)
{
    printD("resolve_path: cwd=%s,path=%s",cwd,path);
    if(*path=='/')
    {
        *cwd = 0;
        path++;
    }
    if(*cwd && *cwd-'/') return; // cwd must be empty or begin with '/'

    char bfr[FILENAME_SIZE+1];
    for(;;)
    {
        bzero(bfr, FILENAME_SIZE+1);
        consume_path_part(&path, bfr);
        printD("resolve_path: path=%s, bfr=%s", path, bfr);
        if(!*bfr) break;   // nothing consumed -> end it
        if(*path) path++;  // can only be null term or '/' -> consume the '/'

        if(!strcmp(bfr,".")) continue; // '.' points to the same file...
        if(!strcmp(bfr, ".."))
        {
            if(*cwd) *strrchr(cwd,'/') = 0; // ".." is parent dir, therefore "delete" the last subdir from the string
            continue;
        }

        sprintf(cwd+strlen(cwd), "/%s",bfr); // append whatever path is there
        printD("resolve_path: new_cwd=%s",cwd);
    }
    
}

cmd_err_code_t cmd_cd(void *arg){
    char *path = ((char **)arg)[0];
    if(!path) return CMD_PATH_404;
    int pwdlen = strlen(fat_file.pwd);
    trim_slash(path,pwdlen);

    switch (fat_goto_dir(&fat_file, curr_dir, path))
    {
        case FAT_PATH_404: return CMD_PATH_404;
        default: break;
    }
    resolve_path(fat_file.pwd, path);
    printD("cmd_cd: new_pwd=%s,path=%s",fat_file.pwd, path);
    return CMD_OK;
}

cmd_err_code_t cmd_pwd(void *args)
{
    printf(fat_file.pwd);
    return CMD_OK;
}

cmd_err_code_t cmd_ls(void *args)
{
    char *path = ((char **)args)[0];
    if(!path)
    {
        path = alloca(2);
        strcpy(path, ".");
    }

    fat_dir_t *cwd = malloc(sizeof(fat_dir_t));// dir struct for traversing
    memcpy(cwd, curr_dir, sizeof(fat_dir_t));

    switch (fat_goto_dir(&fat_file,cwd,path))
    {
    case FAT_PATH_404: return CMD_PATH_404;
    default: break;
    }

    for(int i=0; i<cwd->fnum; i++)
    {
        if(cwd->files[i].type == FTYPE_DIR)
        {
            color_print(ANSI_YELLOW);
            printf("%s ",cwd->files[i].name);
            color_print(ANSI_RST);
            continue;
        }
        printf("%s ",cwd->files[i].name);
    }
    printf("\n");
    free(cwd);
    return CMD_OK;
}

cmd_err_code_t cmd_load(void *args)
{
    char *batch_path = ((char **)args)[0];
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

cmd_err_code_t cmd_format(void *args)
{
    char *arg = ((char **)args)[0];
    char *units, *valid_units[] = {"", "B", "KB", "MB", "GB"};
    unsigned long size;

    size = strtoul(arg, &units, 10);
    printD("size=%lu,units=%s", size,units);
    if(size <=0 )
    {
        return CMD_INV_ARG;
    }

    int i;
    for(i=0; i<4 && strcmp(units, valid_units[i]);i++) ;
    if(i > 4) return CMD_INV_ARG;
    if(units[0]) size <<= (10*(i-1));
    printD("demanded size: %lu B", size);

    // fill the structure with new metadata
    fat_info_create(&fat_file, size);
    printD("actual size: %d B", fat_file.data_blocks*BLOCK_SIZE);
    printD("BS=%d,DB=%d,FS=%d,OFF=0x%X,fs=%s", fat_file.block_size,fat_file.data_blocks,fat_file.fat_size,fat_file.first_block_offset,fat_file.fs_file);

    // trim the file to 0 or create it
    FILE *fs = fopen(fat_file.fs_file, "wb");
    fclose(fs);

    // write the info
    switch (fat_write_info(&fat_file)){
        case FAT_FILE_404: return CMD_FILE_404;
        case FAT_ERR_CRITICAL: return CMD_CANNOT_CREATE_FILE;
        default: break;
    }

    // pad the rest of the FS with zeros (only data blocks are remaining now)
    fs = fopen(fat_file.fs_file, "ab");
    for(unsigned long i = 0; i < fat_file.data_blocks * BLOCK_SIZE; i++)
    {
        fputc(0, fs);
    }
    printD("datablocks write ftell=0x%lX", ftell(fs));
    printD("FS size: %ld",fat_file.first_block_offset + (long)fat_file.data_blocks*BLOCK_SIZE);
    fclose(fs);

    // make root dir empty
    if(fat_mkdir(&fat_file, NULL, NULL) != FAT_OK)
    {
        return CMD_CANNOT_CREATE_FILE;
    }
    init_dir_cache();

    return CMD_OK;
}

cmd_err_code_t traverse_to_parent(char *whole_path, char **name, fat_dir_t **cwd)
{
    int i = strlen(whole_path);

    //trim optional ending '/'
    trim_slash(whole_path, i);

    *name = strrchr(whole_path,'/'); // mkdir in cwd -> this returns NULL (there is no path, only dir name)
    if(*name){
        **name = 0; // split into two strings
        (*name)++;
    }
    else *name = whole_path;
    // length of the name does not matter rn. It will be handeled in fat_mkdir
    printD("traverse_to_parent: path=%s,dname=%s",whole_path,*name);

    *cwd = malloc(sizeof(fat_dir_t));// dir struct for traversing
    memcpy(*cwd, curr_dir, sizeof(fat_dir_t)); 

    if(*name != whole_path) //there is path and the name
    if(fat_goto_dir(&fat_file, *cwd, whole_path) != FAT_OK) // -> traverse to that path to make the dir there
    {
        free(*cwd);
        return CMD_PATH_404;
    }
    return CMD_OK;
}

cmd_err_code_t cmd_mkdir(void *args)
{
    char *newdir_path = ((char **)args)[0];
    char *newdir_name = NULL;
    fat_dir_t *cwd = NULL;
    int i;
    if(traverse_to_parent(newdir_path, &newdir_name, &cwd) != CMD_OK)
    {
        return CMD_PATH_404;
    }

    i=fat_mkdir(&fat_file,cwd,newdir_name);
    free(cwd);
    return fat_to_cmd_err[i];
}

cmd_err_code_t cmd_rmdir(void *args)
{
    char *rmdir_path = ((char **)args)[0];
    char *rmdir_name = NULL;
    fat_dir_t *cwd = NULL;
    int i;
    if(traverse_to_parent(rmdir_path, &rmdir_name, &cwd) != CMD_OK)
    {
        return CMD_PATH_404;
    }

    i = fat_remove_file(&fat_file, cwd, rmdir_name);

    free(cwd);
    return fat_to_cmd_err[i];
}

cmd_err_code_t cmd_rm(void *args)
{
    char *rmf_path = ((char **)args)[0];
    char *rmf_name = NULL;
    fat_dir_t *cwd = NULL;
    int i;
    if(traverse_to_parent(rmf_path, &rmf_name, &cwd) != CMD_OK)
    {
        return CMD_PATH_404;
    }

    i = fat_remove_file(&fat_file, cwd, rmf_name);

    free(cwd);
    return i;
}

cmd_err_code_t cmd_incp(void *args)
{
    char *outf =  ((char **)args)[0];
    char *inf  =  ((char **)args)[1];
    char *inf_name;

    fat_dir_t *cwd = NULL;
    if(traverse_to_parent(inf, &inf_name, &cwd) != CMD_OK || inf_name[strlen(inf_name)-1] == '/')
    {
        if(cwd) free(cwd);
        return CMD_PATH_404;
    }

    if(cwd->fnum >= DDIR_LEN)
    {
        free(cwd);
        return CMD_NO_MEM;
    }

    for(int i=0; i<cwd->fnum; i++)
    {
        if(!strcmp(inf_name,cwd->files[i].name))
        {
            free(cwd);
            return CMD_EXIST;
        }
    }

    FILE *cpyf = fopen(outf, "rb+");
    if(!cpyf)
    {
        free(cwd);
        return CMD_FILE_404;
    }
    
    dblock_idx_t idx = fat_get_free_cluster(&fat_file);
    printD("incp start_idx=%d",idx);
    if(idx == FAT_ERR)
    {
        fclose(cpyf); free(cwd);
        return CMD_NO_MEM;
    }
    fat_file.FAT[idx] = FAT_EOF;
    dblock_idx_t start = idx;
    FILE *fs = fat_copen(&fat_file, idx, CLUSTER_WRITE);

    char *cluster_cont = malloc(BLOCK_SIZE*sizeof(char));
    int read_bytes;
    long fsize = 0;

    while(1)
    {
        bzero(cluster_cont,BLOCK_SIZE);
        read_bytes = fread(cluster_cont, 1, BLOCK_SIZE, cpyf);
        fsize += read_bytes;
        fwrite(cluster_cont, 1, read_bytes, fs);
        if(ungetc(getc(cpyf), cpyf) == EOF || read_bytes < BLOCK_SIZE) break;

        fat_file.FAT[idx] = fat_get_free_cluster(&fat_file);
        //printD("incp: loop idx=%d,next_idx=%d",idx, fat_file.FAT[idx]);
        idx = fat_file.FAT[idx];
        if( idx  == FAT_ERR)
        {
            fclose(fs); fclose(cpyf);
            free(cwd); free(cluster_cont);
            return CMD_NO_MEM;
        }
        fat_file.FAT[idx] = FAT_EOF; //not to obtain it in next iter
        fat_cseek(&fat_file, fs, idx);
    }
    fclose(cpyf);
    //fat_file.FAT[idx] = FAT_EOF;

    fat_cseek(&fat_file,fs, cwd->idx);
    cwd->fnum++;
    fwrite(&cwd->fnum, sizeof(int), 1, fs);
    fseek(fs, (cwd->fnum-1)*FINFO_SIZE,SEEK_CUR);
    fat_file_info_t newinfo = {
        .size = fsize,
        .type = FTYPE_FILE,
        .start = start
    };
    strncpy(newinfo.name, inf_name, FILENAME_SIZE);
    fwrite(&newinfo, FINFO_SIZE, 1, fs);

    fclose(fs);
    free(cwd);
    return CMD_OK;
}

cmd_err_code_t cmd_outcp(void *args)
{
    char *inf =  ((char **)args)[0];
    char *inf_name;
    char *outf  =  ((char **)args)[1];

    fat_dir_t *cwd = NULL;
    if(traverse_to_parent(inf, &inf_name, &cwd) != CMD_OK || inf_name[strlen(inf_name)-1] == '/')
    {
        if(cwd) free(cwd);
        return CMD_PATH_404;
    }

    dblock_idx_t idx = FAT_ERR;
    unsigned long fsize;

    for(int i=0; i<cwd->fnum; i++)
    {
        if(!strcmp(inf_name,cwd->files[i].name))
        {
            idx = cwd->files[i].start;
            fsize = cwd->files[i].size;
            break;
        }
    }
    if(idx == FAT_ERR)
    {
        free(cwd);
        printD("outcp not_found_in cwd_idx=%i",cwd->idx);
        return CMD_FILE_404;
    }

    FILE *cpyf = fopen(outf, "wb");
    if(!cpyf) return CMD_FILE_404;
    FILE *fs = fat_copen(&fat_file, idx, CLUSTER_READ);
    char *bfr = malloc(BLOCK_SIZE);

    while(1)
    {
        bzero(bfr, BLOCK_SIZE);
        fread(bfr, 1, BLOCK_SIZE, fs);
        fwrite(bfr, 1, (fsize/BLOCK_SIZE)? BLOCK_SIZE : (fsize%BLOCK_SIZE), cpyf );
        fsize -= BLOCK_SIZE;
        idx = fat_file.FAT[idx];
        if((long)fsize <= 0 || idx == FAT_EOF) break;
        fat_cseek(&fat_file, fs, idx);
    }

    fclose(cpyf); fclose(fs);
    free(cwd); free(bfr);

    return CMD_OK;

}

cmd_err_code_t cmd_lw(void *null)
{
    time_t t = fat_file.last_write;
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}
cmd_err_code_t cmd_lr(void *null)
{
    time_t t = fat_file.last_read;
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);
    printf("%s\n", s);
    return CMD_OK;
}




// utility functions
cmd_err_code_t cmd_exec(char *cmd_id, void *args){
    cmd_err_code_t err;
    const fat_shell_cmd_t *cmd;
    for(cmd = command_arr; cmd->id; cmd++)
        if(!strcmp(cmd_id, cmd->id))
        {
            err = cmd->callback(args);
            if(cmd->modifying)
            {
                update_cache();
                fat_write_info(&fat_file);
                printD("cmd_exec: modifying=%s",cmd->id);
            }
            return err;
        }

    return CMD_UNKNOWN;
}


char *cmd_err_msgs[] = {
    "Success.",
    "File not found.",
    "Path does not exist.",
    "Already exists.",
    "Directory not empty.",
    "Format failed.",
    "Out of memory.",
    "Invalid argument",
    "Filesystem error.",
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
    cmd_err_code_t ret = CMD_OK;

    fgets(bfr, bfr_len-1, from);
    bfr[strcspn(bfr, "\r\n")] = 0; // delete newline character
    printD("bfr=%s", bfr);
    if(from != stdin) printf("%s\n",bfr);

    if(!(cmd = strtok(bfr, " "))) return CMD_OK; // no command (empty line) -> ignore
    args[0] = strtok(NULL, " "); // first argument, can be string or NULL
    args[1] = strtok(NULL, " "); // second argument, can be string or NULL
    printD("cmd=%s, arg1=%s, arg2=%s", cmd, args[0]?args[0]:"NULL",args[1]?args[1]:"NULL");

    ret = cmd_exec(cmd, (void*) args);
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