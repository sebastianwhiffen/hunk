#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TAG_FREE,
    TAG_GENERAL,
    TAG_BOTLIB,
    TAG_RENDERER,
    TAG_SMALL,
    TAG_STATIC
} memtag_t;

void H_MemInfo();

int H_InitHunk();

void *H_HunkAlloc(int size);

#ifdef HUNK_IMPL

#define ZONEID 0x1d4a11
#define MINFRAGMENT 64

void (*errorFunc)(const char *message);

typedef struct memblock_s {
    int size; // including the header and possibly tiny fragments
    int tag;  // a tag of 0 is a free block
    struct memblock_s *next, *prev;
    int id; // should be ZONEID
} memblock_t;

typedef struct {
    int size;             // total bytes malloced, including header
    int used;             // total bytes used
    memblock_t blocklist; // start / end cap for linked list
    memblock_t *rover;
} memzone_t;

// main zone for all "dynamic" memory allocation
static memzone_t *mainzone;
// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
static memzone_t *smallzone;

typedef struct {
    int magic;
    int size;
} hunkHeader_t;

typedef struct {
    int mark;
    int permanent;
    int temp;
    int tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
    int size;
    uint8_t printed;
    struct hunkblock_s *next;
    char *label;
    char *file;
    int line;
} hunkblock_t;

static hunkblock_t *hunkblocks;

static hunkUsed_t hunk_low, hunk_high;
static hunkUsed_t *hunk_permanent, *hunk_temp;

static uint8_t *hunkData = NULL;
static int hunkTotal;

static	int		s_zoneTotal;
static	int		s_smallZoneTotal;


void H_SetErrorFunc(void (*func)(const char *message)) { errorFunc = func; }

void H_MemInfo() { printf("hi"); }

static void Z_ClearZone( memzone_t *zone, int size ) {
	memblock_t	*block;
	
	// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block =
		(memblock_t *)( (char *)zone + sizeof(memzone_t) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;
	zone->size = size;
	zone->used = 0;
	
	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}

void Z_InitSmallZoneMemory( void ) {
	s_smallZoneTotal = 512 * 1024;
	smallzone = calloc( s_smallZoneTotal, 1 );
	if ( !smallzone ) {
		exit(1);
		// errorFunc("Small zone data failed to allocate %1.1f megs", (float)s_smallZoneTotal / (1024*1024) );
	}
	Z_ClearZone( smallzone, s_smallZoneTotal );
}

// zone malloc
void *Z_TagMalloc(int size, int tag) {

    int extra;
    memblock_t *start, *rover, *new, *base;
    memzone_t *zone;

    if (tag == TAG_SMALL) {
        zone = smallzone;
    } else {
        zone = mainzone;
    }

    //
    // scan through the block list looking for the first free block
    // of sufficient size
    //
    size += sizeof(memblock_t); // account for size of block header
    size += 4;                  // space for memory trash tester
    size = (size + sizeof(intptr_t) - 1) &
           ~(sizeof(intptr_t) - 1); // align to 32/64 bit boundary

    base = rover = zone->rover;
    start = base->prev;

    do {
        if (rover == start) {
            printf("cant allocate");
            return NULL;
        }
        if (rover->tag) {
            base = rover = rover->next;
        } else {
            rover = rover->next;
        }
        // rove until region that fits the size is found
    } while (base->tag || base->size < size);

    extra = base->size - size;
    if (extra > MINFRAGMENT) {
        // there will be a free fragment after the allocated block
        new = (memblock_t *)((char *)base + size);
        new->size = extra;
        new->tag = 0; // free block
        new->prev = base;
        new->id = ZONEID;
        new->next = base->next;
        new->next->prev = new;
        base->next = new;
        base->size = size;
    }

    base->tag = tag; // no longer a free block

    zone->rover = base->next; // next allocation will start looking here
    zone->used += base->size; //

    base->id = ZONEID;

    *(int *)((char *)base + base->size - 4) = ZONEID;

    return (void *)((char *)base + sizeof(memblock_t));
}

void Z_Free( void *ptr ) {
	memblock_t	*block, *other;
	memzone_t *zone;
	
	if (!ptr) {
	errorFunc("Z_Free: NULL pointer");
	}

	block = (memblock_t *) ( (char *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		errorFunc("Z_Free: freed a pointer without ZONEID" );
	}
	if (block->tag == 0) {
		errorFunc("Z_Free: freed a freed pointer" );
	}
	// if static memory
	if (block->tag == TAG_STATIC) {
		return;
	}

	// check the memory trash tester
	if ( *(int *)((char *)block + block->size - 4 ) != ZONEID ) {
		errorFunc("Z_Free: memory block wrote past end" );
	}

	if (block->tag == TAG_SMALL) {
		zone = smallzone;
	}
	else {
		zone = mainzone;
	}

	zone->used -= block->size;
	// set the block to something that should cause problems
	// if it is referenced...
	memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = 0;		// mark as free
	
	other = block->prev;
	if (!other->tag) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == zone->rover) {
			zone->rover = other;
		}
		block = other;
	}

	zone->rover = block;

	other = block->next;
	if ( !other->tag ) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
	}
}

int H_Clear() {

    hunk_low.mark = 0;
    hunk_low.permanent = 0;
    hunk_low.temp = 0;
    hunk_low.tempHighwater = 0;

    hunk_high.mark = 0;
    hunk_high.permanent = 0;
    hunk_high.temp = 0;
    hunk_high.tempHighwater = 0;

    hunk_permanent = &hunk_low;
    hunk_temp = &hunk_high;

    hunkblocks = NULL;

    return 1;
}

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
//
//	more memory may be allocated by your systems malloc.
//	but our internal representation is aligned; so no worries.
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

    printf("hunk initialised with: %d bytes \n",
           (hunkTotal + cachelineMask + 1));

    // this will always align it to the mask
    // we add the size of another mask to ensure that the size here cannot round
    // beneath the address
    hunkData =
        (uint8_t *)(((uintptr_t)hunkData + cachelineMask) & ~cachelineMask);
    H_Clear();

    return 1;
}

void *H_HunkAlloc(int size) {
    void *buf;

    if (hunkData == NULL) {
        printf("hunk data not initialised");
    }

    size = (size + 31) & ~31;

    memset(buf, 0, size);

    return buf;
}

#endif
