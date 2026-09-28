#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "record.h"
#include "file.h"
#include "crud.h"
#include "date.h"
#include "lua_start.h"
#include "json.h"

lua_State *L = NULL;
static time_t sec = 0;
static char *loaded_config_file = NULL;
static time_t last_cache_flush = 0;
static time_t last_cache_check = 0;
static int top_lua_stack = 0;

static const int CACHE_SIZE = 30;
table_to_record_fn tbl_to_rec = NULL;
struct Cache *dbcache_ptr = NULL;
HashTable *cache_r_ptr =NULL;
static int load(lua_State *L, char *file_config);
static void free_inactive_caches(struct Cache *c);
static int create_lua_table(ui8 *data, char *file_name,size_t data_size,size_t *bwalked);
static int create_nested_lua_table(ui8 *data,size_t data_size,size_t *bwalked, unsigned depth);

int init_lua(char *config_file)
{
    if(L || !config_file) return -1;
    loaded_config_file = strdup(config_file);
    if(!loaded_config_file) return -1;
	L = luaL_newstate();
    if(!L) { free(loaded_config_file); loaded_config_file=NULL; return -1; }
	luaL_openlibs(L);

	if(load(L,config_file) == -1) goto failed;
		
	lua_getglobal(L,"dbCache_ptr");
	if(!lua_islightuserdata(L,-1)) goto failed;
	dbcache_ptr = (struct Cache *) lua_touserdata(L,-1);
	lua_pop(L,1);

	lua_getglobal(L,"cache_register_ptr");
	if(!lua_islightuserdata(L,-1)) goto failed;
	cache_r_ptr = (HashTable *) lua_touserdata(L,-1);
	lua_pop(L,1);

	lua_getglobal(L,"port_table_function");
	if(!lua_islightuserdata(L,-1)) goto failed;
	tbl_to_rec = (table_to_record_fn)lua_touserdata(L,-1);
	lua_pop(L,1);

    struct stat config_stat;
    if(stat(loaded_config_file,&config_stat) == 0) sec = config_stat.st_mtime;
    last_cache_flush = now_seconds();
	top_lua_stack = lua_gettop(L);
	return 0;
failed:
	close_lua();
	return -1;
}

/* Recover before Lua can load any records. The default matches db_config.lua. */
int init_durable_lua(char *config_file)
{
    const char *directory=getenv("WSER_DB_DIRECTORY");
    if(!directory) directory="/root/db";
    if(db_durable_open(directory)<0){
        fprintf(stderr,"database: cannot lock/recover durable store in %s\n",directory);
        return -1;
    }
    if(init_lua(config_file)<0){ db_durable_close(); return -1; }
    return 0;
}

int commit_lua_caches(void)
{
    if(!db_durable_active() || !dbcache_ptr) return -1;
    return db_durable_commit(dbcache_ptr,CACHE_SIZE);
}

/* All successful worker mutations are already checkpointed. Discarding caches
 * therefore rolls an unsuccessful request back to the last durable state. */
int discard_lua_caches(void)
{
    int i;
    if(!dbcache_ptr || !cache_r_ptr) return -1;
    for(i=0;i<CACHE_SIZE;++i){
        struct Cache *c=&dbcache_ptr[i];
        Node *entry;
        if(!c->index_file || !c->file_name) continue;
        entry=ht_delete(c->file_name,cache_r_ptr,STR);
        if(!entry) return -1;
        free_ht_node(entry);
        free_cache(c);
    }
    return 0;
}

/* Call only from normal execution, never from a signal handler. */
int flush_lua_caches(void)
{
    int result = 0;
    if(db_durable_active()) return commit_lua_caches();
    if(!dbcache_ptr) return 0;
    for(int i = 0; i < CACHE_SIZE; ++i){
        struct Cache *cache = &dbcache_ptr[i];
        if(!cache->file_name || !cache->index_file) continue;
        if(write_cache_to_disk(cache) == -1){
            fprintf(stderr,"database shutdown: failed to flush %s\n",cache->file_name);
            result = -1;
        }
    }
    return result; /* Attempt every slot, even if an earlier write failed. */
}

