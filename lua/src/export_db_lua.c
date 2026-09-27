#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "export_db_lua.h"
#include "date.h"
#include "common.h"
#include "file.h"
#include "lock.h"
#include "date.h"
#include "hash_tbl.h"
#include "key.h"

#define LOWER_STR(s) for(char *p = &s[0]; *p && ((int)*p >= 65 || (int)*p <= 90) ;*p = ((int)*p) + 22,p++)
#define UPPER_STR(s) for(char *p = &s[0]; *p && ((int)*p >= 97 || (int)*p <= 122) ;(int)*p -= 22,p++)

struct Cache dbCache[CACHE_SIZE] = {0};
HashTable cache_register = {7,NULL,NULL};
static int get_free_slot_cache(struct Cache *c);
static int check_and_free_one_cache(struct Cache *c);

static int is_test(lua_State *L);
static int l_get_record(lua_State *L);
static int l_get_all_records(lua_State *L);
static int l_write_record(lua_State *L);
static int l_update_record(lua_State *L);
static int l_create_record(lua_State *L);
static int l_string_data_to_add_template(lua_State *L);
static int l_get_numeric_key(lua_State *L);
static int l_save_key_at_index(lua_State *L);
static int l_delete_record(lua_State *L);
static int l_get_all_key(lua_State *L);
static int l_get_offset_for_new_record(lua_State *L);

/* functions that will be callable from Lua scripts*/
static const luaL_Reg db_funcs[] = {
	{"get_record",l_get_record}, 			/* get_record(file_name,key) */
	{"get_all_records",l_get_all_records},	/* get_all_records(file_name) */
	{"write_record",l_write_record},		/* write_record(file_name,data) -- some optional args -- */
	{"create_record",l_create_record},		/* create_record(file_name,data) */
	{"string_data_to_add_template",
		l_string_data_to_add_template},		/* string_data_to_add_template(file_name) */
	{"get_numeric_key",l_get_numeric_key},	/* get_numeric_key(file_name,mode) -- some optional args --  */
	{"save_key_at_index",
		l_save_key_at_index},               /* save_key_at_index(file_name,key,index,offset)*/
	{"update_record",l_update_record},		/* update_record(file_name,data,key) */
	{"delete_record",l_delete_record},		/* delete_record(file_name,key) -- index is optional */
	{"get_all_key",l_get_all_key},			/* get_all_key(file_name,index,mode)*/
	{"get_offset",l_get_offset_for_new_record}, /*get_offset(file_name)*/
	{NULL,NULL}
};

int luaopen_db(lua_State *L){
	luaL_newlib(L,db_funcs);

	/*this is to access cache data from lua for our export_db_lua.c module*/
	lua_pushlightuserdata(L,dbCache);
	lua_setglobal(L,"dbCache_ptr");
	

	lua_pushlightuserdata(L,&cache_register);
	lua_setglobal(L,"cache_register_ptr");

	lua_pushlightuserdata(L,(void*)port_table_to_record);
	lua_setglobal(L,"port_table_function");
	return 1;
}

static int l_get_offset_for_new_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};
		
	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(is_test(L)) goto get_offset_test;
	/*check if the file is cached in memory*/
	off_t file_pos_in_the_cache = -1;
	if((file_pos_in_the_cache = get((void*)file_name,&cache_register,STR)) != -1){
		goto use_cache;
	}

	if(open_files(file_name,fds,file_names,-1) == -1)
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;

	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	close_file(3,fds[0],fds[1],fds[2]);
	INIT_FILE_T_ARRAY(fds,3);
	free_schema(hd.sch_d);

use_cache:
	
	if(file_pos_in_the_cache != -1){
		struct Cache *p = &dbCache[file_pos_in_the_cache];
		lua_pushinteger(L,(lua_Integer)p->data_file.size);
		p->used = now_seconds();
	}else{
		struct Cache *p = &dbCache[first_free_cache];
		lua_pushinteger(L,(lua_Integer)p->data_file.size);
		p->used = now_seconds();
	}

	return 1;
get_offset_test:
	if(open_files(file_name,fds,file_names,-1) == -1)
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;

	free_schema(hd.sch_d);

	file_offset fo = go_to_EOF(fds[1]);
	if(fo == -1){
		lua_pushnil(L);
		lua_pushstring(L,"go_to_EOF() failed.(l_get_offset_for_new_record).");
		return 2;
	}

	close_file(3,fds[0],fds[1],fds[2]);
	lua_pushinteger(L,(lua_Integer)fo);
	return 1;


err_cache:
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	lua_pushnil(L);
	lua_pushinteger(L,(lua_Integer)-CACHE_FAILED);
	return 2;

err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
}

/* 
 * from lua:
 * 		.get_record(file_name,key,index) 
 * 	 		index is optional, index 0 will be used if not specified
 * */
static int l_get_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");
	int key_type = 0;
	int n = 0;
	void *k = NULL;
	int type = lua_type(L,2);
	if(type == LUA_TNUMBER){
		key_type = UINT; 
		n = (int)luaL_checkinteger(L,2);
		if( n < 0) goto err_key;
		k = (void*)&n;
	}else if(type == LUA_TSTRING){
		key_type = STR; 
		k = (void*)luaL_checkstring(L,2);
	}else{
		goto err_key;
	}

	int index = 0;
	type = lua_type(L,3);
	if(type == LUA_TNUMBER){
		index = luaL_checkinteger(L,3);               
	}

	struct Record_f rec;
	memset(&rec,0,sizeof(struct Record_f));

	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};
		
	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};


	if(is_test(L)) goto get_rec_test;
	/*check if the file is cached in memory*/
	off_t file_pos_in_the_cache = -1;
	if((file_pos_in_the_cache = get((void*)file_name,&cache_register,STR)) != -1){
		goto use_cache;
	}

	if(open_files(file_name,fds,file_names,-1) == -1)
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;


	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	close_file(3,fds[0],fds[1],fds[2]);
	INIT_FILE_T_ARRAY(fds,3);
	free_schema(hd.sch_d);

use_cache:
	
	off_t pos = 0;
	if(file_pos_in_the_cache != -1){
		struct Cache *p = &dbCache[file_pos_in_the_cache];
		if((pos = get(k, &p->index_file[index],key_type)) == -1) goto err_cache_rec_not_found;
		p->data_file.offset = (uint64_t)pos;
		if(read_ram_file(file_name, &p->data_file, &rec, p->sch) == -1) goto err_read_ram_file;
		if(port_record(L,&rec)) goto err_exp_data_to_lua;
		p->used = now_seconds();
	}else{
		struct Cache *p = &dbCache[first_free_cache];
		if((pos = get(k, &p->index_file[index],key_type)) == -1) goto err_cache_rec_not_found;
		p->data_file.offset = (uint64_t)pos;
		if(read_ram_file(file_name, &p->data_file, &rec, p->sch) == -1) goto err_read_ram_file;
		if(port_record(L,&rec)) goto err_exp_data_to_lua;
		p->used = now_seconds();
	}
	

	free_record(&rec,rec.fields_num);
	return 1;


