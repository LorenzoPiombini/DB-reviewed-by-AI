#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include "allocator.h"

/* Align both metadata strides and payloads for all ordinary C scalar types. */
union zone_align { long double ld; void *ptr; long long ll; };
#define ALIGN sizeof(union zone_align)
#define ROUND(n) (((n) + ALIGN - 1) / ALIGN * ALIGN)
#define HEADER ROUND(sizeof(struct Memblock_s))
#define ZHEADER ROUND(sizeof(struct Memzone_t))
#define M_ZONE_ID 0x0F453210
#define MINFRAG 64
static void *unowned;
static void **zone_used = &unowned;
int mb_used = 8;
struct Memzone_t *mainzone;

int A_init_mainzone(void)
{
    size_t size;
    struct Memblock_s *block;
    if(mainzone) return 0; /* Also used by library callers, once per process. */
    if(mb_used <= 0 || (size_t)mb_used > (size_t)INT_MAX / (1024 * 1024)) {
        errno = EINVAL; return -1;
    }
    size = (size_t)mb_used * 1024 * 1024;
    mainzone = malloc(size);
    if(!mainzone) return -1;
    memset(mainzone,0,size);
    mainzone->size = size;
    block = (struct Memblock_s *)((unsigned char *)mainzone + ZHEADER);
    mainzone->blocklist.next = mainzone->blocklist.prev = block;
    mainzone->blocklist.user = zone_used;
    mainzone->blocklist.tag = M_STATIC;
    mainzone->rover = block;
    block->prev = block->next = &mainzone->blocklist;
    block->size = size - ZHEADER;
    return 0;
}

static struct Memblock_s *checked_block(void *ptr)
{
    uintptr_t p=(uintptr_t)ptr, begin=(uintptr_t)mainzone;
    struct Memblock_s *b;
    if(!mainzone || p < begin + ZHEADER + HEADER ||
       p >= begin + mainzone->size || (p - begin) % ALIGN) abort();
    b=(struct Memblock_s *)((unsigned char *)ptr - HEADER);
    if(b->id != M_ZONE_ID || !b->user) abort();
    return b;
}

static void split_block(struct Memblock_s *b,size_t size)
{
    struct Memblock_s *n;
    if(b->size - size < HEADER + MINFRAG) return;
    n=(struct Memblock_s *)((unsigned char *)b + size);
    memset(n,0,HEADER);
    n->size=b->size-size;
    n->prev=b; n->next=b->next; n->next->prev=n;
    b->next=n; b->size=size;
}

void *A_Malloc(size_t size,int tag,void *user)
{
    struct Memblock_s *b,*start;
    size_t need;
    void *result;
    if(tag >= M_PURGELEVEL && !user){ errno=EINVAL; return NULL; }
    if(size > (size_t)INT_MAX - HEADER - ALIGN){ errno=ENOMEM; return NULL; }
    if(A_init_mainzone() < 0) return NULL;
    need=ROUND(size ? size : 1)+HEADER;
    if(need > mainzone->size-ZHEADER){ errno=ENOMEM; return NULL; }
    /* Purge only explicitly discardable blocks. DB caches use M_STATIC. */
restart:
    b=start=mainzone->rover;
    do {
        if(b != &mainzone->blocklist){
            if(b->user && b->tag >= M_PURGELEVEL){
                A_free((unsigned char *)b+HEADER);
                goto restart; /* Coalescing invalidates traversal pointers. */
            }
            if(!b->user && b->size >= need){
                split_block(b,need);
                b->user=user ? (void **)user : zone_used;
                b->tag=tag; b->id=M_ZONE_ID;
                result=(unsigned char *)b+HEADER;
                memset(result,0,b->size-HEADER);
                if(user) *(void **)user=result;
                mainzone->rover=b->next;
                return result;
            }
        }
        b=b->next;
    } while(b != start);
    errno=ENOMEM; return NULL;
}