void close_lua()
{
	if(L) lua_close(L);
    L=NULL;
    dbcache_ptr=NULL;
    cache_r_ptr=NULL;
    tbl_to_rec=NULL;
    top_lua_stack=0;
    free(loaded_config_file);
    loaded_config_file=NULL;
    sec=0;
    last_cache_flush=0;
    last_cache_check=0;
    db_durable_close();
}

void check_config_file()
{
    /* Maintenance must run even when the configuration was moved/deleted. */
    if(dbcache_ptr) free_inactive_caches(dbcache_ptr);
    /* Executable Lua configuration must not mutate records outside a request.
     * Restart the durable worker to deploy configuration changes. */
    if(db_durable_active() || !L || !loaded_config_file) return;
    struct stat file_data;
    if(stat(loaded_config_file,&file_data) == -1) return;
    if(file_data.st_mtime != sec){
        int saved_top = lua_gettop(L);
        int result = load(L,loaded_config_file);
        lua_settop(L,saved_top); /* A failed reload leaves an error on the stack. */
        if(result == 0) sec = file_data.st_mtime;
    }
}
void clear_lua_stack()
{
	if(L) lua_settop(L,top_lua_stack);
}

int execute_lua_function(char *func_name, char *func_sig,...)
{
	if(!L || !func_name || !func_sig || !lua_checkstack(L,8)) return -1;
    int saved_top=lua_gettop(L);
	va_list vl;
	int narg, nres;

	va_start(vl,func_sig);
	lua_getglobal(L,func_name);

	for(narg = 0; *func_sig != '\0'; narg++,func_sig++){

		if(!lua_checkstack(L,8)) goto failed;

		switch(*func_sig){
			case 't':
			{
				ui8 *data = va_arg(vl,ui8*);	
				size_t data_size = va_arg(vl,size_t);
				char *file_name = va_arg(vl,char*);

				if(!data || !file_name) goto failed;
				size_t bwalked = 0; 
				/* data_size bounds readable bytes; one table need not consume them all. */
				if(create_lua_table(data,file_name,data_size,&bwalked) == -1) {
					goto failed;
				}
				break;
			}
			case 'r': goto failed; /* record arguments are not implemented */
			case 'd':	lua_pushnumber(L,va_arg(vl,double));			break;	/* double */	
			case 'i': 	lua_pushinteger(L,va_arg(vl,int));				break;	/* integer*/
			case 'I': 	lua_pushinteger(L,va_arg(vl,uint32_t));			break;	/*unsigned integer*/	
			case 'l': 	lua_pushinteger(L,va_arg(vl,long));				break;	/* long integer*/	
			case 's': 	{char *s = va_arg(vl,char*);lua_pushstring(L,s);break;}	/* string*/	
			case '>': 	func_sig++; goto fcall;/*end of input*/
			default: 	goto failed;
		}
	}

fcall:
	nres = strlen(func_sig);/*number of Lua function results*/

	/*execute the lua function*/
	if(lua_pcall(L,narg,nres,0) != 0){
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		lua_pop(L,1);
		goto failed;
	}

	if(nres > 0){
		/*get the results */ 
		nres = -nres;
		while(*func_sig){
			switch(*func_sig){
				case 'r':
					{
						/*record*/
						/*
							TODO:
							if(port_table_to_record(L,*va_arg(vl,struct Record_f**)) == -1){
							return -1;
							}
							*/
						break;
					}
				case 'd':
					{
						int is_num;
						double d = lua_tonumberx(L,nres,&is_num);
						if(!is_num){
							goto failed;
						}
						*va_arg(vl, double *) = d;
						break;
					}
				case 'i':
					{
						int is_num;
						int l = (int)lua_tointegerx(L,nres,&is_num);
						if(!is_num){
							/*get error code*/
							l = lua_tointegerx(L,-1,&is_num);
							*va_arg(vl, int*) = l;
							goto failed;
						}
						*va_arg(vl, int*) = l;
						break;
					}
				case 'l':
					{
						int is_num;
						long long l = (long long)lua_tointegerx(L,nres,&is_num);
						if(!is_num){
							/*get error code*/
							l = lua_tointegerx(L,-1,&is_num);
							*va_arg(vl, long long*) = l;
							goto failed;
						}
						*va_arg(vl, long long*) = l;
						break;
					}
				case 's':
					{
						char *s = (char*)lua_tostring(L,nres);
						if(!s){
							goto failed;
						}
						*va_arg(vl,char **) = s;
						break;
					}
				default:
					goto failed;
			}
			nres++;
			func_sig++;
		}
	}
	va_end(vl);
	return 0;
failed:
    lua_settop(L,saved_top);
    va_end(vl);
    return -1;
}