get_rec_test:
	if(open_files(file_name,fds,file_names,-1) == -1)
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;
	int result = -1;
	if((result = get_record(-1,file_name,&rec,k,key_type,hd,fds, index >= 0 ? index : 0)) == -1) goto err_get_record_failed;
	if(result == KEY_NOT_FOUND) goto err_rec_not_found;
	if(port_record(L,&rec)) goto err_exp_data_to_lua;

	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 1; /*return the record*/
err_cache:
	result = -1;
	if((result = get_record(-1,file_name,&rec,k,key_type,hd,fds,index >=0 ? index : 0)) == -1) goto err_get_record_failed;
	if(result == KEY_NOT_FOUND) goto err_rec_not_found;
	if(port_record(L,&rec)) goto err_exp_data_to_lua;

	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	lua_pushinteger(L,(lua_Integer)-CACHE_FAILED);
	return 2; /*return the record and the cache error*/
err_key:
		lua_pushnil(L);
		lua_pushstring(L,"only string or unsigned integer are allowed as key.");
		return 2;

err_open_file:
		lua_pushnil(L);
		lua_pushstring(L,"could not open the file.");
		return 2;

err_read_ram_file:
		lua_pushnil(L);
		lua_pushstring(L,"red_ram_file failed.");
		free_record(&rec,rec.fields_num);
		return 2;
err_get_record_failed:
		lua_pushnil(L);
		lua_pushstring(L,"get_record failed.");
		close_file(3,fds[0],fds[1],fds[2]);
		free_schema(hd.sch_d);
		free_record(&rec,rec.fields_num);
		return 2;

err_not_db_file:
		lua_pushnil(L);
		lua_pushstring(L,"not a db file.");
		close_file(3,fds[0],fds[1],fds[2]);
		return 2;
err_cache_rec_not_found:
		lua_pushnil(L);
		lua_pushstring(L,"record not found.");
		free_record(&rec,rec.fields_num);
		return 2;
err_rec_not_found:
		lua_pushnil(L);
		lua_pushstring(L,"record not found.");
		close_file(3,fds[0],fds[1],fds[2]);
		free_schema(hd.sch_d);
		free_record(&rec,rec.fields_num);
		return 2;
err_exp_data_to_lua:
		lua_pushnil(L);
		lua_pushstring(L,"cannot export record data.");
		close_file(3,fds[0],fds[1],fds[2]);
		free_schema(hd.sch_d);
		free_record(&rec,rec.fields_num);
		return 2;
}


static int l_get_all_records(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");


	struct Record_f **recs = NULL;
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};
		
	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(open_files(file_name,fds,file_names,-1) == -1) 
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1)
		goto err_not_db_file;
	size_t size_records_memory = 0;
	if(get_all_records(file_name,fds,&recs,hd,&size_records_memory) == -1)
		goto err_get_record_failed;

	close_file(3,fds[0],fds[1],fds[2]);

	int i = 0;
	lua_newtable(L);
	for(i = 0;i < fds[3]; i++){
		if(!recs[i])
			break;

		port_record(L,recs[i]);
		free_record(recs[i], recs[i]->fields_num);
		lua_rawseti(L, -2, i + 1);
	}
	
	free(recs);
	free_schema(hd.sch_d);
	return 1;

err_open_file:
		lua_pushnil(L);
		lua_pushstring(L,"could not open the file.");
		return 2;

err_get_record_failed:
		lua_pushnil(L);
		lua_pushstring(L,"get_record failed.");
		close_file(3,fds[0],fds[1],fds[2]);
		free_record_array(size_records_memory,recs);
		free_schema(hd.sch_d);
		return 2;

err_not_db_file:
		lua_pushnil(L);
		lua_pushstring(L,"not a db file.");
		close_file(3,fds[0],fds[1],fds[2]);
		return 2;
}
/*
 * calling from lua write_record(file_name,data_to_add)
 * --@param file_name
 * --@param data_to_add 
 *
 * both param are mandatory and they must be present.
 * if no other @params are passed, the funtion will compute a key for the record
 * that you want to write.
 *
 *
 * --@param key #OPTIONAL# but must be the 3th param, if present.
 *  this will be the key, or a key mode.
 *
 * exmaple (from lua):
 * 		write_record(file_name,data_to_add) 
 * 		--@@ will write a record in the file (file_name) with automatic numeric key.
 *
 * 		write_record(file_name,data_to_add,"hey")
 * 		--@@ will write a record in the file (file_name) with "hey" as a key.
 *
 * 		write_record(file_name,data_to_add,23)
 * 		--@@ will write a record in the file (file_name) with 23 as a key.
 * 		
 * 		write_record(file_name,data_to_add,"base",100)
 * 		--@@ will write a record in the file (file_name) with 100 + nr of records in the file, as a key.
 * */
