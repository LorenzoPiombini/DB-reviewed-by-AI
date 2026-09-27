#if defined(__linux__) || defined(__APPLE__)
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <time.h>
#else /*linux*/
	#include <windows.h>
#endif

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "file.h"
#include "crud.h"
#include "str_op.h"
#include "common.h"
#include "endian.h"
#include "debug.h"
#include "lock.h"
#include "string_utilities.h"

static char prog[] = "db";
static size_t get_disk_size_record(struct Record_f *rec);
static void move_ram_file_ptr(struct Ram_file *ram,size_t size);
static size_t get_string_size(file_t fd, struct Ram_file *ram);

static int ERROR_CODE_FILE_OPERATION = 0;

int open_file(char *fileName, int use_trunc, file_t *fd)
{
	ERROR_CODE_FILE_OPERATION = 0;
#if defined(__linux__) || defined(__APPLE__)
	errno = 0;
	if (!use_trunc) {
		*fd = open(fileName, O_RDWR , S_IRWXU);
	} else {
		*fd = open(fileName, O_WRONLY | O_TRUNC, S_IRWXU);
	}

	if ( errno != 0) {
		*fd = errno;
		return errno;			
	}
#elif defined(_WIN32)
	DWORD creation = 0;	
	DWORD access = 0;

	if(!use_trunc){
		access = GENERIC_WRITE | GENERIC_READ;
		creation = OPEN_EXISTING;
	}else{
		access = GENERIC_WRITE | GENERIC_READ;
		creation = TRUNCATE_EXISTING;
	}

	*fd = (file_t)CreateFileA(fileName,access,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,creation,FILE_ATTRIBUTE_NORMAL,NULL);
	if(!(*fd) || *fd == INVALID_HANDLE_VALUE){
		ERROR_CODE_FILE_OPERATION = GetLastError();
		return -1;
	}
#endif
	return 0;
}

int create_file(char *fileName, file_t *fd)
{

	ERROR_CODE_FILE_OPERATION = 0;
#if defined(__linux__) || defined(__APPLE__)
	*fd = open(fileName, O_RDONLY);
	if (*fd != STATUS_ERROR) {
		printf("File already exist.\n");
		close(*fd);
		*fd = EEXIST;
		return -1;
	}

	*fd = open(fileName, O_RDWR | O_CREAT, S_IRWXU);

	if (*fd == STATUS_ERROR) {
		perror("open");
		return STATUS_ERROR;
	}

	return 0;
#elif defined(_WIN32)
	DWORD err = GetFileAttributesA(fileName);
	if(err !=  INVALID_FILE_ATTRIBUTES){
		fprintf(stderr,"file already exist\n");
		return -1;
	}

	*fd = CreateFileA(fileName,GENERIC_READ | GENERIC_WRITE, 
								FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
								NULL,
								CREATE_NEW,
								FILE_ATTRIBUTE_NORMAL,
								NULL);


	if(fd == NULL || fd == INVALID_HANDLE_VALUE){
		ERROR_CODE_FILE_OPERATION = GetLastError();
		return -1;
	}
	return 0;
#endif
}

#if defined(__linux__) || defined(__APPLE__)
void close_file(int count, ...)
{
	int sum = 0;
	short i = 0;

	va_list args;
	va_start(args, count);

	for (i = 0; i < count; i++) {
		file_t fd = va_arg(args, file_t);
		if (fd > 2 && close(fd) != 0)
			sum++;
	}

	if (sum > 0)
		perror("close");
}

void delete_file(unsigned short count, ...)
{
	int sum = 0;
	short i = 0;
	va_list args;
	va_start(args, (int)count);

	for (i = 0; i < count; i++) {
		char *file_name = va_arg(args, char *);
		if (unlink(file_name) != 0)
			sum++;
	}

	if (sum > 0)
		perror("unlink");
}

file_offset begin_in_file(file_t fd)
{

	file_offset pos = lseek((int)fd, 0, SEEK_SET);
	if (pos == STATUS_ERROR) {
		perror("set begin in file");
		return pos;
	}

	return pos;
}

file_offset get_file_offset(file_t fd)
{

	file_offset offset = lseek((int)fd, 0, SEEK_CUR);

	if (offset == STATUS_ERROR)
	{
		perror("get offset: ");
		return offset;
	}

	return offset;
}

file_offset go_to_EOF(file_t fd)
{
	file_offset eof = lseek((int)fd, 0, SEEK_END);
	if (eof == STATUS_ERROR)
	{
		perror("could not find end of file");
		return eof;
	}

	return eof;
}

file_offset find_record_position(file_t fd, file_offset offset)
{
	file_offset pos = lseek((int)fd, offset, SEEK_SET);
	if (pos == STATUS_ERROR)
	{
		perror("seeking offset.");
		return pos;
	}

	return pos;
}

/*
 * move_in_file_bytes will change the file pointer of offset bytes
 *	example:
 *		move_in_file_bytes(fd, -4); will move the file pointer backwords of 4 bytes
 *
 * */
file_offset move_in_file_bytes(file_t fd, file_offset offset)
{
	file_offset current_p = get_file_offset(fd);
	file_offset move_to = current_p + offset;
	file_offset pos = 0;
	if ((pos = lseek((int)fd, move_to, SEEK_SET)) == STATUS_ERROR) {
		perror("seeking offset.");
		return pos;
	}

	return pos;
}

file_offset get_file_size(file_t fd, char *file_name)
{
	struct stat st;
	if (!file_name)
	{
		if (fstat((int)fd, &st) == -1)
		{
			perror("stat failed :");
			printf("%s:%d.\n", F, L - 3);
			return -1;
		}

		return (file_offset)st.st_size;
	}
	else
	{
		if (stat(file_name, &st) == -1)
		{
			perror("stat failed :");
			printf("%s:%d.\n", F, L - 3);
			return -1;
		}

		return (file_offset)st.st_size;
	}
}
	/*END OF LINUX SPECIFIC CODE*/
#elif defined(_WIN32) 



int error_to_string(ui64 error,char *buffer){
	DWORD systemResult = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			buffer,
			1024,
			NULL
			);
	return 0;
}


void close_file(int count, ...)
{
	int fails = 0;

	va_list args;
	va_start(args,count);

	int i;
	for(i = 0; i < count; i++){
		file_t h = va_arg(args,file_t);
		if(h != INVALID_HANDLE_VALUE && h){
			if(!CloseHandle(h))
				fails++;
		}
	}
		
	va_end(args);
	if(fails) fprintf(stderr,"error closing the file: %ld",GetLastError());
}

int delete_file(int count,...)
{
	int err = 0;
	va_list args;
	va_start(args,count);
	int i;
	for(i = 0; i < count; i++){
		char *file_name = va_arg(args, char*);
		if(file_name){
			if(!DeleteFileA(file_name))
				err++;
		}
	}

	if(err){
		fprintf(stderr,"failed to delete one or more file, error code: %ld\n",GetLastError());
		va_end(args);
		return -1;
	}

	va_end(args);
	return 0;

}

static file_offset seek_file_win(file_t file_handle,long long offset, DWORD file_position)
{
	LARGE_INTEGER li;
	li.QuadPart = offset;
	LARGE_INTEGER new_file_ptr;
	if(!SetFilePointerEx(file_handle,li,&new_file_ptr,file_position)){
		fprintf(stderr,"seek failed %s:%d\n",__FILE__,__LINE__-1);
		return -1;
	}

	return (file_offset) new_file_ptr.QuadPart;
}

file_offset begin_in_file(file_t fd)
{
	return seek_file_win(fd,0,FILE_BEGIN);

}

file_offset get_file_offset(file_t fd)
{
	return seek_file_win(fd,0,FILE_CURRENT);
}

file_offset find_record_position(file_t fd, long long offset)
{
	return seek_file_win(fd,offset,FILE_BEGIN);

}

file_offset go_to_EOF(file_t fd)
{
	return seek_file_win(fd,0,FILE_END);
}

file_offset move_in_file_bytes(file_t fd, file_offset offset)
{
	return seek_file_win(fd,offset,FILE_CURRENT);
}

DWORD get_file_size(file_t file_handle)
{
	return GetFileSize(file_handle, NULL);
}
#endif

static size_t get_disk_size_record(struct Record_f *rec)
{
	size_t size = sizeof(ui8);
	int i;
	for(i = 0; i < rec->fields_num; i++){
		if(rec->field_set[i] == 0) continue;

		size += sizeof(ui8);
		switch(rec->fields[i].type){
		case -1:
			break;
		case TYPE_KEY:
		case TYPE_INT:
		case TYPE_DATE:
		case TYPE_FLOAT:
			size += sizeof(ui32);
			break;
		case TYPE_LONG:
		case TYPE_DOUBLE:
			size += sizeof(ui64);
			break;
		case TYPE_BYTE:
			size += sizeof(ui8);
			break;
		case TYPE_STRING:
			size += (sizeof(ui32) + sizeof(ui16));
			size += ((strlen(rec->fields[i].data.s) * 2) + 1);
			break;
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
			size++;
			size += (sizeof(ui32) * 2);
			size += (sizeof(ui32) * rec->fields[i].data.v.size);
			size += (sizeof(ui64));
			break;
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
			size++;
			size += (sizeof(ui32) * 2);
			size += (sizeof(ui64) * rec->fields[i].data.v.size);
			size += (sizeof(ui64));
			break;
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
			size++;
			size += (sizeof(ui32) * 2);
			size += (sizeof(ui8) * rec->fields[i].data.v.size);
			size += (sizeof(ui64));
			break;
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
			size++;
			size += (sizeof(ui32) * 2);
			size += (sizeof(ui32) * rec->fields[i].data.v.size);
			size += (sizeof(ui64));
			break;
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
			size++;
			size += (sizeof(ui32) * 2);
			size += (sizeof(ui64) * rec->fields[i].data.v.size);
			size += (sizeof(ui64));
			break;
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
			size++; /*one byte for option set*/
			size += (sizeof(ui32) * 2);
			size += ((sizeof(ui32) + sizeof(ui16)) * rec->fields[i].data.v.size);
			int j;
			for(j = 0; j < rec->fields[i].data.v.size; j++){
				size += ((strlen(rec->fields[i].data.v.elements.s[j]) * 2) + 1);
			}
			size += (sizeof(ui64));
			break;
		case TYPE_FILE:
			size += (sizeof(ui32) * 2);
			ui32 k;
			for(k = 0; k < rec->fields[i].data.file.count; k++){
				size += get_disk_size_record(&rec->fields[i].data.file.recs[k]);
			}
			size += (sizeof(ui64));
			break;
		default:
			return -1;
		}
	}
	
	size += (sizeof(ui64));
	return size;
}

int os_read(file_t fd, void* data, size_t size)
{
#if defined(__linux__) || defined(__APPLE__)
		if(read(fd,data,size) == -1){
			fprintf(stderr,"(%s):read from file failed,%s:%d.\n",prog,__FILE__,__LINE__);
			return -1;
		}

#elif defined(_WIN32)
		DWORD bread = 0;
		if(!ReadFile(fd,data,size,&bread,NULL)){
			fprintf(stderr,"(%s):read from file failed,%s:%d.\n",prog,__FILE__,__LINE__);
			return -1;
		}
#endif
		return 0;

}
int os_write(file_t fd, void* data, size_t size)
{

#if defined(__linux__) || defined(__APPLE__)
		if(write(fd,data,size) == -1){
			fprintf(stderr,"(%s):read from file failed,%s:%d.\n",prog,__FILE__,__LINE__);
			return -1;
		}

#elif defined(_WIN32)
		DWORD written = 0;
		if(!WriteFile(fd,data,size,&written,NULL)){
			fprintf(stderr,"(%s):read from file failed,%s:%d.\n",prog,__FILE__,__LINE__);
			return -1;
		}
#endif
		return 0;
}



static int is_array_last_block(file_t fd, struct Ram_file *ram, int element_nr, size_t bytes_each_element, int type)
{

#if defined(__linux__) || defined(__APPLE__)
	if(fd != -1)
#elif defined(_WIN32)
	if(fd)
#endif
	{
		file_offset go_back_to = 0;
		if ((go_back_to = get_file_offset(fd)) == -1)
		{
			__er_file_pointer(F, L - 1);
			return -1;
		}

		if(type == TYPE_ARRAY_STRING || type == TYPE_SET_STRING){
			int i;
			for(i = 0; i < element_nr; i++){
				if(get_string_size(fd,NULL) == (size_t)-1){
					__er_file_pointer(F, L - 1);
					return -1;
				} 
			}
		}else {
			if(move_in_file_bytes(fd, element_nr * bytes_each_element) == -1)
			{
				__er_file_pointer(F, L - 1);
				return -1;
			}
		}	
		ui64 update_off_ne = 0;
		if(os_read(fd,&update_off_ne,sizeof(update_off_ne)) == -1){
			fprintf(stderr,"failed read update file_offset int array %s:%d.\n",__FILE__,__LINE__);
			return 0;
		}

		if (find_record_position(fd, go_back_to) == -1)
		{
			__er_file_pointer(F, L - 1);
			return -1;
		}

		file_offset update_pos = (file_offset)swap64(update_off_ne);

		return update_pos == 0;
	}

	if(ram){
		if(ram->mem){
			file_offset go_back_to = ram->offset;

			if(type == TYPE_ARRAY_STRING || type == TYPE_SET_STRING){
				int i;
				for(i = 0; i < element_nr; i++){
#if defined(__linux__) || defined(__APPLE__)
					if(get_string_size(-1,ram) == (size_t)-1)
#elif defined(_WIN32)
					if(get_string_size(NULL,ram) == (size_t)-1)
#endif

					{
						__er_file_pointer(F, L - 1);
						return -1;
					} 
				}
			}else {
				ram->offset += (element_nr * bytes_each_element);
			}	

			ui64 update_off_ne = 0;
			memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
			ram->offset += sizeof(ui64);

			ram->offset = go_back_to;

			file_offset update_pos = (file_offset)swap64(update_off_ne);

			return update_pos == 0;


		}
	}

	return -1;
}

static size_t get_string_size(file_t fd, struct Ram_file *ram)
{

#if defined(__linux__) || defined(__APPLE__)
	if(fd != -1 && ram) {
		fprintf(stderr,"(%s): wrong usage of %s(), you can pass either fd or Ram_file, both is not allowed.\n",prog,__func__);
		return -1;
	}

	if(fd != -1){
		if(move_in_file_bytes(fd,sizeof(ui32)) == -1) return -1;

		ui16 bu_ne = 0;
		if(os_read(fd,&bu_ne,sizeof(bu_ne)) == -1) return -1;

		size_t buffer_update = (size_t)swap16(bu_ne);

		if(move_in_file_bytes(fd,buffer_update) == -1) return -1;

		return 0;
	}
#else

	if(!fd && ram){
		fprintf(stderr,"(%s): wrong usage of %s(), you can pass either fd or Ram_file, both is not allowed.\n",prog,__func__);
		return -1;
	}
	if(fd && !ram){
		if(move_in_file_bytes(fd,sizeof(ui32)) == -1) return -1;

		ui16 bu_ne = 0;
		DWORD written = 0;
		if(!ReadFile(fd,&bu_ne,sizeof(bu_ne),&written,NULL)) return -1;

		size_t buffer_update = (size_t)swap16(bu_ne);

		if(move_in_file_bytes(fd,buffer_update) == -1) return -1;

		return 0;

	}
#endif

	if(ram){
		if(ram->mem){
			ram->offset += sizeof(ui32);			

			ui16 bu_ne = 0;
			memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
			ram->offset += sizeof(ui16);			

			size_t buff_up = (size_t)swap16(bu_ne); 
			ram->offset += buff_up;

			return 0;
		}	
	}

	return -1;
}

static void move_ram_file_ptr(struct Ram_file *ram,size_t size)
{
	if(ram->size == ram->offset){
		ram->size += size;
		ram->offset = ram->size;
	}else{
		ram->offset += size;
	}
}


unsigned char write_index_file_head(file_t fd, int index_num)
{

	file_offset pos = 0;

	long bwritten = 0;
	long msize = (long)(sizeof(pos) * index_num) + sizeof(index_num);
	ui8 *buff = malloc(msize);
	if(!buff)
		return 0;
	
	memset(buff,0,msize);

	ui32 in = swap32(index_num);
	memcpy(&buff[bwritten],&in,sizeof(ui32));
	bwritten += sizeof(ui32);

	int i = 0;
	for (i = 0; i < index_num; i++){
		ui64 p_n = swap64(pos);
		memcpy(&buff[bwritten],&p_n,sizeof(ui64));
		bwritten += sizeof(ui64);
	}

	if (os_write(fd,buff,bwritten) == -1){
		free(buff);
		return 0;
	}

	free(buff);
	return 1;
}

