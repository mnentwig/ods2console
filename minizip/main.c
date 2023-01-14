#include "unzip.h"
#include <stdio.h>

//#define USEWIN32IOAPI
#ifdef USEWIN32IOAPI
#include "iowin32.h"
#endif
int main(void){

  const char* fn = "sampleInput.ods";
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
  fill_win32_filefunc64A(&ffunc);  
  unzFile uf = unzOpen2_64(fn, &ffunc);
#else
  unzFile uf = unzOpen64(fn);
#endif

  if (!uf)
    return -1;
  printf("opened\n");
  if (unzLocateFile(uf, "content.xml",/*case sensitive*/0) != UNZ_OK)
    return -1;
  printf("located\n");

  unz_file_info64 file_info;
  char filename_inzip[256];
  if (unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) != UNZ_OK)
    return -1;
  printf("got info\n");

  if (unzOpenCurrentFilePassword(uf,/*password*/NULL))
    return -1;
  printf("opened file\n");

#define NBUF (8192)
  char buf[NBUF];
  int nRead = 0;
  char* retBuf = NULL;
  while (1){
    int nBytes = unzReadCurrentFile(uf, buf, NBUF);
    if (nBytes < 0){
      printf("error %d with zipfile in unzReadCurrentFile\n", nBytes);
      
      return -1;}
    
    if (nBytes == 0)
      break;
    retBuf = realloc(retBuf, nRead + nBytes);
    memcpy(retBuf+nRead, buf, nBytes);
    nRead += nBytes;
  }    
  printf("read\n");
  
  for (int ix = 0; ix < nRead; ++ix){
    printf("%c", *(retBuf+ix));
}
  printf("\n");
  retBuf = realloc(retBuf, 0); // free

  return 0;
}
