#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "record.h"
#include "file.h"
#include "crud.h"
#include "date.h"
#include "lua_start.h"
#include "json.h"

lua_State *L = NULL;
static time_t sec = 0; 
static int top_lua_stack = 0;

static const int CACHE_SIZE = 30;
table_to_record_fn tbl_to_rec = NULL;
struct Cache *dbcache_ptr = NULL;
HashTable *cache_r_ptr =NULL;
static int load(lua_State *L, char *file_config);
static void free_inactive_caches(struct Cache *c);
static int create_lua_table(ui8 *data, char *file_name,size_t data_size,size_t *bwalked);
static int create_nested_lua_table(ui8 *data,size_t data_size,size_t *bwalked);

int init_lua(char *config_file)
{
	L = luaL_newstate();
	luaL_openlibs(L);

	if(load(L,config_file) == -1) return -1;
		
	lua_getglobal(L,"dbCache_ptr");
	if(!lua_islightuserdata(L,-1)) return -1;
	dbcache_ptr = (struct Cache *) lua_touserdata(L,-1);
	lua_pop(L,1);

	lua_getglobal(L,"cache_register_ptr");
	if(!lua_islightuserdata(L,-1)) return -1;
	cache_r_ptr = (HashTable *) lua_touserdata(L,-1);
	lua_pop(L,1);

	lua_getglobal(L,"port_table_function");
	if(!lua_islightuserdata(L,-1)) return -1;
	tbl_to_rec = (table_to_record_fn)lua_touserdata(L,-1);
	lua_pop(L,1);

	check_config_file();
	top_lua_stack = lua_gettop(L);
	return 0;
}

void close_lua()
{
	lua_close(L);
}

void check_config_file()
{
	struct stat file_data;
	if(stat("/root/db/lua/db_config.lua",&file_data) == -1) {
		return;
	}

	if(sec == 0){
		sec = file_data.st_mtim.tv_sec;
		if(dbcache_ptr) free_inactive_caches(dbcache_ptr);
		return;
	}

	if(file_data.st_mtim.tv_sec > sec){
		clear_lua_stack();
		if(load(L,"/root/db/lua/db_config.lua") == -1) return;
		sec = file_data.st_mtim.tv_sec;
		top_lua_stack = lua_gettop(L);
	}

	if(dbcache_ptr) free_inactive_caches(dbcache_ptr);
}
void clear_lua_stack()
{
	lua_settop(L,top_lua_stack);
}

int execute_lua_function(char *func_name, char *func_sig,...)
{
	va_list vl;
	int narg, nres;

	va_start(vl,func_sig);
	lua_getglobal(L,func_name);

	for(narg = 0; *func_sig != '\0'; narg++,func_sig++){

		luaL_checkstack(L,1,"too many arguments");

		switch(*func_sig){
			case 't':
			{
				ui8 *data = va_arg(vl,ui8*);	
				size_t data_size = va_arg(vl,size_t);
				char *file_name = va_arg(vl,char*);

				if(!data || !file_name) return -1;
				size_t bwalked = 0; 
				if(create_lua_table(data,file_name,data_size,&bwalked) == -1) {
					va_end(vl);
					return -1;
				}
				break;
			}
			case 'r':
			/*TODO:
			  Record_f
			  if(port_record(L,va_arg(vl,struct Record_f*)) == -1){
			  return -1;
			  }
			  */
			break;
			case 'd':	lua_pushnumber(L,va_arg(vl,double));			break;	/* double */	
			case 'i': 	lua_pushinteger(L,va_arg(vl,int));				break;	/* integer*/
			case 'I': 	lua_pushinteger(L,va_arg(vl,uint32_t));			break;	/*unsigned integer*/	
			case 'l': 	lua_pushinteger(L,va_arg(vl,long));				break;	/* long integer*/	
			case 's': 	{char *s = va_arg(vl,char*);lua_pushstring(L,s);break;}	/* string*/	
			case '>': 	func_sig++; goto fcall;/*end of input*/
			default: 	va_end(vl); return -1;
		}
	}

fcall:
	nres = strlen(func_sig);/*number of Lua function results*/

	/*execute the lua function*/
	if(lua_pcall(L,narg,nres,0) != 0){
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		lua_pop(L,1);
		va_end(vl);
		return -1;
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
							clear_lua_stack();
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
							clear_lua_stack();
							va_end(vl);
							return -1;
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
							clear_lua_stack();
							va_end(vl);
							return -1;
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
							clear_lua_stack();
							va_end(vl);
							return -1;
						}
						*va_arg(vl, long long*) = l;
						break;
					}
				case 's':
					{
						char *s = (char*)lua_tostring(L,nres);
						if(!s){
							clear_lua_stack();
							va_end(vl);
							return -1;
						}
						*va_arg(vl,char **) = s;
						break;
					}
				default:
					clear_lua_stack();
					va_end(vl);
					return -1;
			}
			nres++;
			func_sig++;
		}
	}
	va_end(vl);
	return 0;
}

int get_function_signature(char *function_name,char *signature)
{
	int size = strlen(function_name);
	char *var = malloc(size+3);
	memset(var,0,size+3);
	strncpy(var,function_name,size);
	var[size] = '_';
	var[size+1] = 's';

	lua_getglobal(L,var);
	free(var);

	char *s  = (char*)lua_tostring(L,-1);
	if(s){
		strncpy(signature,s,strlen(s));
	}else{
		lua_pop(L,1);
		return -1;
	}

	lua_pop(L,1);
	return 0;
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
	int i;
	for(i = 0; i < CACHE_SIZE; i++){
		if(c[i].ts == 0 || c[i].used == 0) continue;

		if((long)(c[i].used - c[i].ts) > (long) THREE_HOURS){
			if(write_cache_to_disk(&c[i]) == -1){
				fprintf(stderr,"!!! CANNOT WRITE THE CACHE TO FILE !!!!!!%s:%d\n",__FILE__,__LINE__);
				return;
			}

			Node *r = ht_delete((void*)c[i].file_name,cache_r_ptr,STR);
			if(!r){
				fprintf(stderr,"!!! SOMENTHIG WRONG WITH THE CACHE!!!%s:%d\n",__FILE__,__LINE__);
				return;
			}
			free_ht_node(r);
			free_cache(&c[i]);
		}else if((long)(c[i].used - now_seconds()) > (long) TWENTY_MINUTES){
			/*Just flush the content to disk*/
			if(write_cache_to_disk(&c[i]) == -1){
				fprintf(stderr,"!!! CANNOT WRITE THE CACHE TO FILE !!!!!!%s:%d\n",__FILE__,__LINE__);
				return;
			}
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
	if((fields_num = create_nested_lua_table(data,data_size,bwalked)) == -1) return -1;

	lua_pushinteger(L,fields_num);
	lua_setfield(L,-2,"fields_number");
	lua_settable(L,-3);
	return 0;
}

static int create_nested_lua_table(ui8 *data,size_t data_size,size_t *bwalked)
{
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

				lua_newtable(L);
				lua_newtable(L);

				int fn = 0;
				if((fn=create_nested_lua_table(data,data_size,bwalked)) == -1) return -1;
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
			if(create_nested_lua_table(data,data_size,bwalked) == -1) return -1;
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
			if(endptr == nb || errno == EINVAL) return -1;
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