void A_free(void *ptr)
{
    struct Memblock_s *b,*other;
    if(!ptr) return;
    b=checked_block(ptr);
    if(b->user != zone_used) *b->user=NULL;
    b->user=NULL; b->tag=0; b->id=0;
    other=b->prev;
    if(!other->user){
        other->size+=b->size; other->next=b->next; b->next->prev=other;
        if(mainzone->rover==b) mainzone->rover=other;
        b=other;
    }
    other=b->next;
    if(!other->user){
        b->size+=other->size; b->next=other->next; other->next->prev=b;
        if(mainzone->rover==other) mainzone->rover=b;
    }
    memset((unsigned char *)b+HEADER,0,b->size-HEADER);
}

void *A_Realloc(void *ptr,size_t size,int tag,void *user)
{
    struct Memblock_s *b,*next;
    size_t need,old_size;
    void *result;
    void **owner;
    int old_tag;
    if(!ptr) return A_Malloc(size,tag,user);
    b=checked_block(ptr);
    if(!size){ A_free(ptr); return NULL; }
    owner=user ? (void **)user : b->user;
    if(tag >= M_PURGELEVEL && owner==zone_used){ errno=EINVAL; return NULL; }
    if(size > (size_t)INT_MAX-HEADER-ALIGN){ errno=ENOMEM; return NULL; }
    need=ROUND(size)+HEADER;
    old_size=b->size-HEADER;
    if(need > b->size){
        next=b->next;
        if(!next->user && next->size >= need-b->size){
            b->size+=next->size; b->next=next->next; next->next->prev=b;
            if(mainzone->rover==next) mainzone->rover=b;
            memset((unsigned char *)ptr+old_size,0,b->size-HEADER-old_size);
            split_block(b,need);
        }
    }
    if(need <= b->size){
        if(b->user != zone_used && b->user != owner) *b->user=NULL;
        b->user=owner; b->tag=tag;
        if(owner!=zone_used) *owner=ptr;
        return ptr;
    }
    /* Do not allow the allocation scan to purge the source being copied. */
    old_tag=b->tag; b->tag=M_STATIC;
    result=A_Malloc(size,M_STATIC,NULL);
    b->tag=old_tag;
    if(!result) return NULL; /* Source and its owner remain valid. */
    memcpy(result,ptr,old_size);
    A_free(ptr);
    b=checked_block(result); b->user=owner; b->tag=tag;
    if(owner!=zone_used) *owner=result;
    return result;
}

void A_change_tag(void *ptr,int tag)
{
    struct Memblock_s *b=checked_block(ptr);
    if(tag >= M_PURGELEVEL && b->user==zone_used) return;
    b->tag=tag;
}

void A_clear_zone(struct Memzone_t *zone)
{
    struct Memblock_s *b,*next;
    size_t size;
    if(!zone || zone!=mainzone) abort();
    for(b=zone->blocklist.next;b!=&zone->blocklist;b=next){
        next=b->next;
        if(b->user && b->user!=zone_used) *b->user=NULL;
    }
    size=zone->size;
    memset(zone,0,size); zone->size=size;
    b=(struct Memblock_s *)((unsigned char *)zone+ZHEADER);
    zone->blocklist.next=zone->blocklist.prev=zone->rover=b;
    zone->blocklist.user=zone_used; zone->blocklist.tag=M_STATIC;
    b->prev=b->next=&zone->blocklist; b->size=size-ZHEADER;
}
void A_close_mainzone(void)
{
    if(!mainzone) return;
    A_clear_zone(mainzone);
    free(mainzone); mainzone=NULL;
}
void *A_alloc(size_t n) { return A_Malloc(n,M_STATIC,NULL); }
void *A_calloc(size_t n,size_t size)
{
    if(size && n > (size_t)-1/size){ errno=ENOMEM; return NULL; }
    return A_alloc(n*size);
}
void *A_realloc(void *p,size_t n) { return A_Realloc(p,n,M_STATIC,NULL); }
char *A_strdup(const char *s)
{
    size_t n=strlen(s)+1;
    char *p=A_alloc(n);
    if(p) memcpy(p,s,n);
    return p;
}