unsigned char write_index_body(file_t fd, int i, HashTable *ht)
{
	file_offset pos = 0;

	ui32 i_n = swap32(i);
	if (os_write(fd,&i_n,sizeof(i_n)) == -1){
		printf("write to file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	if ((pos = get_file_offset(fd)) == -1)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}

	if (!ht->write(fd, ht))
	{
		printf("write to file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	if (begin_in_file(fd) == -1)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}
	if (find_record_position(fd, sizeof(int)) == -1) {
		__er_file_pointer(F, L - 2);
		return 0;
	}

	if (i != 0) {
		if (move_in_file_bytes(fd, i * sizeof(pos)) == -1) {
			__er_file_pointer(F, L - 2);
			return 0;
		}
	}

	ui64 p_n = swap64(pos);
	if (os_write(fd,&p_n,sizeof(p_n)) == -1){
		printf("write to file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	if (go_to_EOF(fd) == STATUS_ERROR)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}
	return 1;
}

unsigned char read_index_nr(int i_num, file_t fd, HashTable **ht)
{
	if (begin_in_file(fd) == STATUS_ERROR)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}

	ui32 a_s = 0;
	if (os_read(fd, &a_s, sizeof(a_s)) == STATUS_ERROR)
	{
		fprintf(stderr,"read from file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	int array_size = (int)swap32(a_s);
	if (array_size == 0)
	{
		printf("wrong reading from file, check position. %s:%d.\n", F, L - 8);
		return 0;
	}

	if (array_size <= i_num)
	{
		printf("index number out of bound.\n");
		return 0;
	}

	file_offset move_to = i_num * sizeof(file_offset);
	if (move_in_file_bytes(fd, move_to) == STATUS_ERROR)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}

	ui64 i_p = 0;
	if(os_read(fd, &i_p, sizeof(i_p)) == STATUS_ERROR)
	{
		printf("read from fiel failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	file_offset index_pos = (file_offset)swap64(i_p);
	if (index_pos == 0)
	{
		printf("wrong reading from file, check position. %s:%d.\n", F, L - 8);
		return 0;
	}

	if (find_record_position(fd, index_pos) == STATUS_ERROR)
	{
		__er_file_pointer(F, L - 2);
		return 0;
	}

	if (!read_index_file(fd, *ht)){
		printf("read from file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	return 1;
}

unsigned char indexes_on_file(file_t fd, int *p_i_nr)
{

	if (begin_in_file(fd) == STATUS_ERROR) {
		__er_file_pointer(F, L - 2);
		return 0;
	}

	ui32 a_s = 0;
	if (os_read(fd, &a_s, sizeof(a_s)) == STATUS_ERROR) {
		printf("read from file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	int array_size = (int)swap32(a_s);
	if (array_size == 0) {
		printf("wrong reading from file, check position. %s:%d.\n", F, L - 8);
		return 0;
	}

	*p_i_nr = array_size;
	return 1;
}

unsigned char nr_bucket(file_t fd, int *p_buck)
{
	HashTable ht = {0};
	HashTable *pht = &ht;
	if (!read_index_nr(0, fd, &pht)) {
		printf("read from file failed. %s:%d.\n", F, L - 2);
		if (ht.size > 0) {
			destroy_hasht(&ht);
		}
		return 0;
	}

	*p_buck = ht.size;
	destroy_hasht(&ht);
	return 1;
}

unsigned char read_all_index_file(file_t fd, HashTable **ht, int *p_index)
{

	if (begin_in_file(fd) == STATUS_ERROR) {
		__er_file_pointer(F, L - 2);
		return 0;
	}

	ui32 a_s = 0;
	if (os_read(fd, &a_s, sizeof(a_s)) == STATUS_ERROR) {
		printf("read from file failed. %s:%d.\n", F, L - 2);
		return 0;
	}

	int array_size = (int)swap32(a_s);
	if (array_size == 0) {
		printf("wrong reading from file, check position. %s:%d.\n", F, L - 8);
		return 0;
	}

	*p_index = array_size;

	*ht = (HashTable*)malloc(array_size * sizeof(HashTable));
	if (!(*ht)) {
		printf("malloc failed. %s:%d.\n", F, L - 3);
		return 0;
	}

	memset(*ht,0,array_size * sizeof(HashTable));
	int i = 0;
	for (i = 0; i < array_size; i++)
		(*ht)[i].write = write_ht;

	file_offset move_to = (array_size * sizeof(file_offset)) + sizeof(int);
	if (move_in_file_bytes(fd, move_to) == STATUS_ERROR) {
		__er_file_pointer(F, L - 2);
		free(ht);
		return 0;
	}

	for (i = 0; i < array_size; i++)
	{
		if (!read_index_file(fd, &((*ht)[i])))
		{
			printf("read from file failed. %s:%d.\n", F, L - 2);
			free_ht_array(*ht, i);
			return 0;
		}

		if ((array_size - i) > 1)
		{
			if (move_in_file_bytes(fd, sizeof(int)) == STATUS_ERROR)
			{
				__er_file_pointer(F, L - 2);
				free_ht_array(*ht, i);
				return 0;
			}
		}
	}
	return 1;
}


unsigned char read_index_file(file_t fd, HashTable *ht)
{
	ui32 s_n = 0;
	if (os_read(fd, &s_n, sizeof(s_n)) < 0){
		perror("reading Index file");
		return 0;
	}

	ht->size = (int)swap32(s_n); 

	ui32 ht_ln = 0;
	if (os_read(fd, &ht_ln, sizeof(ht_ln)) == STATUS_ERROR)
	{
		perror("reading ht length");
		return 0;
	}
	
	int ht_l = (int)swap32(ht_ln);
	register int i = 0;
	for (i = 0; i < ht_l; i++) {
		ui32 type = 0;
		if (os_read(fd, &type, sizeof(type)) == -1) 
		{
			fprintf(stderr, "can't read key type, %s:%d.\n",
					F, L - 3);
			free_nodes(ht->data_map, ht->size);
			return 0;
		}

		int key_type = (int)swap32(type);

		switch (key_type) {
#if defined(__linux__) || defined(__APPLE__)
		case STR:
#elif defined(_WIN32)
		case STR_KEY:
#endif
		{
			ui64 key_l = 0;
			if (os_read(fd, &key_l, sizeof(key_l)) == 0) 
			{
				size_t size = (size_t)swap64(key_l);
				char *key = (char*)malloc(size + 1);
				if (!key) {
					fprintf(stderr,"(%s): malloc failed, %s:%d.\n",prog,F,L-2);		
					free_nodes(ht->data_map, ht->size);
					return 0;
				}

				memset(key,0,size+1);
				ui64 v_n = 0l;
				if (os_read(fd, key, size + 1) == -1 ||
					os_read(fd, &v_n, sizeof(v_n)) == -1) 
				{
					fprintf(stderr,"(%s): read key failed, %s:%d.\n",prog,F,L-2);		
					free_nodes(ht->data_map, ht->size);
					free(key);
					return 0;
				}

				file_offset value = (file_offset)swap64(v_n);
				key[size] = '\0';
				Node *new_node = malloc(sizeof *new_node);
				if (!new_node){
					perror("memory for node");
					free_nodes(ht->data_map, ht->size);
					free(key);
					return 0;
				}

				memset(new_node,0,sizeof *new_node);
				new_node->key.k.s = duplicate_str(key);
				if (!new_node->key.k.s){
					fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F, L - 3);
					free_nodes(ht->data_map, ht->size);
					free(key);
					return 0;
				}
				free(key);
				new_node->next = NULL;
				new_node->key.type = key_type;
				new_node->value = value;

				int bucket = hash((void *)new_node->key.k.s, ht->size, key_type);
				if (ht->data_map[bucket]){
					Node *current = ht->data_map[bucket];
					while (current->next != NULL)
					{
						current = current->next;
					}
					current->next = new_node;
				}
				else{
					ht->data_map[bucket] = new_node;
				}
			}else{
				fprintf(stderr,"read index failed, %s:%d\n", F, L - 2);
				free_nodes(ht->data_map, ht->size);
				return 0;
			}
			break;
		}
#if defined(__linux__) || defined(__APPLE__)
		case UINT:
#elif defined(_WIN32)
		case UINT_KEY:
#endif
		{
			ui8 size = 0;
			ui32 k = 0;
			ui16 k16 = 0;
			ui64 value = 0;
			if (os_read(fd, &size, sizeof(size)) == -1)
			{
				fprintf(stderr, "read index failed.\n");
				free_nodes(ht->data_map, ht->size);
				return 0;
			}
			if(size == 16){
				if (os_read(fd, &k16, sizeof(k16)) == -1)
				{	
					fprintf(stderr, "read index failed.\n");
					free_nodes(ht->data_map, ht->size);
					return 0;
				}
			}else{
				if (os_read(fd, &k, sizeof(k)) == -1)
				{
					fprintf(stderr, "read index failed.\n");
					free_nodes(ht->data_map, ht->size);
					return 0;
				}
			}

			if (os_read(fd, &value, sizeof(value)) == -1){
				fprintf(stderr, "read index failed.\n");
				free_nodes(ht->data_map, ht->size);
				return 0;
			}

			Node *new_node = (Node*)malloc(sizeof *new_node);
			if (!new_node){
				fprintf(stderr,"malloc failed, %s:%d.\n",F,L-2);
				free_nodes(ht->data_map, ht->size);
				return 0;
			}
			memset(new_node,0,sizeof *new_node);
			new_node->key.type = key_type;
			if(size == 16)
				new_node->key.k.n16 = swap16(k16);
			else
				new_node->key.k.n = swap32(k);

			new_node->value = (file_offset)swap64(value);
			new_node->next = NULL;
			new_node->key.size = size;

			int index = hash((void *)&new_node->key.k.n, ht->size, key_type);
			if (index == -1){
				fprintf(stderr, "read index failed.%s:%d\n", F, L - 2);
				free_nodes(ht->data_map, ht->size);
				free_ht_node(new_node);
				return 0;
			}

			if (ht->data_map[index]){
				Node *current = ht->data_map[index];
				if(current->next){
					while (current->next) current = current->next;
					current->next = new_node;
				}else{
					current->next = new_node;
				}
			}else{
				ht->data_map[index] = new_node;
			}

			break;
		}
		default:
			fprintf(stderr, "key type not supported.\nsize ht is %d\nht_l is %d\n",ht->size,ht_l);
			free_nodes(ht->data_map, ht->size);
			return 0;
		}
	}

	return 1; /*true*/
}

size_t record_size_on_disk(void *rec_f)
{
	size_t rec_size = 0;
	struct Record_f *rec = (struct Record_f *)rec_f;

	rec_size += sizeof(rec->fields_num);
	/*each field name length*/
	rec_size += (sizeof(ui64) * rec->fields_num);

	/*each field type*/
	rec_size += (sizeof(ui32) * rec->fields_num);

	int i;
	for (i = 0; i < rec->fields_num; i++)
	{
		/*actual name length wrote to disk*/
		rec_size += strlen(rec->fields[i].field_name);

		switch (rec->fields[i].type)
		{
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DATE:
			rec_size += sizeof(ui32);
			break;
		case TYPE_LONG:
		case TYPE_DOUBLE:
			rec_size += sizeof(ui64);
			break;
		case TYPE_BYTE:
			rec_size += sizeof(ui16);
			break;
		case TYPE_STRING:
			rec_size += (sizeof(ui64) * 3);
			rec_size += (strlen(rec->fields[i].data.s) * 2) + 1;
			break;
		default:
			printf("unknown type");
			return -1;
		}
	}

	/*any eventual update position*/
	rec_size += sizeof(ui64);
	return rec_size;
}

/*TODO: are we using this?????*/
int write_file(file_t fd, struct Record_f *rec, file_offset update_file_offset, unsigned char update)
{
	file_offset go_back_to = 0;
	        
	
	/* ----------these variables are used to handle the strings-------- */
	/* now each string fields can be updated regardless the string size */
	/* ----------some realities might required such a feature----------- */

	size_t lt = 0, new_lt = 0;
	file_offset str_loc = 0, af_str_loc_pos = 0, eof = 0;
	size_t buff_update = 0;
	size_t __n_buff_update = 0;
	file_offset move_to = 0, bg_pos = 0;

	/*--------------------------------------------------*/

	/*count the active fields in the record*/
	ui8 count = 0;

	int i;
	for(i = 0; i < rec->fields_num; i++){
		if (rec->field_set[i] == 1)  count++;
	}

	if(count == 0) return NTG_WR;

	/*
	 * writes the number of fields active 
	 * that are going to be written to the file
	 * */

	if (os_write(fd, &count, sizeof(count)) < 0) 
	{
		perror("could not write fields number");
		return 0;
	}


	for(i = 0; i < rec->fields_num; i++){
		if (rec->field_set[i] == 0) continue;

		/*position in the field_set array*/
		ui8 i_ne = (ui8)i;
		if (os_write(fd, &i_ne, sizeof(i_ne)) < 0) 
		{
			perror("could not write fields number");
			return 0;
		}
	}

	for (i = 0; i < rec->fields_num; i++) {
		if(rec->field_set[i] == 0) continue;

		switch (rec->fields[i].type){
		case TYPE_INT:
		{
			ui32 i_ne = swap32(rec->fields[i].data.i);
			if (os_write(fd, &i_ne, sizeof(i_ne)) < 0){
				perror("error in writing int type to file.\n");
				return 0;
			}
			break;
		}
		case TYPE_LONG: 
		{
			ui64 l_ne = swap64((ui64)rec->fields[i].data.l);
			if (os_write(fd, &l_ne, sizeof(l_ne)) < 0){
				perror("error in writing long type to file.\n");
				return 0;
			}
			break;
		}
		case TYPE_FLOAT:
		{
			ui32 f = htonf(rec->fields[i].data.f);
			if (os_write(fd, &f, sizeof(f)) < 0){
				perror("error in writing float type.\n");
				return 0;
			}
			break;
		}
		case TYPE_STRING:
		{
			if (!update)
			{
				lt = strlen(rec->fields[i].data.s);
				buff_update = lt * 2;

				/* adding 1 for '\0'*/
				lt++, buff_update++;
				char buff_w[buff_update];
				memset(buff_w,0,buff_update);

				strncpy(buff_w, rec->fields[i].data.s, lt - 1);

				ui16 bu_ne = swap16((ui16)buff_update);
				ui32 str_loc_ne = swap32((ui32)str_loc);

				if (os_write(fd, &str_loc_ne, sizeof(str_loc_ne)) < 0 ||
					os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
					os_write(fd, buff_w, buff_update) < 0)
				{
					perror("write file failed: ");
					printf(" %s:%d", F, L - 2);
					return 0;
				}
			}
			else
			{

				__n_buff_update = 0;
				eof = 0;
				/*save the starting offset for the string record*/
				if ((bg_pos = get_file_offset(fd)) == STATUS_ERROR){
					__er_file_pointer(F, L - 2);
					return 0;
				}

				/*read pos of new str if any*/
				ui32 str_loc_ne = 0;
				if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == STATUS_ERROR)
				{
					perror("can't read string location: ");
					printf(" %s:%d", F, L - 3);
					return 0;
				}

				str_loc = (file_offset)swap32(str_loc_ne);

				/*store record beginning pos*/
				if ((af_str_loc_pos = get_file_offset(fd)) == STATUS_ERROR)
				{
					__er_file_pointer(F, L - 2);
					return 0;
				}

				ui16 bu_ne = 0;
				if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
				{
					perror("can't read safety buffer before writing string.\n");
					printf("%s:%d", F, L - 3);
					return 0;
				}

				buff_update = (file_offset)swap16(bu_ne);

				/*save the end offset of the first string record */
				if((go_back_to = get_file_offset(fd)) == STATUS_ERROR)
				{
					__er_file_pointer(F, L - 2);
					return 0;
				}

				/*add the buffer size to this file file_offset*/
				/*so we can reposition right after the 1st string after the writing */
				go_back_to += buff_update;
				if (str_loc > 0)
				{
					/*set the file pointer to str_loc*/
					if (find_record_position(fd, str_loc) == STATUS_ERROR)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
					/*
					 * in the case of a regular buffer update we have
					 *  to save the file_offset to get back to it later
					 * */
					if ((move_to = get_file_offset(fd)) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}

					ui16 bu_ne = 0;
					if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
					{
						perror("read file.\n");
						printf("%s:%d", F, L - 3);
						return 0;
					}

					buff_update = (file_offset)swap16(bu_ne);
				}

				new_lt = strlen(rec->fields[i].data.s) + 1; /*get new str length*/

				if (new_lt > buff_update) {
					/*
					 * if the new length is bigger then the buffer,
					 * set the file pointer to th of the file
					 * to write the new data
					 * */
					if ((eof = go_to_EOF(fd)) == STATUS_ERROR){
						__er_file_pointer(F, L - 2);
						return 0;
					}

					/*expand the buff_update only for the bytes needed*/
					buff_update += (new_lt - buff_update);
				}
				char buff_w[buff_update];
				memset(buff_w,0,buff_update);

				strncpy(buff_w, rec->fields[i].data.s, new_lt - 1);
				/*
				 * if we did not move to another position
				 * set the file pointer back to the begginning of the string record
				 * to overwrite the data accordingly
				 * */
				if (str_loc == 0 && (__n_buff_update == 0))
				{
					if (find_record_position(fd, af_str_loc_pos) == -1)
					{
						__er_file_pointer(F, L - 3);
						return 0;
					}
				}
				else if (str_loc > 0 && (__n_buff_update == 0))
				{
					if (find_record_position(fd, move_to) == STATUS_ERROR)
					{
						__er_file_pointer(F, L - 3);
						return 0;
					}
				}

				/*
				 * write the data to file --
				 * the file pointer is always pointing to the
				 * right position at this point */
				bu_ne = swap16((ui16)buff_update);

				if (os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
					os_write(fd, buff_w, buff_update) < 0)
				{
					perror("error in writing type string (char *)file.\n");
					return 0;
				}


				/*
				 * if eof is bigger than 0 means we updated the string
				 * we need to save the file_offset of the new written data
				 * at the start of the original.
				 * */
				if (eof > 0)
				{
					/*go at the beginning of the str record*/
					if (find_record_position(fd, bg_pos) == STATUS_ERROR)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}

					/*update new string position*/
					ui32 eof_ne = swap32((ui32)eof);
					if (os_write(fd, &eof_ne, sizeof(eof_ne)) == STATUS_ERROR)
					{
						perror("write file: ");
						printf(" %s:%d", F, L - 3);
						return 0;
					}

					/*set file pointer to the end of the 1st string rec*/
					/*this step is crucial to avoid losing data        */

					if (find_record_position(fd, go_back_to) == STATUS_ERROR)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
				else if (str_loc > 0)
				{
					/*
					 * Make sure that in all cases
					 * we go back to the end of the 1st record
					 * */
					if (find_record_position(fd, go_back_to) == STATUS_ERROR){
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}
			break;
		}
		case TYPE_BYTE:
		{
			if (os_write(fd, &rec->fields[i].data.b, sizeof(unsigned char)) < 0)
			{
				perror("error in writing type byte to file.\n");
				return 0;
			}
			break;
		}
		case TYPE_PACK:
		{
			ui32 p_ne = swap32(rec->fields[i].data.p);
			if (os_write(fd, &p_ne, sizeof(p_ne)) < 0){
				perror("error in writing type byte to file.\n");
				return 0;
			}
			break;
		}
		case TYPE_DOUBLE:
		{
			ui64 d_ne = htond(rec->fields[i].data.d);
			if (os_write(fd, &d_ne, sizeof(d_ne)) < 0){
				perror("error in writing double to file.\n");
				return 0;
			}
			break;
		}
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)	{
					perror("error in writing size array to file.\n");
					return 0;
				}


				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1){
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = swap32(0);
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1){
					perror("error in writing size array to file.\n");
					return 0;
				}

				int k;
				for (k = 0; k < rec->fields[i].data.v.size; k++)
				{
					if (!rec->fields[i].data.v.elements.i[k])
						continue;
					ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[k]);

					if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;

				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, sizeof(int),0)) == -1)
						{
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);

							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
							sz--;
						}

						if (exit)
						{
							/*write the epty update offset*/
							ui64 empty_offset = swap64(0);
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1){
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								if (!rec->fields[i].data.v.elements.i[step])
									continue;

								ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.v.size)) exit = 0;
							}
						}

						if (exit)
						{

							if (padding_value > 0)
							{
								if (move_in_file_bytes(fd, padding_value * sizeof(int)) == -1)
								{
									__er_file_pointer(F, L - 1);
									return 0;
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = swap64(0);
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						if (move_in_file_bytes(fd, padding_value * sizeof(int)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);
					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = 0;
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for(j = 0; j < size_left; j++){
							if (step < rec->fields[i].data.v.size)
							{
								if (!rec->fields[i].data.v.elements.i[step])
									continue;

								ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = swap64(0);
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);

					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}
					/*read and check the padding, */
					ui32 pad_ne = 0;

					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int j;
					for (j = step; j < rec->fields[i].data.v.size; j++)
					{
						if (!rec->fields[i].data.v.elements.i[j])
							continue;

						ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[j]);
						if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
						{
							perror("failed write int array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					if (move_in_file_bytes(fd, pd_he * sizeof(int)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					ui64 update_arr_ne = swap64(0);
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							if (!rec->fields[i].data.v.elements.i[step])
								continue;

							ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);

							if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
							{
								perror("failed write int array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						if (move_in_file_bytes(fd, pd_he * sizeof(int)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}

			break;
		}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = swap32(0);
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				int j;
				for ( j = 0; j < rec->fields[i].data.v.size; j++)
				{
					ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[j]);
					if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;

				ui8 set = (ui8) rec->fields[i].data.v.is_set;

				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, sizeof(long),0)) == -1)
						{
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);

							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);

								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
							sz--;
						}

						if (exit)
						{
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);

									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.v.size)) exit = 1;
							}
						}

						if (exit)
						{
							if (padding_value > 0)
							{
								if (move_in_file_bytes(fd, padding_value * sizeof(long)) == -1)
								{
									__er_file_pointer(F, L - 1);
									return 0;
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						if (move_in_file_bytes(fd, padding_value * sizeof(long)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);

					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = swap32(0);
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);		
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}


					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int k;
					for (k = step; k < rec->fields[i].data.v.size; k++)
					{
						ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[k]);
						if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
						{
							perror("failed write int array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					if (move_in_file_bytes(fd, pd_he * sizeof(long)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);

						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for (j = 0; k < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
							if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
							{
								perror("failed write int array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						if (move_in_file_bytes(fd, pd_he * sizeof(long)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}
			break;
		}
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = 0;
				
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				int k;
				for (k = 0; k < rec->fields[i].data.v.size; k++)
				{
					ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[k]);
					if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;

				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/ ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					if(pd_ne != 0)
						padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, sizeof(float),0)) == -1)
						{
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
							sz--;
						}

						if (exit)
						{
							/*write the epty update offset*/
							ui64 empty_offset = swap64(0);
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);

									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.v.size)) exit = 1;
							}
						}

						if (exit)
						{

							if (padding_value > 0)
							{
								if (move_in_file_bytes(fd, padding_value * sizeof(float)) == -1)
								{
									__er_file_pointer(F, L - 1);
									return 0;
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)

							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						if (move_in_file_bytes(fd, padding_value * sizeof(float)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);

					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = 0;
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}
						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int k;
					for (k = step; k < rec->fields[i].data.v.size; k++)
					{

						ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[k]);
						if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
						{
							perror("failed write int array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					if (move_in_file_bytes(fd, pd_he * sizeof(float)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for (j = 0; j < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							ui32 num_ne = swap32(rec->fields[i].data.v.elements.f[step]);
							if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
							{
								perror("failed write int array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						if (move_in_file_bytes(fd, pd_he * sizeof(float)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}

			break;
		}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = 0;
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
				int j;
				for (j = 0; j < rec->fields[i].data.v.size; j++)
				{
					if (!rec->fields[i].data.v.elements.s[j])
						continue;

					lt = strlen(rec->fields[i].data.v.elements.s[j]);
					buff_update = lt * 2;

					/* adding 1 for '\0'*/
					lt++, buff_update++;
					char buff_w[buff_update];
					memset(buff_w,0,buff_update);

					strncpy(buff_w, rec->fields[i].data.v.elements.s[j], lt - 1);

					ui16 bu_ne = swap16((ui16)buff_update);
					ui32 str_loc_ne = swap32((ui32)str_loc);

					if (os_write(fd, &str_loc_ne, sizeof(str_loc_ne)) < 0 ||
						os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
						os_write(fd, buff_w, buff_update) < 0)
					{
						perror("write file failed: ");
						printf(" %s:%d", F, L - 2);
						return 0;
					}

				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;

					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					if(pd_ne != 0)
						padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, 0,rec->fields[i].type)) == -1){
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								/*string update process*/
								/*save the starting offset for the string record*/
								if ((bg_pos = get_file_offset(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*read pos of new str if any*/
								ui32 str_loc_ne = 0;
								if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == STATUS_ERROR)
								{
									perror("can't read string location: ");
									printf(" %s:%d", F, L - 3);
									return 0;
								}
								
								if(str_loc_ne != 0)
									str_loc = (file_offset)swap32(str_loc_ne);

								/*store record  beginning pos*/
								if ((af_str_loc_pos = get_file_offset(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								ui16 bu_ne = 0;
								if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
								{
									perror("can't read safety buffer before writing string.\n");
									printf("%s:%d", F, L - 3);
									return 0;
								}

								buff_update = (file_offset)swap16(bu_ne);

								/*save the end offset of the first string record */
								if ((go_back_to = get_file_offset(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*add the buffer size to this file file_offset*/
								/*so we can reposition right after the 1st string after the writing */
								go_back_to += buff_update;
								if (str_loc > 0)
								{
									/*set the file pointer to str_loc*/
									if (find_record_position(fd, str_loc) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
									/*
									 * in the case of a regular buffer update we have
									 *  to save the file_offset to get back to it later
									 * */
									if ((move_to = get_file_offset(fd)) == -1)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}

									ui16 bu_ne = 0;
									if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
									{
										perror("read file.\n");
										printf("%s:%d", F, L - 3);
										return 0;
									}

									buff_update = (file_offset)swap16(bu_ne);
								}

								new_lt = strlen(rec->fields[i].data.v.elements.s[step]) + 1; /*get new str length*/

								if (new_lt > buff_update)
								{
									/*
									 * if the new length is bigger then the buffer,
									 * set the file pointer to the end of the file
									 * to write the new data
									 * */
									if ((eof = go_to_EOF(fd)) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}

									/*expand the buff_update only for the bytes needed*/
									__n_buff_update = buff_update;
									__n_buff_update += (new_lt - buff_update);
									buff_update = __n_buff_update;

								}
								char buff_w[buff_update];
								memset(buff_w,0,buff_update);

								strncpy(buff_w, rec->fields[i].data.v.elements.s[step], new_lt - 1);
								/*
								 * if we did not move to another position
								 * set the file pointer back to the begginning of the string record
								 * to overwrite the data accordingly
								 * */
								if (str_loc == 0 && (__n_buff_update == 0))
								{
									if (find_record_position(fd, af_str_loc_pos) == -1)
									{
										__er_file_pointer(F, L - 3);
										return 0;
									}
								}
								else if (str_loc > 0 && (__n_buff_update == 0))
								{
									if (find_record_position(fd, move_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 3);
										return 0;
									}
								}

								/*
								 * write the data to file --
								 * the file pointer is always pointing to the
								 * right position at this point */
								bu_ne = swap16((ui16)buff_update);

								if (os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
									os_write(fd, buff_w, buff_update) < 0)
								{
									perror("error in writing type string (char *)file.\n");
									return 0;
								}


								/*
								 * if eof is bigger than 0 means we updated the string
								 * we need to save the file_offset of the new written data
								 * at the start of the original.
								 * */
								if (eof > 0)
								{
									/*go at the beginning of the str record*/
									if (find_record_position(fd, bg_pos) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}

									/*update new string position*/
									ui32 eof_ne = swap32((ui32)eof);
									if (os_write(fd, &eof_ne, sizeof(eof_ne)) == STATUS_ERROR)
									{
										perror("write file: ");
										printf(" %s:%d", F, L - 3);
										return 0;
									}

									/*set file pointer to the end of the 1st string rec*/
									/*this step is crucial to avoid losing data        */

									if (find_record_position(fd, go_back_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
								}
								else if (str_loc > 0)
								{
									/*
									 * Make sure that in all cases
									 * we go back to the end of the 1st record
									 * */
									if (find_record_position(fd, go_back_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
								}
								__n_buff_update = 0;
								eof = 0;
								str_loc = 0;
								step++;
							}
							sz--;
						}

						if (exit){
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);

									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								if (!rec->fields[i].data.v.elements.s[step])
									continue;

								/*string update process*/
								/*save the starting offset for the string record*/
								if ((bg_pos = get_file_offset(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*read pos of new str if any*/
								ui32 str_loc_ne = 0;
								if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == STATUS_ERROR)
								{
									perror("can't read string location: ");
									printf(" %s:%d", F, L - 3);
									return 0;
								}
								str_loc = (file_offset)swap32(str_loc_ne);

								/*store record  beginning pos*/
								if ((af_str_loc_pos = get_file_offset(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								ui16 bu_ne = 0;
								if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
								{
									perror("can't read safety buffer before writing string.\n");
									printf("%s:%d", F, L - 3);
									return 0;
								}

								buff_update = (file_offset)swap16(bu_ne);

								/*save the end offset of the first string record */
								if ((go_back_to = get_file_offset(fd)) == STATUS_ERROR){
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*add the buffer size to this file file_offset*/
								/*so we can reposition right after the 1st string after the writing */
								go_back_to += buff_update;
								if (str_loc > 0)
								{
									/*set the file pointer to str_loc*/
									if (find_record_position(fd, str_loc) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
									/*
									 * in the case of a regular buffer update we have
									 *  to save the file_offset to get back to it later
									 * */
									if ((move_to = get_file_offset(fd)) == -1)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}

									ui16 bu_ne = 0;
									if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
									{
										perror("read file.\n");
										printf("%s:%d", F, L - 3);
										return 0;
									}

									buff_update = (file_offset)swap16(bu_ne);
								}

								new_lt = strlen(rec->fields[i].data.v.elements.s[step]) + 1; /*get new str length*/

								if (new_lt > buff_update){
									/*
									 * if the new length is bigger then the buffer,
									 * set the file pointer to the end of the file
									 * to write the new data
									 * */
									if ((eof = go_to_EOF(fd)) == STATUS_ERROR){
										__er_file_pointer(F, L - 2);
										return 0;
									}

									/*expand the buff_update only for the bytes needed*/
									__n_buff_update = buff_update;
									__n_buff_update += (new_lt - buff_update);
									buff_update = __n_buff_update;
								}
								char buff_w[buff_update];
								memset(buff_w,0,buff_update);

								strncpy(buff_w, rec->fields[i].data.v.elements.s[step], new_lt - 1);
								/*
								 * if we did not move to another position
								 * set the file pointer back to the begginning of the string record
								 * to overwrite the data accordingly
								 * */
								if (str_loc == 0 && (__n_buff_update == 0))
								{
									if (find_record_position(fd, af_str_loc_pos) == -1)
									{
										__er_file_pointer(F, L - 3);
										return 0;
									}
								}
								else if (str_loc > 0 && (__n_buff_update == 0))
								{
									if (find_record_position(fd, move_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 3);
										return 0;
									}
								}

								/*
								 * write the data to file --
								 * the file pointer is always pointing to the
								 * right position at this point */
								bu_ne = swap16((ui16) buff_update);

								
								if (os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
									os_write(fd, buff_w, buff_update))
								{
									perror("error in writing type string (char *)file.\n");
									return 0;
								}


								/*
								 * if eof is bigger than 0 means we updated the string
								 * we need to save the file_offset of the new written data
								 * at the start of the original.
								 * */
								if (eof > 0)
								{
									/*go at the beginning of the str record*/
									if (find_record_position(fd, bg_pos) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}

									/*update new string position*/
									ui32 eof_ne = swap32((ui32)eof);
									if (os_write(fd, &eof_ne, sizeof(eof_ne)) == STATUS_ERROR)
									{
										perror("write file: ");
										printf(" %s:%d", F, L - 3);
										return 0;
									}

									/*set file pointer to the end of the 1st string rec*/
									/*this step is crucial to avoid losing data        */

									if (find_record_position(fd, go_back_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
								}
								else if (str_loc > 0)
								{
									/*
									 * Make sure that in all cases
									 * we go back to the end of the 1st record
									 * */
									if (find_record_position(fd, go_back_to) == STATUS_ERROR)
									{
										__er_file_pointer(F, L - 2);
										return 0;
									}
								}
							}
							__n_buff_update = 0;
							eof = 0;
							str_loc = 0;
							step++;
							if(!(step < rec->fields[i].data.v.size)) exit = 1;
						}

						if (exit){
							if (padding_value > 0) {
								int i;
								for(i = 0; i < padding_value; i++){
									if(get_string_size(fd,NULL) == (size_t) -1){
										__er_file_pointer(F, L - 1);
										return 0;
									}
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0){
						int i;
						for(i = 0; i < padding_value; i++){
							if(get_string_size(fd,NULL) == (size_t) -1){
								__er_file_pointer(F, L - 1);
								return 0;
							}
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to_string_array = get_file_offset(fd);

					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{

						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0){
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = swap32(0);
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++){
							if (step < rec->fields[i].data.v.size){
								if (!rec->fields[i].data.v.elements.s[step])
									continue;

								/*new string writing process*/

								str_loc = 0;
								lt = strlen(rec->fields[i].data.v.elements.s[step]);
								buff_update = lt * 2;

								/* adding 1 for '\0'*/
								lt++, buff_update++;
								char buff_w[buff_update];
								memset(buff_w,0,buff_update);

								strncpy(buff_w, rec->fields[i].data.v.elements.s[step], lt - 1);

								ui16 bu_ne = swap16((ui16)buff_update);
								ui32 str_loc_ne = swap32((ui32)str_loc);

								if (os_write(fd, &str_loc_ne, sizeof(str_loc_ne)) < 0 ||
									os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
									os_write(fd, buff_w, buff_update) < 0)
								{
									perror("write file failed: ");
									printf(" %s:%d", F, L - 2);
									return 0;
								}

								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to_string_array) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1){
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int k;
					for (k = step; k < rec->fields[i].data.v.size; k++)
					{
						if (!rec->fields[i].data.v.elements.s[k])
							continue;

						/*string update process*/
						if ((bg_pos = get_file_offset(fd)) == STATUS_ERROR)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						/*read pos of new str if any*/
						ui32 str_loc_ne = 0;
						if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == STATUS_ERROR)
						{
							perror("can't read string location: ");
							printf(" %s:%d", F, L - 3);
							return 0;
						}
						str_loc = (file_offset)swap32(str_loc_ne);

						/*store record  beginning pos*/
						if ((af_str_loc_pos = get_file_offset(fd)) == STATUS_ERROR)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						ui16 bu_ne = 0;
						if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
						{
							perror("can't read safety buffer before writing string.\n");
							printf("%s:%d", F, L - 3);
							return 0;
						}

						buff_update = (file_offset)swap16(bu_ne);

						/*save the end offset of the first string record */
						if ((go_back_to = get_file_offset(fd)) == STATUS_ERROR)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						/*add the buffer size to this file file_offset*/
						/*so we can reposition right after the 1st string after the writing */
						go_back_to += buff_update;
						if (str_loc > 0)
						{
							/*set the file pointer to str_loc*/
							if (find_record_position(fd, str_loc) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}
							/*
							 * in the case of a regular buffer update we have
							 *  to save the file_offset to get back to it later
							 * */
							if ((move_to = get_file_offset(fd)) == -1)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}

							ui16 bu_ne = 0;
							if(os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)	
							{
								perror("read file.\n");
								printf("%s:%d", F, L - 3);
								return 0;
							}

							buff_update = (file_offset)swap16(bu_ne);
						}

						new_lt = strlen(rec->fields[i].data.v.elements.s[k]) + 1; /*get new str length*/

						if (new_lt > buff_update)
						{
							/*
							 * if the new length is bigger then the buffer,
							 * set the file pointer to the end of the file
							 * to write the new data
							 * */
							if ((eof = go_to_EOF(fd)) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}

							/*expand the buff_update only for the bytes needed*/
							__n_buff_update = buff_update;
							__n_buff_update += (new_lt - buff_update);
							buff_update = __n_buff_update;
						}
						char buff_w[buff_update];
						memset(buff_w,0,buff_update);

						strncpy(buff_w, rec->fields[i].data.v.elements.s[k], new_lt - 1);
						/*
						 * if we did not move to another position
						 * set the file pointer back to the begginning of the string record
						 * to overwrite the data accordingly
						 * */
						if (str_loc == 0 && (__n_buff_update == 0))
						{
							if (find_record_position(fd, af_str_loc_pos) == -1)
							{
								__er_file_pointer(F, L - 3);
								return 0;
							}
						}
						else if (str_loc > 0 && (__n_buff_update == 0))
						{
							if (find_record_position(fd, move_to) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 3);
								return 0;
							}
						}

						/*
						 * write the data to file --
						 * the file pointer is always pointing to the
						 * right position at this point */
						bu_ne = swap16((ui16)buff_update);
						if(os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
							os_write(fd, buff_w, buff_update) < 0)
						{
							perror("error in writing type string (char *)file.\n");
							return 0;
						}

						/*
						 * if eof is bigger than 0 means we updated the string
						 * we need to save the file_offset of the new written data
						 * at the start of the original.
						 * */
						if (eof > 0)
						{
							/*go at the beginning of the str record*/
							if (find_record_position(fd, bg_pos) == STATUS_ERROR){
								__er_file_pointer(F, L - 2);
								return 0;
							}

							/*update new string position*/
							ui32 eof_ne = swap32((ui32)eof);
							if (os_write(fd, &eof_ne, sizeof(eof_ne)) == STATUS_ERROR)
							{
								perror("write file: ");
								printf(" %s:%d", F, L - 3);
								return 0;
							}

							/*set file pointer to the end of the 1st string rec*/
							/*this step is crucial to avoid losing data        */

							if (find_record_position(fd, go_back_to) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}
						}
						else if (str_loc > 0)
						{
							/*
							 * Make sure that in all cases
							 * we go back to the end of the 1st record
							 * */
							if (find_record_position(fd, go_back_to) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					int i;
					for(i = 0; i < pd_he; i++){
						if(get_string_size(fd,NULL) == (size_t) -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for ( j = 0; k < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							if (!rec->fields[i].data.v.elements.s[step])
								continue;

							/*string update process*/
							if ((bg_pos = get_file_offset(fd)) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}

							/*read pos of new str if any*/
							ui32 str_loc_ne = 0;
							if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == STATUS_ERROR)
							{
								perror("can't read string location: ");
								printf(" %s:%d", F, L - 3);
								return 0;
							}
							str_loc = (file_offset)swap32(str_loc_ne);

							/*store record  beginning pos*/
							if ((af_str_loc_pos = get_file_offset(fd)) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}

							ui16 bu_ne = 0;
							if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
							{
								perror("can't read safety buffer before writing string.\n");
								printf("%s:%d", F, L - 3);
								return 0;
							}

							buff_update = (file_offset)swap16(bu_ne);

							/*save the end offset of the first string record */
							if ((go_back_to = get_file_offset(fd)) == STATUS_ERROR)
							{
								__er_file_pointer(F, L - 2);
								return 0;
							}

							/*add the buffer size to this file file_offset*/
							/*so we can reposition right after the 1st string after the writing */
							go_back_to += buff_update;
							if (str_loc > 0)
							{
								/*set the file pointer to str_loc*/
								if (find_record_position(fd, str_loc) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}
								/*
								 * in the case of a regular buffer update we have
								 *  to save the file_offset to get back to it later
								 * */
								if ((move_to = get_file_offset(fd)) == -1)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								ui16 bu_ne = 0;
								if (os_read(fd, &bu_ne, sizeof(bu_ne)) < 0)
								{
									perror("read file.\n");
									printf("%s:%d", F, L - 3);
									return 0;
								}

								buff_update = (file_offset)swap16(bu_ne);
							}

							new_lt = strlen(rec->fields[i].data.v.elements.s[step]) + 1; /*get new str length*/

							if (new_lt > buff_update)
							{
								/*
								 * if the new length is bigger then the buffer,
								 * set the file pointer to the end of the file
								 * to write the new data
								 * */
								if ((eof = go_to_EOF(fd)) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*expand the buff_update only for the bytes needed*/
								__n_buff_update = buff_update;
								__n_buff_update += (new_lt - buff_update);
								buff_update = __n_buff_update;							
							}
							
							char buff_w[buff_update];
							memset(buff_w,0,buff_update);

							strncpy(buff_w, rec->fields[i].data.v.elements.s[step], new_lt - 1);
							/*
							 * if we did not move to another position
							 * set the file pointer back to the begginning of the string record
							 * to overwrite the data accordingly
							 * */
							if (str_loc == 0 && (__n_buff_update == 0))
							{
								if (find_record_position(fd, af_str_loc_pos) == -1)
								{
									__er_file_pointer(F, L - 3);
									return 0;
								}
							}
							else if (str_loc > 0 && (__n_buff_update == 0))
							{
								if (find_record_position(fd, move_to) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 3);
									return 0;
								}
							}

							/*
							 * write the data to file --
							 * the file pointer is always pointing to the
							 * right position at this point */
							bu_ne = swap16((ui16)buff_update);

							if(os_write(fd, &bu_ne, sizeof(bu_ne)) < 0 ||
								os_write(fd, buff_w, buff_update) < 0)
							{
								perror("error in writing type string (char *)file.\n");
								return 0;
							}


							/*
							 * if eof is bigger than 0 means we updated the string
							 * we need to save the file_offset of the new written data
							 * at the start of the original.
							 * */
							if (eof > 0)
							{
								/*go at the beginning of the str record*/
								if (find_record_position(fd, bg_pos) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}

								/*update new string position*/
								ui32 eof_ne = swap32((ui32)eof);
								if (os_write(fd, &eof_ne, sizeof(eof_ne)) == STATUS_ERROR)
								{
									perror("write file: ");
									printf(" %s:%d", F, L - 3);
									return 0;
								}

								/*set file pointer to the end of the 1st string rec*/
								/*this step is crucial to avoid losing data        */

								if (find_record_position(fd, go_back_to) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}
							}
							else if (str_loc > 0)
							{
								/*
								 * Make sure that in all cases
								 * we go back to the end of the 1st record
								 * */
								if (find_record_position(fd, go_back_to) == STATUS_ERROR)
								{
									__er_file_pointer(F, L - 2);
									return 0;
								}
							}

							/*zeroing the variables for th new cycle*/
							__n_buff_update = 0;
							eof = 0;
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						int i;
						for(i = 0; i < pd_he; i++){
							if(get_string_size(fd,NULL) == (size_t) -1){
								__er_file_pointer(F, L - 2);
								return 0;
							}
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}

			break;
		}
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1){
					perror("error in writing size array to file.\n");
					return 0;
				}
				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = 0;
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				int k;
				for ( k = 0; k < rec->fields[i].data.v.size; k++)
				{
					ui8 num_ne = rec->fields[i].data.v.elements.b[k];
					if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;

				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1){
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					if(pd_ne != 0)
						padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, sizeof(unsigned char),0)) == -1)
						{
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui8 num_ne = rec->fields[i].data.v.elements.b[step];
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
							sz--;
						}

						if (exit)
						{
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								if (!rec->fields[i].data.v.elements.b[step])
									continue;

								ui8 num_ne = rec->fields[i].data.v.elements.b[step];
								
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.v.size)) exit = 1;
							}
						}

						if (exit)
						{
							if (padding_value > 0)
							{
								if (move_in_file_bytes(fd, padding_value * sizeof(unsigned char)) == -1)
								{
									__er_file_pointer(F, L - 1);
									return 0;
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						if (move_in_file_bytes(fd, padding_value * sizeof(unsigned char)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);

					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = swap32(0);
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui8 num_ne = rec->fields[i].data.v.elements.b[step];
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int j;
					for (j = step; k < rec->fields[i].data.v.size; j++)
					{
						ui8 num_ne = rec->fields[i].data.v.elements.b[j];
						if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
						{
							perror("failed write int array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					if (move_in_file_bytes(fd, pd_he * sizeof(unsigned char)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for (j = 0; k < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							ui8 num_ne = rec->fields[i].data.v.elements.b[step];
							
							if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
							{
								perror("failed write int array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						if (move_in_file_bytes(fd, pd_he * sizeof(unsigned char)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}
			break;
		}
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
		{
			if (!update)
			{
				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1){
					perror("error in writing size array to file.\n");
					return 0;
				}
				/*write the size of the array */
				ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = 0;
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				int k;
				for (k = 0; k < rec->fields[i].data.v.size; k++)
				{
					ui64 num_ne = htond(rec->fields[i].data.v.elements.d[k]);
					if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				int step = 0;
				int sz = 0;
				int k = 0;
				int padding_value = 0;

				ui8 set = (ui8) rec->fields[i].data.v.is_set;
				if (os_write(fd, &set, sizeof(set)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}


				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1)
					{
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = (int)swap32(sz_ne);
					if (rec->fields[i].data.v.size < sz ||
						rec->fields[i].data.v.size == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1)
					{
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}
					
					if(pd_ne != 0)
						padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						if ((array_last = is_array_last_block(fd,NULL, sz, sizeof(double),0)) == -1)
						{
							fprintf(stderr, "can't verify array last block %s:%d.\n", F, L - 1);
							return 0;
						}

						if (rec->fields[i].data.v.size < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.v.size - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.v.size == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
							sz--;
						}

						if (exit)
						{
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.v.size)
								{
									int pad = sz - (rec->fields[i].data.v.size - step);
									padding_value += pad;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = htond(rec->fields[i].data.v.elements.d[k]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.v.size)) exit = 1;
							}
						}

						if (exit)
						{
							if (padding_value > 0)
							{
								if (move_in_file_bytes(fd, padding_value * sizeof(double)) == -1)
								{
									__er_file_pointer(F, L - 1);
									return 0;
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, (void*)&empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						if (move_in_file_bytes(fd, padding_value * sizeof(double)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);

					if (os_read(fd, (void*)&update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.v.size - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1)
						{
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = swap32(0);
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++)
						{
							if (step < rec->fields[i].data.v.size)
							{
								ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
								if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
								{
									perror("failed write int array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.v.size < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32((ui32)rec->fields[i].data.v.size);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)
					{
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.v.size);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}


					int j;
					for (j = step; j < rec->fields[i].data.v.size; j++)
					{
						ui64 num_ne = htond(rec->fields[i].data.v.elements.d[j]);
						if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
						{
							perror("failed write int array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					if (move_in_file_bytes(fd, pd_he * sizeof(double)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					ui64 update_arr_ne = swap64(0);
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.v.size == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.v.size - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					int j;
					for ( j = 0; j < rec->fields[i].data.v.size; j++)
					{
						if (step < rec->fields[i].data.v.size)
						{
							ui64 num_ne = htond(rec->fields[i].data.v.elements.d[j]);
							if (os_write(fd, &num_ne, sizeof(num_ne)) == -1)
							{
								perror("failed write int array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						if (move_in_file_bytes(fd, pd_he * sizeof(double)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}
			break;
		}
		case TYPE_FILE:
		{
			/*TODO: do we need this TYPE_FILE ????*/
#if 0
			if (!update){	

				/*write the size of the LIST */
				ui32 size_ne = swap32(rec->fields[i].data.file.count);
				if (os_write(fd, &size_ne, sizeof(size_ne)) == -1)	{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 padding_ne = 0;
				if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}

				ui32 i;
				for(i = 0; i< rec->fields[i].data.file.count; i++){
					if(write_file(fd,&rec->fields[i].data.file.recs[i],0,0) == 0){
						perror("failed write record array to file");
						return 0;
					}
				}
				

				ui64 upd_ne = 0;
				if (os_write(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("error in writing size array to file.\n");
					return 0;
				}
			}
			else
			{
				size_t len = strlen(rec->fields[i].field_name);
				char *sfx = ".sch";
				int sfxl = (int)strlen(sfx);
				int totl = sfxl + (int)len + 1;
				char sch_file[totl];
				memset(sch_file,0,totl);
				strncpy(sch_file,rec->fields[i].field_name,len);
				strncat(sch_file,sfx,sfxl);
				/* open the file */
				int fd_schema = open_file(sch_file,0);
				if(file_error_handler(1,fd_schema) != 0) return 0;			

				struct Schema sch;
				memset(&sch,0,sizeof(struct Schema));
				struct Header_d hd = {0,0,&sch};	

				if (!read_header(fd_schema, &hd)) {
					close(fd_schema);
					free_record(rec, rec->fields_num);
					return -1;
				}

				close(fd_schema);
				/* update branch*/
				file_offset update_pos = 0;
				file_offset go_back_to_first_rec = 0;
				ui32 step = 0;
				ui32 sz = 0;
				ui32 k = 0;
				int padding_value = 0;
				do
				{
					/* check the size */
					ui32 sz_ne = 0;
					if (os_read(fd, &sz_ne, sizeof(sz_ne)) == -1){
						fprintf(stderr, "can't read int array size.\n");
						return 0;
					}

					sz = swap32(sz_ne);
					if (rec->fields[i].data.file.count < sz || rec->fields[i].data.file.count == sz)
						break;

					/*read the padding data*/
					ui32 pd_ne = 0;
					if (os_read(fd, &pd_ne, sizeof(pd_ne)) == -1){
						fprintf(stderr, "can't read padding array.\n");
						return 0;
					}

					padding_value = (int)swap32(pd_ne);

					if (step >= sz)
					{
						int array_last = 0;
						int exit = 0;
						
						/*check if the array of type file is in the last block*/
						file_offset reset_pointer_here = 0;
						if((reset_pointer_here = get_file_offset(fd)) == -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}

						ui32 y;
						for(y = 0; y < sz;y++){
							struct Record_f dummy;
							memset(&dummy,0,sizeof(struct Record_f));
							if(read_file(fd, rec->fields[i].field_name, &dummy, *hd.sch_d) == -1){
								fprintf(stderr,"cannot read type file %s:%d.\n",F,L-1);
								return -1;
							}

							free_record(&dummy,dummy.fields_num);

							if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
								__er_file_pointer(F, L - 1);
								return 0;
							}
						}

						ui64 update_arr = 0;
						if (os_read(fd, &update_arr, sizeof(update_arr)) == -1){
							perror("failed read update file_offset int array.\n");
							return 0;
						}

						if(find_record_position(fd,reset_pointer_here) == -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}

						file_offset up_he = (file_offset) swap64(update_arr);
						if(up_he == 0) array_last = 1;

						if (rec->fields[i].data.file.count < (sz + step) && array_last)
						{
							int pad_value = sz - (rec->fields[i].data.file.count - step);
							padding_value += pad_value;

							sz = rec->fields[i].data.v.size - step;
							exit = 1;

							if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
							{
								__er_file_pointer(F, L - 1);
								return 0;
							}

							/* write the updated size of the array */
							ui32 new_sz = swap32((ui32)sz);
							if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
							{
								perror("error in writing remaining size int array.\n");
								return 0;
							}

							/* write the updated padding value */
							ui32 new_pd = swap32((ui32)padding_value);
							if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
							{
								perror("error in writing new pading value int array.\n");
								return 0;
							}
						}
						else if (rec->fields[i].data.file.count == (sz + step) && array_last)
						{
							exit = 1;
						}

						while (sz)
						{
							if (step < rec->fields[i].data.file.count){

								if(write_file(fd,&rec->fields[i].data.file.recs[step],0,0) == 0){
									perror("failed write record array to file");
									return 0;
								}
									
								step++;
							}
							sz--;
						}

						if (exit){
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}
					else
					{

						int exit = 0;
						for (k = 0; k < sz; k++)
						{
							if (step > 0 && k == 0)
							{
								if ((step + sz) > rec->fields[i].data.file.count)
								{
									int pad = sz - (rec->fields[i].data.file.count - step);
									padding_value += pad;

									sz = rec->fields[i].data.file.count - step;
									exit = 1;

									if (move_in_file_bytes(fd, 2 * (-sizeof(ui32))) == -1)
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									if (os_write(fd, &new_sz, sizeof(new_sz)) == -1)
									{
										perror("error in writing remaining size int array.\n");
										return 0;
									}

									/*write padding */
									ui32 new_pd = swap32((ui32)padding_value);
									if (os_write(fd, &new_pd, sizeof(new_pd)) == -1)
									{
										perror("error in writing new padd int array.\n");
										return 0;
									}
								}
							}

							if (step < rec->fields[i].data.file.count){
								if(write_file(fd,&rec->fields[i].data.file.recs[step],0,0) == 0){
									perror("failed write record array to file");
									return 0;
								}
								step++;
								if(!(step < rec->fields[i].data.file.count)) exit = 1;
							}
						}
						
						if (exit)
						{

							if (padding_value > 0)
							{
								int y;
								for(y = 0; y < padding_value;y++){
									struct Record_f dummy;
									memset(&dummy,0,sizeof(struct Record_f));
									if(read_file(fd, rec->fields[i].field_name,
												&dummy, *hd.sch_d) == -1){
										fprintf(stderr,"cannot read type file %s:%d.\n"
												,F,L-1);
										return -1;
									}

									free_record(&dummy,dummy.fields_num);

									if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
										__er_file_pointer(F, L - 1);
										return 0;
									}
								}
							}
							/*write the epty update offset*/
							ui64 empty_offset = 0;
							if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
							{
								perror("error in writing size array to file.\n");
								return 0;
							}
							break;
						}
					}

					if (padding_value > 0)
					{
						int y;
						for(y = 0; y < padding_value;y++){
							struct Record_f dummy ={0};
							if(read_file(fd, rec->fields[i].field_name,
										&dummy, *hd.sch_d) == -1){
								fprintf(stderr,"cannot read type file %s:%d.\n"
										,F,L-1);
								return -1;
							}

							free_record(&dummy,dummy.fields_num);

							if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
								__er_file_pointer(F, L - 1);
								return 0;
							}
						}

					}

					ui64 update_off_ne = 0;
					file_offset go_back_to = get_file_offset(fd);

					if (os_read(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
					{
						perror("failed read update file_offset int array.\n");
						return 0;
					}

					if (go_back_to_first_rec == 0)
						go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

					update_pos = (file_offset)swap64(update_off_ne);
					if (update_pos == 0)
					{
						/*go to EOF*/
						if ((update_pos = go_to_EOF(fd)) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}
						/* write the size of the array */
						int size_left = rec->fields[i].data.file.count - step;
						ui32 size_left_ne = swap32((ui32)size_left);
						if (os_write(fd, &size_left_ne, sizeof(size_left_ne)) == -1) {
							perror("error in writing remaining size int array.\n");
							return 0;
						}

						ui32 padding_ne = 0;
						if (os_write(fd, &padding_ne, sizeof(padding_ne)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						int j;
						for (j = 0; j < size_left; j++){
							if (step < rec->fields[i].data.file.count){
								if(write_file(fd,&rec->fields[i].data.file.recs[step],0,0) == 0){
									perror("failed write record array to file");
									return 0;
								}
								step++;
							}
						}

						/*write the empty update offset*/
						ui64 empty_offset = 0;
						if (os_write(fd, &empty_offset, sizeof(empty_offset)) == -1)
						{
							perror("error in writing size array to file.\n");
							return 0;
						}

						if (find_record_position(fd, go_back_to) == -1)
						{
							__er_file_pointer(F, L - 1);
							return 0;
						}

						update_off_ne = (ui64)swap64((ui64)update_pos);
						if (os_write(fd, &update_off_ne, sizeof(update_off_ne)) == -1)
						{
							fprintf(stderr, "can't write update position int array, %s:%d.\n",
									F, L - 1);
							return 0;
						}

						break;
					}

					if (find_record_position(fd, update_pos) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

				} while (update_pos > 0);

				if (rec->fields[i].data.file.count < sz)
				{

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/*write the size of the array */
					ui32 size_ne = swap32(rec->fields[i].data.file.count);
					if (os_write(fd, &size_ne, sizeof(size_ne)) == -1){
						perror("error in writing size array to file.\n");
						return 0;
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1) {
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					pd_he += (sz - rec->fields[i].data.file.count);

					if (move_in_file_bytes(fd, -sizeof(ui32)) == -1)
					{
						__er_file_pointer(F, L - 1);
						return 0;
					}

					/* write the padding to apply after the  array */
					pad_ne = swap32((ui32)pd_he);
					if (os_write(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}


					ui32 j;
					for (j = step; j < rec->fields[i].data.file.count; j++){

						if(write_file(fd,&rec->fields[i].data.file.recs[j],0,0) == 0){
							perror("failed write record array to file");
							return 0;
						}
					}

					/*
					 * move the file pointer after the array
					 * as much as the pad
					 * */
					int y;
					for(y = 0; y < pd_he;y++){
						struct Record_f dummy;
						memset(&dummy,0,sizeof(struct Record_f));
						if(read_file(fd, rec->fields[i].field_name,
									&dummy, *hd.sch_d) == -1){
							fprintf(stderr,"cannot read type file %s:%d.\n"
									,F,L-1);
							return -1;
						}

						free_record(&dummy,dummy.fields_num);

						if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
							__er_file_pointer(F, L - 1);
							return 0;
						}
					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}
				else if (rec->fields[i].data.file.count == sz)
				{
					/*
					 * the sizes are the same
					 * we simply write the array.
					 * */
					if (step > 0)
					{
						if (move_in_file_bytes(fd, -sizeof(sz)) == -1)
						{
							__er_file_pointer(F, L - 2);
							return 0;
						}

						int size_left = rec->fields[i].data.file.count - step;
						ui32 sz_ne = swap32((ui32)size_left);
						if (os_write(fd, &sz_ne, sizeof(sz_ne)) == -1)
						{
							fprintf(stderr, "write failed %s:%d.\n", F, L - 1);
							return 0;
						}
					}

					/*read and check the padding, */
					ui32 pad_ne = 0;
					if (os_read(fd, &pad_ne, sizeof(pad_ne)) == -1)
					{
						perror("error in writing padding array to file.\n");
						return 0;
					}

					int pd_he = (int)swap32(pad_ne);
					ui32 j;
					for (j = 0; j < rec->fields[i].data.file.count; j++)
					{
						if (step < rec->fields[i].data.file.count){

							if(write_file(fd,&rec->fields[i].data.file.recs[j],0,0) == 0){
								perror("failed write record array to file");
								return 0;
							}
							step++;
						}
					}

					/*
					 * move the file pointer
					 * as much as the padding value
					 * if it si bigger than 0
					 * */
					if (pd_he > 0)
					{
						int y;
						for(y = 0; y < padding_value;y++){
							struct Record_f dummy ={0};
							if(read_file(fd, rec->fields[i].field_name,
										&dummy, *hd.sch_d) == -1){
								fprintf(stderr,"cannot read type file %s:%d.\n"
										,F,L-1);
								return -1;
							}

							free_record(&dummy,dummy.fields_num);

							if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
								__er_file_pointer(F, L - 1);
								return 0;
							}
						}

					}

					ui64 update_arr_ne = 0;
					if (os_write(fd, &update_arr_ne, sizeof(update_arr_ne)) == -1)
					{
						perror("failed write int array to file");
						return 0;
					}
				}

				if (go_back_to_first_rec > 0)
				{
					if (find_record_position(fd, go_back_to_first_rec) == -1)
					{
						__er_file_pointer(F, L - 2);
						return 0;
					}
				}
			}

			break;
#endif
		}
	
		default:
			break;
		}
	}

	ui64 uot_ne = 0;
	if(update_file_offset > 0)
		uot_ne = swap64((ui64)update_file_offset);

	if (os_write(fd, &uot_ne, sizeof(uot_ne)) == STATUS_ERROR)
	{
		perror("writing file_offset for future update");
		printf(" %s:%d", F, L - 3);
		return 0;
	}

	return 1; /*write to file succssed!*/
}


#if defined(__linux__) || defined(__APPLE__)
file_offset get_update_offset(int fd)
#elif defined(_WIN32)
file_offset get_update_offset(HANDLE file_handle)
#endif
{
	ui64 urec_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
	if (os_read(fd, &urec_ne, sizeof(urec_ne)) == STATUS_ERROR)
#elif defined(_WIN32)
	DWORD written = 0;
	if (!ReadFile(file_handle,&urec_ne,sizeof(urec_ne),&written,NULL))
#endif
	{
		perror("could not read the update record position (file.c l 424).\n");
		return -1;
	}

	return (file_offset)swap64(urec_ne);
}

/*
 * read_file:
 *  reads a record from a file,
 *  the caller must inistialized the struct Record_f
 * */
#if defined(__linux__) || defined(__APPLE__)
int read_file(int fd, char *file_name, struct Record_f *rec, struct Schema sch)
#else
int read_file(HANDLE fd, char *file_name, struct Record_f *rec, struct Schema sch)
#endif
{
	create_record(file_name, sch,rec);
	/*read the count of the field written*/
	ui8 cvf_ne = 0;

#if defined(__linux__) || defined(__APPLE__)
	if (os_read(fd, &cvf_ne, sizeof(cvf_ne)) < 0) 
#elif defined(_WIN32)
	DWORD written = 0;
	if (!ReadFile(fd,&cvf_ne,sizeof(cvf_ne),&written,NULL))
#endif
	{

		perror("could not read fields number:");
		printf(" %s:%d.\n", __FILE__, __LINE__ - 1);
		return -1;
	}

	ui8 i_ne = 0;
	ui8 i;
	for( i = 0; i < cvf_ne; i++){
		/*position in the field_set array*/
#if defined(__linux__) || defined(__APPLE__)
		if (os_read(fd, &i_ne, sizeof(i_ne)) < 0) 
#elif defined(_WIN32)
		DWORD written = 0;
		if (!ReadFile(fd,&i_ne,sizeof(i_ne),&written,NULL))
#endif
	{
			perror("could not write fields number");
			return 0;
		}
		rec->field_set[i_ne] = 1;
	}

	

	file_offset str_loc = 0;
	size_t buff_update = 0; /* to get the real size of the string*/
	file_offset move_to = 0;


	for (i = 0; i < (ui8) rec->fields_num; i++) {
		if(rec->field_set[i] == 0) continue;

		switch (rec->fields[i].type) {
		case TYPE_KEY:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}
			ui32 i_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &i_ne, sizeof(ui32)) < 0) 
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&i_ne,sizeof(i_ne),&written,NULL))
#endif
			{
				printf("could not read type key %s:%d\n",__FILE__,__LINE__-2);
				free_record(rec, rec->fields_num);
				return -1;
			}
			rec->fields[i].data.k = (ui32)swap32(i_ne);
			break;
		}
		case TYPE_INT:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}

			ui32 i_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &i_ne, sizeof(ui32)) < 0) 
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&i_ne,sizeof(i_ne),&written,NULL))
#endif
			{
				perror("could not read type int file.c 491.\n");
				free_record(rec, rec->fields_num);
				return -1;
			}
			rec->fields[i].data.i = (int)swap32(i_ne);
			break;
		}
		case TYPE_LONG: 
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui64)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}
			ui64 l_ne = 0;



#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &l_ne, sizeof(l_ne)) < 0)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&l_ne,sizeof(l_ne),&written,NULL))
#endif
			{
				perror("could not read type long, file.c 498.\n");
				free_record(rec, rec->fields_num);
				return -1;
			}

			rec->fields[i].data.l = (long)swap64(l_ne);
			break;
		}
		case TYPE_FLOAT:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}
			ui32 f_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &f_ne, sizeof(ui32)) < 0)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&f_ne,sizeof(f_ne),&written,NULL))
#endif
			{
				perror("could not read type float, file.c l 505.\n");
				free_record(rec, rec->fields_num);
				return -1;
			}

			rec->fields[i].data.f = ntohf(f_ne);
			break;
		}
		case TYPE_PACK:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}
			ui32 p_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &p_ne, sizeof(ui32)) < 0)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&p_ne,sizeof(p_ne),&written,NULL))
#endif
			{
				perror("could not read type float, file.c l 505.\n");
				free_record(rec, rec->fields_num);
				return -1;
			}

			rec->fields[i].data.p = swap32(p_ne);
			break;
		}
		case TYPE_DATE:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}
			ui32 p_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &p_ne, sizeof(ui32)) < 0)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&p_ne,sizeof(p_ne),&written,NULL))
#endif
			{
				perror("could not read type float, file.c l 505.\n");
				free_record(rec, rec->fields_num);
				return -1;
			}

			rec->fields[i].data.date = swap32(p_ne);
			break;
		}
		case TYPE_STRING:
		{
			/*read pos of new str if any*/
			ui32 str_loc_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == -1)
#elif defined(_WIN32)
			written = 0;
			if (!ReadFile(fd,&str_loc_ne,sizeof(str_loc_ne),&written,NULL))
#endif
			{
				perror("can't read string location: ");
				printf(" %s:%d", F, L - 3);
				free_record(rec, rec->fields_num);
				return -1;
			}
			str_loc = (file_offset)swap32(str_loc_ne);

			ui16 bu_up_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &bu_up_ne, sizeof(bu_up_ne)) < 0)
#elif defined(_WIN32)
			written = 0;
			if (!ReadFile(fd,&bu_up_ne,sizeof(bu_up_ne),&written,NULL))
#endif
			{
				perror("read from file failed: ");
				printf("%s:%d.\n", F, L - 2);
				free_record(rec, rec->fields_num);
				return -1;
			}

			buff_update = (size_t)swap16(bu_up_ne);

			if (str_loc > 0)
			{
				/*save the offset past the buff_w*/
				move_to = get_file_offset(fd) + buff_update;
				/*move to (other)string location*/
				if (find_record_position(fd, str_loc) == -1)
				{
					__er_file_pointer(F, L - 2);
					free_record(rec, rec->fields_num);
					return -1;
				}

				ui16 bu_up_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &bu_up_ne, sizeof(bu_up_ne)) < 0)
#elif defined(_WIN32)
				DWORD written = 0;
				if (!ReadFile(fd,&bu_up_ne,sizeof(bu_up_ne),&written,NULL))
#endif
				{
					perror("read file: ");
					printf("%s:%d", F, L - 2);
					free_record(rec, rec->fields_num);
					return -1;
				}

				buff_update = (file_offset)swap16(bu_up_ne);
			}
		   			
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,buff_update) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			} else{
				rec->fields[i].data.s = malloc(buff_update);
				if (!rec->fields[i].data.s){
					fprintf(stderr,"malloc failed: %s:%d.\n", F, L - 3);
					free_record(rec, rec->fields_num);
					return -1;
				}
				memset(rec->fields[i].data.s,0,buff_update);

				/*read the actual string*/
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, rec->fields[i].data.s, buff_update) < 0)
#elif defined(_WIN32)
				DWORD written = 0;
				if (!ReadFile(fd,rec->fields[i].data.s,buff_update,&written,NULL))
#endif
				{
					perror("could not read buffer string, file.c l 539.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

			}
			/*set file pointer back at the end of the original str record*/
			if (str_loc > 0)
			{
				if (find_record_position(fd, move_to) == -1){
					__er_file_pointer(F, L - 2);
					free_record(rec, rec->fields_num);
					return -1;
				}
			}

			break;
		}
		case TYPE_BYTE:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(unsigned char)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}

#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &rec->fields[i].data.b, sizeof(unsigned char)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&rec->fields[i].data.b,sizeof(unsigned char),&written,NULL))
#endif
			{
				perror("could not read type byte: ");
				printf(" %s:%d.\n", F, L - 2);
				free_record(rec, rec->fields_num);
				return -1;
			}

			break;
		}
		case TYPE_DOUBLE:
		{
			if(rec->fields[i].is_dropped){
				if(move_in_file_bytes(fd,sizeof(ui32)) == -1){
					printf("could not move file pointer %s:%d\n",__FILE__,__LINE__-2);
					free_record(rec, rec->fields_num);
					return -1;
				}
				break;
			}

			ui64 d_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &d_ne, sizeof(d_ne)) < 0)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&d_ne,sizeof(d_ne),&written,NULL))
#endif
			{
				perror("could not read type double:");
				printf(" %s:%d.\n", F, L - 2);
				free_record(rec, rec->fields_num);
				return -1;
			}

			rec->fields[i].data.d = ntohd(d_ne);
			break;
		}
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
		{
			if (!rec->fields[i].data.v.elements.i){
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;
			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;

			do{
				ui32 size_array = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&size_array,sizeof(size_array),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;

#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for ( j = 0; j < sz; j++){
					ui32 num_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &num_ne, sizeof(num_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&num_ne,sizeof(num_ne),&written,NULL))
#endif
					{
						perror("can't read int array from file.\n");
						free_record(rec, rec->fields_num);
						return -1;
					}

					int num = (int)swap32(num_ne);
					rec->fields[i].data.v.insert((void *)&num,
						&rec->fields[i].data.v,
						rec->fields[i].type);
				}

				if (padd > 0)
				{
					if (move_in_file_bytes(fd, padd * sizeof(int)) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}

				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}

				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				return -1;
			}
			break;
		}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
		{
			if (!rec->fields[i].data.v.elements.l)
			{
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;

			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;

			do
			{
				ui32 size_array = 0;

#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&size_array,sizeof(size_array),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for (j = 0; j < sz; j++)
				{
					ui64 num_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &num_ne, sizeof(num_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&num_ne,sizeof(num_ne),&written,NULL))
#endif
					{
						perror("can't read int array from file.\n");
						free_record(rec, rec->fields_num);
						return -1;
					}
					long num = (long)swap64(num_ne);
					rec->fields[i].data.v.insert((void *)&num,
												 &rec->fields[i].data.v,
												 rec->fields[i].type);
				}

				if (padd > 0)
				{
					if (move_in_file_bytes(fd, padd * sizeof(long)) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}

				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}
				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				return -1;
			}
			break;
		}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
		{
			if (!rec->fields[i].data.v.elements.s)
			{
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;
			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;

			do
			{
				ui32 size_array = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&size_array,sizeof(size_array),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif 
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for (j = 0; j < sz; j++)
				{

					/*read pos of new str if any*/
					ui32 str_loc_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &str_loc_ne, sizeof(str_loc_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&str_loc_ne,sizeof(str_loc_ne),&written,NULL))
#endif
					{
						perror("can't read string location: ");
						printf(" %s:%d", F, L - 3);
						return -1;
					}
					str_loc = (size_t)swap32(str_loc_ne);

					ui16 bu_up_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &bu_up_ne, sizeof(bu_up_ne)) < 0)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&bu_up_ne,sizeof(bu_up_ne),&written,NULL))
#endif
					{
						perror("read from file failed: ");
						printf("%s:%d.\n", F, L - 2);
						free_record(rec, rec->fields_num);
						return -1;
					}

					buff_update = (size_t)swap16(bu_up_ne);

					if (str_loc > 0)
					{
						/*save the offset past the buff_w*/
						move_to = get_file_offset(fd) + buff_update;
						/*move to (other)string location*/
						if (find_record_position(fd, str_loc) == -1)
						{
							__er_file_pointer(F, L - 2);
							free_record(rec, rec->fields_num);
							return -1;
						}

						ui16 bu_up_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
						if (os_read(fd, &bu_up_ne, sizeof(bu_up_ne)) < 0)
#elif defined(_WIN32)
						written = 0;
						if (!ReadFile(fd,&bu_up_ne,sizeof(bu_up_ne),&written,NULL))
#endif
						{
							perror("read file: ");
							printf("%s:%d", F, L - 2);
							free_record(rec, rec->fields_num);
							return -1;
						}

						buff_update = (size_t)swap16(bu_up_ne);
					}

					char *all_buf = (char*)malloc(buff_update);
					memset(all_buf,0,buff_update);
					if (!all_buf){
						printf("malloc() failed, %s:%d.\n",F,L-2);
						free_record(rec, rec->fields_num);
						return -1;
					}

					/*read the actual string*/
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, all_buf, buff_update) < 0)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,all_buf,sizeof(buff_update),&written,NULL))
#endif
					{
						perror("could not read buffer string, file.c l 539.\n");
						free_record(rec, rec->fields_num);
						return -1;
					}

					rec->fields[i].data.v.insert((void *)all_buf,
							 &rec->fields[i].data.v,
							 rec->fields[i].type);
					free(all_buf);

					/*set file pointer back at the end of the original str record*/
					if (str_loc > 0)
					{
						if (find_record_position(fd, move_to) == -1)
						{
							__er_file_pointer(F, L - 2);
							free_record(rec, rec->fields_num);
							return -1;
						}
					}
				}

				if (padd > 0)
				{
					int x;
					for( x = 0; x < padd; x++){
						if(get_string_size(fd,NULL) == (size_t)-1){
							__er_file_pointer(F, L - 1);
							free_record(rec, rec->fields_num);
							return -1;
						}

					}
				}

				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}
				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				return -1;
			}
			break;
		}
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
		{
			if (!rec->fields[i].data.v.elements.f)
			{
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;

			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;
			do
			{
				ui32 size_array = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&size_array,sizeof(size_array),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for (j = 0; j < sz; j++)
				{
					ui32 num_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &num_ne, sizeof(num_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&num_ne,sizeof(num_ne),&written,NULL))
#endif
					{
						perror("can't read int array from file.\n");
						free_record(rec, rec->fields_num);
						return -1;
					}

					float num = ntohf(num_ne);
					rec->fields[i].data.v.insert((void *)&num,
												 &rec->fields[i].data.v,
												 rec->fields[i].type);
				}

				if (padd > 0)
				{
					if (move_in_file_bytes(fd, padd * sizeof(float)) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}

				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}

				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				return -1;
			}
			break;
		}
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
		{
			if (!rec->fields[i].data.v.elements.d)
			{
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;

			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			DWORD written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;

			do
			{
				ui32 size_array = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for (j = 0; j < sz; j++)
				{
					ui64 num_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
					if (os_read(fd, &num_ne, sizeof(num_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&num_ne,sizeof(num_ne),&written,NULL))
#endif
					{
						perror("can't read int array from file.\n");
						free_record(rec, rec->fields_num);
					return -1;
					}
					double num = ntohd(num_ne);
					rec->fields[i].data.v.insert((void *)&num,
												 &rec->fields[i].data.v,
												 rec->fields[i].type);
				}

				if (padd > 0)
				{
					if (move_in_file_bytes(fd, padd * sizeof(double)) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
					return -1;
					}
				}

				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}
				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
					return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
					return -1;
			}
			break;
		}
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
		{
			if (!rec->fields[i].data.v.elements.b)
			{
				rec->fields[i].data.v.insert = insert_element;
				rec->fields[i].data.v.destroy = free_dynamic_array;
			}

			file_offset array_upd = 0;
			file_offset go_back_here = 0;

			ui8 set = 0;
#if defined(__linux__) || defined(__APPLE__)
			if (os_read(fd, &set, sizeof(set)) == -1)
#elif defined(_WIN32)
			written = 0;
			if (!ReadFile(fd,&set,sizeof(set),&written,NULL))
#endif
			{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
			}

			rec->fields[i].data.v.is_set = set == 1 ? 1 : 0;

			do
			{
				ui32 size_array = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &size_array, sizeof(size_array)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&size_array,sizeof(size_array),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int sz = (int)swap32(size_array);
				ui32 padding = 0;
#if defined(__linux__) || defined(__APPLE__)
				if (os_read(fd, &padding, sizeof(padding)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&padding,sizeof(padding),&written,NULL))
#endif
				{
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					return -1;
				}

				int padd = (int)swap32(padding);

				int j;
				for (j = 0; j < sz; j++)
				{
					ui8 num_ne = 0;
#if defined(__linux__) || defined(__APPLE__)	
					if (os_read(fd, &num_ne, sizeof(num_ne)) == -1)
#elif defined(_WIN32)
					written = 0;
					if (!ReadFile(fd,&num_ne,sizeof(num_ne),&written,NULL))
#endif
					{
						perror("can't read int array from file.\n");
						free_record(rec, rec->fields_num);
					return -1;
					}

					rec->fields[i].data.v.insert((void *)&num_ne,
									 &rec->fields[i].data.v,
									 rec->fields[i].type);
				}

				if (padd > 0)
				{
					if (move_in_file_bytes(fd, padd * sizeof(unsigned char)) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
					return -1;
					}
				}


				ui64 upd_ne = 0;
#if defined(__linux__) || defined(__APPLE__)	
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
#elif defined(_WIN32)
				written = 0;
				if (!ReadFile(fd,&upd_ne,sizeof(upd_ne),&written,NULL))
#endif
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					return -1;
				}

				if (go_back_here == 0)
				{
					go_back_here = get_file_offset(fd);
				}
				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0)
				{
					if (find_record_position(fd, array_upd) == -1)
					{
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
					return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				return -1;
			}
			break;
		}
		case TYPE_FILE:
		{
			/*TODO: do we need type_file?*/
#if 0
			size_t len = strlen(rec->fields[i].field_name);
			char *sfx = ".sch";
			int sfxl = (int)strlen(sfx);
			int totl = sfxl + (int)len + 1;
			char sch_file[totl];
			memset(sch_file,0,totl);
			strncpy(sch_file,rec->fields[i].field_name,len);
			strncat(sch_file,sfx,sfxl);

			/* open the file*/
			int fd_schema = open_file(sch_file,0);
			if(file_error_handler(1,fd_schema) != 0) return 0;			

			struct Schema sch;
			memset(&sch,0,sizeof(struct Schema));
			struct Header_d hd = {0,0,&sch};	

			if (!read_header(fd_schema, &hd)) {
				close(fd_schema);
				free_record(rec, rec->fields_num);
				return -1;
			}

			close(fd_schema);



			file_offset array_upd = 0;
			file_offset go_back_here = 0;
			do
			{
				ui32 size_array = 0;
				if (os_read(fd, &size_array, sizeof(size_array)) == -1){
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					free_schema(hd.sch_d);
					return -1;
				}

				ui32 sz = swap32(size_array);
				ui32 padding = 0;
				if (os_read(fd, &padding, sizeof(padding)) == -1){
					perror("error readig array.");
					free_record(rec, rec->fields_num);
					free_schema(hd.sch_d);
					return -1;
				}

				int padd = (int)swap32(padding);
				ui32 j;
				for(j = 0; j < sz; j++){
					if (!rec->fields[i].data.file.recs){
						rec->fields[i].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
						rec->fields[i].data.file.count = sz;
						if(!rec->fields[i].data.file.recs){
							fprintf(stderr,"malloc failed %s:%d.\n",F,L-3);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
					
						memset(rec->fields[i].data.file.recs,0,sizeof(struct Record_f));

						if(read_file(fd, rec->fields[i].field_name, 
									&rec->fields[i].data.file.recs[j],
									*hd.sch_d) == -1){
							fprintf(stderr,"cannot read type file %s:%d.\n",F,L-1);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
					}else{

						ui32 new_size = rec->fields[i].data.file.count + 1;	
						struct Record_f* new_rec = (struct Record_f*)realloc(rec->fields[i].data.file.recs,
																	new_size * sizeof(struct Record_f));

						if(!new_rec){
							fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__-4);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
						rec->fields[i].data.file.recs = new_rec;
						rec->fields[i].data.file.count = new_size;

						if(read_file(fd, rec->fields[i].field_name,
									&rec->fields[i].data.file.recs[new_size -1]
									,*hd.sch_d) == -1){
							fprintf(stderr,"cannot read type file %s:%d.\n",F,L-1);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
					}

					/*TODO refactor this code file.c l 5930*/

					/*check if the record has updates */
					file_offset rests_pos_here = get_file_offset(fd);
					file_offset update_rec_pos = 0; 
					while ((update_rec_pos = get_update_offset(fd)) > 0) {
						ui32 new_size = rec->fields[i].data.file.count + 1;

						struct Record_f *n = realloc(rec->fields[i].data.file.recs,	
								new_size * sizeof(struct Record_f));

						if (!n) {
							fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__-6);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
						rec->fields[i].data.file.recs = n;
						rec->fields[i].data.file.count = new_size;
						if (find_record_position(fd, update_rec_pos) == -1) {
							__er_file_pointer(F, L - 1);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}

						if(read_file(fd, 
								rec->fields[i].field_name,
								&rec->fields[i].data.file.recs[new_size -1],
								*hd.sch_d) == -1) {
							fprintf(stderr,"cannot read record of embedded file,%s:%d.\n",F,L-1);
							free_record(rec, rec->fields_num);
							free_schema(hd.sch_d);
							return -1;
						}
					}

					if(update_rec_pos == -1 || rests_pos_here== -1){
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						free_schema(hd.sch_d);
						return -1;
					}

					if (find_record_position(fd, rests_pos_here) == -1) {
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						free_schema(hd.sch_d);
						return -1;
					}

					if (move_in_file_bytes(fd, sizeof(file_offset)) == -1) {
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						free_schema(hd.sch_d);
						return -1;
					}
					
				}

				if (padd > 0)
				{
					int y;
					for(y = 0; y < padd;y++){
						struct Record_f dummy;
						memset(&dummy,0,sizeof(struct Record_f));
						if(read_file(fd, rec->fields[i].field_name,
									&dummy, *hd.sch_d) == -1){
							fprintf(stderr,"cannot read type file %s:%d.\n"
									,F,L-1);
							free_schema(hd.sch_d);
							free_record(rec, rec->fields_num);
							return -1;
						}

						free_record(&dummy,dummy.fields_num);

						if(move_in_file_bytes(fd,sizeof(file_offset)) == -1){
							__er_file_pointer(F, L - 1);
							free_schema(hd.sch_d);
							free_record(rec, rec->fields_num);
							return 0;
						}
					}
				}

				ui64 upd_ne = 0;
				if (os_read(fd, &upd_ne, sizeof(upd_ne)) == -1)
				{
					perror("can't read int array from file.\n");
					free_record(rec, rec->fields_num);
					free_schema(hd.sch_d);
					return -1;
				}

				if (go_back_here == 0) go_back_here = get_file_offset(fd);

				array_upd = (file_offset)swap64(upd_ne);
				if (array_upd > 0){
					if (find_record_position(fd, array_upd) == -1){
						__er_file_pointer(F, L - 1);
						free_record(rec, rec->fields_num);
						free_schema(hd.sch_d);
						return -1;
					}
				}
			} while (array_upd > 0);

			if (find_record_position(fd, go_back_here) == -1)
			{
				__er_file_pointer(F, L - 1);
				free_record(rec, rec->fields_num);
				free_schema(hd.sch_d);
				return -1;
			}

			free_schema(hd.sch_d);
			break;
#endif
		}
		default:
			break;
		}
	}

	return 0;
}

int file_error_handler(int count, ...)
{
	va_list args;
	va_start(args, count);
#if defined(__linux__) || defined(__APPLE__)
	file_t fds[count];
	INIT_FILE_T_ARRAY(fds,count);
#elif defined(_WIN32)
	file_t handles[count];
	INIT_FILE_T_ARRAY(handles,count);
	
#endif
	int i = 0, j = 0;

	int err = 0, exist = 0;
	for (i = 0; i < count; i++) {
#if defined(__linux__) || defined(__APPLE__)
		int fd = va_arg(args, int);
		if (fd == STATUS_ERROR){
#elif defined(_WIN32)
		HANDLE file_handle = va_arg(args,HANDLE);
		if (file_handle == INVALID_HANDLE_VALUE){
			err++;
#endif
		
			j++;
			continue;
		}
#if defined(__linux__) || defined(__APPLE__)
		if (fd == ENOENT) err++;
		if (fd == EEXIST) exist++;
		fds[i] = fd;
#elif defined(_WIN32)
		if (file_handle == INVALID_HANDLE_VALUE) err++;
		handles[i] = file_handle;
#endif
	}

	if(j != 0 ){
		int x;
		for(x = 0 ;x < count; x++){
#if defined(__linux__) || defined(__APPLE__)
			if(fds[x] > 2) close(fds[x]);
#elif defined(_WIN32)
			if(handles[x] !=  INVALID_HANDLE_VALUE && handles[x])
				CloseHandle(handles[x]);
#endif
	   } 
	}
#if defined(__linux__) || defined(__APPLE__)
	if(err > 0) return ENOENT;	
	if(exist > 0) return EEXIST;	
#elif defined(_WIN32)
	if(err > 0) return ERROR_CODE_FILE_OPERATION;	
#endif
	return j;
}


int add_index(int index_nr, char *file_name, int bucket)
{
	if (index_nr == 0)
		return -1;

	size_t l = strlen(file_name) + strlen(".inx") + 1;
	char buff[l];
	memset(buff, 0, l);

	if (copy_to_string(buff, l, "%s%s", file_name, ".inx") < 0) {
		fprintf(stderr,
				"copy_to_string() failed. %s:%d.\n",
				F, L - 3);
		return -1;
	}
	file_t fd;
	if(open_file(buff, 0,&fd) == -1){
		/*TODO expand file_error_handler errors */
		file_error_handler(1, fd);
		fprintf(stderr,"can't open %s, %s:%d.\n", buff, F, L - 3);
		return -1;
	}

	HashTable *ht = NULL;
	int ht_i = 0;
	if (!read_all_index_file(fd, &ht, &ht_i))
	{
		fprintf(stderr,"read_all_index_file(),failed %s:%d.\n",
				F, L - 4);
		return -1;
	}

	HashTable *ht_new = realloc(ht,(ht_i + index_nr) * sizeof(HashTable));
	if (!ht_new){
		fprintf(stderr,"(%s): realloc failed, %s:%d.\n",prog,F, L - 2);
		free_ht_array(ht, ht_i);
		return -1;
	}

	ht = ht_new;
	int total_indexes = ht_i + index_nr;
	close_file(1, fd);

	int j;
	for (j = ht_i; j < total_indexes; ++j)
	{
		HashTable dummy;
		memset(&dummy,0,sizeof(HashTable)); 
		dummy.write = write_ht;
		dummy.size = bucket;

		ht[j] = dummy;
	}

	/*reopen the file with O_TRUNCATE flag to overwrite the old content*/
	if(open_file(buff, 1,&fd) == -1){
		file_error_handler(1, fd);
		fprintf(stderr,	"can't open %s, %s:%d.\n", buff, F, L - 3);
		free_ht_array(ht, total_indexes);
		return -1;
	}

	if (!write_index_file_head(fd, total_indexes)) {
		fprintf(stderr, "write_index_file_head() failed, %s:%d.",
				F, L - 3);
		free_ht_array(ht, total_indexes);
		return -1;
	}

	int i;
	for (i = 0; i < total_indexes; ++i) {
		/*
		 * create an Hashtable in the
		 * reallocated hastable array
		 * */
		if (ht[i].data_map[0] == NULL) {
			if (!write_index_body(fd, i, &ht[i])) {
				fprintf(stderr,"write_index_body() failed %s:%d.\n",F, L - 3);
				free_ht_array(ht, total_indexes);
				return -1;
			}

			destroy_hasht(&ht[i]);
			continue;
		}

		if (!write_index_body(fd, i, &ht[i])) {
			fprintf(stderr,	"write_index_body() failed %s:%d.\n",F, L - 3);
			free_ht_array(ht, total_indexes);
			return -1;
		}

		destroy_hasht(&ht[i]);
	}

	free(ht);
	close_file(1, fd);
	return 0;
}


int init_ram_file(struct Ram_file *ram, size_t size)
{
	if(size == 0){
		ram->mem = (ui8*)malloc(STD_RAM_FILE*sizeof(ui8)); 
		if(!ram->mem){
			fprintf(stderr,"malloc failed, %s:%d.\n",F,L-2);
			return -1;
		}
		ram->size = 0;
		ram->offset = 0;
		ram->capacity = STD_RAM_FILE;
		return 0;
	}

	ram->mem = (ui8*)malloc(size*sizeof(ui8));
	if(!ram->mem){
		fprintf(stderr,"malloc failed, %s:%d.\n",F,L-2);
		return -1;
	}
	ram->size = 0;
	ram->offset = 0;
	ram->capacity = size;
	return 0;
}

void clear_ram_file(struct Ram_file *ram)
{
	memset(ram->mem,0,ram->capacity);
	ram->size = 0;
}

void close_ram_file(struct Ram_file *ram)
{
	if(ram->mem)
		free(ram->mem);
	ram->size = 0;
	ram->capacity = 0;
}

size_t get_offset_ram_file(struct Ram_file *ram)
{
	return ram->size;
}

int get_all_record(file_t fd, struct Ram_file *ram)
{

	file_offset eof = go_to_EOF(fd);
	if(begin_in_file(fd) == -1) return -1;

	if(init_ram_file(ram,(size_t)eof) == -1) return -1;	
	if(os_read(fd,ram->mem,ram->capacity) == -1) return -1;
	ram->size = (size_t)eof;
	ram->offset = 0;
	return 0;
}

long long read_ram_file(char* file_name, struct Ram_file *ram, struct Record_f *rec, struct Schema sch)
{
	create_record(file_name, sch,rec);

	rec->offset = ram->offset; 
	ui8 fields_on_file = 0;
	memcpy(&fields_on_file,&ram->mem[ram->offset],sizeof(ui8));
	move_ram_file_ptr(ram,sizeof(ui8));

	ui8 indexes[fields_on_file];
	memset(indexes,-1,fields_on_file);

	memcpy(indexes,&ram->mem[ram->offset],sizeof(ui8)*fields_on_file);		
	move_ram_file_ptr(ram,sizeof(ui8)*fields_on_file);

	ui8 i;
	for(i = 0; i < fields_on_file; i++){
		rec->field_set[indexes[i]] = 1;
		switch(rec->fields[indexes[i]].type){
			case TYPE_INT:
				{
					ui32 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					rec->fields[indexes[i]].data.i = swap32(n);
					break;
				}
			case TYPE_DATE:
				{
					ui32 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					rec->fields[indexes[i]].data.date = swap32(n);
					break;
				}
			case TYPE_LONG:
				{
					ui64 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui64));
					move_ram_file_ptr(ram,sizeof(ui64));
					rec->fields[indexes[i]].data.l = swap64(n);
					break;
				}
			case TYPE_BYTE:
				{
					ui8 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.b = n;
					break;
				}
			case TYPE_KEY:
				{
					ui32 k = 0;
					memcpy(&k,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					rec->fields[indexes[i]].data.k = (ui32)swap32(k);
					break;
				}
			case TYPE_STRING:
				{
					ui32 str_loc_ne = 0;
					memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					file_offset str_loc = (file_offset)swap32(str_loc_ne);

					ui16 buf_up_ne = 0;
					memcpy(&buf_up_ne,&ram->mem[ram->offset],sizeof(ui16));
					move_ram_file_ptr(ram,sizeof(ui16));
					size_t buf_up = (size_t)swap16(buf_up_ne);

					file_offset move_back_to = 0;
					if(str_loc > 0){
						move_back_to = ram->offset + buf_up;	
						/* move to the new location*/
						ram->offset = str_loc;

						buf_up_ne = 0;
						memcpy(&buf_up_ne,&ram->mem[ram->offset],sizeof(ui16));
						move_ram_file_ptr(ram,sizeof(ui16));
						buf_up = swap16(buf_up_ne);
					}

					rec->fields[indexes[i]].data.s = (char*) malloc(buf_up*sizeof(char));
					memset(rec->fields[indexes[i]].data.s,0,buf_up);
					if(!rec->fields[indexes[i]].data.s){
						fprintf(stderr,"malloc failed, %s:%d.\n",F,L-2);
						return -1;
					}

					memcpy(rec->fields[indexes[i]].data.s,&ram->mem[ram->offset],buf_up);
					move_ram_file_ptr(ram,buf_up);
					if(str_loc > 0) ram->offset = move_back_to;


					break;
				}
			case TYPE_FLOAT:
				{
					ui32 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					rec->fields[indexes[i]].data.f = ntohf(n);
					break;
				}
			case TYPE_PACK:
				{
					ui32 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
					move_ram_file_ptr(ram,sizeof(ui32));
					rec->fields[indexes[i]].data.p = swap32(n);
					break;
				}
			case TYPE_DOUBLE:
				{
					ui64 n = 0;
					memcpy(&n,&ram->mem[ram->offset],sizeof(ui64));
					move_ram_file_ptr(ram,sizeof(ui64));
					rec->fields[indexes[i]].data.d = ntohd(n);
					break;
				}
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
				{
					if (!rec->fields[indexes[i]].data.v.elements.i){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}

					file_offset array_upd = 0;
					file_offset go_back_here = 0;
					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		

					do
					{

						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui32 n = 0;
							memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
							move_ram_file_ptr(ram,sizeof(ui32));
							int num = (int)swap32(n);
							rec->fields[indexes[i]].data.v.insert((void *)&num,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);
						}

						if (padd > 0) ram->offset += (sizeof(ui32) * padd);

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while (array_upd > 0);

					ram->offset = go_back_here;
					break;
				}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
				{
					if (!rec->fields[indexes[i]].data.v.elements.l){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}
					file_offset array_upd = 0;
					file_offset go_back_here = 0;
					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		

					do{

						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui64 n = 0;
							memcpy(&n,&ram->mem[ram->offset],sizeof(ui64));
							move_ram_file_ptr(ram,sizeof(ui64));

							long num = (long)swap64(n);

							rec->fields[indexes[i]].data.v.insert((void *)&num,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);
						}

						if (padd > 0) ram->offset += (sizeof(ui64) * padd);

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while (array_upd > 0);

					ram->offset = go_back_here;
					break;
				}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
				{
					if (!rec->fields[indexes[i]].data.v.elements.b){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}
					file_offset array_upd = 0;
					file_offset go_back_here = 0;

					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		

					do
					{
						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui8 n = 0;
							memcpy(&n,&ram->mem[ram->offset],sizeof(ui8));
							move_ram_file_ptr(ram,sizeof(ui8));
							rec->fields[indexes[i]].data.v.insert((void *)&n,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);
						}

						if (padd > 0) ram->offset += (sizeof(ui8) * padd);

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while (array_upd > 0);

					ram->offset = go_back_here;
					break;
				}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
				{
					if (!rec->fields[indexes[i]].data.v.elements.f){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}
					file_offset array_upd = 0;
					file_offset go_back_here = 0;

					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		

					do
					{
						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui32 n = 0;
							memcpy(&n,&ram->mem[ram->offset],sizeof(ui32));
							move_ram_file_ptr(ram,sizeof(ui32));
							float num = (float)ntohf(n);
							rec->fields[indexes[i]].data.v.insert((void *)&num,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);
						}

						if (padd > 0) ram->offset += (sizeof(ui32) * padd);

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while (array_upd > 0);

					ram->offset = go_back_here;
					break;
				}

			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
				{
					if (!rec->fields[indexes[i]].data.v.elements.d){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}
					file_offset array_upd = 0;
					file_offset go_back_here = 0;
					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		
					do
					{
						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui64 n = 0;
							memcpy(&n,&ram->mem[ram->offset],sizeof(ui64));
							move_ram_file_ptr(ram,sizeof(ui64));

							double num = (double)ntohd(n);

							rec->fields[indexes[i]].data.v.insert((void *)&num,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);
						}

						if (padd > 0) ram->offset += (sizeof(ui64) * padd);

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while(array_upd > 0);

					ram->offset = go_back_here;
					break;
				}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
				{
					if (!rec->fields[indexes[i]].data.v.elements.s){
						rec->fields[indexes[i]].data.v.insert = insert_element;
						rec->fields[indexes[i]].data.v.destroy = free_dynamic_array;
					}
					file_offset array_upd = 0;
					file_offset go_back_here = 0;

					ui8 set = 0;
					memcpy(&set,&ram->mem[ram->offset],sizeof(ui8));
					move_ram_file_ptr(ram,sizeof(ui8));
					rec->fields[indexes[i]].data.v.is_set = set;		
					do
					{
						ui32 size_array = 0;
						memcpy(&size_array,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int sz = (int)swap32(size_array);


						ui32 padding = 0;
						memcpy(&padding,&ram->mem[ram->offset],sizeof(ui32));
						move_ram_file_ptr(ram,sizeof(ui32));
						int padd = (int)swap32(padding);

						int j;
						for (j = 0; j < sz; j++){
							ui32 str_loc_ne = 0;
							memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
							move_ram_file_ptr(ram,sizeof(ui32));
							file_offset str_loc = (file_offset)	swap32(str_loc_ne);


							ui16 buf_up_ne = 0;
							memcpy(&buf_up_ne,&ram->mem[ram->offset],sizeof(ui16));
							move_ram_file_ptr(ram,sizeof(ui16));
							size_t buf_up = (size_t)swap16(str_loc_ne);

							file_offset move_back_to = 0;
							if(str_loc > 0){
								move_back_to = ram->offset + buf_up;	
								/* move to the new location*/
								ram->offset = str_loc;

								buf_up_ne = 0;
								memcpy(&buf_up_ne,&ram->mem[ram->offset],sizeof(ui16));
								move_ram_file_ptr(ram,sizeof(ui16));
								buf_up = (size_t)swap16(str_loc_ne);
							}

							char *string = malloc(buf_up*sizeof(char));
							memset(string,0,buf_up);
							if(!string){
								fprintf(stderr,"malloc failed, %s:%d.\n",F,L-2);
								return -1;
							}

							memcpy(string,&ram->mem[ram->offset],buf_up);
							move_ram_file_ptr(ram, buf_up);
							if(str_loc > 0) ram->offset = move_back_to;

							rec->fields[indexes[i]].data.v.insert((void *)string,
									&rec->fields[indexes[i]].data.v,
									rec->fields[indexes[i]].type);

							free(string);
						}

						if (padd > 0) {
							int x;
							for( x = 0; x < padd; x++){
								move_ram_file_ptr(ram,sizeof(ui32));
								ui16 buf_upn = 0;
								memcpy(&buf_upn,&ram->mem[ram->offset],sizeof(ui16));
								move_ram_file_ptr(ram,sizeof(ui16));
								size_t buf_up = swap16(buf_upn);
								move_ram_file_ptr(ram,buf_up);
							}
						}

						ui64 upd_ne = 0;
						memcpy(&upd_ne,&ram->mem[ram->offset],sizeof(ui64));
						move_ram_file_ptr(ram,sizeof(ui64));

						if (go_back_here == 0) go_back_here = ram->offset;

						array_upd = (file_offset)swap64(upd_ne);
						if (array_upd > 0) ram->offset = array_upd;

					} while (array_upd > 0);

					ram->offset = go_back_here;
					break;
				}
			case TYPE_FILE:
				/*coming soon*/
				break;
			default:
				fprintf(stdout,"wrong type or not handled,type -> %s, %s:%d.\n",
						type_to_str(rec->fields[indexes[i]].type),__FILE__,__LINE__);
				return -1;
		}
	}

	return (long long) ram->offset;
}

/*NOTE this is to be used after read_ram_file */
file_offset read_update_offset_ram_file(struct Ram_file *ram){
	ui64 pos = 0;
	memcpy(&pos,&ram->mem[ram->offset],sizeof(ui64));
	ram->offset += sizeof(ui64);
	return (file_offset) swap64(pos);
}

/*
 *
 * if you pass init_ram_size as 0, in case the struct Ram_file memory is NULL
 * it will allocate the STD_RAM_FILE size to it (3 Mib)
 *
 * offset parameter is the offset of another part of the record, when the record is in more than one place in the file
 * */
int write_ram_record(struct Ram_file *ram, struct Record_f *rec, int update, size_t init_ram_size, file_offset offset)
{
	if(ram->capacity == 0)
		if(init_ram_file(ram, init_ram_size) == -1) return -1;

	size_t rec_disk_size = get_disk_size_record(rec);

	/* if this is true it means we are at EOF
	 * this block check if we have enough size in the ram file*/
	if(ram->offset == ram->size){
		if(rec_disk_size > (ram->capacity - ram->size)){
			size_t new_size = ram->capacity + rec_disk_size;
			ui8 *n_buff = (ui8*)realloc(ram->mem, new_size * sizeof(ui8));
			if(!n_buff){
				fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__-2);
				return -1;
			}

			ram->capacity = new_size;
			ram->mem = n_buff;
			ram->offset = ram->size;/*this may be unneccessery*/
		}
	}

	ui8 cnt = 0;
	ui8 i;
	for(i = 0; i < rec->fields_num; i++){
		if(rec->field_set[i] == 0) continue;
		cnt++;
	}

	if(cnt == 0){
		fprintf(stderr,"no active fields in the record %s:%d\n",__FILE__,__LINE__);
		return -1;
	}

	if(ram->size == ram->capacity && ram->offset != ram->size)
		memcpy(&ram->mem[ram->offset],&cnt,sizeof(ui8));
	else
		memcpy(&ram->mem[ram->size],&cnt,sizeof(ui8));

	move_ram_file_ptr(ram,sizeof(ui8));


	for(i = 0; i < rec->fields_num; i++){
		if(rec->field_set[i] == 0) 
			continue;

		if(ram->size == ram->capacity && ram->offset != ram->size)
			memcpy(&ram->mem[ram->offset],&i,sizeof(ui8));
		else
			memcpy(&ram->mem[ram->size],&i,sizeof(ui8));

		move_ram_file_ptr(ram,sizeof(ui8));
	}

	for(i = 0; i < rec->fields_num; i++){
		if (rec->field_set[i] == 0)continue;

		switch(rec->fields[i].type){
			case TYPE_INT:
				{
					if(rec->fields[i].is_dropped){
						move_ram_file_ptr(ram,sizeof(ui32));
						break;
					}

					ui32 value = swap32((ui32)rec->fields[i].data.i);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&value,sizeof(ui32));
					else
						memcpy(&ram->mem[ram->size],&value,sizeof(ui32));

					move_ram_file_ptr(ram,sizeof(ui32));
					break;
				}
			case TYPE_KEY:
				{
					if(rec->fields[i].is_dropped){
						move_ram_file_ptr(ram,sizeof(ui32));
						break;
					}


					ui32 value = swap32(rec->fields[i].data.k);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&value,sizeof(ui32));
					else
						memcpy(&ram->mem[ram->size],&value,sizeof(ui32));

					move_ram_file_ptr(ram,sizeof(ui32));
					break;
				}
			case TYPE_LONG:
				{
					if(rec->fields[i].is_dropped){
						move_ram_file_ptr(ram,sizeof(ui64));
						break;
					}
					
					ui64 value = swap64((ui64)rec->fields[i].data.l);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&value,sizeof(ui64));
					else
						memcpy(&ram->mem[ram->size],&value,sizeof(ui64));

					move_ram_file_ptr(ram,sizeof(ui64));
					break;
				}
			case TYPE_BYTE:
			{
				if(rec->fields[i].is_dropped){
					move_ram_file_ptr(ram,sizeof(ui8));
					break;
				}
				if(ram->size == ram->capacity && ram->offset != ram->size)
					memcpy(&ram->mem[ram->offset],&rec->fields[i].data.b,sizeof(ui8));
				else
					memcpy(&ram->mem[ram->size],&rec->fields[i].data.b,sizeof(ui8));

				move_ram_file_ptr(ram,sizeof(ui8));
				break;
			}
			case TYPE_DATE:
			{
					if(rec->fields[i].is_dropped){
						move_ram_file_ptr(ram,sizeof(ui32));
						break;
					}

					ui32 value = swap32(rec->fields[i].data.date);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&value,sizeof(ui32));
					else
						memcpy(&ram->mem[ram->size],&value,sizeof(ui32));

					move_ram_file_ptr(ram,sizeof(ui32));
					break;
			}
			case TYPE_FLOAT:
			{
				if(rec->fields[i].is_dropped){
					move_ram_file_ptr(ram,sizeof(ui32));
					break;
				}
				ui32 value = htonf(rec->fields[i].data.f);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&value,sizeof(ui32));
					else
						memcpy(&ram->mem[ram->size],&value,sizeof(ui32));

					move_ram_file_ptr(ram,sizeof(ui32));
					break;
			}
			case TYPE_PACK:
			{
				if(rec->fields[i].is_dropped){
					move_ram_file_ptr(ram,sizeof(ui32));
					break;
				}

				ui32 value = swap32((ui32)rec->fields[i].data.p);
				if(ram->size == ram->capacity && ram->offset != ram->size)
					memcpy(&ram->mem[ram->offset],&value,sizeof(ui32));
				else
					memcpy(&ram->mem[ram->size],&value,sizeof(ui32));

				move_ram_file_ptr(ram,sizeof(ui32));
				break;
			}
			case TYPE_DOUBLE:
			{
				if(rec->fields[i].is_dropped){
					move_ram_file_ptr(ram,sizeof(ui64));
					break;
				}

				ui64 value = htond(rec->fields[i].data.d);
				if(ram->size == ram->capacity && ram->offset != ram->size)
					memcpy(&ram->mem[ram->offset],&value,sizeof(ui64));
				else
					memcpy(&ram->mem[ram->size],&value,sizeof(ui64));

				move_ram_file_ptr(ram,sizeof(ui64));
				break;
			}
			case TYPE_STRING:
			{
				if(!update){
					ui16 l = (ui16)strlen(rec->fields[i].data.s);
					ui16 buf_up_ne = swap16((l*2)+1);	
					ui32 str_loc = 0;	

					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&str_loc,sizeof(ui32));
					else
						memcpy(&ram->mem[ram->size],&str_loc,sizeof(ui32));

					move_ram_file_ptr(ram,sizeof(ui32));

					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],&buf_up_ne,sizeof(ui16));
					else
						memcpy(&ram->mem[ram->size],&buf_up_ne,sizeof(ui16));

					move_ram_file_ptr(ram,sizeof(ui16));

					char buff[(l * 2) + 1];
					memset(buff,0,(l * 2) +1);
					strncpy(buff,rec->fields[i].data.s,l);
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&ram->mem[ram->offset],buff,(l * 2) + 1);
					else
						memcpy(&ram->mem[ram->size],buff,(l * 2) + 1);

					move_ram_file_ptr(ram,(l * 2) +1);
					break;
				}else{
					ui64 move_to = 0;
					ui32 eof = 0;
					ui16 bu_ne = 0;
					ui16 new_lt = 0;
					/*save the starting offset for the string record*/
					ui64 bg_pos = ram->offset;
					ui32 str_loc_ne= 0;

					/*read the other str_loc if any*/
					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
					else
						memcpy(&str_loc_ne,&ram->mem[ram->size],sizeof(ui32));


					move_ram_file_ptr(ram,sizeof(ui32));
					ui32 str_loc = swap32(str_loc_ne);

					/* save pos where the data starts*/
					ui64 af_str_loc_pos = 0;
					if(ram->size == ram->capacity && ram->offset != ram->size)
						af_str_loc_pos = ram->offset;
					else
						af_str_loc_pos = ram->size;

					ui16 buff_update_ne = 0;

					if(ram->size == ram->capacity && ram->offset != ram->size)
						memcpy(&buff_update_ne,&ram->mem[ram->offset],sizeof(ui16));
					else
						memcpy(&buff_update_ne,&ram->mem[ram->size],sizeof(ui16));

					move_ram_file_ptr(ram,sizeof(ui16));

					ui16 buff_update = swap16(buff_update_ne);

					ui64 pos_after_first_str_record = 0; 
					if(ram->size == ram->capacity && ram->offset != ram->size)
						pos_after_first_str_record = ram->offset + buff_update; 
					else
						pos_after_first_str_record = ram->size + buff_update; 

					
					if(rec->fields[i].is_dropped){
						if(ram->size == ram->capacity && ram->offset != ram->size)
						 	ram->offset = pos_after_first_str_record;
						else
						 	ram->size = pos_after_first_str_record;
						break;
					}

					if (str_loc > 0){
						/*set the file pointer to str_loc*/
						ram->offset = str_loc;

						/*
						 * in the case of a regular buffer update we have
						 *  to save the file_offset to get back to it later
						 * */
						move_to = ram->offset; 

						ui16 bu_ne = 0;
						memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
						ram->offset +=  sizeof(ui16);	

						buff_update = (file_offset)swap16(bu_ne);
					}

					new_lt = strlen(rec->fields[i].data.s) + 1; /*get new str length*/

					if (new_lt > buff_update) {
						/*expand the buff_update only for the bytes needed*/
						buff_update += (new_lt - buff_update);

						/*
						 * if the new length is bigger then the buffer,
						 * set the file pointer to EOF to write the new data
						 * */
						eof = ram->size;
						ram->offset = eof;
						if(eof == ram->capacity){
							errno = 0;
							ui8 *n_mem = (ui8*)realloc(ram->mem,(ram->capacity + buff_update + sizeof(ui16)) * sizeof(ui8));
							if(!n_mem){
								fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
								return -1;
							}
							ram->mem = n_mem;
							ram->capacity += buff_update; 
							ram->capacity += sizeof(ui16);
							/*offset = ram->capacity;*/

						}else if((eof + buff_update) > ram->capacity){
							errno = 0;
							ui8 *n_mem = (ui8*)realloc(ram->mem,(ram->capacity + buff_update +sizeof(ui16)) * sizeof(ui8));
							if(!n_mem){
								fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
								return -1;
							}
							ram->mem = n_mem;
							ram->capacity += ((eof + buff_update) - ram->capacity); 
							ram->capacity += sizeof(ui16);
							/*ram->offset = ram->capacity;*/
						}
					}

					char buff_w[buff_update];
					memset(buff_w,0,buff_update);

					strncpy(buff_w, rec->fields[i].data.s, new_lt - 1);
					/*
					 * if we did not move to another position
					 * set the file pointer back to the begginning of the string record
					 * to overwrite the data accordingly
					 * */
					if (str_loc == 0 && ((new_lt - buff_update) <= 0) && eof == 0){
						ram->offset = af_str_loc_pos;
					} else if (str_loc > 0 && ((new_lt - buff_update) <= 0) && eof == 0){
						ram->offset = move_to;
					}

					/*
					 * write the data to file --
					 * the file pointer is always pointing to the
					 * right position at this point */
					bu_ne = swap16((ui16)buff_update);

					if(eof == 0)
						memcpy(&ram->mem[ram->offset],&bu_ne,sizeof(ui16));
					else 
						memcpy(&ram->mem[ram->size],&bu_ne,sizeof(ui16));


					move_ram_file_ptr(ram,sizeof(ui16));
					memcpy(&ram->mem[ram->offset],buff_w,buff_update);
					move_ram_file_ptr(ram,buff_update);

					/*
					 * if eof is bigger than 0 means we updated the string
					 * we need to save the file_offset of the new written data
					 * at the start of the original.
					 * */
					if (eof > 0){
						/*go at the beginning of the str record*/
						ram->offset = bg_pos;

						/*update new string position*/
						ui32 eof_ne = swap32((ui32)eof);
						memcpy(&ram->mem[ram->offset],&eof_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*set file pointer to the end of the 1st string rec*/
						/*this step is crucial to avoid losing data        */

						ram->offset = pos_after_first_str_record;
					}else if (str_loc > 0){
						/*
						 * Make sure that in all cases
						 * we go back to the end of the 1st record
						 * */
						ram->offset = pos_after_first_str_record;
					}
				}
				break;	
			}
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
			{
				if(!update){

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set, sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 place_holder = 0;
					memcpy(&ram->mem[ram->size],&place_holder, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 arr[rec->fields[i].data.v.size];
					memset(arr,0,sizeof(ui32) * rec->fields[i].data.v.size);

					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){
						arr[j] = swap32(rec->fields[i].data.v.elements.i[j]);
					} 

					memcpy(&ram->mem[ram->size],arr,sizeof(ui32) * rec->fields[i].data.v.size);
					ram->size += (sizeof(ui32) * rec->fields[i].data.v.size);
					ram->offset += (sizeof(ui32) * rec->fields[i].data.v.size);

					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += (sizeof(ui64));
					ram->offset += sizeof(ui64);
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						ram->offset += (sizeof(ui32) * s);
						ram->offset += sizeof(ui64);
						break;
					}

					/*write the set flag*/
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set, sizeof(ui8));
					ram->offset++;

					do{
						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int) swap32(pd_ne);


						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__)
							if((array_last = is_array_last_block(-1,ram,sz,sizeof(int),TYPE_INT)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,sizeof(int),TYPE_INT)) == -1)
