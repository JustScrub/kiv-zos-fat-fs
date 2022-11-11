#ifndef KIV_ZOS_FAT_FS_CMDS
#define KIV_ZOS_FAT_FS_CMDS

#include "fat_manager.h"


/**
 * @brief Enumeration of possible error codes
 */
typedef enum {
    CMD_OK = 0,                 /**< Command successful */
    CMD_FILE_404,               /**< File not found */
    CMD_PATH_404,               /**< Path not found */
    CMD_EXIST,                  /**< Already exists (e.g. directory) */
    CMD_NOT_EMPTY,              /**< Not empty (e.g. directory) */
    CMD_CANNOT_CREATE_FILE,     /**< Cannot create (format to specified size) the FS file */
    CMD_NO_MEM,                 /**< No free memory in the file system */
    CMD_INV_ARG,                /**< Invalid argument */
    CMD_FAT_ERR,                /**< Other error within the FAT FS*/
    CMD_RM_CURR,

    CMD_UNKNOWN                 /**< Unknown command */
} cmd_err_code_t;

#define CMD_MODIFYING 1
#define CMD_READING   0
typedef struct {
    char *id;
    char modifying;
    cmd_err_code_t (*callback)(void *args);
} fat_shell_cmd_t;

typedef enum
{
    ANSI_RST = 0,
    ANSI_RED = 31,
    ANSI_GREEN,
    ANSI_YELLOW,
    ANSI_BLUE,
    ANSI_MAGENTA,
    ANSI_CYAN,
    ANSI_WHITE
} ansi_color_t;

void color_print(ansi_color_t color);
char set_fat_info(char *fs_file);
void save_fat_info();

/**
 * @brief Prints the error message for the given error code
 * @param err 
 */
void pcmderr(cmd_err_code_t err);

/**
 * @brief Loads and executes command from the specified file. Prints error if one occurs.
 * 
 * @param from The file from which to read the command. Typically, this is \c stdin .
 * @param bfr (optional) the buffer to use inside this function. Use when calling this function multiple times not to allocate memory for each call. If NULL, the function allocates the buffer itself.
 * @param bfr_len (optional) length of the buffer. If \c bfr is NULL, this parameter is ignored.
 * @return cmd_err_code_t 
 */
cmd_err_code_t load_cmd(FILE *from, char *bfr, int bfr_len);

/**
 * @brief Copy files. 
 * 
 * Example: cp s1 s2
 * 
 * On success, writes OK
 * 
 * copies file s1 to file s2
 * 
 * @param args char * array with the specified paths (must be of size 2)
 * @return cmd_err_code_t: 
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 *  - CMD_PATH_404: Path not found 
 */
cmd_err_code_t cmd_cp(void *args);

/**
 * @brief Move files. 
 * 
 * Example: mv s1 s2
 * 
 * On success, writes OK
 * 
 * moves (or renames) file s1 to file s2
 * 
 * @param args char * array with the specified paths (must be of size 2)
 * @return cmd_err_code_t: 
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 *  - CMD_PATH_404: Path not found 
 */
cmd_err_code_t cmd_mv(void *args);

/**
 * @brief Removes a file
 * 
 * Example: rm s1
 * 
 * On success, writes OK
 * 
 * @param args char *, the path of the file
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 */
cmd_err_code_t cmd_rm(void *args);

/**
 * @brief Creates a directory
 * 
 * Example: mkdir a1
 * 
 * On success, writes OK
 * 
 * @param args char *, the path and name of the directory (can end with '/')
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_PATH_404: Path not found
 *  - CMD_EXIST: Such directory already exists
 */
cmd_err_code_t cmd_mkdir(void *args);

/**
 * @brief Removes a directory
 * 
 * Example: rmdir a1
 * 
 * On success, writes OK
 * 
 * @param args char *, the path and name of the directory (can end with '/')
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 *  - CMD_NOT_EMPTY: The directory is not empty
 */
cmd_err_code_t cmd_rmdir(void *args);

/**
 * @brief Lists contents of a directory
 * 
 * Example: 
 *  - ls a1
 *  - ls
 * 
 * Output: 
 * @code
 * FILE: f1, f2, ...
 * DIR: a2, a3, ...
 * @endcode
 * 
 * @param args char *, the path and name of the directory (can end with '/')
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_PATH_404: Path not found
 */
