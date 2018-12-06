#include "min.h"

int zoneSize, part_offset = 0;

/*
 * split_path
 * Take the path string argument and split it into
 * an array of strings.
 */
char **split_path(args *args, char *path) {
   char *token;
   char *temp = calloc(strlen(args->path) + 1, sizeof(char));
   char **path_array = NULL;
   int n_spaces = 0;

   strcpy(temp, path);

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

   args->num_levels = n_spaces;

   return path_array;
}

/*
 * find_partition_table
 * Find the partition table in the disk image and validate it.
 */
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
      print_partition_table(ptable);
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

/*
 * find_super_block
 * Find the superblock at a certain offset from
 * beginning of the filesystem. Read the superblock into
 * the args struct.
 */
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
      print_superblock(args->superblock);
   }
}

/*
 * get_inodes
 * Reads the inodes array into the allocated memory. 
 * The inodes array is at an offset from the beginning
 * of the filesystem of 2 blocks (boot block, super block) 
 * plus the number of i_blocks and z_blocks.
 */
inode *get_inodes(FILE *image, args *args) {

   uint16_t blocksize = args->superblock->blocksize;
   int16_t i_blocks = args->superblock->i_blocks;
   int16_t z_blocks = args->superblock->z_blocks;
   uint32_t ninodes = args->superblock->ninodes;

   inode *inodes = malloc(ninodes * sizeof(struct i_node));
   fseek(image, part_offset + (2 + i_blocks + z_blocks) * blocksize, SEEK_SET);

   fread(inodes,  sizeof(struct i_node), ninodes, image);

   return inodes;
}

/*
 * list_file
 * If the target is a file (not a directory), we have to list
 * just the file. So we print the permissions, then its size and path
 */
void list_file(args *args, inode *target) {
   print_permissions(target->mode);
   printf(" %9u %s\n", target->size, args->path);
}

/*
 * traverse_path
 * If a path argument is given, this function traverses
 * the path of directories to find the target file or
 * directory. Returns the inode of target file/directory.
 */
inode *traverse_path(args *args, inode *inodes,
                     dirent *root, FILE *image) {

   int i = 0;
   int j, found = 0;
   inode *next_inode = malloc(sizeof(inode *));
   dirent *directory = malloc(zoneSize);
   directory = root;

   if (args->path_array[0] == NULL) {
      return inodes;
   }
   
   for (j = 0; j < args->num_levels; j++) {
      while (directory[i].name[0] != '\0' || directory[i].ino != 0) {
         if (!strcmp(args->path_array[j], directory[i].name)) {
            if(j == args->num_levels-1) {
               found = 1;
            }
            // We have found the desired directory entry
            next_inode = &inodes[directory[i].ino - 1];

            fseek(image, (zoneSize * next_inode->zone[0]) 
               + part_offset, SEEK_SET);
            fread(directory, zoneSize, 1, image);
            break;
         }
         i++;
      }
      i = 0;
   }
   if(found == 0) {
      return NULL;
   }

   return next_inode;
}

/*
 * get_target
 * Copies regular file to destination path or stdout. 
 */
void get_target(FILE *image, args *args, inode *inodes) {

   dirent *root = malloc(zoneSize);
   int root_zone = inodes[0].zone[0];
   inode *target;

   fseek(image, (zoneSize * root_zone) + part_offset, SEEK_SET);
   fread(root, zoneSize, 1, image);

   target = traverse_path(args, inodes, root, image);

   if(target == NULL) {
      fprintf(stderr, "%s: File not found.\n", args->path);
      exit(255);
   }

   switch (target->mode & BITMASK) {
      case REG_FILE:
         list_file(args, target);
         break;
      case DIR_MASK:
         fseek(image, (zoneSize * target->zone[0]) + part_offset,
               SEEK_SET);
         fread(root, zoneSize, 1, image);
         print_directory(root, args, inodes);
         break;
      default:
         perror("Not file/direc?");
         break;
   }
   free(root);
}


/**************************************************************************
 * PRINTING FUNCTIONS
 * Self explanatory
 *************************************************************************/
void print_superblock(s_block *superblock) {
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
   printf("  firstIblock %8u\n", 2 + superblock->i_blocks 
      + superblock->z_blocks);
   printf("  zonesize %11u\n", zoneSize);
   printf("  ptrs_per_zone %6u\n", zoneSize/4);
   printf("  ino_per_block %6u\n", superblock->blocksize/INO_SIZE);
   printf("  wrongended %9u\n", superblock->magic == MINIX_MAGIC_REV);
   printf("  fileent_size %7u\n", DIR_ENT_SIZE);
   printf("  max_filename %7u\n", MAX_FILENAME);
   printf("  ent_per_zone %7u\n", zoneSize/DIR_ENT_SIZE);
}

void print_target(FILE *image, args *args, inode *inodes) {

   dirent *root = malloc(zoneSize);
   int root_zone = inodes[0].zone[0];
   inode *target;

   fseek(image, (zoneSize * root_zone) + part_offset, SEEK_SET);
   fread(root, zoneSize, 1, image);

   if (args->path_array == NULL) {
      //Print root directory
      if (args->v) {
         print_inode(&inodes[0]);
      }
      print_directory(root, args, inodes);
   } else {
      //Traverse path and list the directory/file
      target = traverse_path(args, inodes, root, image);

      if(target == NULL) {
         fprintf(stderr, "%s: File not found.\n", args->path);
         exit(255);
      }

      switch (target->mode & BITMASK) {
         case REG_FILE:
            list_file(args, target);
            break;
         case DIR_MASK:
            fseek(image, (zoneSize * target->zone[0]) + part_offset,
                        SEEK_SET);
            fread(root, zoneSize, 1, image);
            print_directory(root, args, inodes);
            break;
         default:
            perror("Not file/direc?");
            break;
      }

   }

   free(root);
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

void print_partition_table(p_table *ptable) {
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