#endif
							{

								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->offset += sizeof(ui32);
									step++;
								}
								sz--;
							}

							if(exit){
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;	
							}
						} else {

							/* 
							 * in the first iteration in the 
							 * do-while loop this will
							 * overwrite the array on file
							 * */

							int exit = 0;
							for(k = 0; k < sz ; k++){
								if(step > 0 && k == 0 ){
									if((step + sz) > rec->fields[i].data.v.size){
										int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
										if((array_last = is_array_last_block(-1,ram,sz,sizeof(int),TYPE_INT)) == -1)
#elif defined(_WIN32)
										if((array_last = is_array_last_block(NULL,ram,sz,sizeof(int),TYPE_INT)) == -1)
#endif
										{
											fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
											return -1;
										}

										if(rec->fields[i].data.v.size < (sz + step) && array_last){
											padding += (sz - (rec->fields[i].data.v.size - step));

											sz = rec->fields[i].data.v.size - step;
											exit = 1;
											ram->offset -= (2*sizeof(ui32));
											/*write the update size of the array*/
											ui32 new_sz_ne = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write the padding*/
											ui32 pd_ne = swap32((ui32)padding);
											memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}
								}

								if(step < rec->fields[i].data.v.size){
									if(!rec->fields[i].data.v.elements.i[step]) continue;

									ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->offset += sizeof(ui32);

									step++;
									if(!(step < rec->fields[i].data.v.size)) exit = 0;
								}
							}

							if(exit){
								if(padding > 0) ram->offset += (padding * sizeof(int));

								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;/*from the main do-while loop*/
							}
						}

						if(padding > 0) ram->offset += (sizeof(int) * padding);

						ui64 update_off_ne = 0;
						file_offset go_back_to = ram->offset;

						memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
						ram->offset += sizeof(ui64);

						if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

						update_pos = (file_offset) swap64(update_off_ne);
						/*
						 * if the update_pos is == 0 it means 
						 * we need to move at the end of the file and write the remaining 
						 * element of the array
						 *
						 * */
						if(update_pos == 0){
							/*go to EOF*/	
							update_pos = ram->size;
							ram->offset = ram->size;

							ui32 size_left = rec->fields[i].data.v.size - step;
							/*
							 * compute the size that we need to write
							 * remeber that each array record is:
							 * 	- ui32 sz;
							 * 	- ui32 padding;
							 * 	- sizeof(each element) * sz;
							 * 	- ui64 update_pos;
							 *
							 * 	we account for 2 ui64 due to the whole record structure
							 * */
							ui64 remaining_write_size = ( (2 * sizeof(ui32)) + (size_left * sizeof(int)) + sizeof(ui64));
							if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
								/*you have to expand the capacity*/
								ui8 *n_mem = (ui8*)realloc(ram->mem, ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
								if(!n_mem){
									fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
									return -1;
								}
								ram->mem = n_mem;
								ram->capacity += (remaining_write_size + 1);
								memset(&ram->mem[ram->offset],0,remaining_write_size +1);
							}


							ui32 sz_left_ne = swap32(size_left);
							memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 pd_ne = 0;
							memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 j;
							for(j = 0; j < size_left; j++){
								if(step < rec->fields[i].data.v.size){
									if(!rec->fields[i].data.v.elements.i[step]) continue;

									ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->size += sizeof(ui32);
									ram->offset = ram->size;

									step++;
								}
							}
							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->size += sizeof(ui64);
							ram->offset = ram->size;

							/*we need to write this rec position in the old rec*/
							ram->offset = go_back_to;
							update_off_ne = swap64((ui64)update_pos);
							memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);


							break;
						}

						ram->offset = update_pos;

					}while(update_pos > 0);

					if(rec->fields[i].data.v.size < sz){
						ram->offset -= sizeof(ui32);

						padding = sz - rec->fields[i].data.v.size;
						ui32 p_ne = swap32((ui32)padding);
						ui32 sz_ne = swap32(rec->fields[i].data.v.size);
						memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*write the padding*/
						memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						int j;
						for(j = step; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){
								if(!rec->fields[i].data.v.elements.i[j]) continue;

								ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[j]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);
								step++;
							}
						}
						ram->offset += (padding * sizeof(int));
						ui64 up_pos_ne = 0;

						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);


					}else if(rec->fields[i].data.v.size == sz){

						if(step > 0){

							ram->offset -= sizeof(ui32);

							int size_left = rec->fields[i].data.v.size - step;

							ui32 sz_ne = swap32((ui32)size_left);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);
						}

						/* read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);
						int j;		
						for(j = 0; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){

								ui32 num_ne = swap32(rec->fields[i].data.v.elements.i[step]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);
								step++;
							}
						}

						if(padding > 0)	ram->offset += (padding * sizeof(ui32));
						/*ram->offset += sizeof(ui32);*/

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}

					if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
				}
				break;
			}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
			{
				if(!update){
					/*write the set flag*/
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set, sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 pad = 0;
					memcpy(&ram->mem[ram->size],&pad, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui64 arr[rec->fields[i].data.v.size];
					memset(arr,0,sizeof(ui64) * rec->fields[i].data.v.size);
					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){
						arr[j] = swap64(rec->fields[i].data.v.elements.l[j]);
					} 

					memcpy(&ram->mem[ram->size],arr,sizeof(ui64) * rec->fields[i].data.v.size);
					ram->size += (sizeof(ui64) * rec->fields[i].data.v.size);
					ram->offset += (sizeof(ui64) * rec->fields[i].data.v.size);

					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += (sizeof(ui64));
					ram->offset += sizeof(ui64);
					break;
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						ram->offset += (sizeof(ui64) * s);
						ram->offset += sizeof(ui64);
						break;
					}
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set,sizeof(ui8));
					ram->offset++;


					do{

						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
							if((array_last = is_array_last_block(-1,ram,sz,sizeof(long),TYPE_INT)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,sizeof(long),TYPE_INT)) == -1)