int get_function_signature(char *function_name,char *signature,size_t capacity)
{
    if(!L || !function_name || !signature || capacity==0) return -1;
    signature[0]=0;
    size_t size=strlen(function_name);
    if(size > 8192) return -1;
    char *var=malloc(size+3);
    if(!var) return -1;
    memcpy(var,function_name,size);
    memcpy(var+size,"_s",3);
    lua_getglobal(L,var);
    free(var);
    size_t length=0;
    const char *value=lua_tolstring(L,-1,&length);
    int result=-1;
    if(value && length < capacity && !memchr(value,0,length)){
        memcpy(signature,value,length);
        signature[length]=0;
        result=0;
    }
    lua_pop(L,1);
    return result;
}

static int load(lua_State *L, char *file_config)
{
	if(luaL_loadfile(L,file_config) || lua_pcall(L,0,0,0)){
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		return -1;
	}
	return 0;
}

static void free_inactive_caches(struct Cache *c)
{
    time_t now = now_seconds();
    if(now <= 0) return;
    /* On a clock adjustment, start a new interval instead of underflowing. */
    if(last_cache_flush == 0 || now < last_cache_flush) last_cache_flush = now;
    int periodic_flush = difftime(now,last_cache_flush) >= TWENTY_MINUTES;
    if(!periodic_flush && last_cache_check && now >= last_cache_check &&
       difftime(now,last_cache_check) < 60) return;
    last_cache_check = now; /* Bound retries for a failed inactive-cache flush. */
    if(periodic_flush) last_cache_flush = now;
    for(int i = 0; i < CACHE_SIZE; ++i){
        if(!c[i].file_name || !c[i].index_file) continue;
        time_t last_used = c[i].used ? c[i].used : c[i].ts;
        int inactive = last_used > 0 && difftime(now,last_used) >= THREE_HOURS;
        if(!inactive && !periodic_flush) continue;
        if(write_cache_to_disk(&c[i]) == -1){
            fprintf(stderr,"cache maintenance: failed to flush %s\n",c[i].file_name);
            continue; /* Retain this cache; still attempt the other files. */
        }
        if(inactive){
            Node *entry = ht_delete(c[i].file_name,cache_r_ptr,STR);
            if(!entry){
                fprintf(stderr,"cache maintenance: missing mapping for %s\n",c[i].file_name);
                continue;
            }
            free_ht_node(entry);
            free_cache(&c[i]);
        }
    }
}

/* 
 * create a table from json tokens
 * in this case a lua table is mappable to a Record_f
 * */
static int create_lua_table(ui8 *data, char *file_name,size_t data_size,size_t *bwalked)
{
	lua_newtable(L);
	lua_pushstring(L,file_name);
	lua_setfield(L,-2,"file_name");

	lua_pushinteger(L,0); /*you do not know this*/
	lua_setfield(L,-2,"offset");

	lua_pushlstring(L,"fields",6);
	lua_newtable(L);

	int fields_num = 0;
	if((fields_num = create_nested_lua_table(data,data_size,bwalked,1)) == -1) return -1;

	lua_pushinteger(L,fields_num);
	lua_setfield(L,-2,"fields_number");
	lua_settable(L,-3);
	return 0;
}

