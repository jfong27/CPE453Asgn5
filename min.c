#include <unistd.h>
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
   if(temp == NULL) {
      perror("Calloc temp string");
      exit(1);
   }
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
   if (path_array == NULL) {
      perror("Realloc in path splitting");
      exit(-1);
   }
   path_array[n_spaces] = 0;

   args->num_levels = n_spaces;

   return path_array;
}

/*
 * find_partition_table
 * Find the partition table in the disk image and validate it.
 */
void find_partition_table(FILE *image, args *args, int type) {
   int res;
   uint8_t bootSector[BOOT_SECTOR_SIZE];
   p_table *ptable;

   res = fseek(image, args->location + part_offset, SEEK_SET);
   if(res == -1) {
      perror("Error while seeking");
      exit(1);
   }
   fread(bootSector, BOOT_SECTOR_SIZE, 1, image);
   res = ferror(image);
   if(res != 0) {
      perror("Error while reading");
      exit(1);
   }

   if(bootSector[510] != SIG510 || bootSector[511] != SIG511) {
      fprintf(stderr,"Invalid partition table.\n");
      fprintf(stderr,"Unable to open disk image \"%s\".\n", args->image);
      res = fclose(image);
      if(res != 0) {
         perror("Error closing file");
         exit(1);
      }
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
      res = fclose(image);
      if(res != 0) {
         perror("Error closing file");
         exit(1);
      }
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
   int res;
   uint8_t superBlockSector[SUPER_BLOCK_SIZE];

   res = fseek(image, part_offset+SUPER_BLOCK_SIZE, SEEK_SET);
   if(res == -1) {
      perror("Error while seeking");
      exit(1);
   }
   fread(superBlockSector, SUPER_BLOCK_SIZE, 1, image);
   res = ferror(image);
   if(res != 0) {
      perror("Error while reading");
      exit(1);
   }

   args->superblock = malloc(sizeof(s_block));
   if(args->superblock == NULL) {
      perror("Malloc error");
      exit(1);
   }
   memcpy(args->superblock, superBlockSector, sizeof(s_block));

   if(args->superblock->magic != MINIX_MAGIC) {
      printf("Bad magic number. (0x%04x)\n", args->superblock->magic);
      printf("This doesn't look like a MINIX filesystem.\n");
      res = fclose(image);
      if(res != 0) {
         perror("Error closing file");
         exit(1);
      }
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

   int res;
   uint16_t blocksize = args->superblock->blocksize;
   int16_t i_blocks = args->superblock->i_blocks;
   int16_t z_blocks = args->superblock->z_blocks;
   uint32_t ninodes = args->superblock->ninodes;

   inode *inodes = malloc(ninodes * sizeof(struct i_node));
   if(inodes == NULL) {
      perror("Malloc failure");
      exit(1);
   }
   res = fseek(image, part_offset + 
      (2 + i_blocks + z_blocks) * blocksize, SEEK_SET);
   if(res == -1) {
      perror("Error while seeking");
      exit(1);
   }

   fread(inodes,  sizeof(struct i_node), ninodes, image);
   res = ferror(image);
   if(res != 0) {
      perror("Error while reading");
      exit(1);
   }

   return inodes;
}

/*
 * list_file
 * If the target is a file (not a directory), we have to list
 * just the file. So we print the permissions, then its size and path
 */
void list_file(args *args, inode *target) {
   print_permissions(target->mode, STDOUT);
   printf(" %9u %s\n", target->size, args->path);
}

/*
 * traverse_path
 * If a path argument is given, this function traverses
 * the path of directories to find the target file or
 * directory. Returns the inode of target file/directory.
 */
inode *traverse_path(args *args, inode *inodes,
                     dirent **root, FILE *image) {

   int i = 0, res;
   int j, found = 0, k, size_left, offset = 0;
   inode *next_inode = malloc(sizeof(inode *));
   if(next_inode == NULL) {
      perror("Malloc failure");
      exit(1);
   }
   dirent *directory = malloc(inodes[0].size);
   if(directory == NULL) {
      perror("Malloc failure");
      exit(1);
   }
   directory = *root;

   if (args->path_array[0] == NULL) {
      free(directory);
      return inodes;
   }
   
   for (j = 0; j < args->num_levels; j++) {
      while (directory[i].name[0] != '\0' || directory[i].ino != 0) {
         if (!strcmp(args->path_array[j], directory[i].name)) {
            if ((j != args->num_levels - 1) &&
                ((inodes[directory[i].ino - 1].mode & BITMASK) !=
                DIR_MASK)) {
               fprintf(stderr, "%s: File not found.\n", args->path);
               exit(-1);
            }

            if(j == args->num_levels-1)
               found = 1;
            // We have found the desired directory entry
            next_inode = &inodes[directory[i].ino - 1];
            size_left = next_inode->size;
            directory = malloc(next_inode->size);
            for(k = 0; k < DIRECT_ZONES; k++) {
               res = fseek(image, (zoneSize * next_inode->zone[k]) 
                  + part_offset, SEEK_SET);
               if(res == -1) {
                  perror("Error while seeking");
                  exit(1);
               }
               if(size_left >= zoneSize) {
                  fread(&directory[offset], zoneSize, 1, image);
                  res = ferror(image);
                  if(res != 0) {
                     perror("Error while reading");
                     exit(1);
                  }
                  size_left -= zoneSize;
                  offset += (zoneSize/DIR_ENT_SIZE);
               } else {
                  fread(&directory[offset], size_left, 1, image);
                  res = ferror(image);
                  if(res != 0) {
                     perror("Error while reading");
                     exit(1);
                  }
                  break;
               }
            }
            break;
         }
         i++;
      }
      offset = 0;
      i = 0;
   }
   if(found == 0) {
      return NULL;
   }
   free(directory);
   return next_inode;
}

/*
 * get_target
 * Copies regular file to destination path or stdout. 
 */
void get_target(FILE *image, args *args, inode *inodes) {

   int i, res;
   dirent *root = malloc(inodes[0].size);
   if(root == NULL) {
      perror("Malloc failure");
      exit(1);
   }
   int size_left = inodes[0].size, offset = 0;
   inode *target;

   for(i = 0; i < DIRECT_ZONES; i++) {
      res = fseek(image, (zoneSize * 
         inodes[0].zone[i]) + part_offset, SEEK_SET);
      if(res == -1) {
         perror("Error while seeking");
         exit(1);
      }
      if(size_left >= zoneSize) {
         fread(&root[offset], zoneSize, 1, image);
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }
         size_left -= zoneSize;
         offset += (zoneSize/DIR_ENT_SIZE);
      } else {
         fread(&root[offset], size_left, 1, image);
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }
         break;
      }
   }

   target = traverse_path(args, inodes, &root, image);

   if(target == NULL) {
      fprintf(stderr, "%s: File not found.\n", args->path);
      exit(255);
   }

   if ((target->mode & BITMASK) != REG_FILE) {
      fprintf(stderr, "%s: Not a regular file.\n", args->path);
      exit(-1);
   }

   if(args->v) {
      print_inode(target);
   }
   FILE *dst = NULL;
   if (args->dstpath != NULL) {
      dst = fopen(args->dstpath, "wb");
      if(dst == NULL) {
         perror("Error opening host path file");
         exit(1);
      }
   }

   uint8_t buffer[zoneSize];
   size_left = target->size;

   for (i = 0; i < DIRECT_ZONES; i++) {
      res = fseek(image, (zoneSize * target->zone[i]) + part_offset,
               SEEK_SET);
      if(res == -1) {
         perror("Error while seeking");
         exit(1);
      }
      if(target->zone[i] == 0) {
         memset(buffer, 0, zoneSize);
      }
      else if(size_left >= zoneSize) {
         fread(buffer, zoneSize, 1, image);    
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }    
      } else {
         fread(buffer, size_left, 1, image);
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }
      }
      if (dst == NULL) {
         if(size_left >= zoneSize) {
            res = write(1, buffer, zoneSize);
            if(res == -1) {
               perror("Error writing to stdout");
               exit(1);
            }
            size_left -= zoneSize;
         } else {
            res = write(1, buffer, size_left);
            if(res == -1) {
               perror("Error writing to stdout");
               exit(1);
            }
            size_left = 0;
         }
         if(size_left == 0)
            break;
      } else {
         if(size_left >= zoneSize) {
            fwrite(buffer, zoneSize, 1, dst);
            res = ferror(dst);
            if(res != 0) {
               perror("Error writing to host path file");
               exit(1);
            }
            size_left -= zoneSize;
         } else {
            fwrite(buffer, size_left, 1, dst);
            res = ferror(dst);
            if(res != 0) {
               perror("Error writing to host path file");
               exit(1);
            }
            size_left = 0;
         }
         if(size_left == 0)
            break;
      }
   }

   uint32_t indir_zones[zoneSize / sizeof(uint32_t)];
   int num_zones = zoneSize / sizeof(uint32_t);
   if (size_left > 0) {
      // There are still file contents in indirect zones
      fseek(image, (zoneSize * target->indirect) + part_offset,
               SEEK_SET);
      fread(indir_zones, zoneSize, 1, image);
      for (i = 0; i < num_zones; i++) {
         fseek(image, (zoneSize * indir_zones[i]) + part_offset,
               SEEK_SET);
         if(indir_zones[i] == 0) {
            memset(buffer, 0, zoneSize);
         }
         else if(size_left >= zoneSize) {
            fread(buffer, zoneSize, 1, image);        
         } else {
            fread(buffer, size_left, 1, image);
         }
         if (dst == NULL) {
            if(size_left >= zoneSize) {
               write(1, buffer, zoneSize);
               size_left -= zoneSize;
            } else {
               write(1, buffer, size_left);
               size_left = 0;
            }
            if(size_left == 0)
               break;
         } else {
            if(size_left >= zoneSize) {
               fwrite(buffer, zoneSize, 1, dst);
               size_left -= zoneSize;
            } else {
               fwrite(buffer, size_left, 1, dst);
               size_left = 0;
            }
            if(size_left == 0)
               break;
         }
      }
   }

   if (dst != NULL) {
      res = fclose(dst);
      if(res != 0) {
         perror("Error closing host path file");
         exit(1);
      }
   }

   free(root);
}