#endif

							{
								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									step++;
								}
								sz--;
							}

							if(exit){
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;	
							}
						} else {

							/* 
							 * in the first iteration in the 
							 * do-while loop this will
							 * overwrite the array on file
							 * */

							int exit = 0;
							for(k=0;k < sz; k++){
								if(step > 0 && k == 0 ){
									if((step + sz) > rec->fields[i].data.v.size){
										int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
										if((array_last = is_array_last_block(-1,ram,sz,sizeof(long),TYPE_INT)) == -1)
#elif defined(_WIN32)
										if((array_last = is_array_last_block(NULL,ram,sz,sizeof(long),TYPE_INT)) == -1)

#endif
										{
											fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
											return -1;
										}

										if(rec->fields[i].data.v.size < (sz + step) && array_last){
											padding += (sz - (rec->fields[i].data.v.size - step));

											sz = rec->fields[i].data.v.size - step;
											exit = 1;
											ram->offset -= (2*sizeof(ui32));
											/*write the update size of the array*/
											ui32 new_sz_ne = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write the padding*/
											ui32 pd_ne = swap32((ui32)padding);
											memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}
								}

								if(step < rec->fields[i].data.v.size){
									if(!rec->fields[i].data.v.elements.l[step]) continue;

									ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									step++;

									if(!(step < rec->fields[i].data.v.size)) exit = 0;
								}
							}

							if(exit){
								if(padding > 0) ram->offset += (padding * sizeof(long));

								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;/*from the main do-while loop*/
							}
						}

						if(padding > 0) ram->offset += (sizeof(long) * padding);

						ui64 update_off_ne = 0;
						file_offset go_back_to = ram->offset;

						memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
						ram->offset += sizeof(ui64);

						if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

						update_pos = (file_offset) swap64(update_off_ne);
						/*
						 * if the update_pos is == 0 it means 
						 * we need to move at the end of the file and write the remaining 
						 * element of the array
						 *
						 * */
						if(update_pos == 0){
							/*go to EOF*/	
							update_pos = ram->size;
							ram->offset = ram->size;

							ui32 size_left = rec->fields[i].data.v.size - step;
							/*
							 * compute the size that we need to write
							 * remeber that each array record is:
							 * 	- ui32 sz;
							 * 	- ui32 padding;
							 * 	- sizeof(each element) * sz;
							 * 	- ui64 update_pos;
							 *
							 * 	we account for 2 ui64 due to the whole record structure
							 * */
							ui64 remaining_write_size = ( (2 * sizeof(ui32)) + (size_left * sizeof(long)) + sizeof(ui64));
							if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
								/*you have to expand the capacity*/
								ui8 *n_mem = (ui8*) realloc(ram->mem,ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
								if(!n_mem){
									fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
									return -1;
								}
								ram->mem = n_mem;
								ram->capacity += (remaining_write_size + 1);
								memset(&ram->mem[ram->offset],0,remaining_write_size +1);
							}


							ui32 sz_left_ne = swap32(size_left);
							memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 pd_ne = 0;
							memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 j;
							for(j = 0; j < size_left; j++){
								if(step < rec->fields[i].data.v.size){
									ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->size += sizeof(ui64);
									ram->offset = ram->size;

									step++;
								}
							}
							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->size += sizeof(ui64);
							ram->offset = ram->size;

							/*we need to write this rec position in the old rec*/
							ram->offset = go_back_to;
							update_off_ne = swap64((ui64)update_pos);
							memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);

							break;
						}

						ram->offset = update_pos;

					}while(update_pos > 0);

					if(rec->fields[i].data.v.size < sz){
						ram->offset -= sizeof(ui32);

						padding = sz - rec->fields[i].data.v.size;
						ui32 p_ne = swap32((ui32)padding);
						ui32 sz_ne = swap32(rec->fields[i].data.v.size);
						memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*write the padding*/
						memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						int j;
						for(j = step; j <rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){

								ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[j]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								step++;
							}
						}
						ram->offset += (padding * sizeof(long));

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}else if(rec->fields[i].data.v.size == sz){

						if(step > 0){

							ram->offset -= sizeof(ui32);
							int size_left = rec->fields[i].data.v.size - step;

							ui32 sz_ne = swap32((ui32)size_left);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);
						}
						/* read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						int j;
						for(j = 0; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){
								ui64 num_ne = swap64(rec->fields[i].data.v.elements.l[step]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								step++;
							}
						}

						if(padding > 0)	ram->offset += (padding * sizeof(long));

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}

					if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
				}
				break;

			}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
			{
				if(!update){

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set,sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 place_holder = 0;
					memcpy(&ram->mem[ram->size],&place_holder, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui8 arr[rec->fields[i].data.v.size];
					memset(arr,0,sizeof(ui8) * rec->fields[i].data.v.size);

					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){
						arr[j] = rec->fields[i].data.v.elements.b[j];
					} 

					memcpy(&ram->mem[ram->size],arr,sizeof(ui8) * rec->fields[i].data.v.size);
					ram->size += (sizeof(ui8) * rec->fields[i].data.v.size);
					ram->offset += (sizeof(ui8) * rec->fields[i].data.v.size);

					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += (sizeof(ui64));
					ram->offset += sizeof(ui64);
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						ram->offset += (sizeof(ui8) * s);
						ram->offset += sizeof(ui64);
						break;
					}

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set,sizeof(ui8));
					ram->offset++;

					do{

						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
							if((array_last = is_array_last_block(-1,ram,sz,sizeof(unsigned char),0)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,sizeof(unsigned char),0)) == -1)
#endif

							{
								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									ui8 num_ne = rec->fields[i].data.v.elements.b[step];
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui8));
									ram->offset += sizeof(ui8);
									step++;
								}
								sz--;
							}

							if(exit){
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;	
							}
						} else {

							/* 
							 * in the first iteration in the 
							 * do-while loop this will
							 * overwrite the array on file
							 * */

							int exit = 0;
							for(k = 0;k < sz; k++){
								if(step > 0 && k == 0 ){
									if((step + sz) > rec->fields[i].data.v.size){
										int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
										if((array_last = is_array_last_block(-1,ram,sz,
														sizeof(unsigned char),
														0)) == -1)
#elif defined(_WIN32)
										if((array_last = is_array_last_block(NULL,ram,sz,
														sizeof(unsigned char),
														0)) == -1)
#endif

										{
											fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
											return -1;
										}

										if(rec->fields[i].data.v.size < (sz + step) && array_last){
											padding += (sz - (rec->fields[i].data.v.size - step));

											sz = rec->fields[i].data.v.size - step;
											exit = 1;
											ram->offset -= (2*sizeof(ui32));
											/*write the update size of the array*/
											ui32 new_sz_ne = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write the padding*/
											ui32 pd_ne = swap32((ui32)padding);
											memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}
								}

								if(step < rec->fields[i].data.v.size){
									ui8 num_ne = rec->fields[i].data.v.elements.b[step];
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui8));
									ram->offset += sizeof(ui8);

									step++;
									if(!(step < rec->fields[i].data.v.size)) exit = 0;
								}
							}

							if(exit){
								if(padding > 0) ram->offset += (padding * sizeof(ui8));

								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;/*from the main do-while loop*/
							}
						}

						if(padding > 0) ram->offset += (sizeof(ui8) * padding);

						ui64 update_off_ne = 0;
						file_offset go_back_to = ram->offset;

						memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
						ram->offset += sizeof(ui64);

						if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

						update_pos = (file_offset) swap64(update_off_ne);
						/*
						 * if the update_pos is == 0 it means 
						 * we need to move at the end of the file and write the remaining 
						 * element of the array
						 *
						 * */
						if(update_pos == 0){
							/*go to EOF*/	
							update_pos = ram->size;
							ram->offset = ram->size;

							ui32 size_left = rec->fields[i].data.v.size - step;
							/*
							 * compute the size that we need to write
							 * remeber that each array record is:
							 * 	- ui32 sz;
							 * 	- ui32 padding;
							 * 	- sizeof(each element) * sz;
							 * 	- ui64 update_pos;
							 *
							 * 	we account for 2 ui64 due to the whole record structure
							 * */
							ui64 remaining_write_size = ( (2 * sizeof(ui32)) + (size_left * sizeof(ui8)) + sizeof(ui64));
							if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
								/*you have to expand the capacity*/
								ui8 *n_mem = (ui8*)realloc(ram->mem,
										ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
								if(!n_mem){
									fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
									return -1;
								}
								ram->mem = n_mem;
								ram->capacity += (remaining_write_size + 1);
								memset(&ram->mem[ram->offset],0,remaining_write_size +1);
							}


							ui32 sz_left_ne = swap32(size_left);
							memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 pd_ne = 0;
							memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 j;
							for(j = 0; j < size_left; j++){
								if(step < rec->fields[i].data.v.size){

									ui8 num_ne = rec->fields[i].data.v.elements.b[step];
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui8));
									ram->size += sizeof(ui8);
									ram->offset = ram->size;

									step++;
								}
							}
							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->size += sizeof(ui64);
							ram->offset = ram->size;

							/*we need to write this rec position in the old rec*/
							ram->offset = go_back_to;
							update_off_ne = swap64((ui64)update_pos);
							memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);
							break;
						}

						ram->offset = update_pos;

					}while(update_pos > 0);

					if(rec->fields[i].data.v.size < sz){
						ram->offset -= sizeof(ui32);

						padding = sz - rec->fields[i].data.v.size;
						ui32 p_ne = swap32((ui32)padding);
						ui32 sz_ne = swap32(rec->fields[i].data.v.size);
						memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*write the padding*/
						memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						int j;
						for(j = step; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){

								ui8 num_ne = rec->fields[i].data.v.elements.b[j];
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui8));
								ram->offset += sizeof(ui8);
								step++;
							}
						}
						ram->offset += (padding * sizeof(ui8));
						ui64 up_pos_ne = 0;

						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);


					}else if(rec->fields[i].data.v.size == sz){

						if(step > 0){

							ram->offset -= sizeof(ui32);

							int size_left = rec->fields[i].data.v.size - step;

							ui32 sz_ne = swap32((ui32)size_left);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);
						}

						/* read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);
						int j;		
						for(j = 0; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){

								ui8 num_ne = rec->fields[i].data.v.elements.b[step];
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui8));
								ram->offset += sizeof(ui8);
								step++;
							}
						}

						if(padding > 0)	ram->offset += (padding * sizeof(ui8));
						/*ram->offset += sizeof(ui32);*/

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}

					if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
				}
				break;
			}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
			{
				if(!update){

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set,sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 pad = 0;
					memcpy(&ram->mem[ram->size],&pad, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 arr[rec->fields[i].data.v.size];
					memset(arr,0,sizeof(ui32) * rec->fields[i].data.v.size);

					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){
						arr[j] = htonf(rec->fields[i].data.v.elements.f[j]);
					} 

					memcpy(&ram->mem[ram->size],arr,sizeof(ui32) * rec->fields[i].data.v.size);
					ram->size += (sizeof(ui32) * rec->fields[i].data.v.size);
					ram->offset += (sizeof(ui32) * rec->fields[i].data.v.size);

					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += (sizeof(ui64));
					ram->offset += sizeof(ui64);
					break;
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						ram->offset += (sizeof(ui32) * s);
						ram->offset += sizeof(ui64);
						break;
					}
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set,sizeof(ui8));
					ram->offset++;

					do{

						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
							if((array_last = is_array_last_block(-1,ram,sz,sizeof(float),TYPE_FLOAT)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,sizeof(float),TYPE_FLOAT)) == -1)
