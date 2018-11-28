#include <stdio.h>
#include <string.h>
#include "minls.h"

int main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("usage: minls [-v] [-p num [ -s num ] ] imagefile [path]\n");
      return 0;
   }

   FILE *image_fp = fopen(argv[1], "rb");
   
   fseek(image_fp, 0, SEEK_SET);
   char buffer[100];
   fread(buffer, 10, 10, image_fp);

   printf("%s\n", buffer);

   fclose(image_fp);





   return 0;
}

//TODO: Parse command linearguments for partition 
//and verbose flags, and image file name.
void parse_args() {

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


