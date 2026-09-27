#ifndef FILE_H
#define FILE_H 1

#include "db_types.h"
#include "record.h"
#include "hash_tbl.h"
#include "parse.h"
#include <time.h>

#define STD_RAM_FILE 4096*1000 /* 4 MiB */  
#if defined(__linux__) || defined(__APPLE__)
#define WOS 0
#elif defined(_WIN32) || defined(_WIN64)
#define WOS 1
#endif

#define INIT_FILE_T_ARRAY(f,size) \
	do{\
		if(WOS){\
			memset((f),0,sizeof(file_t)*(size));\
		}else{\
			memset((f),-1,sizeof(file_t)*(size));\
		}\
	}while(0)

#if WOS
#define IS_FILE_T_VALID(f) ((f) != NULL)
#else
#define IS_FILE_T_VALID(f) ((f) != -1)
#endif

struct Ram_file{
	ui8 *mem; /* memory */
	ui64 capacity; /*memory size*/
	ui64 size; /* size of the data written to memory*/
	ui64 offset; /* the place where we are in the file in memory */
};

#define FILE_IS_CACHED -2 /*error value to define the presence of a file in the cache*/


struct Cache{
	HashTable *index_file;
	int indexes;
	char *file_name;
	struct Ram_file data_file;
	struct Schema sch;
	time_t ts;
	time_t used;
};

/*these are valid for all OS*/
int open_file(char *fileName, int use_trunc, file_t *fd);
int create_file(char *fileName, file_t *fd);
int os_read(file_t fd, void* data, size_t size);
int os_write(file_t fd, void* data, size_t size);
int write_ram_record(struct Ram_file *ram, struct Record_f *rec, int update, size_t init_ram_size, file_offset offset);
long long read_ram_file(char* file_name, struct Ram_file *ram, struct Record_f *rec, struct Schema sch);
file_offset read_update_offset_ram_file(struct Ram_file *ram);
void clear_ram_file(struct Ram_file *ram);
void close_ram_file(struct Ram_file *ram);
int init_ram_file(struct Ram_file *ram, size_t size);
int file_error_handler(int count, ...);
unsigned char read_index_file(file_t file_handle, HashTable *ht);
unsigned char read_all_index_file(file_t file_handle, HashTable **ht, int *p_index);
unsigned char nr_bucket(file_t file_handle, int *p_buck);
unsigned char indexes_on_file(file_t file_handle, int *p_i_nr);
unsigned char read_index_nr(int i_num, file_t file_handle, HashTable **ht);
unsigned char write_index_file_head(file_t file_handle,int index_num);
unsigned char write_index_body(file_t file_handle, int i, HashTable *ht);
int write_file(file_t fd, struct Record_f *rec, file_offset update_file_offset, unsigned char update);
int read_file(file_t fd, char *file_name, struct Record_f *rec, struct Schema sch);
int get_all_record(file_t fd, struct Ram_file *ram);
int add_index(int index_nr, char *file_name, int bucket);

#if defined(__linux__) || defined(__APPLE__)

/*API end points*/
void close_file(int count, ...);
file_offset get_file_offset(file_t fd);
file_offset go_to_EOF(file_t fd);
file_offset find_record_position(int fd, file_offset offset);
void delete_file(unsigned short count, ...);
file_offset begin_in_file(int fd);
file_offset move_in_file_bytes(int fd, file_offset offset);
size_t record_size_on_disk(void *rec_f);
int buffered_write(int *fd, struct Record_f *rec, int update, file_offset rec_ram_file_pos, file_offset offset);
file_offset get_update_offset(int fd);
int padding_file(int fd, int bytes, size_t hd_st);
unsigned char indexes_on_file(int fd, int *p_i_nr);
unsigned char nr_bucket(int fd, int *p_buck);
file_offset get_file_size(int fd, char *file_name);
int cache_file(int *fds,char *file_name,struct Schema *sch,struct Cache *c,HashTable *cache_register,int cache_pos);
void free_cache(struct Cache *c);
#elif defined(_WIN32)

#include <windows.h>

int error_to_string(ui64 error,char *buffer);
int cache_file(file_t *fd,char *file_name,struct Schema *sch,struct Cache *c,HashTable *cache_register,int cache_pos);
int buffered_write(file_t *file_handle, struct Record_f *rec, int update, file_offset rec_ram_file_pos, file_offset offset);
int get_all_record(file_t file_handle, struct Ram_file *ram);
void close_file(int count, ...);
void free_cache(struct Cache *c);
int delete_file(int count,...);
file_offset get_update_offset(file_t file_handle);
file_offset begin_in_file(file_t file_handle);
file_offset get_file_offset(file_t file_handle);
file_offset find_record_position(file_t file_handle, long long offset);
file_offset go_to_EOF(file_t file_handle);
file_offset move_in_file_bytes(file_t file_handle, file_offset offset);
DWORD get_file_size(file_t file_handle);
#endif /* os if*/
#endif /* ifndef */
