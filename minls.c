#include "min.h"


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

   print_target(image_fp, args, inodes);

   fclose(image_fp);

   free(inodes);
   free(args->superblock);
   free(args);

   return 0;
}