static int l_write_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	/*second argument must be a table*/
	luaL_checktype(L,2,LUA_TTABLE);
	

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);

	struct Record_f rec = {0};
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};

	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	/*read a key from the function argument,if there is no key,compute one*/
	int type = lua_type(L,3);
	void* k = NULL;
	int key_type = -1;
	long long n = 0;

	off_t file_pos_in_the_cache = get((void*)file_name,&cache_register,STR);
	if(type == LUA_TNIL || type == -1) {	/*we have to generate a key*/
		if(type == -1){
			key_type = UINT;
			int key_mode = file_pos_in_the_cache == -1 ? KEY_GEN_DISK_MODE : KEY_GEN_CACHE_MODE;
			if(key_mode == KEY_GEN_DISK_MODE){
				if(open_files(file_name,fds,file_names,-1) == -1) 
					goto err_open_file;
				if(is_db_file(&hd,fds) == -1) 
					goto err_not_db_file;
				if((n = generate_numeric_key(fds,REG | key_mode,-1,NULL)) == -1) goto err_key_gen;
			}else{
				if((n = generate_numeric_key(fds,REG | key_mode,-1,&dbCache[file_pos_in_the_cache])) == -1) goto err_key_gen;
			}	

			if(n < (int)USHRT_MAX){
				k = (void*)(uint16_t*)&n;
			} else{
				k = (void*)(uint32_t*)&n;
			}
			lua_pushinteger(L,n);
		}
	}else if(type == LUA_TNUMBER){
		key_type = UINT; 
		n = (long long)luaL_checkinteger(L,3);
		if( n < 0) goto err_key;
		k = (void*)&n;
	}else if(type == LUA_TSTRING){
		char *param = (char*)luaL_checkstring(L,3);
		if(param && (strlen(param) == strlen("base")) &&
			(strncmp("base",param,strlen("base")) == 0)){

			int base = luaL_checkinteger(L,4); 
			int key_mode = file_pos_in_the_cache == -1 ? KEY_GEN_DISK_MODE : KEY_GEN_CACHE_MODE;
			if(key_mode == KEY_GEN_DISK_MODE){
				if(open_files(file_name,fds,file_names,-1) == -1) 
					goto err_open_file;
				if(is_db_file(&hd,fds) == -1) 
					goto err_not_db_file;
				if((n = generate_numeric_key(fds,BASE | key_mode,base,NULL)) == -1) goto err_key_gen;
			}else{
				if((n = generate_numeric_key(fds,BASE | key_mode,base,&dbCache[file_pos_in_the_cache])) == -1) goto err_key_gen;
			}	
			k = (void*)&n;
			lua_pushinteger(L,n);
			key_type = UINT;
		}else if(param 
				&& (strlen(param) == strlen("increment"))
				&& (strncmp(param,"increment",strlen("increment")) == 0)){

			int key_mode = file_pos_in_the_cache == -1 ? KEY_GEN_DISK_MODE : KEY_GEN_CACHE_MODE;
			if(key_mode == KEY_GEN_DISK_MODE){
				if(open_files(file_name,fds,file_names,-1) == -1) 
					goto err_open_file;
				if(is_db_file(&hd,fds) == -1) 
					goto err_not_db_file;
				if((n = generate_numeric_key(fds,INCREM | key_mode,-1,NULL)) == -1) goto err_key_gen;
			}else{
				if((n = generate_numeric_key(fds,INCREM | key_mode,-1,&dbCache[file_pos_in_the_cache])) == -1) goto err_key_gen;
			}	
			k = (void*)&n;
			lua_pushinteger(L,n);
			key_type = UINT;

		}else{
			key_type = STR; 
			k = (void*)param;
			lua_pushstring(L,param);
		}
	}else{
		goto err_key;
	}


	int lock = 0;

	if(is_test(L)) goto write_rec_test;

	/*check if the file is cached in memory*/
	if(file_pos_in_the_cache != -1) goto use_cache;

	/*if is not valid open the file!*/
	if(!IS_FILE_T_VALID(fds[0])){
		if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
		if(is_db_file(&hd,fds) == -1) goto err_not_db_file;
	}


	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	close_file(3,fds[0],fds[1],fds[2]);
	INIT_FILE_T_ARRAY(fds,3);
use_cache:

	if(file_pos_in_the_cache != -1){
		struct Cache *p = &dbCache[file_pos_in_the_cache];

		struct Header_d hd_c = {0,0,&p->sch};
		if(port_table_to_record(L,2,&rec,&p->sch) == -1)goto err_cache_invalid_data;
		/*if(check_data(file_name,data_to_add,fds,file_names,&rec,&hd_c,&lock,-1,0) == -1) goto err_cache_invalid_data;*/

		if(set_tbl(p->index_file,k,p->data_file.size,key_type,0) == -1) goto err_cache_write_index;
		if(check_const_unique(&p->sch,&rec,&p->index_file,p->data_file.size) == -1) goto err_cache_write_const_unique;
		p->data_file.offset = p->data_file.size;
		if(write_ram_record(&p->data_file, &rec, 0, -1, 0) == -1) goto err_cache_write;
		p->used = now_seconds();
	}else{
		/*THIS IS THE FIRST TIME WE CACHE THE FILE!!!!!*/
		struct Cache *p = &dbCache[first_free_cache];

		if(port_table_to_record(L,2,&rec,&sch) == -1)goto err_cache_invalid_data;
		/*if(check_data(file_name,data_to_add,fds,file_names,&rec,&hd_c,&lock,-1,0) == -1) goto err_cache_invalid_data;*/

		if(set_tbl(p->index_file,k,p->data_file.size,key_type,0) == -1) goto err_cache_write_index;
		if(check_const_unique(&p->sch,&rec,&p->index_file,p->data_file.size) == -1) goto err_cache_write_const_unique;
		p->data_file.offset = p->data_file.size;
		if(write_ram_record(&p->data_file, &rec, 0, -1, 0) == -1) goto err_cache_write;

		p->used = now_seconds();
		free_schema(hd.sch_d);
	}
	

	if(key_type == UINT) lua_pushinteger(L,n);
	if(key_type == STR) lua_pushstring(L,(char*)k);

	port_record(L,&rec);/*is this obsolete now?*/
	free_record(&rec,rec.fields_num);

	return 2;/*return the key and the record*/

err_cache:
	if(!IS_FILE_T_VALID(fds[0])){
		if(open_files(file_name,fds,file_names,-1) == -1) 
			goto err_open_file;
		if(is_db_file(&hd,fds) == -1) 
			goto err_not_db_file;
	}

	lock = STD_LOCK | LOCK_FROM_LUA;/*this will lock the file on disk*/
	if(port_table_to_record(L,2,&rec,&sch) == -1) goto err_invalid_data;
	/*if(check_data(file_name,data_to_add,fds,file_names,&rec,&hd,&lock,-1,0) == -1) */
	if(write_record(fds,(void*)k,key_type,&rec,0,file_names,&lock,-1,hd.sch_d) == -1) 
		goto err_write_rec;

	if(key_type == UINT) lua_pushinteger(L,n);
	if(key_type == STR) lua_pushstring(L,(char*)k);
	port_record(L,&rec); /*?obsolete?*/

	if(lock) {
		release_lock(fds,-1);
		lock = 0;
	}
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);

	return 2;

write_rec_test:

	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1) 
			goto err_open_file;
		if(is_db_file(&hd,fds) == -1) 
			goto err_not_db_file;
	}

	lock = STD_LOCK | LOCK_FROM_LUA;/*this will lock the file on disk*/
	if(port_table_to_record(L,2,&rec,&sch) == -1) goto err_invalid_data;
	/*if(check_data(file_name,data_to_add,fds,file_names,&rec,&hd,&lock,-1,0) == -1) */
	if(write_record(fds,(void*)k,key_type,&rec,0,file_names,&lock,-1,hd.sch_d) == -1) 
		goto err_write_rec;

	if(key_type == UINT) lua_pushinteger(L,n);
	if(key_type == STR) lua_pushstring(L,(char*)k);
	port_record(L,&rec);/*?obsolete?*/

	if(lock) {
		release_lock(fds,-1);
		lock = 0;
	}

	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 2;

err_cache_write:
	lua_pushnil(L);
	lua_pushstring(L,"cache write record failed");
	free_record(&rec,rec.fields_num);
	return 2;
