/*
 * Connor Alvin (calvin@calpoly.edu)
 * Jason Fong (jfong27@calpoly.edu)
 * CPE 453: Operating Systems
 * Assignment 5: minget
 */
#include "min.h"


int main(int argc, char *argv[]) {

   if (argc < 3) {
      print_usage();
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
   args->dstpath = NULL;

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

   get_target(image_fp, args, inodes);

   fclose(image_fp);

   free(inodes);
   free(args->superblock);
   free(args);

   return 0;
}

void print_usage() {
   fprintf(stderr, "usage: minget [-v] [-p num [ -s num ] ] imagefile ");
   fprintf(stderr, "srcpath [ dstpath ]\n");
   fprintf(stderr, "Options:\n");
   fprintf(stderr, "-p  part    --- select partition for ");
   fprintf(stderr, "filesystem (default: none)\n");
   fprintf(stderr, "-s  sub     --- select subpartition for ");
   fprintf(stderr, "filesystem (default: none)\n");
   fprintf(stderr, "-h  help    --- print usage information and exit\n");
   fprintf(stderr, "-v  verbose --- increase verbosity level\n");
}

/*
 * parse_args
 * Parse command line arguments into the args struct
 */
void parse_args(args *args, int argc, char *argv[]) {
   int i;

   for (i = 1; i < argc ; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'h':
               print_usage();
               exit(1);
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
         } else if (strcmp(args->path, "/") == 0) {
            args->path = argv[i];
            args->path_array = split_path(args, args->path);
         } else { 
            args->dstpath = argv[i];
         }
      }
   }
   
}


