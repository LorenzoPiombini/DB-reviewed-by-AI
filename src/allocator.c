#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "allocator.h"

#if defined(__x86_64__) || defined(_M_X64)
#	define ALIGN  8
#elif defined(__i386__) || defined(_M_IX86)
#	define ALIGN  4
#endif

static void **zone_used = NULL;

#define M_ZONE_ID 0x0F453210
int mb_used = 8;


struct Memzone_t *mainzone;

int A_init_mainzone(void)
{
	struct Memblock_s *block;
	int32_t size = MEM_SIZE(mb_used);
	
	/*Marker will be used to mark a block in used*/
	int *marker =  malloc(sizeof(int));
	if( !marker) return -1;

	memset(marker,0,sizeof *marker);
	zone_used = (void*)marker;	

	mainzone = (struct Memzone_t*)malloc(size);
	if(!mainzone) return -1;
	memset(mainzone,0,size);

	mainzone->size = size; 
	
	mainzone->blocklist.next = 
	mainzone->blocklist.prev = 
	block = (struct Memblock_s *)((uint8_t*)mainzone + sizeof *mainzone);

	mainzone->blocklist.user = (void*)mainzone;
	mainzone->blocklist.tag = M_STATIC;
	mainzone->rover = block;
		
	block->prev = block->next = &mainzone->blocklist;
	
	block->user = NULL;
	block->size = mainzone->size - sizeof *mainzone;
	return 0;
}

#define MINFRAG 64

void *A_Malloc(int size, int tag, void *user)
{
	int32_t extra = 0;
	struct Memblock_s *start;
	struct Memblock_s *rover;
	struct Memblock_s *newblock;
	struct Memblock_s *base;

	/*allign the size to 4*/
	size = (size + ALIGN -1) & ~(ALIGN -1);

	size += sizeof *newblock;


	base = mainzone->rover;

	/*if a free block is behind the rover */
	if(!base->prev->user) base = base->prev;
	
	rover = base;
	start = base->prev;
	
	do{
		if(rover == start) return NULL;
		
		if(rover->user){
			if(rover->tag < M_PURGELEVEL){
				base = rover = rover->next;
			}else{
				base = base->prev;	
				A_free((uint8_t*)rover + sizeof *rover);
				base = base->next;
				rover = base->next;
			}
		}else{
			rover = rover->next;
		}
	}while(base->user || base->size < size);

	/*big enough block*/
	extra = base->size - size;
	if(extra > MINFRAG){
		newblock = (struct Memblock_s*)((uint8_t *)base + size);
		newblock->size = extra;
		
		newblock->user = NULL;
		newblock->tag = 0;
		newblock->prev = base;
		newblock->next = base->next;
		newblock->next->prev = newblock;

		base->next = newblock;
		base->size = size;
	}
	
	if(user){
		base->user = user;
		*(void**)user = (void*)((uint8_t*)base + sizeof *base);
	}else{
		base->user = zone_used;
	}

	base->tag = tag;
	base->id = M_ZONE_ID;

	mainzone->rover = base->next;
	return (void *)((uint8_t*)base + sizeof *base);
}

void A_free(void *m)
{

	if(!m) return;
	struct Memblock_s *block;
	struct Memblock_s *other;

	block = (struct Memblock_s *)((uint8_t 	*)m - sizeof *block);
	if(block->id != M_ZONE_ID) exit(0);

	

	if(block->user != zone_used) *block->user = NULL;
	
	block->user = NULL;
	block->tag = 0;
	block->id = 0;

	other = block->prev;
	if(!other->user){
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		
		if(block == mainzone->rover)	
			mainzone->rover = other;

		block = other;
	}
	
	other = block->next;

	if(!other->user){
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		
		if(other == mainzone->rover)
			mainzone->rover = block;
	}

	/*
		clear memory
		it is important to clear memory after the merging so we clean
		all absorbed stale headers block
	*/
	memset((uint8_t*)block + sizeof *block,0,block->size - sizeof *block);
}
void *A_Realloc(void *ptr,int size, int tag, void *user)
{
	size = (size + (ALIGN -1)) & ~(ALIGN-1);
	struct Memblock_s *block = (struct Memblock_s *)((uint8_t*)ptr - sizeof *block);
	if((block->size - (int)sizeof *block) >= size) return NULL;
	
	void *m = A_Malloc(size,tag,user);
	if(!m) return NULL;

	memcpy(m,ptr,block->size - sizeof *block);
		
	A_free(ptr);
	return m; 
}
void A_change_tag(void *ptr,int tag)
{
	struct Memblock_s *block;
	
	block = (struct Memblock_s *)((uint8_t*)ptr - sizeof *block);

	if(block->id != M_ZONE_ID) return;

	if(tag >= M_PURGELEVEL && block->user == zone_used) return;

	block->tag = tag;
}

void A_clear_zone(struct Memzone_t *zone)
{
	struct Memblock_s *block;		

	mainzone->blocklist.prev = 
	mainzone->blocklist.next = 
	block =(struct Memblock_s *) ((uint8_t *) zone + sizeof *zone);
	
	zone->blocklist.user = (void*)zone;
	zone->blocklist.tag = M_STATIC;
	zone->rover	= block;
	
	block->prev = block->next = &zone->blocklist;

	block->user = NULL;
	block->size = zone->size - sizeof *zone;
}

void A_close_mainzone(void) /*only for development*/
{
	free(mainzone);
}
