#ifndef _TEST_H
#define _TEST_FILE_H 1

#include "crud.h"
#include "file.h"
#include "record.h"
#include <stdarg.h>

#if defined(__linux__) || defined(__APPLE__)
/*for now keep the lua env only in Linux*/
	#include "export_db_lua.h"
	#include "lua_start.h"
	#ifdef FEDORA 
		#include "lua.h"
		#include "lauxlib.h"
		#include "lualib.h"

	#else
		#include "lua5.4/lua.h"
		#include "lua5.4/lauxlib.h"
		#include "lua5.4/lualib.h"
	#endif
#endif

int DB_test_count_fields();
int DB_test_combine_old_and_new_rec(struct Schema *s,file_t *fds,char files[3][MAX_FILE_PATH_LENGTH], char *file_name);
int create_file_for_test(char *file_name,char *file_definition, struct Schema *sch, char files[3][MAX_FILE_PATH_LENGTH],file_t *fds);
int create_file_test();
int delete_file_test(char files_name[3][MAX_FILE_PATH_LENGTH]);
int lock_file_test();
int CRUD_test_check_data();
int CRUD_test_write_record();

/*LUA just keep it for Linux for now*/
#if defined(__linux__) || defined(__APPLE__)
int LUA_test_save_key_at_index(struct Schema *sch);
int LUA_test_save_key_at_index_chache(struct Schema *sch);
int LUA_test_create_record(struct Schema *sch);
int LUA_test_w_rec(struct Schema *sch);
int LUA_test_get_numeric_key_cache();
int LUA_test_get_numeric_key();
int LUA_test_update_rec_cache();
int LUA_test_w_rec_cache(struct Schema *sch);
int LUA_port_table_to_record_test();
int LUA_test_write_customer_cache();
#elif defined(_WIN32) || defined (_WIN64)
int lock_file_test_WINDOWS();
#endif

#endif

