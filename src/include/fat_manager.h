#ifndef KIV_ZOS_FAT_FS_MANAGER
#define KIV_ZOS_FAT_FS_MANAGER
#include <stdio.h>
#include <time.h>

#define DEBUG

#ifdef DEBUG
#define printD(format, ...) printf("DEBUG: " format "\n", __VA_ARGS__)
#else
#define printD(...) ;
#endif


#define PWD_MAX_LEN 256
#define BLOCK_SIZE 1024 // 1KB blocks, must be at least as big as metadata!

#define FAT_FREE (unsigned)(-1)
#define FAT_EOF  (unsigned)(-2)
#define FAT_ERR  (unsigned)(-3)

#define MAGIC_VAL "MLADY_FS"

typedef unsigned dblock_idx_t;

/*  METRIC              FORMULA                         UNITS           COMMENT
    size of datablocks: N  (=input to format command)   bytes
    block size:         BS (=BLOCK_SIZE)                bytes
    idx len:            IS (=sizeof(dblock_idx_t))      bytes

    data_blocks:        DB = ceil(N/BS)                 blocks

    fat len:            FL = DB                         elements
    fat size:           FS = (IS * DB)                  bytes
    metadata size:      
    overhead size:      OS = MS + FS                    bytes

    all size:           AS = OS + DB*BS                 bytes

    max blocks:         MaxB = (1<<(IS*8))-1            bytes           so that the dblock index can hold the value
    max size:           MaxS = MB*BS                    bytes
*/

/*
    METADATA
    magic sequence: MLADY_FS
    BS
    DB
    FS
    fseek position of the first datablock (=OS), also first byte of the root directory, type long
    timestamps:
        last open
        last modify

    size: 44
     - sum of: {
          strlen("MLADY_FS"),        // magic seq
          sizeof(int),               // BS
          sizeof(int),               // DB
          sizeof(int),               // FS
          sizeof(int),               // OS
          sizeof(time_t),            // last open
          sizeof(time_t),            // last write
          }

    The FAT table follows immediately!
*/
#define METADATA_SIZE 40
#define DIR_NAME_SIZE 12

typedef struct
{
    int block_size;       /**< Size of one block */
    int data_blocks;                /**< Number of data blocks */
    int fat_size;                   /**< The actual size of the fat, might not be divisible by block_size */
    int first_block_offset;         /**< Offset in bytes of the first block (root dir), =OS*/

    time_t last_read;               /**< Last time outcp called*/
    time_t last_write;              /**< Last time incp  called*/

    char *fs_file;                  /**< Path to the FS file */
    char pwd[PWD_MAX_LEN];          /**< Current working directory within the FS*/

    dblock_idx_t *FAT;             /**< The FAT table*/
} fat_info_t;

typedef enum
{
    FAT_OK = 0,
    FAT_BAD_FORMAT,
    FAT_FILE_404,
    FAT_ERR_CRITICAL    /**< critical error, must reformat the FS. Some data might be possible to retrieve */
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
fat_manag_err_code_t fat_load_info(char *fat_file, fat_info_t *info);

fat_manag_err_code_t fat_write_info(fat_info_t *info);

fat_manag_err_code_t fat_info_create(fat_info_t *info, unsigned long size);

/**
 * @brief Move the FS file pointer to the begginig of the specified data block
 * 
 * @param dblock_num The data block to go to
 * @return fat_manag_err_code_t 
 */
fat_manag_err_code_t fat_goto_cluster(fat_info_t *fat, dblock_idx_t dblock_num);

#endif