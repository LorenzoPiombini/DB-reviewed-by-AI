#ifndef _KEY_H
#define _KEY_H 1

#include "db_types.h"
#include "file.h"
#include "hash_tbl.h"

enum mode{
	REG,
	BASE,
	INCREM,
	MAKE_KEY_JS_STRING
};

#define KEY_GET_ALL_CACHE		0x00010000 
#define IS_KEY_GET_ALL_CACHE(n) (((n) & KEY_GET_ALL_CACHE) != 0)
#define KEY_GEN_DISK_MODE 		0x00000100
#define KEY_GEN_CACHE_MODE 		0x00000200
#define IS_KEY_CACHE_MODE(n) 	(((n) & KEY_GEN_CACHE_MODE) != 0 )
#define IS_KEY_DISK_MODE(n) 	(((n) & KEY_GEN_DISK_MODE) != 0)
#define GET_MAKE_KEY_JS_STRING(n) ((n) & MAKE_KEY_JS_STRING)

i64 generate_numeric_key(file_t *fds, int mode,int base,struct Cache *c);
char *get_all_keys_for_file(file_t *fds,int index,int mode,HashTable *index_file);

#endif