#endif
							{
								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->offset += sizeof(ui32);
									step++;
								}
								sz--;
							}

							if(exit){
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;	
							}
						} else {

							/* 
							 * in the first iteration in the 
							 * do-while loop this will
							 * overwrite the array on file
							 * */

							int exit = 0;
							for(k = 0;k < sz; k++){
								if(step > 0 && k == 0 ){
									if((step + sz) > rec->fields[i].data.v.size){
										int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
										if((array_last = is_array_last_block(-1,ram,sz,sizeof(float),TYPE_FLOAT)) == -1)
#elif defined(_WIN32)
										if((array_last = is_array_last_block(NULL,ram,sz,sizeof(float),TYPE_FLOAT)) == -1)
#endif
										{
											fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
											return -1;
										}

										if(rec->fields[i].data.v.size < (sz + step) && array_last){
											padding += (sz - (rec->fields[i].data.v.size - step));

											sz = rec->fields[i].data.v.size - step;
											exit = 1;
											ram->offset -= (2*sizeof(ui32));
											/*write the update size of the array*/
											ui32 new_sz_ne = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write the padding*/
											ui32 pd_ne = swap32((ui32)padding);
											memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}
								}

								if(step < rec->fields[i].data.v.size){
									ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->offset += sizeof(ui32);

									step++;
									if(!(step < rec->fields[i].data.v.size)) exit = 0;
								}
							}

							if(exit){
								if(padding > 0) ram->offset += (padding * sizeof(float));

								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;/*from the main do-while loop*/
							}
						}

						if(padding > 0) ram->offset += (sizeof(float) * padding);

						ui64 update_off_ne = 0;
						file_offset go_back_to = ram->offset;

						memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
						ram->offset += sizeof(ui64);

						if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

						update_pos = (file_offset) swap64(update_off_ne);
						/*
						 * if the update_pos is == 0 it means 
						 * we need to move at the end of the file and write the remaining 
						 * element of the array
						 *
						 * */
						if(update_pos == 0){
							/*go to EOF*/	
							update_pos = ram->size;
							ram->offset = ram->size;

							ui32 size_left = rec->fields[i].data.v.size - step;
							/*
							 * compute the size that we need to write
							 * remeber that each array record is:
							 * 	- ui32 sz;
							 * 	- ui32 padding;
							 * 	- sizeof(each element) * sz;
							 * 	- ui64 update_pos;
							 *
							 * 	we account for 2 ui64 due to the whole record structure
							 * */
							ui64 remaining_write_size = ( (2 * sizeof(ui32)) + (size_left * sizeof(float)) + sizeof(ui64));
							if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
								/*you have to expand the capacity*/
								ui8 *n_mem = (ui8*)realloc(ram->mem, 
										ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
								if(!n_mem){
									fprintf(stderr,"(%s): realloc() failed %s:%d.\n",prog,__FILE__,__LINE__-2);
									return -1;
								}
								ram->mem = n_mem;
								ram->capacity += (remaining_write_size + 1);
								memset(&ram->mem[ram->offset],0,remaining_write_size +1);
							}


							ui32 sz_left_ne = swap32(size_left);
							memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 pd_ne = 0;
							memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 j;
							for(j = 0; j < size_left; j++){
								if(step < rec->fields[i].data.v.size){
									ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
									ram->size += sizeof(ui32);
									ram->offset = ram->size;

									step++;
								}
							}
							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->size += sizeof(ui64);
							ram->offset = ram->size;

							/*we need to write this rec position in the old rec*/
							ram->offset = go_back_to;
							update_off_ne = swap64((ui64)update_pos);
							memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);


							break;
						}

						ram->offset = update_pos;

					}while(update_pos > 0);

					if(rec->fields[i].data.v.size < sz){
						ram->offset -= sizeof(ui32);

						padding = sz - rec->fields[i].data.v.size;
						ui32 p_ne = swap32((ui32)padding);
						ui32 sz_ne = swap32(rec->fields[i].data.v.size);
						memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*write the padding*/
						memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						int j;
						for(j = step; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){
								ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[j]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);
								step++;
							}
						}
						ram->offset += (padding * sizeof(float));
						ui64 up_pos_ne = 0;

						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);


					}else if(rec->fields[i].data.v.size == sz){

						if(step > 0){

							ram->offset -= sizeof(ui32);

							int size_left = rec->fields[i].data.v.size - step;

							ui32 sz_ne = swap32((ui32)size_left);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);
						}

						/* read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						int j;
						for(j = 0; j < rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){
								ui32 num_ne = htonf(rec->fields[i].data.v.elements.f[step]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);
								step++;
							}
						}

						if(padding > 0)	ram->offset += (padding * sizeof(float));

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}

					if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
				}
				break;

			}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
			{
				if(!update){

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set,sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 pad = 0;
					memcpy(&ram->mem[ram->size],&pad, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui64 arr[rec->fields[i].data.v.size];
					memset(arr,0,sizeof(ui64) * rec->fields[i].data.v.size);
					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){
						arr[j] = htond(rec->fields[i].data.v.elements.d[j]);
					} 

					memcpy(&ram->mem[ram->size],arr,sizeof(ui64) * rec->fields[i].data.v.size);
					ram->size += (sizeof(ui64) * rec->fields[i].data.v.size);
					ram->offset += (sizeof(ui64) * rec->fields[i].data.v.size);

					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += (sizeof(ui64));
					ram->offset += sizeof(ui64);
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						ram->offset += (sizeof(ui64) * s);
						ram->offset += sizeof(ui64);
						break;
					}
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set,sizeof(ui8));
					ram->offset++;

					do{

						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
							if((array_last = is_array_last_block(-1,ram,sz,sizeof(double),TYPE_DOUBLE)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,sizeof(double),TYPE_DOUBLE)) == -1)
#endif
							{
								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									step++;
								}
								sz--;
							}

							if(exit){
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;	
							}
						} else {

							/* 
							 * in the first iteration in the 
							 * do-while loop this will
							 * overwrite the array on file
							 * */

							int exit = 0;
							for(k = 0;k < sz; k++){
								if(step > 0 && k == 0 ){
									if((step + sz) > rec->fields[i].data.v.size){
										int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
										if((array_last = is_array_last_block(-1,ram,sz,sizeof(double),0)) == -1)
#elif defined(_WIN32)
										if((array_last = is_array_last_block(NULL,ram,sz,sizeof(double),0)) == -1)
#endif
										{
											fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
											return -1;
										}

										if(rec->fields[i].data.v.size < (sz + step) && array_last){
											padding += (sz - (rec->fields[i].data.v.size - step));

											sz = rec->fields[i].data.v.size - step;
											exit = 1;
											ram->offset -= (2*sizeof(ui32));
											/*write the update size of the array*/
											ui32 new_sz_ne = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write the padding*/
											ui32 pd_ne = swap32((ui32)padding);
											memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}
								}

								if(step < rec->fields[i].data.v.size){
									ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									step++;

									if(!(step < rec->fields[i].data.v.size)) exit = 0;
								}
							}

							if(exit){
								if(padding > 0) ram->offset += (padding * sizeof(double));

								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								break;/*from the main do-while loop*/
							}
						}

						if(padding > 0) ram->offset += (sizeof(double) * padding);

						ui64 update_off_ne = 0;
						file_offset go_back_to = ram->offset;

						memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
						ram->offset += sizeof(ui64);

						if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

						update_pos = (file_offset) swap64(update_off_ne);
						/*
						 * if the update_pos is == 0 it means 
						 * we need to move at the end of the file and write the remaining 
						 * element of the array
						 *
						 * */
						if(update_pos == 0){
							/*go to EOF*/	
							update_pos = ram->size;
							ram->offset = ram->size;

							ui32 size_left = rec->fields[i].data.v.size - step;
							/*
							 * compute the size that we need to write
							 * remeber that each array record is:
							 * 	- ui32 sz;
							 * 	- ui32 padding;
							 * 	- sizeof(each element) * sz;
							 * 	- ui64 update_pos;
							 *
							 * */
							ui64 remaining_write_size = ( (2 * sizeof(ui32)) + (size_left * sizeof(double)) + sizeof(ui64));
							if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
								/*you have to expand the capacity*/
								ui8 *n_mem = (ui8*)realloc(ram->mem, 
										ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
								if(!n_mem){
									fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
									return -1;
								}

								ram->mem = n_mem;
								ram->capacity += (remaining_write_size + 1);
								memset(&ram->mem[ram->offset],0,remaining_write_size +1);
							}


							ui32 sz_left_ne = swap32(size_left);
							memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 pd_ne = 0;
							memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
							ram->size += sizeof(ui32);
							ram->offset = ram->size;

							ui32 j;
							for(j = 0; j < size_left; j++){
								if(step < rec->fields[i].data.v.size){
									ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
									memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
									ram->size += sizeof(ui64);
									ram->offset = ram->size;

									step++;
								}
							}
							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->size += sizeof(ui64);
							ram->offset = ram->size;

							/*we need to write this rec position in the old rec*/
							ram->offset = go_back_to;
							update_off_ne = swap64((ui64)update_pos);
							memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);

							break;
						}

						ram->offset = update_pos;

					}while(update_pos > 0);

					if(rec->fields[i].data.v.size < sz){
						ram->offset -= sizeof(ui32);

						padding = sz - rec->fields[i].data.v.size;
						ui32 p_ne = swap32((ui32)padding);
						ui32 sz_ne = swap32(rec->fields[i].data.v.size);
						memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						/*write the padding*/
						memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
						ram->offset += sizeof(ui32);

						int j;
						for(j = step; j <rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){

								ui64 num_ne = htond(rec->fields[i].data.v.elements.d[j]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								step++;
							}
						}
						ram->offset += (padding * sizeof(long));

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}else if(rec->fields[i].data.v.size == sz){

						if(step > 0){

							ram->offset -= sizeof(ui32);
							int size_left = rec->fields[i].data.v.size - step;

							ui32 sz_ne = swap32((ui32)size_left);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);
						}
						/* read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						int j;
						for(j = 0; j< rec->fields[i].data.v.size; j++){
							if(step < rec->fields[i].data.v.size){
								ui64 num_ne = htond(rec->fields[i].data.v.elements.d[step]);
								memcpy(&ram->mem[ram->offset],&num_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);
								step++;
							}
						}

						if(padding > 0)	ram->offset += (padding * sizeof(double));

						ui64 up_pos_ne = 0;
						memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
						ram->offset += sizeof(ui64);
					}

					if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
				}

				break;
			}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
			{
				if(!update){

					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->size],&set,sizeof(ui8));
					ram->size++;
					ram->offset++;

					ui32 sz = swap32(rec->fields[i].data.v.size);
					memcpy(&ram->mem[ram->size],&sz, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					ui32 pad = 0;
					memcpy(&ram->mem[ram->size],&pad, sizeof(ui32));
					ram->size += sizeof(ui32);
					ram->offset += sizeof(ui32);

					int j;
					for(j = 0; j < rec->fields[i].data.v.size; j++){

						ui16 l = (ui16)strlen(rec->fields[i].data.v.elements.s[j]);
						ui16 buf_up_ne = swap16((l*2)+1);	
						ui32 str_loc = 0;	

						memcpy(&ram->mem[ram->size],&str_loc, sizeof(ui32));
						ram->size += sizeof(ui32);
						ram->offset += sizeof(ui32);

						memcpy(&ram->mem[ram->size],&buf_up_ne, sizeof(ui16));
						ram->size += sizeof(ui16);
						ram->offset += sizeof(ui16);
						char buff[(l * 2) + 1];
						memset(buff,0,(l * 2) +1);
						strncpy(buff,rec->fields[i].data.v.elements.s[j],l);
						memcpy(&ram->mem[ram->size],buff,(l * 2) + 1);
						ram->size += (( l * 2) + 1);
						ram->offset += (( l * 2) + 1);

					}
					ui64 upd = 0;	
					memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
					ram->size += sizeof(ui64);
					ram->offset += sizeof(ui64);
					break;
				}else{
					file_offset update_pos = 0;
					file_offset go_back_to_first_rec = 0;
					int sz = 0;
					int k = 0;
					int step = 0;
					int padding = 0;

					if(rec->fields[i].is_dropped){
						/* skip the record
						 * 1 byte set flag
						 * 4 bytes size
						 * 4 bytees padding
						 * sz * sizeof(array element)
						 * 8 byte for the pointer to a possible update position*/
						ram->offset += 1;
						ui32 s = 0;
						memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += (sizeof(ui32)*2);

						s = swap32(s);

						int j;
						for(j = 0; j < rec->fields[i].data.v.size; j++){

							ram->offset += sizeof(ui32);

							ui16 buf_up = 0;
							memcpy(&buf_up, &ram->mem[ram->size],sizeof(ui16));
							ram->offset += sizeof(ui16);
							buf_up = swap16(buf_up);
							ram->offset += buf_up;

						}

						ram->offset += sizeof(ui64);
						break;
					}
					ui8 set = (ui8) rec->fields[i].data.v.is_set;
					memcpy(&ram->mem[ram->offset],&set,sizeof(ui8));
					ram->offset++;

					do{


						/*check size of the array on file*/
						ui32 sz_ne = 0; 
						memcpy(&sz_ne,&ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						sz = (int)swap32(sz_ne);

						if(rec->fields[i].data.v.size < sz || rec->fields[i].data.v.size == sz) break;

						/* 
						 * the program reach this branch only if the size of 
						 * the new array in the record is bigger than what we already have on file
						 * so we need to extend the array on file.
						 * */

						/*read padding value*/
						ui32 pd_ne = 0;
						memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
						ram->offset += sizeof(ui32);
						padding = (int)swap32(pd_ne);

						/*
						 * this is never true at the first iteration
						 * */
						if(step >= sz){

							int exit = 0;
							int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
							if((array_last = is_array_last_block(-1,ram,sz,0,rec->fields[i].type)) == -1)
#elif defined(_WIN32)
							if((array_last = is_array_last_block(NULL,ram,sz,0,rec->fields[i].type)) == -1)
#endif
							{

								fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
								return -1;
							}

							if(rec->fields[i].data.v.size < (sz + step) && array_last){
								padding += (sz - (rec->fields[i].data.v.size - step));

								sz = rec->fields[i].data.v.size - step;
								exit = 1;
								ram->offset -= (2*sizeof(ui32));
								/*write the update size of the array*/
								ui32 new_sz_ne = swap32((ui32)sz);
								memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

								/*write the padding*/
								ui32 pd_ne = swap32((ui32)padding);
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);

							}else if(rec->fields[i].data.v.size == (sz + step) && array_last){
								exit = 1;
							}

							while(sz){
								if(step < rec->fields[i].data.v.size){
									/*write the string*/
									ui64 move_to = 0;
									ui32 eof = 0;
									ui16 bu_ne = 0;
									ui16 new_lt = 0;
									/*save the starting offset for the string record*/
									ui64 bg_pos = ram->size;
									ui32 str_loc_ne= 0;

									/*read the other str_loc if any*/
									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
									else
										memcpy(&str_loc_ne,&ram->mem[ram->size],sizeof(ui32));


									ram->offset += sizeof(ui32);
									ui32 str_loc = swap32(str_loc_ne);

									/* save pos where the data starts*/
									ui64 af_str_loc_pos = ram->size;
									if(ram->size == ram->capacity && ram->offset != ram->size)
										af_str_loc_pos = ram->offset;
									else
										af_str_loc_pos = ram->size;

									ui16 buff_update_ne = 0;

									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&buff_update_ne,&ram->mem[ram->offset],sizeof(ui16));
									else
										memcpy(&buff_update_ne,&ram->mem[ram->size],sizeof(ui16));

									ram->offset += sizeof(ui16);

									ui16 buff_update = swap16(buff_update_ne);
									ui64 pos_after_first_str_record = ram->offset + buff_update; 
									if(ram->size == ram->capacity && ram->offset != ram->size)
										pos_after_first_str_record = ram->offset + buff_update; 
									else
										pos_after_first_str_record = ram->size + buff_update; 

									if (str_loc > 0){
										/*set the file pointer to str_loc*/
										ram->offset = str_loc;

										/*
										 * in the case of a regular buffer update we have
										 *  to save the file_offset to get back to it later
										 * */
										move_to = ram->offset; 

										ui16 bu_ne = 0;
										memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
										ram->offset +=  sizeof(ui16);	

										buff_update = (file_offset)swap16(bu_ne);
									}

									new_lt = strlen(rec->fields[i].data.s) + 1; /*get new str length*/

									if (new_lt > buff_update) {
										/*expand the buff_update only for the bytes needed*/
										buff_update += (new_lt - buff_update);

										/*
										 * if the new length is bigger then the buffer,
										 * set the file pointer to EOF to write the new data
										 * */
										eof = ram->size;
										ram->offset = eof;
										if(eof == ram->capacity){
											errno = 0;
											ui8 *n_mem = (ui8*)realloc(ram->mem,
													(ram->capacity + buff_update) * sizeof(ui8));
												if(!n_mem){
													fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
													return -1;
												}
												ram->mem = n_mem;
												ram->capacity += buff_update; 

											}else if((eof + buff_update) > ram->capacity){
												errno = 0;
												ui8 *n_mem = (ui8*)realloc(ram->mem,
														(ram->capacity + buff_update) * sizeof(ui8));
												if(!n_mem){
													fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
													return -1;
												}
												ram->mem = n_mem;
												ram->capacity += ((eof + buff_update) - ram->capacity); 
											}
										}

										char buff_w[buff_update];
										memset(buff_w,0,buff_update);

										strncpy(buff_w, rec->fields[i].data.s, new_lt - 1);
										/*
										 * if we did not move to another position
										 * set the file pointer back to the begginning of the string record
										 * to overwrite the data accordingly
										 * */
										if (str_loc == 0 && ((new_lt - buff_update) < 0)){
											ram->offset = af_str_loc_pos;
										} else if (str_loc > 0 && ((new_lt - buff_update) < 0)){
											ram->offset = move_to;
										}

										/*
										 * write the data to file --
										 * the file pointer is always pointing to the
										 * right position at this point */
										bu_ne = swap16((ui16)buff_update);

										if(eof == 0)
											memcpy(&ram->mem[ram->offset],&bu_ne,sizeof(ui16));
										else 
											memcpy(&ram->mem[ram->size],&bu_ne,sizeof(ui16));


										ram->offset += sizeof(ui16);
										memcpy(&ram->mem[ram->offset],buff_w,buff_update);
										ram->offset += sizeof(buff_w);

										/*
										 * if eof is bigger than 0 means we updated the string
										 * we need to save the file_offset of the new written data
										 * at the start of the original.
										 * */
										if (eof > 0){
											/*go at the beginning of the str record*/
											ram->offset = bg_pos;

											/*update new string position*/
											ui32 eof_ne = swap32((ui32)eof);
											memcpy(&ram->mem[ram->offset],&eof_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*set file pointer to the end of the 1st string rec*/
											/*this step is crucial to avoid losing data        */

											ram->offset = pos_after_first_str_record;
										}else if (str_loc > 0){
											/*
											 * Make sure that in all cases
											 * we go back to the end of the 1st record
											 * */
											ram->offset = pos_after_first_str_record;
										}
										step++;
									}
									sz--;
								}

								if(exit){
									ui64 up_pos_ne = 0;
									memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									break;	
								}
							} else {

								/* 
								 * in the first iteration in the 
								 * do-while loop this will
								 * overwrite the array on file
								 * */

								int exit = 0;
								for(k = 0;k < sz; k++){
									if(step > 0 && k == 0 ){
										if((step + sz) > rec->fields[i].data.v.size){
											int array_last = 0;
#if defined(__linux__) || defined(__APPLE__) 
											if((array_last = is_array_last_block(-1,ram,sz,0,rec->fields[i].type)) == -1)
#elif defined(_WIN32)
											if((array_last = is_array_last_block(NULL,ram,sz,0,rec->fields[i].type)) == -1)
#endif
											{
												fprintf(stderr,"(%s): can't verify array last block %s:%d",prog,__FILE__,__LINE__-1);
												return -1;
											}

											if(rec->fields[i].data.v.size < (sz + step) && array_last){
												padding += (sz - (rec->fields[i].data.v.size - step));

												sz = rec->fields[i].data.v.size - step;
												exit = 1;
												ram->offset -= (2*sizeof(ui32));
												/*write the update size of the array*/
												ui32 new_sz_ne = swap32((ui32)sz);
												memcpy(&ram->mem[ram->offset],&new_sz_ne,sizeof(ui32));
												ram->offset += sizeof(ui32);

												/*write the padding*/
												ui32 pd_ne = swap32((ui32)padding);
												memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
												ram->offset += sizeof(ui32);
											}
										}
									}

									if(step < rec->fields[i].data.v.size){
										/*write string*/
										ui64 move_to = 0;
										ui32 eof = 0;
										ui16 bu_ne = 0;
										ui16 new_lt = 0;
										/*save the starting offset for the string record*/
										ui64 bg_pos = ram->size;
										ui32 str_loc_ne= 0;

										/*read the other str_loc if any*/
										if(ram->size == ram->capacity && ram->offset != ram->size)
											memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
										else
											memcpy(&str_loc_ne,&ram->mem[ram->size],sizeof(ui32));


										ram->offset += sizeof(ui32);
										ui32 str_loc = swap32(str_loc_ne);

										/* save pos where the data starts*/
										ui64 af_str_loc_pos = ram->size;
										if(ram->size == ram->capacity && ram->offset != ram->size)
											af_str_loc_pos = ram->offset;
										else
											af_str_loc_pos = ram->size;

										ui16 buff_update_ne = 0;

										if(ram->size == ram->capacity && ram->offset != ram->size)
											memcpy(&buff_update_ne,&ram->mem[ram->offset],sizeof(ui16));
										else
											memcpy(&buff_update_ne,&ram->mem[ram->size],sizeof(ui16));

										ram->offset += sizeof(ui16);

										ui16 buff_update = swap16(buff_update_ne);
										ui64 pos_after_first_str_record = ram->offset + buff_update; 
										if(ram->size == ram->capacity && ram->offset != ram->size)
											pos_after_first_str_record = ram->offset + buff_update; 
										else
											pos_after_first_str_record = ram->size + buff_update; 

										if (str_loc > 0){
											/*set the file pointer to str_loc*/
											ram->offset = str_loc;

											/*
											 * in the case of a regular buffer update we have
											 *  to save the file_offset to get back to it later
											 * */
											move_to = ram->offset; 

											ui16 bu_ne = 0;
											memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
											ram->offset +=  sizeof(ui16);	

											buff_update = (file_offset)swap16(bu_ne);
										}

										new_lt = strlen(rec->fields[i].data.v.elements.s[k]) + 1; /*get new str length*/

										if (new_lt > buff_update) {
											/*expand the buff_update only for the bytes needed*/
											buff_update += (new_lt - buff_update);

											/*
											 * if the new length is bigger then the buffer,
											 * set the file pointer to EOF to write the new data
											 * */
											eof = ram->size;
											ram->offset = eof;
											if(eof == ram->capacity){
												errno = 0;
												ui8 *n_mem = (ui8*)realloc(ram->mem,
														(ram->capacity + buff_update) * sizeof(ui8));
												if(!n_mem){
													fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
													return -1;
												}
												ram->mem = n_mem;
												ram->capacity += buff_update; 

											}else if((eof + buff_update) > ram->capacity){
												errno = 0;
												ui8 *n_mem = (ui8*)realloc(ram->mem,
														(ram->capacity + buff_update) * sizeof(ui8));
												if(!n_mem){
													fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
													return -1;
												}
												ram->mem = n_mem;
												ram->capacity += ((eof + buff_update) - ram->capacity); 
											}
										}

										char buff_w[buff_update];
										memset(buff_w,0,buff_update);

										strncpy(buff_w, rec->fields[i].data.v.elements.s[k], new_lt - 1);
										/*
										 * if we did not move to another position
										 * set the file pointer back to the begginning of the string record
										 * to overwrite the data accordingly
										 * */
										if (str_loc == 0 && ((new_lt - buff_update) < 0)){
											ram->offset = af_str_loc_pos;
										} else if (str_loc > 0 && ((new_lt - buff_update) < 0)){
											ram->offset = move_to;
										}

										/*
										 * write the data to file --
										 * the file pointer is always pointing to the
										 * right position at this point */
										bu_ne = swap16((ui16)buff_update);

										if(eof == 0)
											memcpy(&ram->mem[ram->offset],&bu_ne,sizeof(ui16));
										else 
											memcpy(&ram->mem[ram->size],&bu_ne,sizeof(ui16));


										ram->offset += sizeof(ui16);
										memcpy(&ram->mem[ram->offset],buff_w,buff_update);
										ram->offset += sizeof(buff_w);

										/*
										 * if eof is bigger than 0 means we updated the string
										 * we need to save the file_offset of the new written data
										 * at the start of the original.
										 * */
										if (eof > 0){
											/*go at the beginning of the str record*/
											ram->offset = bg_pos;

											/*update new string position*/
											ui32 eof_ne = swap32((ui32)eof);
											memcpy(&ram->mem[ram->offset],&eof_ne,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*set file pointer to the end of the 1st string rec*/
											/*this step is crucial to avoid losing data        */

											ram->offset = pos_after_first_str_record;
										}else if (str_loc > 0){
											/*
											 * Make sure that in all cases
											 * we go back to the end of the 1st record
											 * */
											ram->offset = pos_after_first_str_record;
										}

										step++;
										if(!(step < rec->fields[i].data.v.size)) exit = 0;
									}
								}

								if(exit){
									if (padding > 0) {
										int i;
										for(i = 0; i < padding; i++){
#if defined(__linux__) || defined(__APPLE__)
											if(get_string_size(-1,ram) == (size_t) -1)
#elif defined(_WIN32)
											if(get_string_size(NULL,ram) == (size_t) -1)
#endif
											{
												__er_file_pointer(F, L - 1);
												return 0;
											}
										}
									}


									ui64 up_pos_ne = 0;
									memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
									ram->offset += sizeof(ui64);
									break;/*from the main do-while loop*/
								}
							}

							if (padding > 0) {
								int i;
								for(i = 0; i < padding; i++){
#if defined(__linux__) || defined(__APPLE__)
									if(get_string_size(-1,ram) == (size_t) -1)
#elif defined(_WIN32)
									if(get_string_size(NULL,ram) == (size_t) -1)
#endif
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}
								}
							}

							ui64 update_off_ne = 0;
							file_offset go_back_to = ram->offset;

							memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
							ram->offset += sizeof(ui64);

							if(go_back_to_first_rec == 0) go_back_to_first_rec = ram->offset;

							update_pos = (file_offset) swap64(update_off_ne);
							/*
							 * if the update_pos is == 0 it means 
							 * we need to move at the end of the file and write the remaining 
							 * element of the array
							 *
							 * */
							if(update_pos == 0){
								/*go to EOF*/	
								update_pos = ram->size;
								ram->offset = ram->size;

								ui32 size_left = rec->fields[i].data.v.size - step;
								/*
								 * compute the size that we need to write
								 * remeber that each array record is:
								 * 	- ui32 sz;
								 * 	- ui32 padding;
								 * 	- sizeof(each element) * sz;
								 * 	- ui64 update_pos;
								 *
								 * 	we account for 1 ui64 due to the whole record structure
								 * */

								size_t each_str_size = 0;
								int x;
								for(x = step; x <  rec->fields[i].data.v.size; x++){
									each_str_size += (sizeof(ui32) + sizeof(ui16));
									each_str_size+= ((strlen(rec->fields[i].data.v.elements.s[x]) * 2) + 1);

								}

								ui64 remaining_write_size = ((2 * sizeof(ui32)) + 
										each_str_size 		+ 
										sizeof(ui64));

								if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
									/*you have to expand the capacity*/
									ui8 *n_mem = (ui8*)realloc(ram->mem, 
											ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
									if(!n_mem){
										fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
										return -1;
									}

									ram->mem = n_mem;
									ram->capacity += (remaining_write_size + 1);
									memset(&ram->mem[ram->offset],0,remaining_write_size +1);
								}


								ui32 sz_left_ne = swap32(size_left);
								memcpy(&ram->mem[ram->offset],&sz_left_ne,sizeof(ui32));
								ram->size += sizeof(ui32);
								ram->offset = ram->size;

								ui32 pd_ne = 0;
								memcpy(&ram->mem[ram->offset],&pd_ne,sizeof(ui32));
								ram->size += sizeof(ui32);
								ram->offset = ram->size;

								ui32 j;
								for(j = 0; j < size_left; j++){
									if(step < rec->fields[i].data.v.size){
										/* write the string*/
										ui16 l = (ui16)strlen(rec->fields[i].data.v.elements.s[j]);
										ui16 buf_up_ne = swap16((l*2)+1);	
										ui32 str_loc = 0;	

										memcpy(&ram->mem[ram->size],&str_loc, sizeof(ui32));
										ram->offset += sizeof(ui32);

										memcpy(&ram->mem[ram->size],&buf_up_ne, sizeof(ui16));
										ram->offset += sizeof(ui16);
										char buff[(l * 2) + 1];
										memset(buff,0,(l * 2) +1);
										strncpy(buff,rec->fields[i].data.v.elements.s[j],l);
										memcpy(&ram->mem[ram->size],buff,(l * 2) + 1);
										ram->offset += (( l * 2) + 1);


										step++;
									}
								}
								ui64 up_pos_ne = 0;
								memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
								ram->size += sizeof(ui64);
								ram->offset = ram->size;

								/*we need to write this rec position in the old rec*/
								ram->offset = go_back_to;
								update_off_ne = swap64((ui64)update_pos);
								memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);

								break;
							}

							ram->offset = update_pos;

						}while(update_pos > 0);

						if(rec->fields[i].data.v.size < sz){
							ram->offset -= sizeof(ui32);

							padding = sz - rec->fields[i].data.v.size;
							ui32 p_ne = swap32((ui32)padding);
							ui32 sz_ne = swap32(rec->fields[i].data.v.size);
							memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);

							/*write the padding*/
							memcpy(&ram->mem[ram->offset],&p_ne,sizeof(ui32));
							ram->offset += sizeof(ui32);

							int j;
							for(j = step; j <rec->fields[i].data.v.size; j++){
								if(step < rec->fields[i].data.v.size){

									/* write string*/
									ui64 move_to = 0;
									ui32 eof = 0;
									ui16 bu_ne = 0;
									ui16 new_lt = 0;
									/*save the starting offset for the string record*/
									ui64 bg_pos = ram->size;
									ui32 str_loc_ne= 0;

									/*read the other str_loc if any*/
									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
									else
										memcpy(&str_loc_ne,&ram->mem[ram->size],sizeof(ui32));


									ram->offset += sizeof(ui32);
									ui32 str_loc = swap32(str_loc_ne);

									/* save pos where the data starts*/
									ui64 af_str_loc_pos = ram->size;
									if(ram->size == ram->capacity && ram->offset != ram->size)
										af_str_loc_pos = ram->offset;
									else
										af_str_loc_pos = ram->size;

									ui16 buff_update_ne = 0;

									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&buff_update_ne,&ram->mem[ram->offset],sizeof(ui16));
									else
										memcpy(&buff_update_ne,&ram->mem[ram->size],sizeof(ui16));

									ram->offset += sizeof(ui16);

									ui16 buff_update = swap16(buff_update_ne);
									ui64 pos_after_first_str_record = ram->offset + buff_update; 
									if(ram->size == ram->capacity && ram->offset != ram->size)
										pos_after_first_str_record = ram->offset + buff_update; 
									else
										pos_after_first_str_record = ram->size + buff_update; 

									if (str_loc > 0){
										/*set the file pointer to str_loc*/
										ram->offset = str_loc;

										/*
										 * in the case of a regular buffer update we have
										 *  to save the file_offset to get back to it later
										 * */
										move_to = ram->offset; 

										ui16 bu_ne = 0;
										memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
										ram->offset +=  sizeof(ui16);	

										buff_update = (file_offset)swap16(bu_ne);
									}

									new_lt = strlen(rec->fields[i].data.v.elements.s[j]) + 1; /*get new str length*/

									if (new_lt > buff_update) {
										/*expand the buff_update only for the bytes needed*/
										buff_update += (new_lt - buff_update);

										/*
										 * if the new length is bigger then the buffer,
										 * set the file pointer to EOF to write the new data
										 * */
										eof = ram->size;
										ram->offset = eof;
										if(eof == ram->capacity){
											errno = 0;
											ui8 *n_mem = (ui8*)realloc(ram->mem,
													(ram->capacity + buff_update) * sizeof(ui8));
											if(!n_mem){
												fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
												return -1;
											}
											ram->mem = n_mem;
											ram->capacity += buff_update; 

										}else if((eof + buff_update) > ram->capacity){
											errno = 0;
											ui8 *n_mem = (ui8*)realloc(ram->mem,
													(ram->capacity + buff_update) * sizeof(ui8));
											if(!n_mem){
												fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
												return -1;
											}
											ram->mem = n_mem;
											ram->capacity += ((eof + buff_update) - ram->capacity); 
										}
									}

									char buff_w[buff_update];
									memset(buff_w,0,buff_update);

									strncpy(buff_w, rec->fields[i].data.v.elements.s[j], new_lt - 1);
									/*
									 * if we did not move to another position
									 * set the file pointer back to the begginning of the string record
									 * to overwrite the data accordingly
									 * */
									if (str_loc == 0 && ((new_lt - buff_update) < 0)){
										ram->offset = af_str_loc_pos;
									} else if (str_loc > 0 && ((new_lt - buff_update) < 0)){
										ram->offset = move_to;
									}

									/*
									 * write the data to file --
									 * the file pointer is always pointing to the
									 * right position at this point */
									bu_ne = swap16((ui16)buff_update);

									if(eof == 0)
										memcpy(&ram->mem[ram->offset],&bu_ne,sizeof(ui16));
									else 
										memcpy(&ram->mem[ram->size],&bu_ne,sizeof(ui16));


									ram->offset += sizeof(ui16);
									memcpy(&ram->mem[ram->offset],buff_w,buff_update);
									ram->offset += sizeof(buff_w);

									/*
									 * if eof is bigger than 0 means we updated the string
									 * we need to save the file_offset of the new written data
									 * at the start of the original.
									 * */
									if (eof > 0){
										/*go at the beginning of the str record*/
										ram->offset = bg_pos;

										/*update new string position*/
										ui32 eof_ne = swap32((ui32)eof);
										memcpy(&ram->mem[ram->offset],&eof_ne,sizeof(ui32));
										ram->offset += sizeof(ui32);

										/*set file pointer to the end of the 1st string rec*/
										/*this step is crucial to avoid losing data        */

										ram->offset = pos_after_first_str_record;
									}else if (str_loc > 0){
										/*
										 * Make sure that in all cases
										 * we go back to the end of the 1st record
										 * */
										ram->offset = pos_after_first_str_record;
									}

									step++;
								}
							}

							if (padding > 0) {
								int i;
								for(i = 0; i < padding; i++){
#if defined(__linux__) || defined(__APPLE__)
									if(get_string_size(-1,ram) == (size_t) -1)
#elif defined(_WIN32)
									if(get_string_size(NULL,ram) == (size_t) -1)
#endif
									{
										__er_file_pointer(F, L - 1);
										return 0;
									}
								}
							}

							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);
						}else if(rec->fields[i].data.v.size == sz){

							if(step > 0){

								ram->offset -= sizeof(ui32);
								int size_left = rec->fields[i].data.v.size - step;

								ui32 sz_ne = swap32((ui32)size_left);
								memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(ui32));
								ram->offset += sizeof(ui32);
							}
							/* read padding value*/
							ui32 pd_ne = 0;
							memcpy(&pd_ne, &ram->mem[ram->offset],sizeof(ui32));
							ram->offset += sizeof(ui32);
							padding = (int)swap32(pd_ne);

							int j;
							for(j = 0; j< rec->fields[i].data.v.size; j++){
								if(step < rec->fields[i].data.v.size){
									/* write string*/
									ui64 move_to = 0;
									ui32 eof = 0;
									ui16 bu_ne = 0;
									ui16 new_lt = 0;
									/*save the starting offset for the string record*/
									ui64 bg_pos = ram->size;
									ui32 str_loc_ne= 0;

									/*read the other str_loc if any*/
									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&str_loc_ne,&ram->mem[ram->offset],sizeof(ui32));
									else
										memcpy(&str_loc_ne,&ram->mem[ram->size],sizeof(ui32));


									ram->offset += sizeof(ui32);
									ui32 str_loc = swap32(str_loc_ne);

									/* save pos where the data starts*/
									ui64 af_str_loc_pos = ram->size;
									if(ram->size == ram->capacity && ram->offset != ram->size)
										af_str_loc_pos = ram->offset;
									else
										af_str_loc_pos = ram->size;

									ui16 buff_update_ne = 0;

									if(ram->size == ram->capacity && ram->offset != ram->size)
										memcpy(&buff_update_ne,&ram->mem[ram->offset],sizeof(ui16));
									else
										memcpy(&buff_update_ne,&ram->mem[ram->size],sizeof(ui16));

									ram->offset += sizeof(ui16);

									ui16 buff_update = swap16(buff_update_ne);
									ui64 pos_after_first_str_record = ram->offset + buff_update; 
									if(ram->size == ram->capacity && ram->offset != ram->size)
										pos_after_first_str_record = ram->offset + buff_update; 
									else
										pos_after_first_str_record = ram->size + buff_update; 

									if (str_loc > 0){
										/*set the file pointer to str_loc*/
										ram->offset = str_loc;

										/*
										 * in the case of a regular buffer update we have
										 *  to save the file_offset to get back to it later
										 * */
										move_to = ram->offset; 

										ui16 bu_ne = 0;
										memcpy(&bu_ne,&ram->mem[ram->offset],sizeof(ui16));
										ram->offset +=  sizeof(ui16);	

										buff_update = (file_offset)swap16(bu_ne);
									}

									new_lt = strlen(rec->fields[i].data.v.elements.s[j]) + 1; /*get new str length*/

									if (new_lt > buff_update) {
										/*expand the buff_update only for the bytes needed*/
										buff_update += (new_lt - buff_update);

										/*
										 * if the new length is bigger then the buffer,
										 * set the file pointer to EOF to write the new data
										 * */
										eof = ram->size;
										ram->offset = eof;
										if(eof == ram->capacity){
											errno = 0;
											ui8 *n_mem = (ui8*)realloc(ram->mem,
													(ram->capacity + buff_update) * sizeof(ui8));
											if(!n_mem){
												fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
												return -1;
											}
											ram->mem = n_mem;
											ram->capacity += buff_update; 

										}else if((eof + buff_update) > ram->capacity){
											errno = 0;
											ui8 *n_mem = (ui8*)realloc(ram->mem,
													(ram->capacity + buff_update) * sizeof(ui8));
											if(!n_mem){
												fprintf(stderr,"realloc failed with '%s', %s:%d.\n",strerror(errno),__FILE__,__LINE__-1);
												return -1;
											}
											ram->mem = n_mem;
											ram->capacity += ((eof + buff_update) - ram->capacity); 
										}
									}

									char buff_w[buff_update];
									memset(buff_w,0,buff_update);

									strncpy(buff_w, rec->fields[i].data.v.elements.s[j], new_lt - 1);
									/*
									 * if we did not move to another position
									 * set the file pointer back to the begginning of the string record
									 * to overwrite the data accordingly
									 * */
									if (str_loc == 0 && ((new_lt - buff_update) < 0)){
										ram->offset = af_str_loc_pos;
									} else if (str_loc > 0 && ((new_lt - buff_update) < 0)){
										ram->offset = move_to;
									}

									/*
									 * write the data to file --
									 * the file pointer is always pointing to the
									 * right position at this point */
									bu_ne = swap16((ui16)buff_update);

									if(eof == 0)
										memcpy(&ram->mem[ram->offset],&bu_ne,sizeof(ui16));
									else 
										memcpy(&ram->mem[ram->size],&bu_ne,sizeof(ui16));


									ram->offset += sizeof(ui16);
									memcpy(&ram->mem[ram->offset],buff_w,buff_update);
									ram->offset += sizeof(buff_w);

									/*
									 * if eof is bigger than 0 means we updated the string
									 * we need to save the file_offset of the new written data
									 * at the start of the original.
									 * */
									if (eof > 0){
										/*go at the beginning of the str record*/
										ram->offset = bg_pos;

										/*update new string position*/
										ui32 eof_ne = swap32((ui32)eof);
										memcpy(&ram->mem[ram->offset],&eof_ne,sizeof(ui32));
										ram->offset += sizeof(ui32);

										/*set file pointer to the end of the 1st string rec*/
										/*this step is crucial to avoid losing data        */

										ram->offset = pos_after_first_str_record;
									}else if (str_loc > 0){
										/*
										 * Make sure that in all cases
										 * we go back to the end of the 1st record
										 * */
										ram->offset = pos_after_first_str_record;
									}

									step++;
								}
							}

							if(padding > 0)	ram->offset += (padding * sizeof(long));

							ui64 up_pos_ne = 0;
							memcpy(&ram->mem[ram->offset],&up_pos_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);
						}

						if(go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
					}

					break;
				}
			case TYPE_FILE:
