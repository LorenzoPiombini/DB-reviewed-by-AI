#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"
extern struct Memzone_t *mainzone;
int main(void)
{
    void *owner=NULL,*p,*q,*blocker,*cache=NULL;
    size_t i; void *blocks[2048]; unsigned n=0;
    /* Library first use, zero fill, alignment, idempotent initialization. */
    p=A_alloc(9); assert(p && A_init_mainzone()==0);
    for(i=0;i<9;i++) assert(((unsigned char *)p)[i]==0);
    assert((uintptr_t)p % _Alignof(max_align_t)==0);
    memset(p,0x5a,9);
    q=A_realloc(p,10); assert(q==p && ((unsigned char *)q)[8]==0x5a);
    q=A_realloc(q,4); assert(q==p);
    A_free(q);
    p=A_Malloc(16,M_STATIC,&owner); assert(owner==p);
    blocker=A_alloc(64); assert(blocker); /* Force a moving realloc. */
    memset(p,0x42,16);
    q=A_Realloc(p,512,M_STATIC,&owner); assert(q && q!=p && owner==q);
    for(i=0;i<16;i++) assert(((unsigned char *)q)[i]==0x42);
    for(i=16;i<512;i++) assert(((unsigned char *)q)[i]==0);
    p=A_Realloc(q,(size_t)-1,M_STATIC,&owner);
    assert(!p && owner==q && ((unsigned char *)q)[0]==0x42);
    A_free(q); assert(!owner); A_free(blocker);
    p=A_Realloc(NULL,16,M_STATIC,&owner); assert(p==owner);
    assert(!A_Realloc(p,0,M_STATIC,&owner) && !owner);
    assert(!A_calloc((size_t)-1,2));
    assert(!A_Malloc(10,M_CACHE,NULL));
    p=A_Malloc(16,M_CACHE,&cache); assert(p && cache==p);
    while(n<2048 && (blocks[n]=A_alloc(8192))) n++;
    assert(n<2048 && !cache); /* Purged, owner cleared; bounded zone. */
    while(n) A_free(blocks[--n]);
    p=A_Malloc(128,M_STATIC,&owner); assert(p);
    memset(p,0xff,128); A_clear_zone(mainzone); assert(!owner);
    p=A_alloc(128); assert(p);
    for(i=0;i<128;i++) assert(((unsigned char *)p)[i]==0);
    A_free(p);
    /* Fragmentation and adjacent merging, with contents preserved on growth. */
    for(i=0;i<500;i++){ blocks[i]=A_alloc(i%31+1); assert(blocks[i]); }
    for(i=0;i<500;i+=2) A_free(blocks[i]);
    for(i=1;i<500;i+=2) A_free(blocks[i]);
    p=A_alloc(7*1024*1024); assert(p); A_free(p);
    A_close_mainzone(); A_close_mainzone();
    assert(A_init_mainzone()==0); A_close_mainzone();
    puts("PASS: zone ownership, zeroing, alignment, realloc, exhaustion, purge and coalescing");
}
