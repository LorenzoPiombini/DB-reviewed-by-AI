#include <assert.h>
#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lualib.h"
static int alloc_attempt, fail_alloc;
static void *tx_malloc(size_t n) { if(++alloc_attempt==fail_alloc) return NULL; return A_alloc(n); }
static void *tx_calloc(size_t n,size_t size) { if(++alloc_attempt==fail_alloc) return NULL; return A_calloc(n,size); }
static char *tx_strdup(const char *s) { if(++alloc_attempt==fail_alloc) return NULL; return A_strdup(s); }
#define A_alloc tx_malloc
#define A_calloc tx_calloc
#define A_strdup tx_strdup
#include "../../lua/src/export_db_lua.c"
#undef A_alloc
#undef A_calloc
#undef A_strdup

/* Only persistence/loading is substituted. The real transaction, Lua order
 * function, cache buffers and index chains are exercised. */
static int calls, fail_at, throw_at, unavailable;
file_offset get(void *name,HashTable *table,int type)
{
    assert(table==&cache_register && type==STR);
    if(unavailable) return -1;
    if(!strcmp(name,"/root/db/sales_orders_head")) return 0;
    if(!strcmp(name,"/root/db/sales_orders_lines")) return 1;
    return -1;
}
long now_seconds(void) { return 123; }
int open_files(char *n,file_t *f,char p[3][MAX_FILE_PATH_LENGTH],int o)
{ (void)n;(void)f;(void)p;(void)o; assert(unavailable); return -1; }
int is_db_file(struct Header_d *h,file_t *f) { (void)h;(void)f; abort(); }
int cache_file(file_t *f,char *n,struct Schema *s,struct Cache *c,HashTable *h,int i)
{ (void)f;(void)n;(void)s;(void)c;(void)h;(void)i; abort(); }
void close_file(int count,...) { (void)count; abort(); }
int free_schema(struct Schema *s) { (void)s; abort(); }
file_offset go_to_EOF(file_t f) { (void)f; abort(); }
int write_cache_to_disk(struct Cache *c) { (void)c; abort(); }
void free_cache(struct Cache *c) { (void)c; abort(); }
Node *ht_delete(void *k,HashTable *t,int type) { (void)k;(void)t;(void)type; abort(); }
void free_ht_node(Node *n) { (void)n; abort(); }