static int create_nested_lua_table(ui8 *data,size_t data_size,size_t *bwalked, unsigned depth)
{
    if(depth > JSON_MAX_DEPTH || !lua_checkstack(L, 8)) return -1;
	if((*bwalked + sizeof(ui16)) > data_size) return -1;

	ui16 fields_num = 0;
	memcpy(&fields_num,&data[*bwalked],sizeof(ui16));
	*bwalked += sizeof(ui16);

	int i;
	for(i = 0; i < fields_num; i++){
		if((*bwalked + sizeof(ui8)) > data_size) return -1;

		ui8 type = 0;
		memcpy(&type,&data[*bwalked],sizeof(type));
		(*bwalked)++;

		if((*bwalked + sizeof(ui16)) > data_size) return -1;

		ui16 f_len = 0;
		memcpy(&f_len,&data[*bwalked],sizeof(ui16));
		*bwalked += sizeof(ui16);

		/*set field name*/
		if((*bwalked + f_len) > data_size) return -1;
		char buf[f_len+1];
		memset(buf,0,f_len+1);
		memcpy(buf,&data[*bwalked],f_len);
        if(memchr(buf,0,f_len)) return -1;

		*bwalked += f_len;       

		if((*bwalked + sizeof(ui16)) > data_size) return -1;

		if(type == OBJECT_JS || type == ARRAY_JS) goto cases;

		ui16 v_len;
		memcpy(&v_len,&data[*bwalked],sizeof(ui16));
		*bwalked += sizeof(ui16);

		if(type != OBJECT_JS && (*bwalked + v_len) > data_size) return -1;

cases:
		switch(type){
		case ARRAY_JS:
		{
			lua_pushlstring(L,buf,f_len);
			lua_newtable(L);
			
			int count = 1;
			for(;;){
				if((*bwalked + sizeof(ui32)) > data_size) return -1;
				ui32 stop = JSON_END_ARRAY;
				if(memcmp(&stop,&data[*bwalked],sizeof(ui32)) == 0){
					*bwalked += sizeof(ui32);	
					break;
				}

				if((*bwalked + sizeof(ui8)) > data_size) return -1;

				ui8 type = 0;
				memcpy(&type,&data[*bwalked],sizeof(ui8));

				(*bwalked)++;
                if(type != OBJECT_JS) return -1;

				lua_newtable(L);
				lua_newtable(L);

				int fn = 0;
				if((fn=create_nested_lua_table(data,data_size,bwalked,depth+1)) == -1) return -1;
				lua_setfield(L,-2,"fields");
				lua_rawseti(L,-2,count++);
			}	
			lua_settable(L,-3);
			break;
		}
		case OBJECT_JS:
		{
			lua_pushlstring(L,buf,f_len);
			lua_newtable(L);
			if(create_nested_lua_table(data,data_size,bwalked,depth+1) == -1) return -1;
			lua_settable(L,-3);
			continue;
		}
		case STRING_JS:	
		{
			lua_pushlstring(L,(const char*)&data[*bwalked],v_len); 
			*bwalked += v_len;
			lua_setfield(L,-2,buf);
			break;
		}
		case TRUE_JS:
		{
			lua_pushinteger(L,1);
			*bwalked += v_len;
			lua_setfield(L,-2,buf);
			break;
		}
		case FALSE_JS:	
		{
			lua_pushinteger(L,0);
			*bwalked += v_len;
			lua_setfield(L,-2,buf);
			break;
		}
		case NUMBER_JS: 
		{
			char nb[64] = {0};
			if(v_len >= sizeof nb) return -1;
			memcpy(nb,&data[*bwalked],v_len);

			errno = 0;
			char *endptr;
			double d = strtod(nb,&endptr);
			if(endptr == nb || endptr != nb + v_len || errno == ERANGE || !isfinite(d)) return -1;
			lua_pushnumber(L,d);
			lua_setfield(L,-2,buf);
			*bwalked += v_len;
			break;
		}
		default:		return -1;
		}
	}
	return fields_num;
}
