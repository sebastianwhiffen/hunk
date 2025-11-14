#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void H_MemInfo();

void H_InitHunk();

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
// 	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//      48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//      32  0 0 0 0  0 0 0 0   1 0 1 0  0 0 0 0
//      16  0 0 0 0  0 0 0 0   0 0 0 1  1 1 1 1
//
//	add another 31 so that it doesnt mask under total
//	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	32  0 0 0 0  0 0 0 0   1 0 1 0  0 0 0 0
//	16  0 0 0 0  0 0 0 0   0 0 1 1  1 1 1 0
//
//	& ~31
//      64  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	48  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	32  1 1 1 1  1 1 1 1   1 1 1 1  1 1 1 1
//	16  1 1 1 1  1 1 1 1   1 1 1 0  0 0 0 0
//
//	= 10485792
//	64  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	48  0 0 0 0  0 0 0 0   0 0 0 0  0 0 0 0
//	32  0 0 0 0  0 0 0 0   1 0 1 0  0 0 0 0
//	16  0 0 0 0  0 0 0 0   0 0 1 0  0 0 0 0
//
//	essentially, the original size of the hunk was: 10485760 bytes
//	after padding:					10485792 bytes
//
//	this promisies that the returned bytes are
// ------------------------------------------------------------------------------

void H_InitHunk(int minMegsAlloc) {
  hunkTotal = minMegsAlloc * 1024 * 1024;
  int cachelineMask = 31;
  printf("intended hunk total: %d \n", hunkTotal);

  hunkData = (uint8_t *)calloc(hunkTotal + cachelineMask, 1);

  if (!hunkData) {
    printf("hunk failed to init with: %d bytes \n", (hunkTotal + cachelineMask));
  }

  printf("hunk initialised with: %d bytes \n", (hunkTotal + cachelineMask + 1));

  hunkData =
      (uint8_t *)(((uintptr_t)hunkData + cachelineMask) & ~cachelineMask);
}

#endif
