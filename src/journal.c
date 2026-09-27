#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "hash_tbl.h"
#include "string_utilities.h"
#include "journal.h"
#include "file.h"
#include "endian.h"
#include "str_op.h"
#include "db_types.h"

static char p[] ="db";

static void error(char *msg,int line);

int journal(file_t caller_fd, file_offset offset, void *key, int key_type, int operation)
{
	int create = 0;
	file_t fd;
	if(open_file(JINX,0,&fd) == -1){
		int err = file_error_handler(1,fd);
		if(err == ENOENT){
			create = 1;
			if(create_file(JINX,&fd) == -1) {
				fprintf(stderr,"(%s): can't create or open '%s'.\n",p,JINX);
				return -1;
			}
		}
	}

	/*
	 * each journal record will store : 
	 * - timestamp
	 * - operation
	 * - the file name
	 * - file_offset
	 * - key   
	 * */
	
	struct stack index;
	struct Node_stack  node;
	memset(&node,0,sizeof(struct Node_stack));
	memset(&index,0,sizeof(struct stack));

	if(!create){
		if (read_journal_index(fd, &index) == -1) {
			fprintf(stderr,"(%s): read index from '%s' failed, %s:%d",
					p,JINX,__FILE__,__LINE__-1);
			close_file(1,fd);
			return -1;
		}
	}
	

#if defined(__linux__) || defined(__APPLE__)
	/*get the file name from the caller file descriptor */
	char path[1024];
	memset(path,0,1024);
	char db_dir[1024];
	memset(db_dir,0,1024);

	
	if(copy_to_string(path,1024,PROC_PATH, caller_fd) < 0){
		error("copy_to_string failed.",__LINE__ - 1);
		close(fd);
		return -1;
	}

	struct stat st;
	if(stat(path,&st) == -1){
		error("can't get file info.",__LINE__ -1);
		close(fd);
		return -1;
	}

	char cwd[1024];
	memset(cwd,0,1024);
	if(getcwd(cwd,1024) == NULL){
		error("can't get current directory.",__LINE__ -1);
		close(fd);
		return -1;
	}

	size_t cwd_length = strlen(cwd);
	char *dynamic_db_path = NULL; 
	if(cwd_length + strlen("/db") >= 1024){
		dynamic_db_path = (char*)malloc(cwd_length+strlen("/db")+1);
		if(!dynamic_db_path){
			error("malloc() failed.",__LINE__ -1);
			close(fd);
			return -1;
		}
		memset(dynamic_db_path,0,cwd_length + strlen("/db")+1);
	}else{
		strncpy(db_dir,cwd,cwd_length);
		strncat(db_dir,"/db",strlen("/db")+1);
	}
	
	
	DIR *database_dir = NULL;
	if(dynamic_db_path){
		if((database_dir = opendir(dynamic_db_path)) == NULL){
			error("can't get current directory.",__LINE__ -1);
			close(fd);
			free(dynamic_db_path);
			return -1;
		}
	}else{
		if((database_dir = opendir(db_dir)) == NULL){
			error("can't get current directory.",__LINE__ -1);
			close(fd);
			return -1;
		}
	}

	struct dirent *dir_data = NULL;
	char *dynamic_file_name = NULL;
	char file_name[MAX_FILE_NAME];
	memset(file_name,0,MAX_FILE_NAME);

	errno = 0;
	while((dir_data = readdir(database_dir))){
		if(dir_data->d_ino != st.st_ino) continue;

		size_t fl_name_length = strlen(dir_data->d_name);
		if(fl_name_length > 1024){
			dynamic_file_name = (char*)malloc(fl_name_length + 1);
			if(!dynamic_file_name){
				error("malloc() failed.\n",__LINE__ -1);
				close(fd);
				closedir(database_dir);
				if(dynamic_db_path)
					free(dynamic_db_path);

				return -1;
			}
			memset(dynamic_file_name,0,fl_name_length+1);
			break;
		}else{
			strncpy(file_name,dir_data->d_name,fl_name_length);
			break;
		}
	}
	
	closedir(database_dir);
	if(dynamic_db_path) free(dynamic_db_path);

	if(dir_data == NULL && errno == 0){
		error("file name not found.\n",__LINE__ -1);
		close(fd);
		if(dynamic_file_name) free(dynamic_file_name);
		return -1;
	}	

	if(dynamic_file_name){
		if(strlen(dynamic_file_name) > MAX_FILE_NAME){
			error("code refactor needed for journal operation.",__LINE__);
			close(fd);
			free(dynamic_file_name);
			return -1;
		}
		strncpy(node.file_name,dynamic_file_name,strlen(dynamic_file_name));
		free(dynamic_file_name);
	}else{
		strncpy(node.file_name,file_name,strlen(file_name));
	}

#elif defined(_WIN32) || defined(_WIN64)
/* 1. Retrieve the file path from caller_fd (Windows equivalent of reading /proc/self/fd/...) */
    WCHAR pathW[MAX_PATH];
    DWORD path_len = GetFinalPathNameByHandleW(caller_fd, pathW, MAX_PATH, FILE_NAME_NORMALIZED);
    if (path_len == 0 || path_len >= MAX_PATH) {
        error("GetFinalPathNameByHandle failed.", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    /* 2. Retrieve file ID info (Windows equivalent of struct stat inode mapping) */
    BY_HANDLE_FILE_INFORMATION caller_info;
    if (!GetFileInformationByHandle(caller_fd, &caller_info)) {
        error("can't get file info.", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    /* 3. Get current working directory (Windows native GetCurrentDirectoryW) */
    WCHAR cwdW[MAX_PATH];
    DWORD cwd_len = GetCurrentDirectoryW(MAX_PATH, cwdW);
    if (cwd_len == 0 || cwd_len >= MAX_PATH) {
        error("can't get current directory.", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    /* Build search pattern: <cwd>\db\* */
    WCHAR db_search_path[MAX_PATH];
    if (swprintf_s(db_search_path, MAX_PATH, L"%s\\db\\*", cwdW) < 0) {
        error("path truncation error.", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    /* 4. Find matching file in directory by comparing Unique File Identifier (Index / File Index) */
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(db_search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        error("can't open db directory.", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    BOOL found = FALSE;
    char target_file_name[MAX_FILE_NAME] = {0};

    do {
        // Skip current and parent directory markers
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        // Form full path to query the exact File Index for inode-like comparison
        WCHAR full_entry_path[MAX_PATH];
        swprintf_s(full_entry_path, MAX_PATH, L"%s\\db\\%s", cwdW, find_data.cFileName);

        HANDLE hEntry = CreateFileW(
            full_entry_path,
            0, // Query metadata only (no read/write access requested)
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        if (hEntry != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION entry_info;
            if (GetFileInformationByHandle(hEntry, &entry_info)) {
                // Compare 64-bit combined file index (Equivalent to st_ino check)
                if (caller_info.nFileIndexHigh == entry_info.nFileIndexHigh &&
                    caller_info.nFileIndexLow == entry_info.nFileIndexLow &&
                    caller_info.dwVolumeSerialNumber == entry_info.dwVolumeSerialNumber) 
                {
                    // Convert target file name from Wide String to UTF-8 / ANSI
                    WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, target_file_name, MAX_FILE_NAME, NULL, NULL);
                    found = TRUE;
                    CloseHandle(hEntry);
                    break;
                }
            }
            CloseHandle(hEntry);
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);

    if (!found) {
        error("file name not found.\n", __LINE__ - 1);
        CloseHandle(fd);
        return -1;
    }

    strncpy(node.file_name, target_file_name, strlen(target_file_name));
#endif
	
	node.offset = offset;
	node.key.type = key_type;
	node.operation = operation;

	switch(key_type){
#if defined(__linux__) || defined(__APPLE__)
	case STR:
#elif defined(_WIN32) || defined(_WIN64)
	case STR_KEY:
#endif
	{	
		size_t l = strlen((char *) key)+1;
		node.key.k.s = (char *)malloc(l);
		if(!node.key.k.s){
			fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-1);
			return -1;
			
		}
		strncpy(node.key.k.s,(char *)key,l);
		break;
	}
#if defined(__linux__) || defined(__APPLE__)
	case UINT:
#elif defined(_WIN32) || defined(_WIN64)
	case UINT_KEY:
#endif
	{
		if(*(ui32 *)key < USHRT_MAX){
			node.key.k.n16 = (*(ui32 *)key);
			node.key.size = 16;
		}else{
			node.key.k.n = (*(ui32 *)key);
			node.key.size = 32;
		}
		break;
	}
	default:
		close_file(1,fd);
		return -1;
	}


	node.timestamp = time(NULL);
	/*push the node on the stack*/	
	if(push_journal(&index,node) == -1){
		error("failed to push on journal stack",__LINE__-1);
		close_file(1,fd);
		return -1;
	}

	/* write the index file */
	if (write_journal_index(&fd, &index) == -1) {
		printf("write to file failed, %s:%d",__FILE__,__LINE__ - 2);
		close_file(1,fd);
		return -1;
	}
	
	close_file(1,fd);
	return 0;
}
 
int push_journal(struct stack *index, struct Node_stack node)
{
	if(index->capacity == 0){
		index->elements = (struct Node_stack*)malloc(sizeof(struct Node_stack));
		if(!index->elements){
			fprintf(stderr,"(%s): malloc failed %s:%d.\n",p,__FILE__,__LINE__-2);
			return -1;
		}

		memcpy(index->elements,&node,sizeof(struct Node_stack));

		index->capacity++;
		return 0;
	
	}

	size_t new_size = index->capacity + 1;
	struct Node_stack *new_elements= (struct Node_stack*) realloc(index->elements,
							new_size * sizeof(struct Node_stack));
	if(!new_elements){
		fprintf(stderr,"(%s): realloc failed %s:%d.\n",p,__FILE__,__LINE__-2);
		return -1;
	}
	
	index->elements = new_elements;
	index->capacity = new_size;

	memcpy(&index->elements[new_size-1],&node,sizeof(struct Node_stack));
	return 0;
}

int pop_journal(struct stack *index)
{
	if(index->capacity == 0) return -1;

	size_t new_size = index->capacity -1;
	struct Node_stack *new_elements = (struct Node_stack*)realloc(index->elements,
						new_size * sizeof(struct Node_stack));
	if(!new_elements){
		fprintf(stderr,"(%s): realloc failed %s:%d.\n",p,__FILE__,__LINE__-2);
		return -1;
	}

	index->elements = new_elements;
	index->capacity = new_size;
	return 0;
}

int peek_journal(struct stack *index, struct Node_stack *node)
{
	if(index->capacity == 0) return -1;

	memcpy(node,&index->elements[index->capacity-1],sizeof(struct Node_stack));

	return 0;
}

int is_empty(struct stack *index)
{
	return index->capacity == 0;

}

void free_stack(struct stack *index)
{
	free(index->elements);
}

int write_journal_index(file_t *fd,struct stack *index)
{
	if(index->capacity == 0) return -1;


	close_file(1,*fd);
	if(open_file(JINX,1,fd) == -1){
		/*TODO !!!!!!!! expand on error*/
		int err = file_error_handler(1,*fd);
		/* this is just to shus gcc warnings*/
		if(err){
			err = 0;
			return -1;
		}else{
			err = 0;
			return -1;
		}
	}

	ui32 cap_ne = swap32(index->capacity);
	if(os_write(*fd,&cap_ne,sizeof(cap_ne)) == -1){
		fprintf(stderr,"(%s): can't write journal index file.",p);
		return -1;
	}

	
	int i;
	for(i = 0; i < index->capacity; i++){
		ui64 ts_ne = swap64((index->elements[i].timestamp));
		if(os_write(*fd,&ts_ne,sizeof(ts_ne)) == -1){
			error("can't write to journal index file.",__LINE__-1);
			return -1;
		}
		size_t size = strlen(index->elements[i].file_name);
		ui64 size_ne = swap64(size);
		if(os_write(*fd,&size_ne,sizeof(size_ne)) == -1 ||
				os_write(*fd,index->elements[i].file_name,size) == -1){
			error("can't write to journal index file.",__LINE__-1);
			return -1;
		}

		switch(index->elements[i].key.type){ 
#if defined(__linux__) || defined(__APPLE__)
		case STR:
#elif defined(_WIN32) || defined(_WIN64)
		case STR_KEY:
#endif
		{
			ui32 type = swap32(index->elements[i].key.type);
			ui64 key_l = swap64(strlen(index->elements[i].key.k.s));

			if (os_write(*fd, &type, sizeof(type)) == -1 ||
					os_write(*fd, &key_l, sizeof(key_l)) == -1 ||
					os_write(*fd, index->elements[i].key.k.s, strlen(index->elements[i].key.k.s) + 1) == -1 ){
				perror("write index:");
				return 0; 
			}
			break;

		}
#if defined(__linux__) || defined(__APPLE__)
		case UINT:
#elif defined(_WIN32) || defined(_WIN64)
		case UINT_KEY:
#endif
		{
			ui32 type = swap32(index->elements[i].key.type);
			ui8 size = (ui8)index->elements[i].key.size;

			ui32 k = 0;
			ui16 k16 = 0;
			if(index->elements[i].key.size == 32)
				k = swap32(index->elements[i].key.k.n);
			else
				k16 = swap16(index->elements[i].key.k.n16);

			if (os_write(*fd, &type, sizeof(type)) == -1 ||
					os_write(*fd, &size, sizeof(size)) == -1) {
				perror("write index:");
				return 0; 
			}

			if(index->elements[i].key.size == 16){
				if (os_write(*fd, &k16, sizeof(k16)) == -1) {
					perror("write index:");
					return 0; /*false*/
				}
			}else{
				if (os_write(*fd, &k, sizeof(k)) == -1) {
					perror("write index:");
					return 0; /*false*/
				}
			}

			break;
		}
		default:
			error("key not supported.",__LINE__);
			return -1;
		}

		ui64 ot_ne = swap64((index->elements[i].offset));
		if(os_write(*fd,&ot_ne,sizeof(ot_ne)) == -1){
			error("can't write to journal index file.",__LINE__-1);
			return -1;
		}

		ui32 op_ne = swap32(index->elements[i].operation);
		if(os_write(*fd,&op_ne,sizeof(op_ne)) == -1){
			error("can't write to journal index file.",__LINE__-1);
			return -1;
		}

	}

	close_file(1,*fd);
	if(open_file(JINX,0,fd) == -1){
		/*TODO !!!!!!!! expand on error*/
		int err = file_error_handler(1,*fd);
		/* this is just to shus gcc warnings*/
		if(err){
			err = 0;
			return -1;
		}else{
			err = 0;
			return -1;
		}
	}

	return 0;
}

int read_journal_index(file_t fd,struct stack *index)
{
	ui32 cap_ne = 0;
	if(os_read(fd,&cap_ne,sizeof(cap_ne)) == -1){
		fprintf(stderr,"(%s): can't write journal index file.",p);
		return -1;
	}


	int cap = (int)swap32(cap_ne);

	index->elements = (struct Node_stack*)malloc(cap *sizeof(struct Node_stack));
	if(!index->elements){
		error("malloc() failed.",__LINE__-2);
		return 0;
	}
	index->capacity = cap;

	int i;
	for(i = 0; i < cap; i++){
		
		ui64 ts_ne = 0;
		if(os_read(fd,&ts_ne,sizeof(ts_ne)) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}
		
		index->elements[i].timestamp = (time_t) swap64(ts_ne);
		
		ui64 size_ne = 0;
		if(os_read(fd,&size_ne,sizeof(size_ne)) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}

		size_t size = swap64(size_ne);
		char buff[size + 1];
		memset(buff,0,size + 1);

		if(os_read(fd,buff,size) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}

		strncpy(index->elements[i].file_name,buff,size);

		ui32 kt_ne = 0;
		if(os_read(fd,&kt_ne,sizeof(kt_ne)) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}
		
		index->elements[i].key.type = (int)swap32(kt_ne);

		switch(index->elements[i].key.type){
#if defined(__linux__) || defined(__APPLE__)
		case STR:
#elif defined(_WIN32) || defined(_WIN64)
		case STR_KEY:
#endif
		{
			ui64 size_ne = 0;
			if(os_read(fd,&size_ne,sizeof(size_ne)) == -1){
				error("read journal index failed",__LINE__-1);
				return -1;
			}

			size_t size = swap64(size_ne) + 1;
			char buff[size];
			memset(buff,0,size);

			if(os_read(fd,buff,size) == -1){
				error("read journal index failed",__LINE__-1);
				return -1;
			}

			index->elements[i].key.k.s = (char *)malloc(size);
			if(!index->elements[i].key.k.s){
				error("malloc() failed",__LINE__-1);
				return -1;
			}

			strncpy(index->elements[i].key.k.s ,buff,size);
			break;
		}
#if defined(__linux__) || defined(__APPLE__)
		case UINT:
#elif defined(_WIN32) || defined(_WIN64)
		case UINT_KEY:
#endif
		{
			ui8 k_size = 0;
			if(os_read(fd,&k_size,sizeof(k_size)) == -1){
				error("read journal index failed",__LINE__-1);
				return -1;
			}

			if(k_size == 16){
				ui16 k_ne = 0;
				if(os_read(fd,&k_ne,sizeof(k_ne)) == -1){
					error("read journal index failed",__LINE__-1);
					return -1;
				}

				index->elements[i].key.k.n16 = swap16(k_ne);
			}else{
				ui32 k_ne = 0;
				if(os_read(fd,&k_ne,sizeof(k_ne)) == -1){


					error("read journal index failed",__LINE__-1);
					return -1;
				}

				index->elements[i].key.k.n = (ui32)swap32(k_ne);
			}
			break;
		}
		default:
			break;
		}

		ui64 os_ne = 0;
		if(os_read(fd,&os_ne,sizeof(os_ne)) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}
		
		index->elements[i].offset = (file_offset) swap64(os_ne);
		ui32 op_ne = 0;
		if(os_read(fd,&op_ne,sizeof(op_ne)) == -1){
			error("read journal index failed",__LINE__-1);
			return -1;
		}
		
		index->elements[i].operation = (int) swap32(op_ne);
	}

	return 0;
}


int show_journal()
{
	struct stack index;
	memset(&index,0,sizeof(struct stack));

	file_t fd;
	if(open_file(JINX,0,&fd) == -1){
		int err = file_error_handler(1,fd);
		error("cannot open the journal file.\n",__LINE__-2);
		/* this is just to shus gcc warnings*/
		if(err){
			err = 0;
			return -1;
		}else{
			err = 0;
			return -1;
		}
	}

	if(read_journal_index(fd,&index)){
		error("cannot read journal file",__LINE__-1);	
		close_file(1,fd);
		return -1;
	}			
	
	int i;		
	for(i = 0;i < index.capacity; i++){
		char *date = ctime(&index.elements[i].timestamp);	
		char dt[strlen(date)+1];
		memset(dt,0,strlen(date)+1);
		strncpy(dt,date,strlen(date)-1);
		printf("%s, ",dt);
		printf("%s, record key: ",index.elements[i].file_name);
		switch(index.elements[i].key.type){

#if defined(__linux__) || defined(__APPLE__)
		case STR:
#elif defined(_WIN32) || defined(_WIN64)
		case STR_KEY:
#endif
			printf("%s, offset: ",index.elements[i].key.k.s);
			break;
#if defined(__linux__) || defined(__APPLE__)
		case UINT:
#elif defined(_WIN32) || defined(_WIN64)
		case UINT_KEY:
#endif
			if(index.elements[i].key.size == 16)
				printf("%d, offset: ",index.elements[i].key.k.n16);	
			else
				printf("%d, offset: ",index.elements[i].key.k.n);	
			break;
		default:
			break;
		}
		printf("%ld, operation: ",index.elements[i].offset);
		switch(index.elements[i].operation){
		case J_DEL:
			printf("DEL.\n");
		default:
			break;
		}
	}


	free_stack(&index);
	close_file(1,fd);
	return 0;
}
static void error(char *msg,int line)
{
	fprintf(stderr,"(%s): %s, %s:%d.\n",p,msg,__FILE__,line);
}
