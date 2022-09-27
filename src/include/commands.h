#ifndef KIV_ZOS_FAT_FS_CMDS
#define KIV_ZOS_FAT_FS_CMDS

typedef enum {
    CMD_OK = 0

} cmd_err_code_t;

typedef struct {
    char *id;
    cmd_err_code_t (*callback)(void *);
} fat_shell_cmd_t;

cmd_err_code_t cmd_cd(void *);

#endif