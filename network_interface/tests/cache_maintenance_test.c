#include <assert.h>
#include <unistd.h>
#include "../src/lua_start.c"

static struct Cache caches[30];
static HashTable indexes[30], registry;
static Node entry;
static long clock_now;
static int writes[30], removed, fail_slot = -1;
long now_seconds(void) { return clock_now; }
int write_cache_to_disk(struct Cache *cache)
{
    int slot = (int)(cache - caches);
    assert(slot >= 0 && slot < 30);
    ++writes[slot];
    return slot == fail_slot ? -1 : 0;
}
Node *ht_delete(void *key, HashTable *table, int type)
{
    assert(key && table == &registry && type == STR);
    ++removed;
    return &entry;
}
void free_ht_node(Node *node) { assert(node == &entry); }
void free_cache(struct Cache *cache) { memset(cache,0,sizeof(*cache)); }
static void setup(void)
{
    memset(caches,0,sizeof(caches));
    memset(writes,0,sizeof(writes));
    removed = 0; fail_slot = -1;
    clock_now = 10000; last_cache_flush = clock_now; last_cache_check = 0;
    dbcache_ptr = caches; cache_r_ptr = &registry;
    for(int i = 0; i < 2; ++i){
        caches[i].file_name = "test-file";
        caches[i].index_file = &indexes[i];
        caches[i].ts = caches[i].used = clock_now;
    }
}
int main(void)
{
    setup();
    clock_now += TWENTY_MINUTES - 1;
    check_config_file(); assert(!writes[0]);
    ++clock_now;
    check_config_file(); assert(writes[0] == 1 && writes[1] == 1 && !removed);
    for(int i = 0; i < 100; ++i) check_config_file();
    assert(writes[0] == 1); /* Idle polling must not rewrite every 250 ms. */
    clock_now -= 2000;
    check_config_file(); assert(writes[0] == 1);
    clock_now += TWENTY_MINUTES;
    check_config_file(); assert(writes[0] == 2);

    setup(); clock_now += THREE_HOURS;
    caches[1].used = clock_now; /* Frequently used old caches stay resident. */
    check_config_file();
    assert(removed == 1 && !caches[0].index_file && caches[1].index_file);
    setup(); clock_now += TWENTY_MINUTES; fail_slot = 0;
    check_config_file();
    assert(writes[0] == 1 && writes[1] == 1 && caches[0].index_file && !removed);
    setup(); clock_now += THREE_HOURS; fail_slot = 0;
    check_config_file();
    assert(caches[0].index_file && !caches[1].index_file && removed == 1);
    for(int i = 0; i < 100; ++i) check_config_file();
    assert(writes[0] == 1); /* Failed eviction is retried at most once a minute. */
    clock_now += 60; check_config_file(); assert(writes[0] == 2);

    setup();
    char path[] = "/tmp/db-config-test-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    const char invalid[] = "this is invalid Lua";
    assert(write(fd,invalid,sizeof(invalid)-1) == sizeof(invalid)-1);
    close(fd);
    L = luaL_newstate(); assert(L);
    loaded_config_file = strdup(path); assert(loaded_config_file);
    lua_pushinteger(L,42);
    sec = 0;
    for(int i = 0; i < 3; ++i){
        check_config_file();
        assert(lua_gettop(L) == 1 && lua_tointeger(L,1) == 42 && sec == 0);
    }
    FILE *config = fopen(path,"w"); assert(config);
    assert(fputs("reload_marker = 123\n",config) >= 0);
    assert(fclose(config) == 0);
    check_config_file(); assert(sec != 0 && lua_gettop(L) == 1);
    lua_getglobal(L,"reload_marker"); assert(lua_tointeger(L,-1) == 123); lua_pop(L,1);
    assert(unlink(path) == 0);
    clock_now += TWENTY_MINUTES;
    check_config_file(); assert(writes[0] == 1 && writes[1] == 1);
    close_lua(); assert(!loaded_config_file && !sec && !last_cache_flush);
    puts("PASS: timed maintenance, idle eviction, failure retention, actual config path, reload stack cleanup");
}
