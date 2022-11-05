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

#define CLUSTER_WRITE "r+b"
#define CLUSTER_READ  "rb"

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

/* METADATA
    magic sequence: MLADY_FS
    BS
    DB
    FS
    fseek position of the first datablock (=OS), also first byte of the root directory, type long
    timestamps:
        last open
        last modify

    size: 40
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
#define FILENAME_SIZE 11

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
    FAT_PATH_404,
    FAT_NO_MEM,
    FAT_FPTR_ERR,       /**< error while handling the open FS FILE* */
    FAT_ERR_CRITICAL    /**< critical error, must reformat the FS. Some data might be possible to retrieve */
} fat_manag_err_code_t;


#define FTYPE_FILE 0x00U
#define FTYPE_DIR  0xFFU
/**
 * @brief The structure holding file info.
 * This struct is directly written to the directory file.
 * If the type is \c FTYPE_DIR then the size parameter is unused and should not be read. 
 * Directory size is calculated using the \c DIR_SIZE macro (see below)
 */
typedef struct file_info{
    char name[FILENAME_SIZE]; /**< file name */
    unsigned char type;       /**< file type*/
    dblock_idx_t start;       /**< file starting block */
    unsigned long size;       /**< file size*/
} fat_file_info_t;
#define FINFO_SIZE sizeof(fat_file_info_t)
/* optimal packing
     0 1 2 3 4 5 6 7
    +----------------+
    |     name       |
    +----------------+
    | name |t| start |
    +----------------+
    |      size      |
    +----------------+
*/


#define DDIR_LEN (BLOCK_SIZE-sizeof(int))/(sizeof(fat_file_info_t)) /**< Number of entries per directory */
#define DIR_SIZE(fnum) fnum*FINFO_SIZE + sizeof(int)
/**
 * @brief cached dir contents
 * STRUCTURE in FS
 *   fnum{fat_file_info_t... 31x}
 */
typedef struct
{
    dblock_idx_t idx;                       /**< The index of the cluster the directory begins at */
    int fnum;                               /**< Number of files in the directory. */
    fat_file_info_t files[DDIR_LEN];        /**< The first file in the directory (the '.' directory)*/
} fat_dir_t;
/* optimally packed as well ;-)
      0 1 2 3 4 5 6 7
    +----------------+
    | idx   | fnum   |
    +----------------+
    |fat_info_t 1    | x(3*42)
    +----------------+
*/

/**
 * @brief Loads information about the filesystem
 * 
 * @param fat_file The file representing the filesystem
 * @param[out] info The info structure to be filled
 * @return fat_manag_err_code_t 
 *  - FAT_OK: success \n
 *  - FAT_BAD_FORMAT: the file is not formatted properly. \n
 *  - FAT_TABLE_DISAG: the 2 FAT tables hold different info. \n
 */
fat_manag_err_code_t fat_load_info(char *fat_file, fat_info_t *info);

/**
 * @brief Writes the info contained in the \c info parameter to the first section of the FS file (overwrites the metadata and FAT table)
 * 
 * @param info 
 * @return fat_manag_err_code_t 
 *  - FAT_OK: Success \n
 *  - FAT_FILE_404: The file name of the FS file is incorrect; the FS file does not exist on the disc \n
 *  - FAT_ERR_CRITICAL: the number of bytes written is not the same as the metadata + FAT size in the \c info structure; metadata inconsistence \n
 */
fat_manag_err_code_t fat_write_info(fat_info_t *info);

/**
 * @brief Fills the fields of the \c info structure so that it conforms invoking the format size command
 *  \n
 * Does not fill the fs_file field of \c info since it must be known and must remain unchaged througout one run.
 * 
 * @param info 
 * @param size size to format to, in bytes
 * @return fat_manag_err_code_t 
 *  - FAT_OK: Success
 */
fat_manag_err_code_t fat_info_create(fat_info_t *info, unsigned long size);

/**
 * @brief Opens the FS file in the specified mode at the first position of the specified cluster (data block)
 * 
 * @param info the FAT to open
 * @param dblock_num the cluster to open at
 * @param mode the mode (should either be "rb" or "ab")
 * @return FILE* the FILE* that can be used as a regular file
 */
FILE *fat_copen(fat_info_t *info, dblock_idx_t dblock_num, char *mode);

/**
 * @brief Go to a specified cluster (its beggining) within the opened FS file
 * 
 * @param info 
 * @param fs the FS FILE* opened with \c fat_copen or \c fopen
 * @param block_num the data block to go to
 * @return fat_manag_err_code_t  \n
 *  - FAT_OK: Success \n
 *  - FAT_FILE_404: \c fs is NULL \n
 *  - FAT_FPTR_ERR: could not fseek (error stored in errno) \n
 */
fat_manag_err_code_t fat_cseek(fat_info_t *info, FILE *fs, dblock_idx_t block_num);

/**
 * @brief Create a subdirectory in the directory pointed to by root.
 * This also writes to clusters (updates \c root and writes the newly created dir to a new cluster)
 * 
 * @param root the directory to create the subdir in. If NULL, initializes the root directory (at cluster 0)
 * @param name the name of the subdir. Must be up to 11 characters long.
 * @return fat_manag_err_code_t 
 *  - FAT_OK: Success \n
 *  - FAT_NO_MEM: no memory in the root directory OR NO FREE BLOCKS (run \c fat_get_free_cluster to check) \n
 *  - FAT_FPTR_ERR: error opening the FS file \n
 *  - FAT_ERR_CRITICAL: write function failed \n
 */
fat_manag_err_code_t fat_mkdir(fat_info_t *info, fat_dir_t *root, char *name);

/**
 * @brief Resolves the path relative to the \c root directory. If \c path starts with '/',
 * the path is treated as absolute and the resolution begins at the root directory of the FS.
 * Same applies when the \c root pointer is NULL. 
 * 
 * @param info the FAT info
 * @param root the root to which the path is relative to. Cannot end with '/'
 * @param path the path
 * @return fat_manag_err_code_t 
 *  - FAT_OK \n
 *  - FAT_PATH_404: Path does not exist or one of the parts was a file, not a directory
 */
fat_manag_err_code_t fat_goto_dir(fat_info_t *info, fat_dir_t *root, char *path);

/**
 * @brief Loads directory info of the directory beggining at cluster index \c dir_idx into the adress specified by \c dir \n
 * The \c dir_idx is assumed to be an index of a directory by the function, whatever lies in it!! \n
 *  \n
 * Does not set \c dir->path 
 * 
 * @param info the FAT info
 * @param dir Pointer where to store the dir info
 * @param dir_idx cluster index of the directory which's info to load
 * @param fs the open stream for the FS. If NULL, the function will open and close it itself. If not null, THIS WILL FSEEK THE PTR TO THE \c dir_idx CLUSTER
 * @return fat_manag_err_code_t 
 */
fat_manag_err_code_t fat_load_dir_info(fat_info_t *info, fat_dir_t *dir, dblock_idx_t dir_idx, FILE* fs);

/**
 * @brief Obtain one free data block from the FS. This does not remove the FAT_FREE mark!
 * 
 * @param info 
 * @return dblock_idx_t 
 *  - FAT_FREE: info is NULL \n 
 *  - FAT_ERR: no free blocks \n
 *  - other: index of the free block \n
 */
dblock_idx_t fat_get_free_cluster(fat_info_t *info);

#endif