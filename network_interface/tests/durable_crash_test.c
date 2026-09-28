#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include "export_db_lua.h"
#include "crud.h"
#include "lualib.h"
#include "lua_start.h"
int luaopen_db(lua_State *);
lua_State *__real_luaL_newstate(void);
lua_State *__wrap_luaL_newstate(void)
{
    lua_State *state=__real_luaL_newstate();
    if(state){ luaL_openlibs(state); luaL_requiref(state,"db",luaopen_db,1); lua_pop(state,1); }
    return state;
}
static const char *crash_phase;
static int crash_index, fail_write_at, fail_sync_at, writes, syncs, inject;
ssize_t __real_write(int, const void *, size_t);
int __real_fsync(int);
ssize_t __wrap_write(int fd, const void *p, size_t n)
{
    if(inject && ++writes==fail_write_at){ errno=ENOSPC; return -1; }
    return __real_write(fd,p,n);
}
int __wrap_fsync(int fd)
{
    if(inject && ++syncs==fail_sync_at){ errno=EIO; return -1; }
    return __real_fsync(fd);
}
int db_durability_test_hook(const char *phase, int index)
{
    if(crash_phase && !strcmp(phase,crash_phase) && index==crash_index){
        raise(SIGKILL); abort();
    }
    return 0;
}
static void fixture(char *name)
{
    file_t fds[3]; INIT_FILE_T_ARRAY(fds,3);
    char paths[3][MAX_FILE_PATH_LENGTH]={0};
    struct Schema sch={0}; struct Header_d hd={0,0,&sch};
    assert(open_files(name,fds,paths,CREATE_FILE)==0);
    assert(create_file_definition_with_no_value(TYPE_DF,1,"name:t_s",&sch));
    assert(create_header(&hd) && write_header(fds[2],&hd));
    close_file(3,fds[0],fds[1],fds[2]); free_schema(&sch);
}
static void lua(lua_State *state, const char *code)
{
    if(luaL_dostring(state,code)!=LUA_OK){ fprintf(stderr,"%s\n",lua_tostring(state,-1)); abort(); }
}
static lua_State *runtime(void)
{
    lua_State *state=luaL_newstate(); assert(state); luaL_openlibs(state);
    luaL_requiref(state,"db",luaopen_db,1); lua_pop(state,1);
    lua(state,"TEST=false"); return state;
}
static void writer(const char *root)
{
    assert(db_durable_open(root)==0);
    lua_State *state=runtime(); db_durable_request(1);
    lua(state,
        "local function rec(f,n) return {file_name=f,offset=0,fields_number=1,fields={name=n}} end\n"
        "local k,e=db.order_transaction('head','lines',function()\n"
        " assert(db.write_record('head',rec('head','durable head'),1))\n"
        " assert(db.write_record('lines',rec('lines','durable line'),1))\n"
        " return 1,0 end)\n"
        "assert(k==1 and e==0)\n");
    inject=1;
    int result=db_durable_commit(dbCache,CACHE_SIZE);
    inject=0;
    if(result<0){ assert(db_durable_failed()); _exit(40); }
    assert(!db_durable_failed());
    /* No flush / atexit: success must already be durable. */
    _exit(0);
}
static int wait_child(pid_t pid)
{
    int status; assert(waitpid(pid,&status,0)==pid); return status;
}
static void verify(const char *root, int expected)
{
    pid_t pid=fork(); assert(pid>=0);
    if(!pid){
        crash_phase=NULL; inject=0;
        assert(db_durable_open(root)==0);
        lua_State *state=runtime();
        lua_pushinteger(state,expected); lua_setglobal(state,"expected");
        lua(state,
            "local h=db.get_record('head',1); local l=db.get_record('lines',1)\n"
            "assert((h==nil)==(l==nil), 'mixed transaction')\n"
            "if expected==0 then assert(h==nil) elseif expected==1 then assert(h~=nil) end\n"
            "if h then assert(h.fields.name=='durable head' and l.fields.name=='durable line') end\n");
        assert(access(".wser-durable/pending",F_OK)<0 && errno==ENOENT);
        assert(access(".wser-durable/building",F_OK)<0 && errno==ENOENT);
        db_durable_close(); lua_close(state); _exit(0);
    }
    int status=wait_child(pid); assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void remove_tree(const char *path)
{
    DIR *dir=opendir(path); assert(dir);
    struct dirent *entry;
    while((entry=readdir(dir))){
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        char child[8192]; snprintf(child,sizeof(child),"%s/%s",path,entry->d_name);
        if(entry->d_type==DT_DIR) remove_tree(child); else assert(unlink(child)==0);
    }
    closedir(dir); assert(rmdir(path)==0);
}
static void setup(char *root)
{
    assert(mkdtemp(root) && chdir(root)==0); fixture("head"); fixture("lines");
}
static void finish(const char *root) { assert(chdir("/")==0); remove_tree(root); }
static void crash_case(const char *phase,int index,int expected)
{
    char root[]="/tmp/db-crash-XXXXXX"; setup(root);
    crash_phase=phase; crash_index=index;
    pid_t pid=fork(); assert(pid>=0); if(!pid) writer(root);
    int status=wait_child(pid);
    assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL);
    verify(root,expected); finish(root);
}
static void repeated_recovery(void)
{
    char root[]="/tmp/db-replay-XXXXXX"; setup(root);
    crash_phase="committed"; crash_index=0;
    pid_t pid=fork(); assert(pid>=0); if(!pid) writer(root);
    int status=wait_child(pid); assert(WIFSIGNALED(status));
    const char *phases[]={"before_install","after_rename","installed"};
    for(int i=0;i<3;++i){
        pid=fork(); assert(pid>=0);
        if(!pid){ crash_phase=phases[i]; crash_index=i; db_durable_open(root); _exit(99); }
        status=wait_child(pid); assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL);
    }
    verify(root,1); finish(root);
}
static void corruption_case(const char *name)
{
    char root[]="/tmp/db-corrupt-XXXXXX"; setup(root);
    crash_phase="committed"; crash_index=0;
    pid_t pid=fork(); assert(pid>=0); if(!pid) writer(root);
    int status=wait_child(pid); assert(WIFSIGNALED(status));
    char path[128]; snprintf(path,sizeof(path),".wser-durable/pending/%s",name);
    int fd=open(path,O_WRONLY|O_TRUNC); assert(fd>=0); assert(write(fd,"bad",3)==3); close(fd);
    assert(db_durable_open(root)==-1); /* Fail closed; retain the journal. */
    assert(access(".wser-durable/pending",F_OK)==0);
    fd=open("head.dat",O_RDONLY); assert(fd>=0 && lseek(fd,0,SEEK_END)==0); close(fd);
    db_durable_close(); finish(root);
}
static void io_failure_case(int writing,int nth)
{
    char root[]="/tmp/db-iofail-XXXXXX"; setup(root);
    crash_phase=NULL; writes=syncs=0;
    fail_write_at=writing?nth:0; fail_sync_at=writing?0:nth;
    pid_t pid=fork(); assert(pid>=0); if(!pid) writer(root);
    int status=wait_child(pid); assert(WIFEXITED(status));
    assert(WEXITSTATUS(status)==0 || WEXITSTATUS(status)==40);
    verify(root,WEXITSTATUS(status)==0?1:-1); /* Failed commit may be old or new, never mixed. */
    fail_write_at=fail_sync_at=0; finish(root);
}
static void exclusive_lock(void)
{
    char root[]="/tmp/db-lock-XXXXXX"; setup(root); crash_phase=NULL;
    assert(db_durable_open(root)==0);
    pid_t pid=fork(); assert(pid>=0);
    if(!pid){ db_durable_close(); assert(db_durable_open(root)==-1); _exit(0); }
    int status=wait_child(pid); assert(WIFEXITED(status) && !WEXITSTATUS(status));
    db_durable_close(); assert(db_durable_open(root)==0); db_durable_close(); finish(root);
}
static void worker_cache_integration(const char *config_path)
{
    char root[]="/tmp/db-worker-store-XXXXXX"; setup(root);
    FILE *f=fopen("config.lua","w"); assert(f);
    assert(fputs("db=require('db')\nTEST=false\n",f)>=0 && fclose(f)==0);
    pid_t pid=fork(); assert(pid>=0);
    if(!pid){
        crash_phase=NULL; inject=0;
        assert(setenv("WSER_DB_DIRECTORY",root,1)==0);
        assert(init_durable_lua("config.lua")==0);
        assert(luaL_dofile(L,config_path)==LUA_OK);
        lua(L,"customers='head'");
        db_durable_request(1);
        lua(L,"local r,k=write_customers({file_name='head',offset=0,fields_number=1,fields={name='Alice'}}); assert(r==0 and k==0)");
        assert(commit_lua_caches()==0); db_durable_request(0);
        lua(L,"assert(db.get_record('head',0).fields.name=='Alice')");
        db_durable_request(1);
        lua(L,"local r,k=write_customers({file_name='head',offset=0,fields_number=1,fields={name='Discard me'}}); assert(r==0 and k==1)");
        assert(discard_lua_caches()==0); db_durable_request(0);
        lua(L,"assert(db.get_record('head',0).fields.name=='Alice'); assert(db.get_record('head',1)==nil); assert(db.get_record('head','Discard me',2)==nil)");
        lua(L,"assert(not pcall(db.delete_record,'head',0)); assert(not pcall(db.create_record,'head','name:X')); assert(not pcall(db.write_record,'head',{},2))");
        close_lua(); _exit(0);
    }
    int status=wait_child(pid); assert(WIFEXITED(status) && !WEXITSTATUS(status));
    pid=fork(); assert(pid>=0);
    if(!pid){
        assert(db_durable_open(root)==0); lua_State *state=runtime();
        lua(state,"assert(db.get_record('head',0).fields.name=='Alice'); assert(db.get_record('head','Alice',2).fields.name=='Alice'); assert(db.get_record('head',1)==nil); assert(db.get_record('head','Discard me',2)==nil)");
        db_durable_close(); _exit(0);
    }
    status=wait_child(pid); assert(WIFEXITED(status) && !WEXITSTATUS(status));
    finish(root);
    puts("PASS: real worker initialization, customer secondary index, durable commit and failed-request cache discard");
}
int main(int argc, char **argv)
{
    assert(argc==2);
    worker_cache_integration(argv[1]);
    for(int i=0;i<6;++i) crash_case("snapshot",i,0);
    crash_case("prepared",0,0);
    crash_case("published",0,1); crash_case("committed",0,1);
    for(int i=0;i<6;++i){
        crash_case("before_install",i,1); crash_case("after_rename",i,1); crash_case("installed",i,1);
    }
    crash_case("checkpoint",0,1); crash_case("retired",0,1);
    repeated_recovery(); exclusive_lock();
    corruption_case("0"); corruption_case("manifest"); corruption_case("seal");
    for(int i=1;i<=90;++i) io_failure_case(1,i);
    for(int i=1;i<=28;++i) io_failure_case(0,i);
    puts("PASS: real-engine SIGKILL boundaries, repeated replay, exclusive lock, corrupt journal rejection, ENOSPC/fsync failures");
}