/*
void copy_out_zone(FILE *image, FILE *dst, inode *target, int size_left) {

      uint8_t buffer[zoneSize];
      fseek(image, (zoneSize * target->zone[i]) + part_offset,
               SEEK_SET);
      if(target->zone[i] == 0) {
         memset(buffer, 0, zoneSize);
      }
      else if(size_left >= zoneSize) {
         fread(buffer, zoneSize, 1, image);        
      } else {
         fread(buffer, size_left, 1, image);
      }
      if (dst == NULL) {
         if(size_left >= zoneSize) {
            write(1, buffer, zoneSize);
            size_left -= zoneSize;
         } else {
            write(1, buffer, size_left);
            size_left = 0;
         }
         if(size_left == 0)
            break;
      } else {
         if(size_left >= zoneSize) {
            fwrite(buffer, zoneSize, 1, dst);
            size_left -= zoneSize;
         } else {
            fwrite(buffer, size_left, 1, dst);
            size_left = 0;
         }
         if(size_left == 0)
            break;
      }

}
*/

/**************************************************************************
 * PRINTING FUNCTIONS
 * Self explanatory
 *************************************************************************/
void print_superblock(s_block *superblock) {
   fprintf(stderr,"\nSuperblock Contents:\n");
   fprintf(stderr,"Stored Fields:\n");
   fprintf(stderr,"  ninodes %12u\n", superblock->ninodes);
   fprintf(stderr,"  i_blocks %11d\n", superblock->i_blocks);
   fprintf(stderr,"  z_blocks %11d\n", superblock->z_blocks);
   fprintf(stderr,"  firstdata %10u\n", superblock->firstdata);
   fprintf(stderr,"  log_zone_size %6d (zone size: %u)\n", 
         superblock->log_zone_size, zoneSize);
   fprintf(stderr,"  max_file %11u\n", superblock->max_file);
   fprintf(stderr,"  magic         0x%04x\n", superblock->magic);
   fprintf(stderr,"  zones %14u\n", superblock->zones);
   fprintf(stderr,"  blocksize %10u\n", superblock->blocksize);
   fprintf(stderr,"  subversion %9u\n", superblock->subversion);
   fprintf(stderr,"Computed Fields:\n");
   fprintf(stderr,"  version            3\n");
   fprintf(stderr,"  firstImap          2\n");
   fprintf(stderr,"  firstZmap %10u\n", 2 + superblock->i_blocks);
   fprintf(stderr,"  firstIblock %8u\n", 2 + superblock->i_blocks 
         + superblock->z_blocks);
   fprintf(stderr,"  zonesize %11u\n", zoneSize);
   fprintf(stderr,"  ptrs_per_zone %6u\n", zoneSize/4);
   fprintf(stderr,"  ino_per_block %6u\n", superblock->blocksize/INO_SIZE);
   fprintf(stderr,"  wrongended %9u\n", superblock->magic == MINIX_MAGIC_REV);
   fprintf(stderr,"  fileent_size %7u\n", DIR_ENT_SIZE);
   fprintf(stderr,"  max_filename %7u\n", MAX_FILENAME);
   fprintf(stderr,"  ent_per_zone %7u\n", zoneSize/DIR_ENT_SIZE);
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

   int res;
   dirent *root = malloc(inodes[0].size), *target_dir;
   if(root == NULL) {
      perror("Malloc failure");
      exit(1);
   }
   int size_left = inodes[0].size, i, offset = 0;
   inode *target;


   for(i = 0; i < DIRECT_ZONES; i++) {
      res = fseek(image, (zoneSize * inodes[0].zone[i])
       + part_offset, SEEK_SET);
      if(res == -1) {
         perror("Error while seeking");
         exit(1);
      }
      if(size_left >= zoneSize) {
         fread(&root[offset], zoneSize, 1, image);
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }
         size_left -= zoneSize;
         offset += (zoneSize/DIR_ENT_SIZE);
      } else {
         fread(&root[offset], size_left, 1, image);
         res = ferror(image);
         if(res != 0) {
            perror("Error while reading");
            exit(1);
         }
         break;
      }
   }
   if (args->path_array == NULL) {
      //Print root directory
      if (args->v) {
         print_inode(&inodes[0]);
      }
      print_directory(root, args, inodes, inodes[0].size);
   } else {
      //Traverse path and list the directory/file
      target = traverse_path(args, inodes, &root, image);

      if(target == NULL) {
         fprintf(stderr, "%s: File not found.\n", args->path);
         exit(255);
      }
      if(args->v) {
         print_inode(target);
      }
      switch (target->mode & BITMASK) {
         case REG_FILE:
            list_file(args, target);
            break;
         case DIR_MASK:
            offset = 0;
            target_dir = malloc(target->size);
            size_left = target->size;
            for(i = 0; i < DIRECT_ZONES; i++) {
               fseek(image, (zoneSize * target->zone[i]) + part_offset,
                  SEEK_SET);
               if(size_left >= zoneSize) {
                  fread(&target_dir[offset], zoneSize, 1, image);
                  res = ferror(image);
                  if(res != 0) {
                     perror("Error while reading");
                     exit(1);
                  }
                  size_left -= zoneSize;
                  offset += (zoneSize/DIR_ENT_SIZE);
               } else {
                  fread(&target_dir[offset], size_left, 1, image);
                  res = ferror(image);
                  if(res != 0) {
                     perror("Error while reading");
                     exit(1);
                  }
                  print_directory(target_dir, args, inodes, target->size);
                  break;
               }
            }
            break;
         default:
            perror("Not file/direc?");
            break;
      }

   }
   free(root);
}