#if 0
				{
					if(!update){
						ui32 size = swap32(rec->fields[i].data.file.count);
						memcpy(&ram->mem[ram->size],&size,sizeof(ui32));
						ram->size += sizeof(ui32);
						ram->offset += sizeof(ui32);

						/*pad*/
						ui32 pad = 0;
						memcpy(&ram->mem[ram->size],&pad,sizeof(ui32));
						ram->size += sizeof(ui32);
						ram->offset += sizeof(ui32);

						ui32 j;
						for(j = 0; j < rec->fields[i].data.file.count; j++){
							if(write_ram_record(ram,&rec->fields[i].data.file.recs[j],update,init_ram_size,offset) == -1){
								fprintf(stderr,"cannot write record to ram. %s:%d.\n", __FILE__,__LINE__ - 1);
								return -1;
							}
						}

						ui64 upd = 0;	
						memcpy(&ram->mem[ram->size],&upd,sizeof(ui64));
						ram->size += sizeof(ui64);
						ram->offset += sizeof(ui64);
					}else{

						if(rec->fields[i].is_dropped){
							/* skip the record
							 * 4 bytes size
							 * 4 bytes padding
							 * sz * sizeof(array element)
							 * 8 byte for the pointer to a possible update position*/
							ui32 s = 0;
							memcpy(&s,&ram->mem[ram->offset],sizeof(ui32));
							ram->offset += (sizeof(ui32)*2);

							s = swap32(s);

							ui32 j;
							for(j = 0; j < s;j++){
								long long sz = 0;
								if ((sz = get_disk_size_record(&rec->fields[i].data.file.recs[j])) == -1){
									fprintf(stderr,"cannot skip record in the ram. %s:%d.\n", __FILE__,__LINE__ - 1);
									return -1;
								}
								ram->offset += sz;
							}

							ram->offset += sizeof(ui64);
							break;
						}

						/*open the schema file*/
						/*create file name*/
						size_t file_name_length = strlen(rec->fields[i].field_name) + strlen(".sch");
						char file_name[file_name_length + 1];
						memset(file_name,0,file_name_length + 1);
						strncpy(file_name,rec->fields[i].field_name,strlen(rec->fields[i].field_name));
						strncat(file_name,".sch",strlen(".sch")+1);

						int fd_schema = open(file_name,0);
						if (fd_schema == -1){
							fprintf(stderr,"schema file not found, %s:%d\n",__FILE__,__LINE__-2);
							return -1;
						}

						struct Schema sch;
						memset(&sch,0,sizeof(struct Schema));
						struct Header_d hd = {0,0,&sch};
						/*I AM NOT USING THIS FEATURE FOR NOW*/
						/*if this will be needed i have to implement the lock properly*/
						/*while((is_locked(1,fd_schema)) == LOCKED);*/
						if(!read_header(fd_schema,&hd)){
							fprintf(stderr,"cannot read schema from file %s:%d",__FILE__,__LINE__-1);
							return -1;
						}
						close(fd_schema);

						/* update branch*/
						file_offset update_pos = 0;
						file_offset go_back_to_first_rec = 0;
						ui32 step = 0;
						ui32 sz = 0;
						ui32 k = 0;
						int padding_value = 0;
						do
						{
							/* check the size */
							ui32 sz_ne = 0;
							memcpy(&sz_ne, &ram->mem[ram->offset],sizeof(sz_ne));
							ram->offset += sizeof(sz_ne);

							sz = swap32(sz_ne);
							if (rec->fields[i].data.file.count < sz || rec->fields[i].data.file.count == sz)
								break;

							/*read the padding data*/
							ui32 pd_ne = 0;
							memcpy(&pd_ne,&ram->mem[ram->offset], sizeof(pd_ne));
							ram->offset += sizeof(ui32);

							padding_value = (int)swap32(pd_ne);

							if (step >= sz)
							{
								int array_last = 0;
								int exit = 0;

								/*check if the array of type file is in the last block*/
								file_offset reset_pointer_here = ram->offset;

								ui32 y;
								for(y = 0; y < sz;y++){
									struct Record_f dummy;
									memset(&dummy,0,sizeof(struct Record_f));
									file_offset offset = 0;
									if(read_ram_file(rec->file_name,
												ram,
												&dummy,
												*hd.sch_d) == -1){
										fprintf(stderr,"cannot read ram file, %s:%d.\n",__FILE__,__LINE__-1);
										free_schema(hd.sch_d);
										return -1;
									}


									free_record(&dummy,dummy.fields_num);

									ram->offset += offset + sizeof(file_offset);
								}

								ui64 update_arr = 0;
								memcpy(&update_arr,&ram->mem[ram->offset],sizeof(update_arr));

								ram->offset = reset_pointer_here;

								file_offset up_he = (file_offset) swap64(update_arr);
								if(up_he == 0) array_last = 1;

								if (rec->fields[i].data.file.count < (sz + step) && array_last)
								{
									int pad_value = sz - (rec->fields[i].data.file.count - step);
									padding_value += pad_value;

									sz = rec->fields[i].data.v.size - step;
									exit = 1;

									ram->offset += ( 2 * (-sizeof(ui32)));

									/* write the updated size of the array */
									ui32 new_sz = swap32((ui32)sz);
									memcpy(&ram->mem[ram->offset], &new_sz, sizeof(new_sz));
									ram->offset += sizeof(ui32);

									ui32 new_pd = swap32((ui32)padding_value);
									memcpy(&ram->mem[ram->offset], &new_pd, sizeof(new_sz));
									ram->offset += sizeof(ui32);
								}
								else if (rec->fields[i].data.file.count == (sz + step) && array_last)
								{
									exit = 1;
								}

								while (sz)
								{
									if (step < rec->fields[i].data.file.count){

										if(write_ram_record(ram,
													&rec->fields[i].data.file.recs[step],
													update,0,0) == -1){
											fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
											free_schema(hd.sch_d);
											return -1;
										}
										step++;
									}
									sz--;
								}

								if (exit){
									/*write the epty update offset*/
									ui64 empty_offset = 0;
									memcpy(&ram->mem[ram->offset],&empty_offset,sizeof(ui64));
									free_schema(hd.sch_d);
									break;
								}
							}
							else
							{

								int exit = 0;
								for (k = 0; k < sz; k++)
								{
									if (step > 0 && k == 0)
									{
										if ((step + sz) > rec->fields[i].data.file.count)
										{
											int pad = sz - (rec->fields[i].data.file.count - step);
											padding_value += pad;

											sz = rec->fields[i].data.file.count - step;
											exit = 1;

											ram->offset += (2 *(-sizeof(ui32)));

											/* write the updated size of the array */
											ui32 new_sz = swap32((ui32)sz);
											memcpy(&ram->mem[ram->offset],&new_sz,sizeof(ui32));
											ram->offset += sizeof(ui32);

											/*write padding */
											ui32 new_pd = swap32((ui32)padding_value);
											memcpy(&ram->mem[ram->offset],&new_pd,sizeof(ui32));
											ram->offset += sizeof(ui32);
										}
									}

									if (step < rec->fields[i].data.file.count){
										if(write_ram_record(ram,
													&rec->fields[i].data.file.recs[step],
													update,0,0) == -1){
											fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
											free_schema(hd.sch_d);
											return -1;
										}
										step++;
										if(!(step < rec->fields[i].data.file.count)) exit = 1;
									}
								}

								if (exit)
								{

									if (padding_value > 0)
									{
										int y;
										for(y = 0; y < padding_value;y++){
											struct Record_f dummy;
											memset(&dummy,0,sizeof(struct Record_f));
											memcpy(&dummy,&ram->mem[ram->offset] ,sizeof(struct Record_f));

											size_t dummy_rec_size = get_disk_size_record(&dummy);
											ram->offset += dummy_rec_size;

											free_record(&dummy,dummy.fields_num);

											ram->offset += sizeof(file_offset);
										}
									}

									/*write the empty update offset*/
									ui64 empty_offset = 0;
									memcpy(&ram->mem[ram->offset],&empty_offset,sizeof(ui64));
									ram->offset += sizeof(ui64);
									free_schema(hd.sch_d);
									break;
								}
							}

							if (padding_value > 0)
							{
								int y;
								for(y = 0; y < padding_value;y++){
									struct Record_f dummy;
									memset(&dummy,0,sizeof(struct Record_f));
									memcpy(&dummy,&ram->mem[ram->offset] ,sizeof(struct Record_f));

									size_t dummy_rec_size = get_disk_size_record(&dummy);
									ram->offset += dummy_rec_size;

									free_record(&dummy,dummy.fields_num);

									ram->offset += sizeof(file_offset);
								}

							}

							file_offset go_back_to = ram->offset;
							ui64 update_off_ne = 0;
							memcpy(&update_off_ne,&ram->mem[ram->offset],sizeof(ui64));
							ram->offset += sizeof(ui64);

							if (go_back_to_first_rec == 0)
								go_back_to_first_rec = go_back_to + sizeof(update_off_ne);

							update_pos = (file_offset)swap64(update_off_ne);
							if (update_pos == 0)
							{
								/*go to EOF*/
								update_pos = ram->size;
								ram->offset = ram->size;
								size_t each_rec_size = 0;
								ui32 x;
								for(x = step; x <  rec->fields[i].data.file.count; x++){
									each_rec_size += get_disk_size_record(&rec->fields[i].data.file.recs[x]);
								}

								ui64 remaining_write_size = ((2 * sizeof(ui32)) + 
										each_rec_size 		+ 
										(2*sizeof(ui64)));

								if(ram->size == ram->capacity || ((ram->size + remaining_write_size) > ram->capacity)){
									/*you have to expand the capacity*/
									ui8 *n_mem = (ui8*)realloc(ram->mem, 
											ram->capacity + (remaining_write_size + 1) * sizeof(ui8));
									if(!n_mem){
										fprintf(stderr,"(%s): realloc failed %s:%d.\n",prog,__FILE__,__LINE__-2);
										free_schema(hd.sch_d);
										return -1;
									}

									ram->mem = n_mem;
									ram->capacity += (remaining_write_size + 1);
									memset(&ram->mem[ram->offset],0,remaining_write_size +1);
								}




								/* write the size of the array */
								int size_left = rec->fields[i].data.file.count - step;
								ui32 size_left_ne = swap32((ui32)size_left);
								memcpy(&ram->mem[ram->offset],&size_left_ne,sizeof(ui32));
								move_ram_file_ptr(ram,sizeof(ui32));

								ui32 padding_ne = 0;
								memcpy(&ram->mem[ram->offset],&padding_ne,sizeof(ui32));
								move_ram_file_ptr(ram,sizeof(ui32));

								int j;
								for (j = 0; j < size_left; j++){
									if (step < rec->fields[i].data.file.count){
										/* i think here i need to change
										 * the update field*/
										if(write_ram_record(ram,
													&rec->fields[i].data.file.recs[step],
													0,0,0) == -1){
											fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
											free_schema(hd.sch_d);
											return -1;
										}
										step++;
									}
								}

								/*write the empty update offset*/
								ui64 empty_offset = 0;
								memcpy(&ram->mem[ram->offset],&empty_offset,sizeof(ui64));
								move_ram_file_ptr(ram,sizeof(ui64));

								ram->offset = go_back_to;

								update_off_ne = swap64((ui64)update_pos);
								memcpy(&ram->mem[ram->offset],&update_off_ne,sizeof(ui64));
								ram->offset += sizeof(ui64);


								free_schema(hd.sch_d);
								break;
							}

							/*if upfaste_pos is not zero we move to that position*/
							ram->offset = update_pos;

						} while (update_pos > 0);

						if (rec->fields[i].data.file.count < sz){

							ram->offset -= sizeof(ui32);

							/*write the size of the array */
							ui32 size_ne = swap32(rec->fields[i].data.file.count);
							memcpy(&ram->mem[ram->offset],&size_ne,sizeof(ui32));


							/*read and check the padding, */
							ui32 pad_ne = 0;
							memcpy(&pad_ne,&ram->mem[ram->offset],sizeof(ui32));
							ram->offset += sizeof(ui32);

							int pd_he = (int)swap32(pad_ne);
							pd_he += (sz - rec->fields[i].data.file.count);

							ram->offset -= sizeof(ui32);

							/* write the padding to apply after the  array */
							pad_ne = swap32((ui32)pd_he);
							memcpy(&ram->mem[ram->offset],&pad_ne,sizeof(ui32));

							ui32 j;
							for (j = step; j < rec->fields[i].data.file.count; j++){
								/*write ram record*/
								if(write_ram_record(ram,
											&rec->fields[i].data.file.recs[step],
											update,0,0) == -1){
									fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
									free_schema(hd.sch_d);
									return -1;
								}
							}

							/*
							 * move the file pointer after the array
							 * as much as the pad
							 * */
							int y;
							for(y = 0; y < pd_he;y++){
								struct Record_f dummy;
								memset(&dummy,0,sizeof(struct Record_f));
								memcpy(&dummy,&ram->mem[ram->offset] ,sizeof(struct Record_f));

								size_t dummy_rec_size = get_disk_size_record(&dummy);
								ram->offset += dummy_rec_size;

								free_record(&dummy,dummy.fields_num);

								ram->offset += sizeof(file_offset);
							}

							ui64 update_arr_ne = 0;
							memcpy(&ram->mem[ram->offset],&update_arr_ne, sizeof(update_arr_ne));
							ram->offset += sizeof(ui64);
							free_schema(hd.sch_d);

						} else if (rec->fields[i].data.file.count == sz){
							/*
							 * the sizes are the same
							 * we simply write the array.
							 * */
							if (step > 0){
								ram->offset -= sizeof(sz);

								int size_left = rec->fields[i].data.file.count - step;
								ui32 sz_ne = swap32((ui32)size_left);
								memcpy(&ram->mem[ram->offset],&sz_ne,sizeof(sz_ne));
								ram->offset += sizeof(sz_ne);
							}

							/*read and check the padding, */
							ui32 pad_ne = 0;
							memcpy(&pad_ne,&ram->mem[ram->offset],sizeof(ui32));
							ram->offset += sizeof(ui32);

							int pd_he = (int)swap32(pad_ne);
							ui32 j;
							for (j = 0; j < rec->fields[i].data.file.count; j++)
							{
								if (step < rec->fields[i].data.file.count){

									/*write ram memory*/
									if(write_ram_record(ram,
												&rec->fields[i].data.file.recs[step],
												update,0,0) == -1){
										fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
										return -1;
									}
									step++;
								}
							}

							/*
							 * move the file pointer
							 * as much as the padding value
							 * if it si bigger than 0
							 * */
							if (pd_he > 0)
							{
								int y;
								for(y = 0; y < padding_value;y++){
									struct Record_f dummy;
									memset(&dummy,0,sizeof(struct Record_f));
									memcpy(&dummy,&ram->mem[ram->offset] ,sizeof(struct Record_f));

									size_t dummy_rec_size = get_disk_size_record(&dummy);
									ram->offset += dummy_rec_size;

									free_record(&dummy,dummy.fields_num);

									ram->offset += sizeof(file_offset);
								}

							}

							ui64 update_arr_ne = 0;
							memcpy(&ram->mem[ram->offset],&update_arr_ne,sizeof(ui64));
							ram->offset += sizeof(ui64);
							free_schema(hd.sch_d);
						}

						if (go_back_to_first_rec > 0) ram->offset = go_back_to_first_rec;
					}

					break;
				}
