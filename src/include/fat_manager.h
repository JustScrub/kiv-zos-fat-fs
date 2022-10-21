#ifndef KIV_ZOS_FAT_FS_MANAGER
#define KIV_ZOS_FAT_FS_MANAGER
#include <stdio.h>

#define PWD_MAX_LEN 256

typedef struct
{
    int block_len;
    int data_blocks;
    char *base_path;
    char pwd[PWD_MAX_LEN];
    int FAT[];
} fat_info_t;

typedef enum
{
    FAT_OK = 0,
    FAT_BAD_FORMAT,
    FAT_TABLE_DISAG
} fat_manag_err_code_t;

/**
 * @brief Loads information about the filesystem
 * 
 * @param fat_file The file representing the filesystem
 * @param[out] info The info structure to be filled
 * @return fat_manag_err_code_t 
 *  - FAT_OK: success
 *  - FAT_BAD_FORMAT: the file is not formatted properly.
 *  - FAT_TABLE_DISAG: the 2 FAT tables hold different info.
 */
fat_manag_err_code_t fat_load_info(FILE *fat_file, fat_info_t *info);

/**
 * @brief Let the user choose which copy of the FAT tables to use as the correct table
 * 
 * Should only be called after FAT_TABLE_DISAG error code.
 * 
 * @param fat 
 * @return fat_manag_err_code_t 
 */
fat_manag_err_code_t fat_restore_table_disag(fat_info_t *fat);

#endif