void print_directory(dirent *d, args *args, inode *inodes, int size) {
   int i = 0;
   uint32_t file_size = 0;

   printf("%s:\n", args->path);
   for(i = 0; i < (size/DIR_ENT_SIZE); i++) {
      if (d[i].ino == 0) {
         continue;
      }
      file_size = inodes[d[i].ino - 1].size;

      print_permissions(inodes[d[i].ino - 1].mode, STDOUT);

      printf(" %9u %s\n", file_size, d[i].name);
   }
}

void print_permissions(uint16_t mode, int type) {
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

   if(type == STDERR)
      fprintf(stderr, "%s", permissions);
   else if(type == STDOUT)
      printf("%s", permissions);
}

void print_inode(inode *inode) {
   time_t time;
   char timebuff[TIME_BUF_SIZE];

   fprintf(stderr, "\nFile inode:\n");
   fprintf(stderr, "  unsigned short mode         0x%04x\t(", inode->mode);
   print_permissions(inode->mode, STDERR);
   fprintf(stderr, ")\n");
   fprintf(stderr, "  unsigned short links %13u\n", inode->links);
   fprintf(stderr, "  unsigned short uid %15u\n", inode->uid);
   fprintf(stderr, "  unsigned short gid %15u\n", inode->gid);
   fprintf(stderr, "  uint32_t  size %14u\n", inode->size);
   time = inode->atime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   fprintf(stderr, "  uint32_t  atime %13u\t--- %s\n", inode->atime, timebuff);
   time = inode->mtime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   fprintf(stderr, "  uint32_t  mtime %13u\t--- %s\n", inode->mtime, timebuff);
   time = inode->ctime;
   strftime(timebuff, TIME_BUF_SIZE, "%a %b %e %T %Y", localtime(&time));
   fprintf(stderr, 
      "  uint32_t  ctime %13u\t--- %s\n\n", inode->ctime, timebuff);
   fprintf(stderr,"  Direct zones:\n");
   fprintf(stderr,"              zone[0]   = %10u\n", inode->zone[0]);
   fprintf(stderr,"              zone[1]   = %10u\n", inode->zone[1]);
   fprintf(stderr,"              zone[2]   = %10u\n", inode->zone[2]);
   fprintf(stderr,"              zone[3]   = %10u\n", inode->zone[3]);
   fprintf(stderr,"              zone[4]   = %10u\n", inode->zone[4]);
   fprintf(stderr,"              zone[5]   = %10u\n", inode->zone[5]);
   fprintf(stderr,"              zone[6]   = %10u\n", inode->zone[6]);
   fprintf(stderr,"   uint32_t  indirect   = %10u\n", inode->indirect);
   fprintf(stderr,"   uint32_t  double     = %10u\n", inode->two_indirect);
}

void print_partition_table(p_table *ptable) {
   int i;
   fprintf(stderr, "       ----Start----      ------End-----\n");
   fprintf(stderr, 
      "  Boot head  sec  cyl Type head  sec  cyl      First       Size\n");
   for(i = 0; i < 4; i++) {
      fprintf(stderr, "  0x%02x ", ptable->bootind);
      fprintf(stderr, "%4u ", ptable->start_head);
      fprintf(stderr, "%4u ", ptable->start_sec);
      fprintf(stderr, "%4u ", ptable->start_cyl);
      fprintf(stderr, "0x%02x ", ptable->type);
      fprintf(stderr, "%4u ", ptable->end_head);
      fprintf(stderr, "%4u ", ptable->end_sec);
      fprintf(stderr, "%4u ", ptable->end_cyl);
      fprintf(stderr, "%10u ", ptable->lFirst);
      fprintf(stderr, "%10u\n", ptable->size);
      ptable++;
   }
}


