/* Full engine + Lua module, with real files and fresh reader processes. */
#include <assert.h>
#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "export_db_lua.h"
#include "lualib.h"

int luaopen_db(lua_State *state);
static void create_fixture(char *name)
{
    file_t fds[3]; INIT_FILE_T_ARRAY(fds,3);
    char paths[3][MAX_FILE_PATH_LENGTH] = {0};
    struct Schema schema = {0};
    struct Header_d header = {0,0,&schema};
    assert(open_files(name,fds,paths,CREATE_FILE) == 0);
    assert(create_file_definition_with_no_value(TYPE_DF,1,"name:t_s",&schema));
    assert(create_header(&header));
    assert(write_header(fds[2],&header));
    close_file(3,fds[0],fds[1],fds[2]);
    /* Failed schema copies must release partial zone allocations safely. */
    for(size_t budget=16;budget<=512;budget+=16){
        void *fill[10000]; size_t n=0;
        void *reserve=A_alloc(budget); assert(reserve);
        while(n<10000 && (fill[n]=A_alloc(4096))) ++n;
        while(n<10000 && (fill[n]=A_alloc(16))) ++n;
        assert(n<10000);
        A_free(reserve);
        struct Schema copy={0};
        int rc=copy_schema(&schema,&copy);
        assert(rc==0 || rc==-1);
        if(rc==0) assert(copy.fields_num==schema.fields_num);
        free_schema(&copy); /* Also safe after a failed partial copy. */
        while(n) A_free(fill[--n]);
    }
    free_schema(&schema);
}
static void run_lua(lua_State *state, const char *code)
{
    if(luaL_dostring(state,code) != LUA_OK){
        fprintf(stderr,"disk roundtrip: %s\n",lua_tostring(state,-1));
        abort();
    }
}
static void child(int writer)
{
    lua_State *state = luaL_newstate(); assert(state);
    luaL_openlibs(state);
    luaL_requiref(state,"db",luaopen_db,1); lua_pop(state,1);
    run_lua(state,"TEST=false");
    if(writer){
        run_lua(state,
            "local function rec(f,n) return {file_name=f,offset=0,fields_number=1,fields={name=n}} end\n"
            "for i=1,32 do\n"
            " local k,e=db.order_transaction('head','lines',function()\n"
            "  assert(db.write_record('head',rec('head','head-'..i),i))\n"
            "  assert(db.write_record('lines',rec('lines','line-'..i),i))\n"
            "  return i,0\n"
            " end)\n"
            " assert(k==i and e==0, tostring(k)..'/'..tostring(e))\n"
            "end\n"
            "local k,e=db.order_transaction('head','lines',function()\n"
            " assert(db.write_record('head',rec('head','must roll back'),33))\n"
            " assert(db.write_record('lines',rec('lines','must roll back'),33))\n"
            " error('injected application failure')\n"
            "end)\n"
            "assert(k==nil and e==-25)\n");
        for(int i=0;i<CACHE_SIZE;++i)
            if(dbCache[i].index_file) assert(write_cache_to_disk(&dbCache[i]) == 0);
    }
    run_lua(state,
        "for i=1,32 do\n"
        " local h=assert(db.get_record('head',i))\n"
        " local l=assert(db.get_record('lines',i))\n"
        " assert(h.fields.name=='head-'..i and l.fields.name=='line-'..i)\n"
        "end\n"
        "assert(db.get_record('head',33)==nil)\n"
        "assert(db.get_record('lines',33)==nil)\n");
    lua_close(state);
    _exit(0);
}
int main(void)
{
    char directory[] = "/tmp/db-roundtrip-XXXXXX";
    assert(mkdtemp(directory) && chdir(directory) == 0);
    create_fixture("head"); create_fixture("lines");
    for(int writer=1;writer>=0;--writer){
        pid_t pid=fork(); assert(pid>=0);
        if(!pid) child(writer);
        int status; assert(waitpid(pid,&status,0) == pid);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    assert(unlink("head.dat") == 0); assert(unlink("head.inx") == 0); assert(unlink("head.sch") == 0);
    assert(unlink("lines.dat") == 0); assert(unlink("lines.inx") == 0); assert(unlink("lines.sch") == 0);
    assert(chdir("/") == 0 && rmdir(directory) == 0);
    puts("PASS: real engine transactions, rollback, disk flush and fresh-process reload");
}