cmd_err_code_t cmd_ls(void *args);

/**
 * @brief Writes out contents of a file
 * 
 * Example: cat s1
 * 
 * @param args char *, the path and name of the file
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 */
cmd_err_code_t cmd_cat(void *args);

/**
 * @brief Changes current working directory
 * 
 * Example: cd a1
 *  * 
 * @param args char *, the path and name of the directory (can end with '/')
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_PATH_404: Path not found
 */
cmd_err_code_t cmd_cd(void *args);

/**
 * @brief Prints current working directory
 * 
 * @param args NULL
 * @return cmd_err_code_t CMD_OK (always)
 */
cmd_err_code_t cmd_pwd(void *args);

/**
 * @brief Writes info about a file/directory
 * 
 * The info states which clusters the file/directory occupies
 * 
 * Example: info a1
 * 
 * @param args char *, the path and name of the directory (can end with '/')
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 */
cmd_err_code_t cmd_info(void *args);

/**
 * @brief Copies a file from the hard drive to the File system
 * 
 * Example: incp s1 s2
 * 
 * On success, writes OK
 * 
 * @param args char * array, first element is the hard drive path, second is the path of the copied file
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 *  - CMD_PATH_404: Path not found
 *  - CMD_NO_MEM
 */
cmd_err_code_t cmd_incp(void *args);

/**
 * @brief Copies a file from the File system to the hard drive
 * 
 * Example: outcp s1 s2
 * 
 * On success, writes OK
 * 
 * @param args char * array, first element is the FS path, second is the path of the copied file onto hard drive
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 *  - CMD_PATH_404: Path not found
 */
cmd_err_code_t cmd_outcp(void *args);

/**
 * @brief Loads a "batch" file from the hard drive and executes it
 * 
 * The "batch" file includes possible known commands with arguments,
 * each command on its own line. The commands are executed in sequence
 * top to bottom.
 * 
 * Example: load s1
 * 
 * @param args char *, path to the "batch" file on the hard drive
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: File not found
 */
cmd_err_code_t cmd_load(void *args);

/**
 * @brief Formats a file to the format of the FS
 * 
 * The file to be formated is the one that was input as the parameter
 * to this program before its execution. If the file does not exist,
 * it will be created by this command. If the file existed, all its
 * data are lost.
 * 
 * Example: format 600MB
 * 
 * On success, writes OK
 * 
 * @param args char *, specifies size of the file system
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_CANNOT_CREATE_FILE: Cannot format.
 */
cmd_err_code_t cmd_format(void *args);

/**
 * @brief Defragments a file
 * 
 * Ensures that the file is layed out on clusters contiguously.
 * Can be checked by the \c info command. 
 * 
 * @warning This command does not change the layout of other files!
 *          There must be enough space on the FS so that the file 
 *          can be layed out contiguously 
 * 
 * On success, writes OK
 * 
 * @param args char *, path and name of the file
 * @return cmd_err_code_t:
 *  - CMD_OK: success
 *  - CMD_FILE_404: file not found
 *  - CMD_NO_MEM: the file cannot be layed out contiguously
 */
cmd_err_code_t cmd_defrag(void *args);

/**
 * @brief Clear the console 
 * 
 * @param args unused
 * @return CMD_OK
 */
cmd_err_code_t cmd_clear(void *args);

/**
 * @brief Prints the "last write" time - the last time incp command was invoked
 * 
 * @param null 
 * @return cmd_err_code_t 
 */
cmd_err_code_t cmd_lw(void *null);
/**
 * @brief Prints the "last read" time - the last time outcp command was invoked
 * 
 * @param null 
 * @return cmd_err_code_t 
 */
cmd_err_code_t cmd_lr(void *null);

/**
 * @brief Executes the specified command with the specified arguments
 * 
 * @param cmd_id the identification string of the command
 * @param args the argument, as specified with each command
 * @return cmd_err_code_t - whatever the command returns or CMD_UNKNOWN
 */
cmd_err_code_t cmd_exec(char *cmd_id, void *args);


#endif