static void seed(void)
{
    for(int i=0;i<2;++i){
        struct Cache *c=&dbCache[i];
        memset(c,0,sizeof(*c));
        c->file_name=i ? "/root/db/sales_orders_lines" : "/root/db/sales_orders_head";
        c->indexes=2; c->index_file=A_calloc(2,sizeof(HashTable)); assert(c->index_file);
        c->data_file.mem=A_alloc(4); assert(c->data_file.mem);
        memcpy(c->data_file.mem,"seed",4);
        c->data_file.size=c->data_file.capacity=c->data_file.offset=4;
        for(int j=0;j<2;++j){
            c->index_file[j].size=7;
            Node *a=A_calloc(1,sizeof(Node)),*b=A_calloc(1,sizeof(Node)); assert(a&&b);
            a->key.type=UINT; a->key.size=16; a->key.k.n16=42; a->value=7;
            b->key.type=STR; b->key.k.s=A_strdup("existing"); assert(b->key.k.s);
            b->value=8; a->next=b; c->index_file[j].data_map[0]=a;
        }
    }
}
static int mock_key(lua_State *L) { lua_pushinteger(L,101); return 1; }
static int mock_offset(lua_State *L)
{
    int i=(int)get((void*)lua_tostring(L,1),&cache_register,STR); assert(i>=0);
    lua_pushinteger(L,dbCache[i].data_file.size); return 1;
}
static int mock_write(lua_State *L)
{
    assert(order_tx_active);
    int i=(int)get((void*)lua_tostring(L,1),&cache_register,STR); assert(i>=0);
    struct Cache *c=&dbCache[i]; ++calls;
    /* Simulate an internal failure AFTER modifying data and multiple indexes. */
    c->data_file.mem=A_realloc(c->data_file.mem,c->data_file.size+1); assert(c->data_file.mem);
    c->data_file.mem[c->data_file.size++]='X';
    c->data_file.capacity=c->data_file.offset=c->data_file.size;
    for(int j=0;j<c->indexes;++j){
        Node *a=c->index_file[j].data_map[0]; a->value=99;
        a->next->key.k.s[0]='X';
    }
    if(calls==throw_at) return luaL_error(L,"injected Lua error");
    if(calls==fail_at){ lua_pushnil(L); lua_pushliteral(L,"injected storage error"); return 2; }
    lua_pushvalue(L,3); lua_pushvalue(L,2); return 2;
}
static void verify_rollback(HashTable **indexes,ui8 **buffers)
{
    assert(!order_tx_active);
    for(int i=0;i<2;++i){
        assert(dbCache[i].index_file==indexes[i] && dbCache[i].data_file.mem==buffers[i]);
        assert(dbCache[i].data_file.size==4 && dbCache[i].data_file.offset==4);
        assert(memcmp(dbCache[i].data_file.mem,"seed",4)==0);
        for(int j=0;j<2;++j){
            Node *a=dbCache[i].index_file[j].data_map[0];
            assert(a->value==7 && a->key.k.n16==42 && a->next->value==8);
            assert(strcmp(a->next->key.k.s,"existing")==0);
        }
    }
}
int main(void)
{
    lua_State *L=luaL_newstate(); assert(L); luaL_openlibs(L);
    lua_newtable(L); lua_pushcfunction(L,l_order_transaction); lua_setfield(L,-2,"order_transaction");
    lua_setglobal(L,"test_db");
    assert(luaL_dostring(L,"package.preload.db=function() return test_db end")==LUA_OK);
    assert(luaL_dofile(L,"network_interface/lua/db_config.lua")==LUA_OK);
    lua_pushcfunction(L,mock_write); lua_setglobal(L,"write_record");
    lua_pushcfunction(L,mock_key); lua_setglobal(L,"get_numeric_key");
    lua_pushcfunction(L,mock_offset); lua_setglobal(L,"g_offset");
    assert(luaL_dostring(L,"function run_order() return write_orders({fields={sales_orders_head={lines_nr=2},sales_orders_lines={{fields={qty=1}},{fields={qty=2}}}}}) end")==LUA_OK);
    for(int mode=0;mode<5;++mode){
        seed(); calls=0; fail_at=mode>=1 && mode<=3 ? mode : 0; throw_at=mode==4 ? 2 : 0;
        HashTable *indexes[2]={dbCache[0].index_file,dbCache[1].index_file};
        ui8 *buffers[2]={dbCache[0].data_file.mem,dbCache[1].data_file.mem};
        lua_getglobal(L,"run_order"); assert(lua_pcall(L,0,2,0)==LUA_OK);
        if(mode==0){
            assert(lua_tointeger(L,-2)==101 && lua_tointeger(L,-1)==0);
            assert(dbCache[0].data_file.size==5 && dbCache[1].data_file.size==6);
            assert(!order_tx_active);
        } else {
            assert(lua_isnil(L,-2));
            assert(lua_tointeger(L,-1)==(mode==3 ? -21 : mode==4 ? -25 : -22));
            verify_rollback(indexes,buffers);
        }
        lua_pop(L,2);
        if(mode){
            calls=fail_at=throw_at=0;
            lua_getglobal(L,"run_order"); assert(lua_pcall(L,0,2,0)==LUA_OK);
            assert(lua_tointeger(L,-2)==101 && lua_tointeger(L,-1)==0);
            assert(dbCache[0].data_file.size==5 && dbCache[1].data_file.size==6);
            lua_pop(L,2);
        }
        for(int i=0;i<2;++i) order_tx_free(&dbCache[i]);
    }
    /* Fail each allocation used by the two snapshots, including mixed-key
     * collision chains and the second cache after the first has been cloned. */
    for(int failure=1;failure<=16;++failure){
        seed(); alloc_attempt=0; fail_alloc=failure; calls=fail_at=throw_at=0;
        HashTable *indexes[2]={dbCache[0].index_file,dbCache[1].index_file};
        ui8 *buffers[2]={dbCache[0].data_file.mem,dbCache[1].data_file.mem};
        lua_getglobal(L,"run_order"); assert(lua_pcall(L,0,2,0)==LUA_OK);
        assert(lua_isnil(L,-2) && lua_tointeger(L,-1)==-25 && calls==0);
        verify_rollback(indexes,buffers); lua_pop(L,2);
        fail_alloc=0;
        for(int i=0;i<2;++i) order_tx_free(&dbCache[i]);
    }
    seed();
    {
        void *fill[4096]; size_t used=0;
        HashTable *ix[2]={dbCache[0].index_file,dbCache[1].index_file};
        ui8 *mem[2]={dbCache[0].data_file.mem,dbCache[1].data_file.mem};
        while(used<4096 && (fill[used]=A_alloc(4096))) ++used;
        assert(used<4096);
        calls=0;
        lua_getglobal(L,"run_order"); assert(lua_pcall(L,0,2,0)==LUA_OK);
        assert(lua_isnil(L,-2) && lua_tointeger(L,-1)==-25 && !calls);
        verify_rollback(ix,mem); lua_pop(L,2);
        while(used) A_free(fill[--used]);
        for(int i=0;i<2;++i) order_tx_free(&dbCache[i]);
    }
    seed();
    HashTable *indexes[2]={dbCache[0].index_file,dbCache[1].index_file};
    ui8 *buffers[2]={dbCache[0].data_file.mem,dbCache[1].data_file.mem};
    unavailable=1; calls=0;
    lua_getglobal(L,"run_order"); assert(lua_pcall(L,0,2,0)==LUA_OK);
    assert(lua_isnil(L,-2) && lua_tointeger(L,-1)==-25 && !calls);
    unavailable=0; verify_rollback(indexes,buffers); lua_pop(L,2);
    lua_pushcfunction(L,l_get_offset_for_new_record); lua_setglobal(L,"real_offset");
    const char *rejections[]={
        "return db.order_transaction(sales_orders.head,sales_orders.lines,function() return db.order_transaction(sales_orders.head,sales_orders.lines,function() return 1,0 end) end)",
        "return db.order_transaction(sales_orders.head,sales_orders.lines,function() real_offset('other-file'); return 1,0 end)",
        "return db.order_transaction(sales_orders.head,sales_orders.lines,function() return 1 end)",
        "TEST=true; return write_orders({fields={sales_orders_head={lines_nr=1},sales_orders_lines={{fields={qty=1}}}}})"
    };
    for(size_t i=0;i<sizeof(rejections)/sizeof(*rejections);++i){
        assert(luaL_dostring(L,rejections[i])==LUA_OK);
        assert(lua_isnil(L,-2) && lua_tointeger(L,-1)==-25);
        verify_rollback(indexes,buffers); lua_pop(L,2);
    }
    for(int i=0;i<2;++i) order_tx_free(&dbCache[i]);
    lua_close(L);
    puts("PASS: actual Lua order transaction commits success; restores bytes and all indexes on first/second line, header, and Lua failures");
}
