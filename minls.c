#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("usage: minls [-v] [-p num [ -s num ] ] imagefile [path]\n");
      return 0;
   }

   FILE *image_fp = fopen(argv[1], "r");
   
   fseek(image_fp, 0, SEEK_SET);
   char buffer[100];
   fread(buffer, 10, 10, image_fp);

   printf("%s\n", buffer);

   fclose(image_fp);





   return 0;
}

void parse_args() {

}

void find_partition_table() {

}

void find_partition() {

}

void find_filesystem() {

}


