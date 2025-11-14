#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void H_MemInfo();

int H_InitHunk();

#define QMEM_IMPLEMENTATION
#ifdef QMEM_IMPLEMENTATION

static uint8_t *hunkData = NULL;
static int hunkTotal;

void H_MemInfo() { printf("hi"); }

// ------------------------------------------------------------------------------
//	alignment example:
//
//	minMegsAlloc = 10;
//
//	MB into byte conversion
//	MB -> KB -> B
//	hunkTotal = 10485760 = minMegsAlloc * 1024 * 1024;
//
//	hunkData = (uint8_t*)calloc( hunkTotal + 31, 1 );
//
//	hunkData pointer:
//	0x0000000155000000
//	cast is done so that we can do bitwise operations, we cast back to
//	internal representation after (uint8_t *)
//
//	intptr_t (16 byte / 64 bit) representation of the pointer:
//	5721030666
//	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 1
//	32  0 1 0 1  0 1 0 1   0 0 0 0  0 0 0 0
//	16  0 0 0 0  0 0 0 0   0 0 0 0  1 0 1 0
//
//	add another 31. when we align; it will not be below the hunkTotal
//	(if it needs to be rounded down)
//
//	5721030697
//	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 1
//	32  0 1 0 1  0 1 0 1   0 0 0 0  0 0 0 0
//	16  0 0 0 0  0 0 0 0   0 0 1 0  1 0 0 1
//
//	& ~31 (round to nearest 32)
//      64  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	48  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	32  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	16  1 1 1 1  1 1 1 1   1 1 1 0  0 0 0 0
//
//	= 5721030688
//	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 1
//	32  0 1 0 1  0 1 0 1   0 0 0 0  0 0 0 0
//	16  0 0 0 0  0 0 0 0   0 0 1 0  0 0 0 0
//				     |
//	thanks to the low bits found in ~31, the last 5 positions are dropped
//
//	essentially, we started with 5721030666 bytes
//	(which is not divisible by 32)
//	and we ended with a padded region of 5721030688
//	(which is)
//
//	we lose the tinyest bit of memory at begining of the pointer by
// 	increasing its raw value
// 	(hunkData is now pointing to a memory address 32-ish bytes
// 	infront of what was actually reserved)
//
//	but we gain perf improvements due to the cpu pulling in even chunks
//	into the cache
// ------------------------------------------------------------------------------

int H_InitHunk(int minMegsAlloc) {
  hunkTotal = minMegsAlloc * 1024 * 1024;
  int cachelineMask = 31;
  printf("intended hunk total: %d \n", hunkTotal);

  hunkData = (uint8_t *)calloc(hunkTotal + cachelineMask, 1);

  if (!hunkData) {
    printf("hunk failed to init with: %d bytes \n",
           (hunkTotal + cachelineMask));
    return 0;
  }

  printf("hunk initialised with: %d bytes \n", (hunkTotal + cachelineMask + 1));

  // this will always align it to the mask
  hunkData =
      (uint8_t *)(((uintptr_t)hunkData + cachelineMask) & ~cachelineMask);

  return 1;
}

#endif
