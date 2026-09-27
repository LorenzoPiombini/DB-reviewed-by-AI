#ifndef LUA_START
#define LUA_START 1

#include <stdarg.h>
#ifdef FEDORA 
		#include "lua.h"
		#include "lauxlib.h"
		#include "lualib.h"

#else
		#include "lua5.4/lua.h"
		#include "lua5.4/lauxlib.h"
		#include "lua5.4/lualib.h"
#endif

#include "hash_tbl.h"
typedef int (*table_to_record_fn)(lua_State*,int,struct Record_f*,struct Schema*);
typedef int (*create_table)(ui8 *,char *);

extern table_to_record_fn tbl_to_rec;
extern lua_State *L;
extern struct Cache *dbcache_ptr; 
extern HashTable *cache_r_ptr;

#define LUA_CONFIG_FILE "/root/db/lua/db_config.lua"


int init_lua(char *config_file);
void clear_lua_stack();
int execute_lua_script(char *buf);
int execute_lua_function(char *func_name, char *func_sig,...);
void close_lua();
void check_config_file();
int get_function_signature(char *function_name,char *signature);
#endif
