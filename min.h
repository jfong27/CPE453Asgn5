#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#define DIRECT_ZONES 7
#define PTABLE_LOC 0x1be
#define BOOT_SECTOR_SIZE 1024
#define SUPER_BLOCK_SIZE 1024
#define SIG510 0x55
#define SIG511 0xAA
#define MINIX_PART 0x81
#define SECTOR_SIZE 512
#define PART 3
#define SUBPART 4
#define MINIX_MAGIC 0x4D5A
#define MINIX_MAGIC_REV 0x5A4D
#define BITMASK 0170000
#define DIR_MASK 0040000
#define REG_FILE 0100000
#define OWN_RD 0000400
#define OWN_WR 0000200
#define OWN_X 0000100
#define GRP_RD 0000040
#define GRP_WR 0000020
#define GRP_X 0000010
#define O_RD 0000004
#define O_WR 0000002
#define O_X 0000001
#define INO_SIZE 64
#define MAX_FILENAME 60
#define DIR_ENT_SIZE 64
#define TIME_BUF_SIZE 28

typedef struct partition_table {
   uint8_t bootind;
   uint8_t start_head;
   uint8_t start_sec;
   uint8_t start_cyl;
   uint8_t type;
   uint8_t end_head;
   uint8_t end_sec;
   uint8_t end_cyl;
   uint32_t lFirst;
   uint32_t size;
} p_table;
   
//Packed structure, compiler cannot add
//padding into memory. 
typedef struct __attribute__((__packed__)) super_block {
   uint32_t ninodes;
   uint16_t pad1;
   int16_t i_blocks;
   int16_t z_blocks;
   uint16_t firstdata;
   int16_t log_zone_size;
   int16_t pad2;
   uint32_t max_file;
   uint32_t zones;
   int16_t magic;
   int16_t pad3;
   uint16_t blocksize;
   uint8_t subversion;
} s_block;

typedef struct arguments {
   int v;
   int p;
   int s;
   char *image;
   char *path;
   char **path_array;
   int num_levels;
   int partition;
   int subpartition;
   int location;
   s_block *superblock;
} args;

typedef struct i_node {
   uint16_t mode;
   uint16_t links;
   uint16_t uid;
   uint16_t gid;
   uint32_t size;
   int32_t atime;
   int32_t mtime;
   int32_t ctime;
   uint32_t zone[DIRECT_ZONES];
   uint32_t indirect;
   uint32_t two_indirect;
   uint32_t unused;
} inode;

typedef struct directory {
   uint32_t ino;
   char name[60];
} dirent;

void parse_args(struct arguments *args, int argc, char *argv[]);
void find_partition_table(FILE *image, struct arguments *args, int type);
void find_super_block(FILE *image, struct arguments *args);
inode *get_inodes(FILE *image, struct arguments *args);
void split_path(args *args);
inode *traverse_path(args *args, inode *inodes, dirent **root, FILE *image);
void print_target(FILE *image, struct arguments *args, inode *inodes);
void print_permissions(uint16_t mode);
void print_partition_table(p_table *ptable);
void print_inode(inode *inode);
void print_directory(dirent *d, args *args, inode *inodes, int size);
void print_superblock(s_block *superblock);
void print_usage(void);
