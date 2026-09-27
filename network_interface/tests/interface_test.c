/* Linux socket/Lua integration regressions. No database files are accessed. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "file.h"
#include "lua_start.h"
#include "worker_process.h"
#include "end_points.h"
#include "json.h"

/* Storage is substituted: test shutdown ordering and failure propagation. */
static struct Cache test_cache[30];
static HashTable test_index;
static int shutdown_mode, in_operation, flushed, fail_flush;
static int durable_commits, durable_discards, fail_commit;
static pid_t signal_child;
static int parent_ready=-1, parent_result=-1;
static void send_termination_later(void)
{
    pid_t target=getpid();
    signal_child=fork(); assert(signal_child>=0);
    if(signal_child==0){ usleep(20000); _exit(kill(target,SIGTERM)!=0); }
}
int write_cache_to_disk(struct Cache *c)
{
    assert(shutdown_mode && !in_operation && L);
    assert(c==&test_cache[0] || c==&test_cache[29]);
    ++flushed;
    if(parent_result>=0) assert(write(parent_result,"F",1)==1);
    /* Repeated termination requests must not interrupt cleanup. */
    raise(SIGTERM);
    return fail_flush && c==&test_cache[0] ? -1 : 0;
}
void free_cache(struct Cache *c) { (void)c; abort(); }
Node *ht_delete(void *k, HashTable *t, int type)
{ (void)k; (void)t; (void)type; abort(); }
void free_ht_node(Node *n) { (void)n; abort(); }
long now_seconds(void) { return 0; }
void __wrap_check_config_file(void) {}

