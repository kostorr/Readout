#include <Common/DataBlock.h>
#include <Common/DataBlockContainer.h>
#include <Common/DataSet.h>

#include "RdhUtils.h"

//#define ERRLOG(args...) fprintf(stderr,args)

#include <stdio.h>
#include <string>

#define ERRLOG(args...) fprintf(stderr, args)

int main(int argc, const char *argv[]) {

  // parameters
  std::string filePath;
  typedef enum {plain, lz4, undefined} FileType;  
  FileType fileType=FileType::plain;
  
  bool dumpRDH=false;
  bool validateRDH=true;
  bool dumpDataBlockHeader=false;
  int dumpData=0; // if set, number of bytes to dump in each data page
  
  // parse input arguments
  // format is a list of key=value pairs
  
  if (argc<2) {
    ERRLOG("Usage: %s [rawFilePath] [options]\nList of options:\n \
    filePath=(string) : path to file\n \
    dumpRDH=0|1 : dump the RDH headers\n \
    validateRDH=0|1 : check the RDH headers\n \
    dumpDataBlockHeader=0|1 : dump the data block headers (internal readout headers)\n \
    dumpData=(int) : dump the data pages. If -1, all bytes. Otherwise, the first bytes only, as specified.\n",argv[0]);
    return -1;
  }
   
  // extra options
  for (int i=1;i<argc;i++) {
    const char *option=argv[i];
    std::string key(option);
    size_t separatorPosition=key.find('=');
    if (separatorPosition==std::string::npos) {    
      // if this is the first argument, use it as the file name (for backward compatibility)
      if (i==1) {
        filePath=argv[i];
      } else { 
        ERRLOG("Failed to parse option '%s'\n",option);
      }
      continue;
    }
    key.resize(separatorPosition);
    std::string value=&(option[separatorPosition+1]);
    
    if (key=="fileType") {
      if (value=="plain") {
        fileType=FileType::plain;
      } else if (value=="lz4") {
        fileType=FileType::lz4;
      } else {
        ERRLOG("wrong file type %s\n",value.c_str());
      }
    } else if (key == "filePath") {
      filePath=value;
    } else if (key == "dumpRDH") {
      dumpRDH=std::stoi(value);
    } else if (key == "validateRDH") {
      validateRDH=std::stoi(value);
    } else if (key == "dumpDataBlockHeader") {
      dumpDataBlockHeader=std::stoi(value);
    } else if (key == "dumpData") {
      dumpData=std::stoi(value);
    } else {
      ERRLOG("unknown option %s\n",key.c_str());
    }
    
  }
  
  if (filePath=="") {
    ERRLOG("Please provide a file name\n");
    return -1;
  }

  ERRLOG("Using data file %s\n",filePath.c_str());
  ERRLOG("dumpRDH=%d validateRDH=%d dumpDataBlockHeader=%d dumpData=%d\n",(int)dumpRDH,(int)validateRDH,(int)dumpDataBlockHeader,dumpData);
    
  // open raw data file
  FILE *fp=fopen(filePath.c_str(),"rb");
  if (fp==NULL) {
    ERRLOG("Failed to open file\n");
    return -1;
  }
  
  // read file
  unsigned long pageCount=0;
  unsigned long RDHBlockCount=0;
  unsigned long fileOffset=0;
  for(fileOffset=0;;) {
  
    #define ERR_LOOP {ERRLOG("Error %d @ 0x%08lX\n",__LINE__,fileOffset); break;}
 
    unsigned long blockOffset=fileOffset;
    
    DataBlockHeaderBase hb;
    if (fread(&hb,sizeof(hb),1,fp)!=1) {break;}
    fileOffset+=sizeof(hb);
    
    if (hb.blockType!=DataBlockType::H_BASE) {ERR_LOOP;}
    if (hb.headerSize!=sizeof(hb)) {ERR_LOOP;}
    
    if (dumpDataBlockHeader) {
      printf("Block header %d @ %lu\n",pageCount+1,fileOffset-sizeof(hb));
      printf("\tblockType = 0x%02X\n",hb.blockType);
      printf("\theaderSize = %u\n",hb.headerSize);
      printf("\tdataSize = %u\n",hb.dataSize);
      printf("\tid = %lu\n",hb.id);
      printf("\tlinkId = %u\n",hb.linkId);
      printf("\tequipmentId = %d\n",(int)hb.equipmentId);
      printf("\tdata @ %lu\n",fileOffset);
    }
    
    void *data=malloc(hb.dataSize);
    if (data==NULL) {ERR_LOOP;}
    if (fread(data,hb.dataSize,1,fp)!=1) {ERR_LOOP;}
    fileOffset+=hb.dataSize;
    pageCount++;

    if (dumpData) {
      int max=hb.dataSize;
      if ((dumpData<max)&&(dumpData>0)) {
        max=dumpData;
      }
      printf("Data page %d @ %lu",pageCount,fileOffset-hb.dataSize);
      for (int i=0;i<max;i++) {
        if (i%16==0) {
          printf("\n\t");
        }
        printf("%02X ",(int)(((unsigned char *)data)[i]));
      }
      printf ("\n\t...\n");
    }

    
    if ((validateRDH)||(dumpRDH)) {
      std::string errorDescription;    
      for (size_t pageOffset=0;pageOffset<hb.dataSize;) {
        RDHBlockCount++;
        RdhHandle h(((uint8_t *)data)+pageOffset);
        
        if (dumpRDH) {
          h.dumpRdh();
        }
        int nErr=h.validateRdh(errorDescription);
        if (nErr) {
          h.dumpRdh();
          ERRLOG("File offset 0x%08lX + %ld\n%s",blockOffset,pageOffset,errorDescription.c_str());
          errorDescription.clear();
          break; // we can not continue decoding if RDH wrong, we are not sure if we can jump to the next ... or should we used fixed 8kB ?
        }
        pageOffset+=(h.getBlockLength())*(size_t)32; // length counted in 256b words
      }
    }
        
    free(data);
  }
  ERRLOG("%lu data pages\n",pageCount);
  if (RDHBlockCount) {
  ERRLOG("%lu RDH blocks\n",RDHBlockCount);
  }
  ERRLOG("%lu bytes\n",fileOffset);
  
  // check file status
  if (feof(fp)) {
    ERRLOG("End of file\n");
  }
  
  // close file
  fclose(fp);
}
