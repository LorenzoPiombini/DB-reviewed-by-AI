#ifndef __ALLOCATOR__H
#define __ALLOCATOR__H 1

extern int mb_used;
#define MEM_SIZE(m) (m) * 1024 * 1024

#define M_STATIC 1
#define M_PURGELEVEL 100
#define M_CACHE 101

struct Memblock_s{
	void **user;
	struct Memblock_s *next, *prev;
	int size;
	int tag;
	int id;
};

struct Memzone_t{
	int size;
	struct Memblock_s blocklist;
	struct Memblock_s *rover; 
}; 

int A_init_mainzone(void);
void *A_Malloc(int size, int tag, void *user);
void A_free(void *m);
void A_clear_zone(struct Memzone_t *zone);
void A_change_tag(void *ptr,int tag);
void *A_Realloc(void *ptr,int size, int tag, void *user);
void A_close_mainzone(void); /*only for development*/

#endif
