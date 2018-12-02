#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minls.h"

void parse_args(struct arguments *args, int argc, char *argv[]);
void find_partition_table(FILE *image, struct arguments *args, int type);
void find_super_block(FILE *image, struct arguments *args);
void get_inodes(FILE *image, struct arguments *args, inode **inodes);

int zoneSize;

int main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("usage: minls [-v] [-p num [ -s num ] ] imagefile [path]\n");
      return 0;
   }

   /*Command line options*/
   struct arguments *args = malloc(sizeof(args));
   args->location = 0;
   args->v = FALSE;
   args->p = FALSE;
   args->s = FALSE;
   args->image = NULL;
   args->path = NULL;

   parse_args(args, argc, argv);

   FILE *image_fp = fopen(args->image, "rb");

   if(!image_fp) {
      printf("Unable to open disk image \"%s\".\n", args->image);
      exit(3);
   }

   if(args->p == TRUE) {
      find_partition_table(image_fp, args, PART);
   }
   if(args->s == TRUE) {
      find_partition_table(image_fp, args, SUBPART);
   }
   
   find_super_block(image_fp, args);


   inode **inodes = malloc(sizeof(inode) * args->superblock->ninodes);
   get_inodes(image_fp, args, inodes);

   fclose(image_fp);

   return 0;
}

//TODO: Parse command linei arguments for partition 
//and verbose flags, and image file name.
void parse_args(struct arguments *args, int argc, char *argv[]) {
   int i;

   for (i = 1; i < argc ; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'v':
               args->v = TRUE;
               break;
            case 'p':
               args->p = TRUE;
               sscanf(argv[i + 1], "%d", &args->partition);
               i += 1;
               break;
            case 's':
               args->s = TRUE;
               sscanf(argv[i + 1], "%d", &args->subpartition);
               i += 1;
               break;
         }
      } else {
         if (args->image == NULL) {
            args->image = argv[i];
         } else {
            args->path = argv[i];
         }
      }
   }

}

void printPartTable(p_table *ptable) {
   int i;
   printf("       ----Start----      ------End-----\n");
   printf("  Boot head  sec  cyl Type head  sec  cyl      First       Size\n");
   for(i = 0; i < 4; i++) {
      printf("  0x%02x ", ptable->bootind);
      printf("%4u ", ptable->start_head);
      printf("%4u ", ptable->start_sec);
      printf("%4u ", ptable->start_cyl);
      printf("0x%02x ", ptable->type);
      printf("%4u ", ptable->end_head);
      printf("%4u ", ptable->end_sec);
      printf("%4u ", ptable->end_cyl);
      printf("%10u ", ptable->lFirst);
      printf("%10u\n", ptable->size);
      ptable++;
   }
}

//TODO: Seek to partition table in MBR(0x1be), then
//read it into the p_table struct
void find_partition_table(FILE *image, struct arguments *args, int type) {
   uint8_t bootSector[BOOT_SECTOR_SIZE];
   p_table *ptable;

   fseek(image, args->location, SEEK_SET);
   fread(bootSector, BOOT_SECTOR_SIZE, 1, image);

   if(bootSector[510] != SIG510 || bootSector[511] != SIG511) {
      printf("Invalid partition table.\n");
      printf("Unable to open disk image \"%s\".\n", args->image);
      fclose(image);
      exit(3);
   }

   if(args->v)
   {
      if(type == PART)
         printf("\nPartition table:\n");
      if(type == SUBPART)
         printf("\nSubpartition table:\n");

   }

   ptable = (p_table *)(bootSector + PTABLE_LOC);

   printPartTable(ptable);
   if(type == PART) {
      ptable += args->partition;
   } else if(type == SUBPART) {
      ptable += args->subpartition;
   }

   if(ptable->type != MINIX_PART) {
      if(type == PART) {
         printf("Not a Minix partition.\n");
      } else if(type == SUBPART) {
         printf("Not a Minix subpartition\n");
      }
      printf("Unable to open disk image \"%s\".\n", args->image);
      fclose(image);
      exit(3);
   }
   args->location = ptable->lFirst*SECTOR_SIZE;
}

void printSBlock(s_block *superblock) {
   printf("\nSuperblock Contents:\n");
   printf("Stored Fields:\n");
   printf("  ninodes %12u\n", superblock->ninodes);
   printf("  i_blocks %11d\n", superblock->i_blocks);
   printf("  z_blocks %11d\n", superblock->z_blocks);
   printf("  firstdata %10u\n", superblock->firstdata);
   printf("  log_zone_size %6d", superblock->log_zone_size);
   printf(" (zone size: %u)\n", zoneSize);
   printf("  max_file %11u\n", superblock->max_file);
   printf("  magic         0x%04x\n", superblock->magic);
   printf("  zones %14u\n", superblock->zones);
   printf("  blocksize %10u\n", superblock->blocksize);
   printf("  subversion %9u\n", superblock->subversion);
}

void find_super_block(FILE *image, struct arguments *args) {
   uint8_t superBlockSector[SUPER_BLOCK_SIZE];
   s_block *superblock;

   printf("A: %d\n", args->location);
   printf("B: %d\n", args->location+SUPER_BLOCK_SIZE);
   fseek(image, args->location+SUPER_BLOCK_SIZE, SEEK_SET);
   fread(superBlockSector, SUPER_BLOCK_SIZE, 1, image);

   superblock = (s_block *)superBlockSector;

   if(superblock->magic != MINIX_MAGIC) {
      printf("Bad magic number. (0x%04x)\n", superblock->magic);
      printf("This doesn't look like a MINIX filesystem.\n");
      fclose(image);
      exit(255);
   }

   zoneSize = superblock->blocksize << superblock->log_zone_size;

   if(args->v) {
      printSBlock(superblock);
   }

   args->superblock = superblock;
}

//TODO: Find partition/subpartition
void find_partition() {

}

//TODO: After partition is found, the superblock
//of the file system should be 1024 bytes from 
//the beginning of filesystem. Seek to superblock,
//read into the super_block struct.
void find_filesystem() {

}

void get_inodes(FILE *image, args *args, inode **inodes) {
   printf("C: %d\n", args->location);

   uint16_t blocksize = args->superblock->blocksize;
   int16_t i_blocks = args->superblock->i_blocks;
   int16_t z_blocks = args->superblock->z_blocks;
   uint32_t ninodes = args->superblock->ninodes;

   inode **inode_array = malloc(sizeof(inode) * ninodes);

   fseek(image,
         args->location + BOOT_SECTOR_SIZE + SUPER_BLOCK_SIZE + 
         (i_blocks * blocksize) + (z_blocks * blocksize),
         SEEK_SET);

   fread(inodes,  sizeof(inode), ninodes, image);


   fprintf(stderr, "ASDSDA\n");
   printf("%32u \n", (*inode_array)->uid);
   printf("%32u \n", inode_array[1]->uid);
   printf("%32u \n", inode_array[2]->uid);
}


