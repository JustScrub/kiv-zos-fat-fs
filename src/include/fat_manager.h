#ifndef KIV_ZOS_FAT_FS_MANAGER
#define KIV_ZOS_FAT_FS_MANAGER
#include <stdio.h>

#define PWD_MAX_LEN 256
#define BLOCK_SIZE 1024 // 1KB blocks, must be at least as big as metadata!

#define FAT_FREE (unsigned)(-1)
#define FAT_EOF  (unsigned)(-2)
#define FAT_ERR  (unsigned)(-3)

typedef unsigned dblock_idx_t;

/*  METRIC              FORMULA                         UNITS           COMMENT
    size of datablocks: N  (=input to format command)   bytes
    block size:         BS (=BLOCK_SIZE)                bytes
    idx len:            IS (=sizeof(dblock_idx_t))      bytes

    data_blocks:        DB = floor(N/BS)                blocks

    fat len:            FL = DB                         elements
    fat size:           FS = (IS * DB)                  bytes
    fat_blocks:         FB = ceil(FS/BS)                blocks

    metadata blocks:    1                               block
    overhead blocks:    OB = FB + 1                     blocks
    overhead size:      OS = OB * BS = FB*BS + BS       bytes

    all blocks:         AB = 1  + FB + DB               blocks
    all size:           AS = BS + FB*BS + DB*BS         bytes

    max blocks:         MB = (1<<(IS*8))-1              bytes           so that the dblock index can hold the value
    max size:           MS = MB*BS                      bytes
*/

/*
    METADATA
    magic sequence: MLADY_FS
    BS
    DB
    FB
    FS
    fseek position of the first datablock (=OS), also first byte of the root directory, type long
    timestamps:
        last open
        last modify
*/

typedef struct
{
    unsigned long block_size;       /**< Size of one block */
    int data_blocks;                /**< Number of data blocks */
    int fat_blocks;                 /**< Number of data blocks used by the FAT table (continuous) */
    int fat_size;                   /**< The actual size of the fat, might not be divisible by block_size */

    int first_block_offset;         /**< Offset in bytes of the first block (root dir), =OS*/

    char *fs_file;                  /**< Path to the FS file */
    char pwd[PWD_MAX_LEN];          /**< Current working directory within the FS*/

    dblock_idx_t *FAT;             /**< The FAT table*/
} fat_info_t;

typedef enum
{
    FAT_OK = 0,
    FAT_BAD_FORMAT
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

/**
 * @brief Move the FS file pointer to the begginig of the specified data block
 * 
 * @param dblock_num The data block to go to
 * @return fat_manag_err_code_t 
 */
fat_manag_err_code_t fat_goto_cluster(fat_info_t *fat, dblock_idx_t dblock_num);

#endif