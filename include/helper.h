#ifndef HELPER_H
#define HELPER_H

#include "hash_tbl.h"
#include "str_op.h"
#include "file.h"

#if defined(_WIN32)
	#include <windows.h>
#endif

unsigned char create_empty_file(file_t fd_schema, file_t fd_index, int bucket_ht);
unsigned char append_to_file(file_t *fds, char *file_path, char *key,char files[][MAX_FILE_PATH_LENGTH],char *data_to_add, HashTable *ht);
int create_file_with_schema(file_t fd_schema, file_t fd_index, char *schema_def, int bucket_ht, int indexes, int file_field);

#endif