err_cache_write_index:
	lua_pushnil(L);
	lua_pushstring(L,"cache write index failed");
	free_record(&rec,rec.fields_num);
	return 2;
err_cache_write_const_unique:
	lua_pushnil(L);
	lua_pushstring(L,"cache check const_unique failed");
	free_record(&rec,rec.fields_num);
	return 2;
err_cache_invalid_data:
	lua_pushnil(L);
	lua_pushstring(L,"data not valid.");
	free_record(&rec,rec.fields_num);
	return 2;
err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_key:
	lua_pushnil(L);
	lua_pushstring(L,"error detecting key.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	return 2;
err_key_gen:
	lua_pushnil(L);
	lua_pushstring(L,"key generation failed");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	return 2;
err_write_rec:
	lua_pushnil(L);
	lua_pushstring(L,"write record failed");
	if(lock){
		release_lock(fds,-1);
		lock = 0;
	}
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 3;
err_invalid_data:
	lua_pushnil(L);
	lua_pushstring(L,"data not valid.");
	if(lock){
		release_lock(fds,-1);
		lock = 0;
	}
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 3;
}

static int l_update_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	/*second argument must be a table*/
	luaL_checktype(L,2,LUA_TTABLE);
	
	char *k_str = NULL;
	void *key = NULL;
	uint32_t n = 0;

	int key_type = 0;
	int type = lua_type(L,3);
	switch(type){
	case -1:
	case LUA_TNIL:
		lua_pushnil(L);
		lua_pushstring(L,"key is missing for update functions.");
		return 2;
	case LUA_TNUMBER:
		n = (uint32_t) luaL_checkinteger(L,3);
		key = (void*)&n;
		key_type = UINT;
		break;
	case LUA_TSTRING:
		k_str = (char*)luaL_checkstring(L,3);
		key = (void*)k_str;
		key_type = STR;
		break;
	default:
		lua_pushnil(L);
		lua_pushstring(L,"wrong key type");
		return 2;
	}

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);

	struct Record_f rec = {0};
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};

	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(is_test(L)) goto update_rec_test;

	/*check if the file is cached in memory*/
	off_t file_pos_in_the_cache = get((void*)file_name,&cache_register,STR);
	if(file_pos_in_the_cache != -1) goto use_cache;

	if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
	if(is_db_file(&hd,fds) == -1) goto err_not_db_file;


	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache_full;/*we cannot free a cache slot*/
	}

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache_first_time;

	close_file(3,fds[0],fds[1],fds[2]);
	memset(fds,-1,3*sizeof(int));
	free_schema(hd.sch_d);

use_cache:

	struct Record_f rec_old = {0};
	off_t pos = 0;
	struct Cache *p = NULL;
	if(file_pos_in_the_cache != -1){
		p = &dbCache[file_pos_in_the_cache];
	}else{
		/*first time we cache the file*/
		p = &dbCache[first_free_cache];
	}

	/*create a new record for the updated data */
	int check = 0;
	int lock = STD_LOCK | LOCK_FROM_LUA;
	struct Header_d hd_c = {0,0,&p->sch};
	/*if((check = check_data(file_name,data_to_add,fds,file_names,&rec,&hd_c,&lock,-1,0)) == -1)*/
	if((check = port_table_to_record(L,2,&rec,&p->sch)) == -1) goto err_cache_invalid_data;
	
	/*get old record*/
	if((pos = get(key, &p->index_file[0],key_type)) == -1) goto err_cache_rec_not_found;
	p->data_file.offset = (uint64_t)pos;

	if(read_ram_file(file_name, &p->data_file, &rec_old, p->sch) == -1) goto err_read_ram_file;
	struct Record_f *temp = &rec_old;

	/*NOTE: we do not have to save the update pos, the ram file functions move the offset
	 * internally*/
	
	
	while((pos = read_update_offset_ram_file(&p->data_file)) != 0){
		struct Record_f *n = malloc(sizeof *n);
		if(!n) goto err_memory_allocation_update;
		memset(n,0,sizeof *n);

		p->data_file.offset =pos;
		if(read_ram_file(file_name, &p->data_file, n, p->sch) == -1){ 
			free(n);
			goto err_read_ram_file;
		}
		temp->next = n;
		temp = temp->next;
		rec_old.count++;
	}

	switch(check){
	case SCHEMA_EQ:
	case SCHEMA_CT:
		if(combine_old_and_new_rec(file_name,&rec_old,&rec,p->sch) == -1) goto err_combine_rec;
		break;
	default:
		fprintf(stderr,"%s(), schema check value not implemented, %s:%d.\n",__func__,__FILE__,__LINE__);
		goto err_cache_wrong_schema;
	}

	struct Record_f *o = &rec_old;
	off_t update_offset = 0;
	p->data_file.offset = o->offset;
	while(o){
		if(o->next){
			if(!o->next->next){ 
				update_offset = o->next->offset == 0 ? p->data_file.size : o->next->offset;
			}else{
				update_offset = o->next->offset;
			}

		}else{
			update_offset = 0;
		}

		if(write_ram_record(&p->data_file,o,1,-1,update_offset) == -1) goto err_cache_write;
		p->data_file.offset = update_offset;
		o = o->next;
	}

	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	lua_pushinteger(L,0); /*return 0 on success*/
	return 1;

update_rec_test:
	lock = STD_LOCK | LOCK_FROM_LUA;/*this will lock the file on disk*/
	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
		if(is_db_file(&hd,fds) == -1) goto err_not_db_file;
	}
	check = -1;
	if((check = port_table_to_record(L,2,&rec,&sch)) == -1) goto err_invalid_data;
	/*if((check = check_data(file_name,data_to_add,fds,file_names,&rec,&hd,&lock,-1,1)) == -1)*/
	int r = 0;
	if((r = update_rec(file_name,fds,key,key_type,&rec,hd,check,&lock,NULL,-1)) == -1) goto err_update_rec;


	if(r == KEY_NOT_FOUND)
		goto err_update_rec;

	if(fds[0] != -1) free_schema(hd.sch_d);
	if(fds[0] != -1) close_file(3,fds[0],fds[1],fds[2]);
	free_record(&rec,rec.fields_num);
	lua_pushinteger(L,0);
	return 1;

err_cache_write:
	lua_pushnil(L);
	lua_pushstring(L,"cannot write cached file.");
	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	return 2;
err_read_ram_file:
	lua_pushnil(L);
	lua_pushstring(L,"cannot read cached file.");
	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	return 2;
err_cache_rec_not_found:
	lua_pushnil(L);
	lua_pushstring(L,"rec not found in the cache.");
	free_record(&rec,rec.fields_num);
	return 2;
err_cache_full:
	lua_pushnil(L);
	lua_pushstring(L,"cache is full, cannot complete your request");
	return 2;
