#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include the implementation to exercise its private eviction routine. Unused
 * Lua entry points are discarded by the linker; no live database is opened. */
#include "../../lua/src/export_db_lua.c"
static int registered, flushed, freed, node_freed, fail_write, fail_delete;
static Node entry;
int write_cache_to_disk(struct Cache *c)
{
    assert(c==dbCache && registered);
    ++flushed;
    return fail_write ? -1 : 0;
}
Node *ht_delete(void *key, HashTable *table, int type)
{
    assert(table==&cache_register && type==STR);
    assert(strcmp(key,"old-file")==0 && flushed==1 && registered);
    if(fail_delete) return NULL;
    registered=0;
    return &entry;
}
void free_ht_node(Node *node) { assert(node==&entry); ++node_freed; }
void free_cache(struct Cache *c)
{
    assert(!registered && node_freed==1);
    ++freed; memset(c,0,sizeof(*c));
}
int main(void)
{
    HashTable index={0};
    for(int mode=0;mode<3;++mode){
        memset(dbCache,0,sizeof(dbCache));
        dbCache[0].file_name="old-file"; dbCache[0].index_file=&index;
        dbCache[0].ts=1; dbCache[0].used=THREE_HOURS+2;
        registered=1; flushed=freed=node_freed=0;
        fail_write=mode==1; fail_delete=mode==2;
        int result=check_and_free_one_cache(dbCache);
        assert(flushed==1);
        if(mode==0) assert(result==0 && !registered && freed==1);
        else assert(result==-1 && registered && !freed && dbCache[0].index_file==&index);
    }
    puts("PASS: eviction removes old mapping; write/register failures retain the cache");
}
