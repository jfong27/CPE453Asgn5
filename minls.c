#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "minls.h"


int zoneSize, part_offset = 0;

int main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("usage: minls [-v] [-p num [ -s num ] ] imagefile [path]\n");
      return 0;
   }

   /*Command line options*/
   args *args = malloc(sizeof(struct arguments));
   args->location = 0;
   args->v = FALSE;
   args->p = FALSE;
   args->s = FALSE;
   args->image = NULL;
   args->path = "/";
   args->path_array = NULL;
   args->superblock = NULL;


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

   inode *inodes = get_inodes(image_fp, args);

   traverse_path(image_fp, args, inodes);

   fclose(image_fp);

   free(inodes);
   free(args->superblock);
   free(args);

   return 0;
}

//TODO: Parse command linei arguments for partition 
//and verbose flags, and image file name.
void parse_args(args *args, int argc, char *argv[]) {
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
            split_path(args);
         }
      }
   }
   
}

/*
 * Take the path string and split it into
 * an array of strings.
 */
void split_path(args *args) {
   char *token;
   char *temp = calloc(strlen(args->path) + 1, sizeof(char));
   char **path_array = NULL;
   int n_spaces = 0;

   strcpy(temp, args->path);

   token = strtok(temp, "/");
   while (token != NULL) {
      path_array = realloc(path_array, sizeof(char*) * ++n_spaces);
      
      if (path_array == NULL) {
         perror("Realloc in path splitting");
         exit(-1);
      }

      path_array[n_spaces - 1] = token;
      token = strtok(NULL, "/");
   }

   path_array = realloc(path_array, sizeof(char*) * (n_spaces + 1));
   path_array[n_spaces] = 0;

   args->path_array = path_array;

   free(temp);
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
void find_partition_table(FILE *image, args *args, int type) {
   uint8_t bootSector[BOOT_SECTOR_SIZE];
   p_table *ptable;

   fseek(image, args->location + part_offset, SEEK_SET);
   fread(bootSector, BOOT_SECTOR_SIZE, 1, image);

   if(bootSector[510] != SIG510 || bootSector[511] != SIG511) {
      printf("Invalid partition table.\n");
      printf("Unable to open disk image \"%s\".\n", args->image);
      fclose(image);
      exit(3);
   }

   if(args->v)
   {
      if (type == PART) {
         printf("\nPartition table:\n");
      }
      if (type == SUBPART) {
         printf("\nSubpartition table:\n");
      }
   }
   ptable = (p_table *)(bootSector + PTABLE_LOC);

   if (args->v) {
      printPartTable(ptable);
   }
   if (type == PART) {
      ptable += args->partition;
   } else if (type == SUBPART) {
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
   part_offset = ptable->lFirst*SECTOR_SIZE;
}

void printSBlock(s_block *superblock) {
   printf("\nSuperblock Contents:\n");
   printf("Stored Fields:\n");
   printf("  ninodes %12u\n", superblock->ninodes);
   printf("  i_blocks %11d\n", superblock->i_blocks);
   printf("  z_blocks %11d\n", superblock->z_blocks);
   printf("  firstdata %10u\n", superblock->firstdata);
   printf("  log_zone_size %6d (zone size: %u)\n", 
      superblock->log_zone_size, zoneSize);
   printf("  max_file %11u\n", superblock->max_file);
   printf("  magic         0x%04x\n", superblock->magic);
   printf("  zones %14u\n", superblock->zones);
   printf("  blocksize %10u\n", superblock->blocksize);
   printf("  subversion %9u\n", superblock->subversion);
   printf("Computed Fields:\n");
   printf("  version            3\n");
   printf("  firstImap          2\n");
   printf("  firstZmap %10u\n", 2 + superblock->i_blocks);
   printf("  firstIblock %8u\n", 2 + superblock->i_blocks + superblock->z_blocks);
   printf("  zonesize %11u\n", zoneSize);
   printf("  ptrs_per_zone %6u\n", zoneSize/4);
   printf("  ino_per_block %6u\n", superblock->blocksize/INO_SIZE);
   printf("  wrongended %9u\n", superblock->magic == MINIX_MAGIC_REV);
   printf("  fileent_size %7u\n", DIR_ENT_SIZE);
   printf("  max_filename %7u\n", MAX_FILENAME);
   printf("  ent_per_zone %7u\n", zoneSize/DIR_ENT_SIZE);
}

void find_super_block(FILE *image, struct arguments *args) {
   uint8_t superBlockSector[SUPER_BLOCK_SIZE];

   fseek(image, part_offset+SUPER_BLOCK_SIZE, SEEK_SET);
   fread(superBlockSector, SUPER_BLOCK_SIZE, 1, image);

   args->superblock = malloc(sizeof(s_block));
   memcpy(args->superblock, superBlockSector, sizeof(s_block));

   if(args->superblock->magic != MINIX_MAGIC) {
      printf("Bad magic number. (0x%04x)\n", args->superblock->magic);
      printf("This doesn't look like a MINIX filesystem.\n");
      fclose(image);
      exit(255);
   }

   zoneSize = args->superblock->blocksize << args->superblock->log_zone_size;

   if(args->v) {
      printSBlock(args->superblock);
   }
}

inode *get_inodes(FILE *image, args *args) {

   uint16_t blocksize = args->superblock->blocksize;
   int16_t i_blocks = args->superblock->i_blocks;
   int16_t z_blocks = args->superblock->z_blocks;
   uint32_t ninodes = args->superblock->ninodes;

   inode *inodes = malloc(ninodes * sizeof(struct i_node));
   //fprintf(stderr, "args->location: %d\n", args->location);
   fseek(image, part_offset + (2 + i_blocks + z_blocks) * blocksize, SEEK_SET);

   fread(inodes,  sizeof(struct i_node), ninodes, image);

  /*if (args->v) {
      print_inode(inodes);
      print_inode(&inodes[16]);
   }

   int i;
   for (i = 0; i < ninodes; i++) {
      //if directory and verbose
      if ((inodes[i].mode & BITMASK) == DIR_MASK && args->v) {
         print_inode(&inodes[i]);
      }
   }
   
    * PROGRESS REPORT:
    * - Successfully found inodes array. 
    * - Root directory is at inodes[0]
    * - When you print the contents of inodes[0], you'll see
    *   that it is stored in zone 16
    * - Seek to zone 16 * zoneSize and read that into a directory struct
    * - directory[0] is the curr directory. You can see name and inode number
    * - directory[1], directory[2], etc. are the contents of the curr directory
    * - If directory[1].ino = 4, then that inode is stored in inodes[3]
    */
   return inodes;
}

void traverse_path(FILE *image, args *args, inode *inodes) {

   dirent *directory = malloc(zoneSize);
   int root_zone = inodes[0].zone[0];
   //char *curr_path = "/";

   fseek(image, (zoneSize * root_zone) + part_offset, SEEK_SET);
   fread(directory, zoneSize, 1, image);

   if (args->path_array == NULL) {
      //Print root directory
      print_inode(&inodes[0]);
      print_directory(directory, args, inodes);
   } else {
      //Traverse path and list the directory/file
   }

   free(directory);

}
void print_directory(dirent *d, args *args, inode *inodes) {
   int i = 0;
   uint32_t file_size = 0;

   printf("%s:\n", args->path);
   while (d[i].name[0] != '\0' || d[i].ino != 0) {
      if (d[i].ino == 0) {
         i++;
         continue;
      }
      file_size = inodes[d[i].ino - 1].size;

      print_permissions(inodes[d[i].ino - 1].mode);

      printf(" %9u %s\n", file_size, d[i].name);
      i++;
   }
}

void print_permissions(uint16_t mode) {
   char permissions[] = "----------";

   permissions[0] = (mode & DIR_MASK) ? 'd' : '-';
   permissions[1] = (mode & OWN_RD) ? 'r' : '-';
   permissions[2] = (mode & OWN_WR) ? 'w' : '-';
   permissions[3] = (mode & OWN_X) ? 'x' : '-';
   permissions[4] = (mode & GRP_RD) ? 'r' : '-';
   permissions[5] = (mode & GRP_WR) ? 'w' : '-';
   permissions[6] = (mode & GRP_X) ? 'x' : '-';
   permissions[7] = (mode & O_RD) ? 'r' : '-';
   permissions[8] = (mode & O_WR) ? 'w' : '-';
   permissions[9] = (mode & O_X) ? 'x' : '-';

   printf("%s", permissions);
}

void print_inode(inode *inode) {
   time_t time;
   char timebuff[TIME_BUF_SIZE];

   printf("\nFile inode:\n");
   printf("  unsigned short mode         0x%04x\t(", inode->mode);
   print_permissions(inode->mode);
   printf(")\n");
   printf("  unsigned short links %13u\n", inode->links);
   printf("  unsigned short uid %15u\n", inode->uid);
   printf("  unsigned short gid %15u\n", inode->gid);
   printf("  uint32_t  size %14u\n", inode->size);
   time = inode->atime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   printf("  uint32_t  atime %13u\t--- %s\n", inode->atime, timebuff);
   time = inode->mtime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   printf("  uint32_t  mtime %13u\t--- %s\n", inode->mtime, timebuff);
   time = inode->ctime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   printf("  uint32_t  ctime %13u\t--- %s\n\n", inode->ctime, timebuff);
   printf("  Direct zones:\n");
   printf("              zone[0]   = %10u\n", inode->zone[0]);
   printf("              zone[1]   = %10u\n", inode->zone[1]);
   printf("              zone[2]   = %10u\n", inode->zone[2]);
   printf("              zone[3]   = %10u\n", inode->zone[3]);
   printf("              zone[4]   = %10u\n", inode->zone[4]);
   printf("              zone[5]   = %10u\n", inode->zone[5]);
   printf("              zone[6]   = %10u\n", inode->zone[6]);
   printf("   uint32_t  indirect   = %10u\n", inode->indirect);
   printf("   uint32_t  double     = %10u\n", inode->two_indirect);
}


