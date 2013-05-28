#include "codebase.h"
#include "rekbase.h"

int main(int argc,char *argv[])
   {
   if (argc!=3)
      {
      printf("usage: %s <input.rek> <output.raw>\n",argv[0]);
      printf(" input: 8bit or 16bit fraunhofer volume file format (with 2048 byte header)\n");
      printf(" output: 8bit or 16bit MSB raw volume\n");
      exit(1);
      }

   copyREKvolume(argv[1],argv[2]);

   return(0);
   }