err_cache_first_time:
	lua_pushnil(L);
	lua_pushstring(L,"cannot initialize file into the cache!");
	free_schema(hd.sch_d);
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_memory_allocation_update:
	lua_pushnil(L);
	lua_pushstring(L,"cannot allocate memory for old rec in the cache.");
	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	return 2;
err_cache_invalid_data:
	lua_pushnil(L);
	lua_pushstring(L,"data not valid.");
	free_record(&rec,rec.fields_num);
	return 2;
err_cache_wrong_schema:
	lua_pushnil(L);
	lua_pushstring(L,"schema is wrong, this feature is not implemented yet");
	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	return 2;
err_combine_rec:
	lua_pushnil(L);
	lua_pushstring(L,"schema is wrong, this feature is not implemented yet");
	free_record(&rec,rec.fields_num);
	free_record(&rec_old,rec_old.fields_num);
	return 2;
err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_update_rec:
	lua_pushinteger(L,(lua_Integer)r);
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 1;
err_invalid_data:
	lua_pushnil(L);
	lua_pushstring(L,"data not valid.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 3;
}

/*
 * from lua:
 * 	.delete_record(file_name,key)
 * 	or
 * 	.delete_record(file_name,key,index)
 *
 * 	if index is not specified, index 0 will be used
 * */
static int l_delete_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");


	char *k_str = NULL;
	void *key = NULL;
	uint32_t n = 0;

	int key_type = 0;
	int type = lua_type(L,2);
	switch(type){
	case -1:
	case LUA_TNIL:
		lua_pushnil(L);
		lua_pushstring(L,"key is missing for delete function.");
		return 2;
	case LUA_TNUMBER:
		n = (uint32_t) luaL_checkinteger(L,2);
		key = (void*)&n;
		key_type = UINT;
		break;
	case LUA_TSTRING:
		k_str = (char*)luaL_checkstring(L,2);
		key = (void*)k_str;
		key_type = STR;
		break;
	default:
		lua_pushnil(L);
		lua_pushstring(L,"wrong key type");
		return 2;
	}

	int index_nr = 0;
	type = lua_type(L,3);
	switch(type){
	case -1:
	case LUA_TNIL:
		break;
	case LUA_TNUMBER:
		index_nr = (int)luaL_checkinteger(L,3);
		break;
	default:
		lua_pushnil(L);
		lua_pushstring(L,"wrong index type");
		return 2;
	}

	int fds[1] = {-1};
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(open_files(file_name,fds,file_names,ONLY_INDEX) == -1) goto err_open_file;
	
	HashTable *ht = NULL;
	int index = 0, *p_index = &index;
	/* load all indexes in memory */
	if (!read_all_index_file(fds[0], &ht, p_index)) {
		free_ht_array(ht,index);
		goto err_reading_index;
	}

	if(index_nr > index)
		goto err_index_out_of_range;

	Node *record_del = ht_delete(key, &ht[index_nr], key_type);
	if(!record_del)
		goto err_record_not_found;

	/*delete was succesfull*/

	if(write_index(fds,index,ht,file_names[0]) == -1) 
		goto err_write_index;

	close_file(1,fds[0]);
	free_ht_array(ht,index);
	lua_pushinteger(L,(lua_Integer)0);
	return 1;
	
err_open_file:
	lua_pushinteger(L,(lua_Integer) -1);
	return 1;
err_reading_index:
	lua_pushinteger(L,(lua_Integer) -1);
	return 1;
err_index_out_of_range:
	lua_pushinteger(L,(lua_Integer)INDEX_OUT_OF_RANGE);
	free_ht_array(ht,index);
	close_file(1,fds[0]);
	return 1;
err_record_not_found:
	free_ht_array(ht,index);
	lua_pushinteger(L,(lua_Integer)-1);
	lua_pushstring(L,"record not found");
	close_file(1,fds[0]);
	return 2;
err_write_index:
	lua_pushnil(L);
	lua_pushstring(L,"could not write index file.");
	close_file(1,fds[0]);
	return 2;
}
/*
 * this function return a record (table) to Lua,
 * without writing to the file.
 * */
static int l_create_record(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	char *data_to_add = (char*)luaL_checkstring(L,2);
	luaL_argcheck(L, data_to_add != NULL, 2,"data expected!");
	
	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);

	struct Record_f rec = {0};
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};

	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	int lock = STD_LOCK | LOCK_FROM_LUA;/*this will lock the file on disk*/
	if(open_files(file_name,fds,file_names,-1) == -1)
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;
	if(check_data(file_name,data_to_add,fds,file_names,&rec,&hd,&lock,-1,0) == -1) 
		goto err_invalid_data;

	rec.offset = go_to_EOF(fds[1]);
	
	port_record(L,&rec);
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);

	return 1;

err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_invalid_data:
	lua_pushnil(L);
	lua_pushstring(L,"data not valid.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_record(&rec,rec.fields_num);
	return 2;
}

static int l_string_data_to_add_template(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);

	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};

	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(open_files(file_name,fds,file_names,ONLY_SCHEMA) == -1) 
		goto err_open_file;
	if(is_db_file(&hd,fds) == -1) 
		goto err_not_db_file;
	

	int i, sum = 0; 
	for(i = 0; i < hd.sch_d->fields_num; i++){
		if(hd.sch_d->is_dropped[i])
			continue;

		sum += strlen(hd.sch_d->fields_name[i]);
		switch(hd.sch_d->types[i]){
			case TYPE_FLOAT:
			case TYPE_DOUBLE:
			case TYPE_ARRAY_FLOAT:
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_FLOAT:
			case TYPE_SET_DOUBLE:
				sum += 4;
				break;
			case TYPE_FILE:
				sum += 6;
				break;
			case TYPE_INT:
			case TYPE_BYTE:
			case TYPE_LONG:
			case TYPE_STRING:
			case TYPE_DATE:
			case TYPE_ARRAY_STRING:
			case TYPE_ARRAY_LONG:
			case TYPE_ARRAY_INT:
			case TYPE_ARRAY_BYTE:
			default:
				sum += 2;
				break;
		}
	}
	
	int colon_nr = (i * 2 ) - 1;
	char *st = (char*)malloc(colon_nr+sum+1);
	if(!st) 
		goto err_ask_mem;

	memset(st,0,colon_nr+sum+1);


	int bwritten = 0;
	for(i = 0; i < hd.sch_d->fields_num; i++){
		if(hd.sch_d->is_dropped[i])
			continue;

		size_t sz = strlen(hd.sch_d->fields_name[i]);
		memcpy(&st[bwritten],hd.sch_d->fields_name[i],sz);
		bwritten += sz;
		memcpy(&st[bwritten],":",1);
		bwritten += 1;
		switch(hd.sch_d->types[i]){
		case TYPE_INT:
		case TYPE_LONG:
		case TYPE_BYTE:
			memcpy(&st[bwritten],"%d",2);
			bwritten += 2;
			break;
		case TYPE_ARRAY_BYTE:
		case TYPE_ARRAY_LONG:
		case TYPE_ARRAY_INT:
		case TYPE_ARRAY_FLOAT:
		case TYPE_ARRAY_DOUBLE:
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
		case TYPE_SET_BYTE:
		case TYPE_SET_LONG:
		case TYPE_SET_INT:
		case TYPE_SET_FLOAT:
		case TYPE_SET_DOUBLE:
			memcpy(&st[bwritten],"%s",2);
			bwritten += 2;
			break;
		case TYPE_DOUBLE:
		case TYPE_FLOAT:
			memcpy(&st[bwritten],"%.2f",4);
			bwritten += 4;
			break;
		case TYPE_FILE:
			memcpy(&st[bwritten],"[w|%s]",6);
			bwritten += 6;
			break;
		case TYPE_STRING:
		default:
			memcpy(&st[bwritten],"%s",2);
			bwritten += 2;
			break;
		}
		if((hd.sch_d->fields_num - i) > 1) {
			memcpy(&st[bwritten],":",1);
			bwritten += 1;
		}
	}

	lua_pushstring(L,st);
	free_schema(hd.sch_d);
	close_file(1,fds[2]);
	free(st);

	return 1;

