// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef _MEMORYPAGESPOOL_H
#define _MEMORYPAGESPOOL_H

#include "CounterStats.h"
#include "DataBlockContainer.h"
#include <Common/Fifo.h>
#include <Common/Timer.h>
#include <functional>
#include <map>
#include <memory>

// This class creates a pool of data pages from a memory block
// Optimized for 1-1 consumers (1 thread to get the page, 1 thread to release
// them) No check is done on validity of address of data pages pushed back in
// queue Base address should be kept while object is in use

class MemoryPagesPool {

public:
  // prototype of function to release memory block
  // argument is the baseAddress provided to constructor
  // NB: may use std::bind to add extra arguments
  using ReleaseCallback = std::function<void(void *)>;

  // constructor
  // parameters:
  // - size of each page (in bytes)
  // - number of pages in the pool
  // - base address of memory block where to create the pages
  // - size of memory block in bytes (if zero, assuming it is big enough for
  // page number * page size - not taking into account firstPageOffset is set)
  // - a release callback to be called at destruction time
  // - firstPageOffset is the offset of first page from base address. This is to
  // control alignment. All pages are created contiguous from this point.
  //   If non-zero, this may reduce number of pages created compared to request
  //   (as to fit in base size)
  MemoryPagesPool(size_t pageSize, size_t numberOfPages, void *baseAddress,
                  size_t baseSize = 0, ReleaseCallback callback = nullptr,
                  size_t firstPageOffset = 0);

  // destructor
  ~MemoryPagesPool();

  // methods to get and release page
  // the two functions can be called concurrently without locking
  // (but a lock is needed if calling the same function concurrently)
  void *
  getPage(); // get a new page from the pool (if available, nullptr if none)
  void releasePage(void *address); // insert back page to the pool after use, to
                                   // make it available again

  // access to variables
  size_t getPageSize();               // get the page size
  size_t getTotalNumberOfPages();     // get number of pages in pool
  size_t getNumberOfPagesAvailable(); // get number of pages currently available
  void *getBaseBlockAddress(); // get the base address of memory pool block
  size_t getBaseBlockSize();   // get the  size of memory pool block. All pages
                             // guaranteed to be within &baseBlockAddress[0] and
                             // &baseBlockAddress[baseBlockSize]

  std::shared_ptr<DataBlockContainer> getNewDataBlockContainer(
      void *page = nullptr); // returns an empty data block container (with data
                             // = a given page, retrieved previously by
                             // getPage(), or new page if null) from the pool.
                             // Page will be put back in pool after use.
                             // The base header is filled, in particular
                             // block->header.dataSize has usable page size and
                             // block->data points to it.

  size_t getDataBlockMaxSize(); // returns usable payload size of blocks
                                // returned by getNewDataBlockContainer()

  bool isPageValid(void *page); // check to see if a page address is valid

private:
  std::unique_ptr<AliceO2::Common::Fifo<void *>>
      pagesAvailable; // a buffer to keep track of individual pages

  size_t numberOfPages;                           // number of pages
  size_t pageSize;                                // size of each page, in bytes
  size_t headerReservedSpace = sizeof(DataBlock); // number of bytes reserved at
                                                  // top of each page for header

  void *baseBlockAddress; // address of block containing all pages
  size_t baseBlockSize;   // size of block containing all pages
  void *firstPageAddress; // address of first page
  void *lastPageAddress;  // address of last page

  ReleaseCallback
      releaseBaseBlockCallback; // the user function called in destructor,
                                // typically to release the baseAddress block.

  AliceO2::Common::Timer clock; // a global clock

  // index to keep track of individual pages in pool
  struct DataPageDescriptor {
    int id;                  // page id
    void *ptr;               // pointer to page
    double timeGetPage;      // time of last getPage()
    double timeGetDataBlock; // time of last getNewDataBlockContainer()
    double timeReleasePage;  // time of last releasePage()
    int nTimeUsed;
  };

  using DataPageMap = std::map<void *, DataPageDescriptor>;
  DataPageMap pagesMap; // reference of all registered pages

  CounterStats t1, t2, t3, t4; // statistics on time for superpage
  // t1: getpage->getdatablock
  // t2: getdatablock->releasepage
  // t3: releasepage->getpage
  // t4: getpage->releasepage
};

#endif // #ifndef _MEMORYPAGESPOOL_H