static int calls, accepted, queued, interrupted;
static int server_fds[256], client_fds[256];
static int counted(lua_State *state)
{
    (void)state; ++calls;
    if(shutdown_mode==3){ in_operation=1; raise(SIGTERM); in_operation=0; }
    return 0;
}
int __wrap_init_lua(char *path)
{
    (void)path;
    L=luaL_newstate(); assert(L);
    luaL_openlibs(L);
    if(shutdown_mode){
        memset(test_cache,0,sizeof(test_cache));
        test_cache[0].file_name="first"; test_cache[0].index_file=&test_index;
        test_cache[29].file_name="last"; test_cache[29].index_file=&test_index;
        dbcache_ptr=test_cache;
    }
    lua_pushcfunction(L,counted); lua_setglobal(L,"counted");
    assert(luaL_dostring(L,
        "function inspect(t) counted(); return t.fields.x end\n"
        "function write_customers(t) counted(); if t.fields.x then return -20, -1 end; return 0, 4294967297 end\n"
        "function write_item(t) counted(); if t.fields.x then return -24, 'failed' end; return 0, 'a\"b' end\n"
        "function write_orders(t) counted(); if t.fields.x then return nil,-20 end; return 4294967297,0 end\n"
        "function g_all_key(file, ...) counted(); if file:match('/item$') then return nil end; return '[1,2]' end\n"
        "function get_customer(k) counted(); return tostring(k) end\n"
        "function good_report() counted(); return string.rep('x',70000) end\n"
        "good_report_s='>s'\n"
        "function bad_report(i) counted(); return 'bad' end\n"
        "bad_report_s='i>s'\n"
        "huge_report_s=string.rep('s',100)\n") == LUA_OK);
    if(parent_ready>=0){ assert(write(parent_ready,"R",1)==1); close(parent_ready); parent_ready=-1; }
    return 0;
}
int __real_poll(struct pollfd *, nfds_t, int);
int __wrap_poll(struct pollfd *fds, nfds_t n, int timeout)
{
    assert(n==1 && timeout==250);
    if(shutdown_mode==6) return __real_poll(fds,n,timeout);
    if(shutdown_mode==4){
        if(!signal_child) send_termination_later();
        return __real_poll(fds,n,timeout);
    }
    if(shutdown_mode==1 || shutdown_mode==2){
        raise(shutdown_mode==1 ? SIGTERM : SIGINT);
        errno=EINTR; return -1;
    }
    fds[0].revents=POLLIN;
    return 1;
}
static int run_worker(void)
{
    int pair[2], before, after;
    assert(socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair)==0);
    int flags=fcntl(pair[0],F_GETFL);
    assert(prctl(PR_GET_PDEATHSIG,&before)==0);
    int result=work_process(pair[0]);
    assert(prctl(PR_GET_PDEATHSIG,&after)==0 && before==after);
    assert(fcntl(pair[0],F_GETFL)==flags);
    close(pair[0]); close(pair[1]);
    return result;
}
int __wrap_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
    (void)fd; (void)addr; (void)len;
    if(!interrupted++){ errno=EINTR; return -1; }
    /* Successful calls retain Lua results only until the next request. */
    assert(lua_gettop(L)<=2);
    if(accepted<queued){
        if(shutdown_mode==5) send_termination_later();
        return server_fds[accepted++];
    }
    errno=EBADF;
    return -1;
}
static void word(unsigned char *p, uint16_t n) { memcpy(p,&n,2); }
static size_t field(unsigned char *p, int type, const char *value)
{
    size_t n=strlen(value);
    word(p,1); p[2]=type; word(p+3,1); p[5]='x';
    word(p+6,n); memcpy(p+8,value,n); return n+8;
}
static void decode_tests(void)
{
    __wrap_init_lua(NULL);
    unsigned char b[2048]={0};
    size_t n=field(b,NUMBER_JS,"12.5");
    double result=0;
    assert(execute_lua_function("inspect","t>d",b,n,"test",&result)==0);
    assert(result==12.5 && calls==1);
    clear_lua_stack();
    const char *invalid[]={"1x","nan","inf","1e999",""};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){
        n=field(b,NUMBER_JS,invalid[i]);
        lua_pushinteger(L,42);
        assert(execute_lua_function("inspect","t>d",b,n,"test",&result)==-1);
        assert(lua_gettop(L)==1 && lua_tointeger(L,-1)==42);
        clear_lua_stack();
    }
    n=field(b,STRING_JS,"abc");
    for(size_t i=0;i<n;++i){
        char *out=NULL;
        assert(execute_lua_function("inspect","t>s",b,i,"test",&out)==-1);
        assert(lua_gettop(L)==0);
    }
    char *out=NULL;
    /* A complete table may occupy only a prefix of the readable buffer. */
    for(int padding=0;padding<2;++padding){
        memset(b+n,padding ? 0xa5 : 0,sizeof(b)-n);
        assert(execute_lua_function("inspect","t>s",b,sizeof(b),"test",&out)==0);
        assert(strcmp(out,"abc")==0);
        clear_lua_stack();
    }
    b[5]=0;
    assert(execute_lua_function("inspect","t>s",b,n,"test",&out)==-1);
    /* Array entries must carry OBJECT_JS, not an arbitrary tag. */
    word(b,1); b[2]=ARRAY_JS; word(b+3,1); b[5]='x';
    b[6]=STRING_JS; word(b+7,0);
    uint32_t stop=JSON_END_ARRAY; memcpy(b+9,&stop,4);
    assert(execute_lua_function("inspect","t>s",b,(size_t)13,"test",&out)==-1);
    /* Deep nesting must return an error, never overflow Lua's C stack. */
    n=0;
    for(int i=0;i<JSON_MAX_DEPTH+1;++i){
        word(b+n,1); b[n+2]=OBJECT_JS; word(b+n+3,1); b[n+5]='x'; n+=6;
    }
    word(b+n,0); n+=2;
    assert(execute_lua_function("inspect","t>s",b,n,"test",&out)==-1);
    assert(lua_gettop(L)==0 && calls==3);
    char sig[20];
    assert(get_function_signature("good_report",sig,sizeof(sig))==0);
    assert(strcmp(sig,">s")==0);
    assert(get_function_signature("huge_report",sig,sizeof(sig))==-1);
    assert(get_function_signature("missing",sig,sizeof(sig))==-1);
    assert(get_function_signature("good_report",sig,2)==-1);
    assert(lua_gettop(L)==0);
    close_lua(); close_lua();
    puts("PASS: malformed tokens, nesting limit, Lua stack recovery, signature bounds");
}
static int enqueue(const void *packet, size_t size)
{
    int pair[2]; assert(queued<256);
    assert(socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair)==0);
    server_fds[queued]=pair[0]; client_fds[queued]=pair[1];
    assert(send(pair[1],packet,size,MSG_NOSIGNAL)==(ssize_t)size);
    return queued++;
}
static int request(uint16_t op, const char *text)
{
    unsigned char b[256]={0}; word(b,op);
    size_t n=text ? strlen(text) : 0;
    if(text) memcpy(b+2,text,n);
    return enqueue(b,n+2);
}
static int write_request(uint16_t op)
{
    unsigned char b[12]={0}; word(b,op); b[2]=12;
    return enqueue(b,sizeof(b));
}
static int rejected_write(uint16_t op)
{
    unsigned char b[128]={0}; word(b,op);
    size_t n=field(b+10,STRING_JS,"reject")+10;
    b[2]=n;
    return enqueue(b,n);
}
static ssize_t receive(int i, char *b, size_t size)
{
    memset(b,0,size);
    ssize_t n=recv(client_fds[i],b,size-1,MSG_DONTWAIT);
    assert(n>=0);
    return n;
}
static void error_reply(int i)
{
    char b[1200]; ssize_t n=receive(i,b,sizeof(b));
    int16_t code; memcpy(&code,b,2);
    assert(n==2 && code==-1);
}
static void worker_tests(void)
{
    calls=accepted=queued=0;
    int short_packet=request(NEW_CUST,NULL);
    unsigned char tiny[3]={NEW_CUST,0,3}; int short_length=enqueue(tiny,3);
    unsigned char huge[9000]={0}; word(huge,NEW_CUST); int oversize=enqueue(huge,sizeof(huge));
    int customer=write_request(NEW_CUST), item=write_request(N_ITEM), order=write_request(NEW_SORD);
    int update=write_request(UPDATE_SORD);
    int bad_customer=rejected_write(NEW_CUST), bad_item=rejected_write(N_ITEM);
    int bad_order=rejected_write(NEW_SORD);
    int badreport=request(RPT,"bad_report"), hugesig=request(RPT,"huge_report");
    int report=request(RPT,"good_report");
    assert(send(client_fds[report],"\1",1,0)==1);
    int eof_report=request(RPT,"good_report");
    assert(shutdown(client_fds[eof_report],SHUT_WR)==0);
    int failed_list=request(ITEM_GET_ALL,NULL);
    int disconnected=write_request(NEW_CUST);
    close(client_fds[disconnected]); client_fds[disconnected]=-1;
    int first_list=queued;
    for(int i=0;i<100;++i) request(CUSTOMER_GET_ALL,NULL);
    int overflow=request(CUSTOMER_GET,"4294967296");
    int key=request(CUSTOMER_GET,"4294967295");
    assert(run_worker()==-1);
    assert(durable_commits==4 && durable_discards==3);
    assert(L==NULL && calls==111); /* 7 writes, 2 reports, 101 lists, 1 key */
    error_reply(short_packet); error_reply(short_length); error_reply(update);
    char b[72000];
    assert(receive(oversize,b,sizeof(b))==0);
    int16_t code;
    assert(receive(bad_customer,b,sizeof(b))>2); memcpy(&code,b,2); assert(code==-20);
    assert(receive(bad_item,b,sizeof(b))>2); memcpy(&code,b,2); assert(code==-24);
    assert(receive(bad_order,b,sizeof(b))>2); memcpy(&code,b,2); assert(code==-20);
    assert(receive(customer,b,sizeof(b))>2 && strstr(b+2,"4294967297"));
    assert(receive(order,b,sizeof(b))>2 && strstr(b+2,"4294967297"));
    assert(receive(item,b,sizeof(b))>2 && strcmp(b+2,"{\"message\":\"'a\\\"b' added!\"}")==0);
    assert(receive(badreport,b,sizeof(b))==1024);
    assert(receive(hugesig,b,sizeof(b))==1024);
    assert(receive(report,b,sizeof(b))==4);
    uint32_t size; memcpy(&size,b,4); assert(size==70000);
    assert(receive(report,b,sizeof(b))==70000);
    for(int i=0;i<70000;++i) assert(b[i]=='x');
    assert(receive(eof_report,b,sizeof(b))==4);
    assert(receive(eof_report,b,sizeof(b))==1024);
    assert(receive(failed_list,b,sizeof(b))==1024);
    for(int i=first_list;i<first_list+100;++i){
        assert(receive(i,b,sizeof(b))>0);
        assert(strcmp(b,"{ \"message\" : [1,2]}")==0);
    }
    assert(receive(overflow,b,sizeof(b))==1024);
    assert(receive(key,b,sizeof(b))==10 && strcmp(b,"4294967295")==0);
    for(int i=0;i<queued;++i){
        if(client_fds[i]<0) continue;
        assert(recv(client_fds[i],b,sizeof(b),MSG_DONTWAIT)==0);
        close(client_fds[i]);
    }
    puts("PASS: packet bounds, error replies, 64-bit IDs, escaped JSON, reports, repeated requests, key range, socket closure");
}
static void shutdown_tests(void)
{
    for(shutdown_mode=1;shutdown_mode<=5;++shutdown_mode){
        for(fail_flush=0;fail_flush<=1;++fail_flush){
            calls=accepted=queued=interrupted=flushed=0;
            signal_child=0;
            if(shutdown_mode==5){
                int pair[2]; assert(socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair)==0);
                server_fds[0]=pair[0]; client_fds[0]=pair[1]; queued=1;
            }
            if(shutdown_mode==3) write_request(NEW_CUST);
            assert(run_worker()==(fail_flush ? -1 : 0));
            if(signal_child){
                int status; assert(waitpid(signal_child,&status,0)==signal_child);
                assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
            }
            assert(flushed==2 && !L);
            assert(calls==(shutdown_mode==3 ? 1 : 0));
            if(queued) close(client_fds[0]);
        }
    }
    shutdown_mode=0;
    puts("PASS: SIGTERM/SIGINT idle shutdown, in-flight completion, all cache slots, repeated signals, flush failures");
}
static void parent_death_test(void)
{
    int result_pipe[2]; assert(pipe(result_pipe)==0);
    pid_t supervisor=fork(); assert(supervisor>=0);
    if(supervisor==0){
        close(result_pipe[0]);
        int ready[2]; assert(pipe(ready)==0);
        pid_t worker=fork(); assert(worker>=0);
        if(worker==0){
            close(ready[0]); parent_ready=ready[1]; parent_result=result_pipe[1];
            shutdown_mode=6; fail_flush=flushed=0;
            assert(prctl(PR_SET_PDEATHSIG,(long)SIGKILL,0L,0L,0L)==0);
            int result=run_worker();
            assert(write(parent_result,result==0 && flushed==2 ? "Y" : "N",1)==1);
            close(parent_result); _exit(0);
        }
        close(ready[1]); close(result_pipe[1]);
        char ready_byte; assert(read(ready[0],&ready_byte,1)==1 && ready_byte=='R');
        close(ready[0]); _exit(0); /* Kernel now delivers the worker's parent-death signal. */
    }
    close(result_pipe[1]);
    char result[3]; size_t used=0;
    while(used<sizeof(result)){
        struct pollfd event={.fd=result_pipe[0],.events=POLLIN};
        assert(__real_poll(&event,1,5000)>0);
        ssize_t n=read(result_pipe[0],result+used,sizeof(result)-used);
        assert(n>0); used+=(size_t)n;
    }
    assert(memcmp(result,"FFY",3)==0);
    close(result_pipe[0]);
    int status; assert(waitpid(supervisor,&status,0)==supervisor);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    puts("PASS: inherited SIGKILL parent-death policy replaced; parent exit flushes both caches");
}
static void durable_reply_test(void)
{
    calls=accepted=queued=0; shutdown_mode=0; fail_commit=1;
    durable_commits=durable_discards=0;
    int customer=write_request(NEW_CUST);
    assert(run_worker()==-1 && L==NULL);
    assert(durable_commits==1 && durable_discards==0 && calls==1);
    error_reply(customer);
    close(client_fds[customer]);
    fail_commit=0;
    puts("PASS: success waits for commit; failed commit sends no success and stops worker");
}
int main(void)
{
    setbuf(stdout,NULL);
    decode_tests(); worker_tests(); shutdown_tests(); parent_death_test(); durable_reply_test();
    L=luaL_newstate(); assert(L); luaL_openlibs(L);
    assert(luaL_dostring(L,"package.preload.db=function() return {} end")==LUA_OK);
    assert(luaL_dofile(L,"network_interface/lua/db_config.lua")==LUA_OK);
    assert(luaL_dofile(L,"network_interface/tests/order_test.lua")==LUA_OK);
    close_lua();
    return 0;
}

/* Worker acknowledgement ordering is checked separately from disk recovery. */
int __wrap_init_durable_lua(char *path) { return __wrap_init_lua(path); }
int __wrap_commit_lua_caches(void)
{
    ++durable_commits;
    assert(!in_operation);
    if(accepted && client_fds[accepted-1]>=0){
        char byte;
        assert(recv(client_fds[accepted-1],&byte,1,MSG_PEEK|MSG_DONTWAIT)==-1);
        assert(errno==EAGAIN || errno==EWOULDBLOCK); /* Not acknowledged yet. */
    }
    return fail_commit ? -1 : 0;
}
int __wrap_discard_lua_caches(void) { ++durable_discards; return 0; }