#endif
			default:
				break;
		}

	} 

	ui64 upd_rec = 0;
	if(offset) upd_rec = swap64(offset); 

	memcpy(&ram->mem[ram->offset],&upd_rec,sizeof(ui64));
	move_ram_file_ptr(ram,sizeof(ui64));

	return 0;
}



int buffered_write(file_t *fd, struct Record_f *rec, int update, file_offset rec_ram_file_pos, file_offset offset)
{
	struct Ram_file ram;
	memset(&ram,0,sizeof(struct Ram_file));
	if(!update){
		long long rec_disk_size = get_disk_size_record(rec);
		if(rec_disk_size < 0) {
			fprintf(stderr,"init_ram_file failed, %s:%d.\n",__FILE__, __LINE__ - 2);
			return -1;
		}

		if(init_ram_file(&ram,(size_t)rec_disk_size) == -1){
			fprintf(stderr,"init_ram_file failed, %s:%d.\n",__FILE__, __LINE__ - 1);
			return -1;
		}
	}else{
		if(get_all_record(*fd,&ram) == -1){
			fprintf(stderr,"cannot read all records %s:%d\n",__FILE__,__LINE__-1);	
			return -1;
		}

		move_ram_file_ptr(&ram,rec_ram_file_pos);
	}

	if(write_ram_record(&ram,rec,update,0,offset) == -1){
		fprintf(stderr,"write_ram_record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
		close_ram_file(&ram);
		return -1;
	}

	/*build the file name*/
	size_t l = strlen(rec->file_name) + strlen(".dat");
	char buf[l+1];
	memset(buf,0,l+1);
	if(update){
		strncpy(buf,rec->file_name,strlen(rec->file_name));
		strncat(buf,".dat",strlen(".dat")+1);		

		close_file(1,*fd);
		if(open_file(buf,1,fd) == -1){ /* open the file back with O_TRUNC*/
			file_error_handler(1,*fd);
			close_ram_file(&ram);
			return -1;
		}
	}

/*TODO undesrtand the logic here!! it seems funny */
	if(os_write(*fd,ram.mem,ram.size) == -1)
	{
		fprintf(stderr,"write to file failed, %s:%d.\n",__FILE__, __LINE__ - 1);
		close_ram_file(&ram);
		return -1;
	}

	if(update){
		close_file(1,*fd);
		if(open_file(buf,0,fd) == -1){ /* open the file back with O_TRUNC*/
			file_error_handler(1,*fd);
			close_ram_file(&ram);
			return -1;
		}
	}
	close_ram_file(&ram);
	return 0;
}


/*
 * parameter cache_pos defines behavior for this funtion.
 * if you are using an array of struct Cache, cache_pos must be 0 or greater.
 *
 * if you are using a single struct Cache, cache_pos must be -1;
 *
 * */
#if defined(__linux__) || defined(__APPLE__)
int cache_file(int *fds,char *file_name,struct Schema *sch,struct Cache *c,HashTable *cache_register,int cache_pos)
#elif defined(_WIN32)
int cache_file(HANDLE *fds,char *file_name,struct Schema *sch,struct Cache *c,HashTable *cache_register,int cache_pos)
#endif
{
	
	/*check if the file is cached already*/
#if defined(__linux__) || defined(__APPLE__)
	if(get((void*)file_name,cache_register,STR) != -1)
#elif defined(_WIN32)
	if(get((void*)file_name,cache_register,STR_KEY) != -1)
#endif
		return FILE_IS_CACHED;

	int index = 0;
	if(cache_pos != -1){
		if(!read_all_index_file(fds[0],&c[cache_pos].index_file,&index))
		{
			free_ht_array(c[cache_pos].index_file,index);
			close_ram_file(&c[cache_pos].data_file);
			return -1;
		}
		c[cache_pos].indexes = index;
	}else{
		if(!read_all_index_file(fds[0],&c->index_file,&index))
		{
			free_ht_array(c->index_file,index);
			close_ram_file(&c->data_file);
			return -1;
		}
		c->indexes = index;
	}

	if(cache_pos != -1){
		if(get_all_record(fds[1],&c[cache_pos].data_file) == -1)
		{
			free_ht_array(c[cache_pos].index_file,index);
			close_ram_file(&c[cache_pos].data_file);
			return -1;
		}
	}else{
		if(get_all_record(fds[1],&c->data_file) == -1)
		{
			free_ht_array(c->index_file,index);
			close_ram_file(&c->data_file);
			return -1;
		}
	}
		
	if(cache_pos != -1){
		if(copy_schema(sch,&c[cache_pos].sch) == -1){
			free_ht_array(c[cache_pos].index_file,index);
			close_ram_file(&c[cache_pos].data_file);
			return -1;
		}

		c[cache_pos].ts = time(NULL);
		c[cache_pos].file_name = duplicate_str(file_name);
		if(!(c[cache_pos].file_name)){
			fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F, L - 3);
			free_ht_array(c[cache_pos].index_file,index);
			close_ram_file(&c[cache_pos].data_file);
			return -1;
		}
	}else{
		if(copy_schema(sch,&c->sch) == -1){
			free_ht_array(c->index_file,index);
			close_ram_file(&c->data_file);
			return -1;
		}

		c->ts = time(NULL);
		c->file_name = duplicate_str(file_name);
		if(!c->file_name){
			fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F, L - 3);
			free_ht_array(c->index_file,index);
			close_ram_file(&c->data_file);
			return -1;
		}
	}
	
#if defined(__linux__) || defined(__APPLE__)
	if(!set((void*)file_name,STR,cache_pos,cache_register))
#elif defined(_WIN32)
	if(!set((void*)file_name,STR_KEY,cache_pos,cache_register))
#endif
	{
		free_ht_array(c->index_file,index);
		close_ram_file(&c->data_file);
		return -1;
	}
	return 0;
}

void free_cache(struct Cache *c)
{
	if(c->index_file == NULL)
		return;

	free_ht_array(c->index_file,c->indexes);
	close_ram_file(&c->data_file);
	free_schema(&c->sch);
	free(c->file_name);
	memset(c,0,sizeof *c);
}
