#ifndef __ALLOCATOR__H
#define __ALLOCATOR__H 1
#include <stddef.h>

extern int mb_used;
#define MEM_SIZE(m) (m) * 1024 * 1024

#define M_STATIC 1
#define M_PURGELEVEL 100
#define M_CACHE 101

struct Memblock_s{
	void **user;
	struct Memblock_s *next, *prev;
	size_t size;
	int tag;
	int id;
};

struct Memzone_t{
	size_t size;
	struct Memblock_s blocklist;
	struct Memblock_s *rover; 
}; 

int A_init_mainzone(void);
void *A_Malloc(size_t size, int tag, void *user);
void A_free(void *m);
void A_clear_zone(struct Memzone_t *zone);
void A_change_tag(void *ptr,int tag);
void *A_Realloc(void *ptr,size_t size, int tag, void *user);
void A_close_mainzone(void); /*only for development*/

/* Zero-filled, single-threaded zone allocations. No fallback to libc heap.
 * Owner pointers must outlive their blocks. Never purge dirty DB caches.
 * A_Realloc failure preserves the source; size zero frees it.
 * Library allocations initialize the zone lazily; explicit init is idempotent. */
void *A_alloc(size_t n);
void *A_calloc(size_t count, size_t size);
void *A_realloc(void *ptr, size_t n);
char *A_strdup(const char *s);
#endif
