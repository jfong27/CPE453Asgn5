#include <stdio.h>
#include <string.h>
#include "minls.h"

int main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("usage: minls [-v] [-p num [ -s num ] ] imagefile [path]\n");
      return 0;
   }

   /*Command line options*/
   args *args = malloc(sizeof(args));
   args->v = FALSE;
   args->p = FALSE;
   args->s = FALSE;
   args->image = NULL;
   args->path = NULL;

   parse_args(flags, argc, argv);

   FILE *image_fp = fopen(argv[1], "rb");
   
   fseek(image_fp, 0, SEEK_SET);
   char buffer[100];


   fclose(image_fp);





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
               args->partition = argv[i + 1];
               i += 1;
               break;
            case 's':
               args->s = TRUE;
               args->subpartition = argv[i + 1];
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

//TODO: Seek to partition table in MBR(0x1be), then
//read it into the p_table struct
void find_partition_table() {

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