err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(1,fds[2]);
	return 2;
err_ask_mem:
	lua_pushnil(L);
	lua_pushstring(L,"malloc() failed");
	close_file(1,fds[2]);
	free(st);
	free_schema(hd.sch_d);
	return 2;
}


/* 
 * This generates a key for a record that you migh want to write
 * */
static int l_get_numeric_key(lua_State *L)
{		
	char *file_name = (char*)luaL_checkstring(L,1);
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	int mode = (int)luaL_checkinteger(L,2);
	luaL_argcheck(L, mode >= 0, 2,"mode must be bigger than 0");
	long long n = 0;

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);

	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0,0,&sch};

	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};

	if(is_test(L)) goto gen_key_test;

	/*check if the file is cached in memory*/
	off_t file_pos_in_the_cache = -1;
	if((file_pos_in_the_cache = get((void*)file_name,&cache_register,STR)) != -1){
		goto use_cache;
	}

	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
	if(is_db_file(&hd,fds) == -1) goto err_not_db_file;

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	free_schema(hd.sch_d);
	close_file(3,fds[0],fds[1],fds[2]);
	memset(fds,-1,3*sizeof(int));

use_cache:

	struct Cache *p = NULL;
	if(file_pos_in_the_cache != -1){
		p = &dbCache[file_pos_in_the_cache];	
	}else{
		p = &dbCache[first_free_cache];	
	}

	switch(mode){
	case REG:
	{
		if((n = generate_numeric_key(fds,REG | KEY_GEN_CACHE_MODE,-1,p)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case BASE:
	{
		int base = (int)luaL_checkinteger(L,3);
		if((n = generate_numeric_key(fds,BASE | KEY_GEN_CACHE_MODE,base,p)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case INCREM:
	{
		if((n = generate_numeric_key(fds,INCREM | KEY_GEN_CACHE_MODE,-1,p)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	default:
		/*error*/
		lua_pushnil(L);
		lua_pushstring(L,"key mode unknown");
		return 2;
	}

	return 1;

gen_key_test:
	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1)
			goto err_open_file;
		if(is_db_file(&hd,fds) == -1) 
			goto err_not_db_file;

		free_schema(hd.sch_d);
	}


	switch(mode){
	case REG:
	{
		if((n = generate_numeric_key(fds,REG,-1,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case BASE:
	{
		int base = (int)luaL_checkinteger(L,3);
		if((n = generate_numeric_key(fds,BASE,base,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case INCREM:
	{
		if((n = generate_numeric_key(fds,INCREM,-1,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	default:
		/*error*/
		lua_pushnil(L);
		lua_pushstring(L,"key mode unknown");
		close_file(1,fds[0]);
		return 2;
	}

	close_file(3,fds[0],fds[1],fds[2]);
	return 1;


err_cache:
	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1)
			goto err_open_file;
		if(is_db_file(&hd,fds) == -1) 
			goto err_not_db_file;

		free_schema(hd.sch_d);
	}


	switch(mode){
	case REG:
	{
		if((n = generate_numeric_key(fds,REG,-1,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case BASE:
	{
		int base = (int)luaL_checkinteger(L,3);
		if((n = generate_numeric_key(fds,BASE,base,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	case INCREM:
	{
		if((n = generate_numeric_key(fds,INCREM,-1,NULL)) == -1) goto err_key_gen;
		lua_pushinteger(L,n);
		break;
	}
	default:
		/*error*/
		lua_pushnil(L);
		lua_pushstring(L,"key mode unknown");
		close_file(1,fds[0]);
		return 2;
	}

	close_file(3,fds[0],fds[1],fds[2]);
	lua_pushinteger(L,(lua_Integer)-CACHE_FAILED);
	return 2;

err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
err_key_gen:
	lua_pushnil(L);
	lua_pushstring(L,"error generating  key.");
	close_file(3,fds[0],fds[1],fds[2]);
	return 2;
}



static int l_get_all_key(lua_State *L)
{
	/*char *get_all_keys_for_file(int *fds,int index,int mode)*/
	char *file_name = (char*)luaL_checkstring(L,1);	
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	int index = (int)luaL_checkinteger(L,2);	
	luaL_argcheck(L, index >= 0, 2,"index cannot be negative");

	int mode = 0, type = lua_type(L,3);
	switch(type){
	case -1:
	case LUA_TNIL:
		break;
	case LUA_TSTRING:
		lua_pushnil(L);
		lua_pushstring(L,"mode must be a number.");
		return 2;
	case LUA_TNUMBER:
		mode = (unsigned int)luaL_checkinteger(L,3);
		break;
	default:
		break;
	}

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};
	struct Schema sch = {0};
	struct Header_d hd = {0,0,&sch};

	if(is_test(L)) goto get_all_keys_test;

	off_t file_pos_in_the_cache = get((void*)file_name,&cache_register,STR);
	if(file_pos_in_the_cache != -1) goto use_cache;

	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
	if(is_db_file(&hd,fds) == -1) goto err_not_db_file;

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	free_schema(hd.sch_d);
	close_file(3,fds[0],fds[1],fds[2]);
	memset(fds,-1,3*sizeof(int));

use_cache:

	struct Cache *p = NULL;
	if(file_pos_in_the_cache != -1){
		p = &dbCache[file_pos_in_the_cache];	
	}else{
		p = &dbCache[first_free_cache];	
	}
	mode =  mode == MAKE_KEY_JS_STRING ? MAKE_KEY_JS_STRING | KEY_GET_ALL_CACHE : 0 | KEY_GET_ALL_CACHE;
	char *r = get_all_keys_for_file(fds,index,mode,p->index_file);
	if(!r)
		goto error_key_null_from_cache;

	lua_pushstring(L,r);
	free(r);
	return 1;
get_all_keys_test:

	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,ONLY_INDEX) == -1)
			goto err_open_file;
	}

	char *res = get_all_keys_for_file(fds,index,mode == MAKE_KEY_JS_STRING ? MAKE_KEY_JS_STRING : 0,NULL);
	if(!res)
		goto error;

	lua_pushstring(L,res);
	close_file(1,fds[0]);
	free(res);
	return 1;

err_cache:
	res = get_all_keys_for_file(fds,index,mode == MAKE_KEY_JS_STRING ? MAKE_KEY_JS_STRING : 0,NULL);
	if(!res)
		goto error;

	lua_pushstring(L,res);
	lua_pushinteger(L,(lua_Integer)-CACHE_FAILED);
	close_file(3,fds[0],fds[1],fds[2]);
	free(res);
	return 2;
err_open_file:
	lua_pushstring(L,"cannot open the file.");
	lua_pushinteger(L,(lua_Integer) -1);
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	return 2;
error:
	lua_pushstring(L,"key results is NULL");
	close_file(1,fds[0]);
	return 1;
error_key_null_from_cache:
	lua_pushstring(L,"key results is NULL");
	return 1;
}

static int l_save_key_at_index(lua_State *L)
{
	char *file_name = (char*)luaL_checkstring(L,1);	
	luaL_argcheck(L, file_name != NULL, 1,"file_name expected");

	void *key = NULL;
	int key_type = 0;
	unsigned int n = 0; /*to store key as a  number before casting*/
	int type = lua_type(L,2);
	switch(type){
	case -1:
	case LUA_TNIL:
		lua_pushnil(L);
		lua_pushstring(L,"key must be present.");
		return 1;
	case LUA_TSTRING:
		key_type = STR;	
		key = (void*)luaL_checkstring(L,2);
		break;
	case LUA_TNUMBER:
		key_type = UINT;
		n = (unsigned int)luaL_checkinteger(L,2);
		key = (void*)&n;
		break;
	default:
		lua_pushnil(L);
		lua_pushstring(L,"key must be present.");
		return 1;
	}

	int index = (int)luaL_checkinteger(L,3);
	luaL_argcheck(L, index >= 0, 3,"index cannot be negative");

	long long record_offset = (int)luaL_checkinteger(L,4);
	luaL_argcheck(L, record_offset >= 0, 4,"offset cannot be negative");

	file_t fds[3];
	INIT_FILE_T_ARRAY(fds,3);
	char file_names[3][MAX_FILE_PATH_LENGTH] = {0};
	struct Schema sch = {0};
	struct Header_d hd = {0,0, &sch};

	off_t file_pos_in_the_cache = get((void*)file_name,&cache_register,STR);
	
	if(is_test(L)) goto indexing_test;

	if(file_pos_in_the_cache != -1) goto use_cache;

	if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
	if(is_db_file(&hd,fds) == -1) goto err_not_db_file;

	/*cache the file*/
	int first_free_cache = 0;
	if((first_free_cache = get_free_slot_cache(dbCache)) == -1){
		/*cache is full free one spot in the cache */
		if((first_free_cache = check_and_free_one_cache(dbCache)) == -1)
			goto err_cache;/*we cannot free a cache slot, we use the disk*/
	}

	if(cache_file(fds,file_name,hd.sch_d,dbCache,&cache_register,first_free_cache) == -1)
		goto err_cache;

	close_file(3,fds[0],fds[1],fds[2]);
	fds[0] = -1;

use_cache:
	free_schema(hd.sch_d);
	struct Cache *p = NULL;
	if(file_pos_in_the_cache != -1){
		p = &dbCache[file_pos_in_the_cache];
	}else{
		p = &dbCache[first_free_cache];
	}
	
	if(set_tbl(p->index_file,key,record_offset,key_type,index) == -1) goto err_set_index_cache;
	p->used = now_seconds();
	lua_pushinteger(L,index);
	lua_pushstring(L,"index write succeed cache");
	return 2;

indexing_test:

	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
		if(is_db_file(&hd,fds) == -1) goto err_not_db_file;
	}

	/* load all indexes in memory */
	HashTable *ht = NULL;
	int tbl_ix = 0;
	if (!read_all_index_file(fds[0], &ht, &tbl_ix))
		goto err_load_index;

	if(set_tbl(ht,key,record_offset,key_type,index) == -1) 
		goto err_set_index;

	if(write_index(fds,tbl_ix,ht,file_names[0]) == -1) 
		goto err_write_index;


	/*if indexing succeed return the index number*/
	lua_pushinteger(L,index);
	lua_pushstring(L,"index write succeed");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	free_ht_array(ht,tbl_ix);
	return 2;

err_cache:
	if(fds[0] == -1){
		if(open_files(file_name,fds,file_names,-1) == -1) goto err_open_file;
		if(is_db_file(&hd,fds) == -1) goto err_not_db_file;
	}

	/* load all indexes in memory */
	ht = NULL;
	tbl_ix = 0;
	if (!read_all_index_file(fds[0], &ht, &tbl_ix))
		goto err_load_index;

	if(set_tbl(ht,key,record_offset,key_type,index) == -1) 
		goto err_set_index;

	if(write_index(fds,tbl_ix,ht,file_names[0]) == -1) 
		goto err_write_index;

	free_schema(hd.sch_d);
	free_ht_array(ht,tbl_ix);
	close_file(3,fds[0],fds[1],fds[2]);
	lua_pushinteger(L,index);
	lua_pushinteger(L,(lua_Integer)-CACHE_FAILED);
	return 2;
	
err_open_file:
	lua_pushnil(L);
	lua_pushstring(L,"could not open the file.");
	return 2;
err_not_db_file:
	lua_pushnil(L);
	lua_pushstring(L,"not a db file.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	return 2;
err_load_index:
	lua_pushnil(L);
	lua_pushstring(L,"could not load index file.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_schema(hd.sch_d);
	return 2;
err_set_index_cache:
	lua_pushnil(L);
	lua_pushstring(L,"cannot set index in the cached file.");
	return 2;
err_set_index:
	lua_pushnil(L);
	lua_pushstring(L,"could not set index.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_ht_array(ht, tbl_ix);
	free_schema(hd.sch_d);
	return 2;
err_write_index:
	lua_pushnil(L);
	lua_pushstring(L,"could not write index file.");
	close_file(3,fds[0],fds[1],fds[2]);
	free_ht_array(ht, tbl_ix);
	free_schema(hd.sch_d);
	return 2;
}

/* external functions*/
int port_record(lua_State *L, struct Record_f* r){
	lua_newtable(L);
	lua_pushstring(L,r->file_name);
	lua_setfield(L,-2,"file_name");

	lua_pushinteger(L,r->offset);
	lua_setfield(L,-2,"offset");
	lua_pushinteger(L,r->fields_num);
	lua_setfield(L,-2,"fields_number");

	lua_newtable(L);
	int i;
	for(i = 0; i < r->fields_num; i++){
		if(r->field_set[i] == 0)
			continue;
		if(r->fields[i].is_dropped)
			continue;

		switch(r->fields[i].type){
			case TYPE_INT:
				lua_pushinteger(L,r->fields[i].data.i);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_LONG:
				lua_pushinteger(L,r->fields[i].data.l);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_BYTE:
				lua_pushinteger(L,r->fields[i].data.b);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_STRING:
				lua_pushstring(L,r->fields[i].data.s);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_DATE:
			{
					char date[11];
					memset(date,0,11);
					if(convert_number_to_date(date,r->fields[i].data.date) == -1) return -1;
					lua_pushstring(L,date);
					lua_setfield(L,-2,r->fields[i].field_name);
					break;
			}
			case TYPE_FLOAT:
				lua_pushnumber(L,r->fields[i].data.f);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_DOUBLE:
				lua_pushnumber(L,r->fields[i].data.d);
				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			case TYPE_FILE:
			{
					lua_newtable(L);
					struct Record_f *t;
					int j;
					for(j = 1, t = r->fields[i].data.file.recs; t != NULL; t = t->next){
						if(port_record(L,t) == -1) 
							return -1;
						lua_rawseti(L, -2, j);
						j++;
					}

					lua_setfield(L,-2,r->fields[i].field_name);
					break;
			}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushstring(L,r->fields[i].data.v.elements.s[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushnumber(L,r->fields[i].data.v.elements.f[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushinteger(L,r->fields[i].data.v.elements.i[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushinteger(L,r->fields[i].data.v.elements.l[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushinteger(L,r->fields[i].data.v.elements.b[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
			{
				lua_newtable(L);
				int j,k;
				for(j = 0, k = 1; j < r->fields[i].data.v.size; j++){
					lua_pushnumber(L,r->fields[i].data.v.elements.d[j]);
					lua_seti(L,-2,k);
					k++;
				}

				lua_setfield(L,-2,r->fields[i].field_name);
				break;
			}
			default:
				/*TODO*/
				break;
		}
	}
	lua_setfield(L,-2,"fields");

	lua_pushinteger(L,r->count);
	lua_setfield(L,-2,"count");

	if(r->next) {
		lua_newtable(L);
		struct Record_f *t;
		for(i = 1,t = r->next; t != NULL; t = t->next){
			port_record(L,t);
			lua_rawseti(L, -2, i);
			i++;
		}
		lua_setfield(L,-2,"next");
		return 0;
	}
	return 0;
}

int port_table_to_record(lua_State *L,int index, struct Record_f *rec,struct Schema *sch)
{
	if(lua_getfield(L,index,"file_name") != LUA_TSTRING) return -1;
	char *file_name = (char*) lua_tostring(L,-1);
	if(!file_name) return -1;

	lua_pop(L,1);

	if(create_record(file_name,*sch,rec) == -1) return -1;

	if(lua_getfield(L,index,"offset") != LUA_TNUMBER) return -1;

	int is_num;
	rec->offset = (file_offset) lua_tonumberx(L,-1,&is_num);
	if(!is_num) return -1;
	lua_pop(L,1);

	if(lua_getfield(L,index,"fields") != LUA_TTABLE){
		/*TODO: error*/
		return -1;
	}
		

	int i, check_type = SCHEMA_EQ;
	for(i = 0; i < sch->fields_num; i++){
		lua_getfield(L,-1,sch->fields_name[i]);
		if(lua_isnil(L,-1)) {
			lua_pop(L,1);
			check_type = SCHEMA_CT;
			continue;
		}
	
		switch(sch->types[i]){
		case TYPE_INT:
		{
			is_num = 0;
			rec->fields[i].data.i = (int) lua_tonumberx(L,-1,&is_num); 
			if(!is_num){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_LONG:
		{
			is_num = 0;
			rec->fields[i].data.l = (long) lua_tonumberx(L,-1,&is_num); 
			if(!is_num){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_BYTE:
		{
			is_num = 0;
			rec->fields[i].data.b = (unsigned char) lua_tonumberx(L,-1,&is_num); 
			if(!is_num){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_FLOAT:
		{
			is_num = 0;
			rec->fields[i].data.f = (float) lua_tonumberx(L,-1,&is_num); 
			if(!is_num){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_DOUBLE:
		{
			is_num = 0;
			rec->fields[i].data.d = (double) lua_tonumberx(L,-1,&is_num); 
			if(!is_num){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_STRING:
		{
			char *s = (char*)lua_tostring(L,-1);
			if(!s){
				return -1;
			}

			size_t sz = strlen(s);
			rec->fields[i].data.s = (char *)malloc(sz+1);
			if(!rec->fields[i].data.s){
				return -1;
			}

			memset(rec->fields[i].data.s,0,sz+1);
			memcpy(rec->fields[i].data.s,s,sz);
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		case TYPE_DATE:
		{ 
			char *s = (char*)lua_tostring(L,-1);
			if(!s){
				return -1;
			}
			if((rec->fields[i].data.date = convert_date_to_number(-1,s)) == 0){
				return -1;
			}
			rec->field_set[i] = 1;
			lua_pop(L,1);
			break;
		}
		/*TYPE ARRAYS*/
		default:
		}
	}

	return check_type;
}



static int get_free_slot_cache(struct Cache *c)
{
	int i;
	for(i = 0; i < CACHE_SIZE; i++)
		if(c[i].index_file == NULL)
			return i;

	return -1;
}

static int check_and_free_one_cache(struct Cache *c)
{
	int i;
	for(i = 0; i < (int)CACHE_SIZE; i++){
		if((long)(c[i].used - c[i].ts) > (long) THREE_HOURS){
			if(write_cache_to_disk(&c[i]) == -1){
				fprintf(stderr,"cannot write cache to disk!!!%s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			free_cache(&c[i]);
			return i;
		}
	}

	return -1;
}

static int is_test(lua_State *L)
{	
	lua_getglobal(L,"TEST");
	int b = lua_toboolean(L,-1);
	if(b){
		lua_pop(L,1);
		return 1;
	}
	lua_pop(L,1);
	return 0;
} 

