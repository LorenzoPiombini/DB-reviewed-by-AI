#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if defined(__linux__)
#include <unistd.h>
#endif

#include "crud.h"
#include "endian.h"
#include "record.h"
#include "lock.h"
#include "globals.h"
#include "str_op.h"
#include "debug.h"
#include "hash_tbl.h"
#include "parse.h"
#include "db_types.h"
#include "input.h"
#include "string_utilities.h"
#include "common.h"

static char *prog = "db";
static file_offset get_rec_position(struct HashTable *ht, void *key, int key_type);

HashTable *g_ht = NULL;
int g_index = 0;
int *p_gi = &g_index;
struct Ram_file ram = {0, 0, 0, 0};

int get_record(int mode,char *file_name,struct Record_f *rec, void *key, int key_type, struct Header_d hd, file_t *fds, int index)
{
	int e = 0;
	if(mode == RAM_FILE){
		e = 1;
		if(get_all_record(fds[1],&ram) == -1){
			e = 0;
			fprintf(stderr,"cannot get all the record from '%s'.\n",file_name);
			return -1;
		}
	}

	HashTable ht;
	memset(&ht,0,sizeof(HashTable));

	HashTable *p_ht = &ht;
	if (!read_index_nr(index >= 0 ? index : 0, fds[0], &p_ht)) {
		printf("reading index file failed, %s:%d.\n", F, L - 1);
		return STATUS_ERROR;
	}

	file_offset offset = 0;
	if((offset = get_rec_position(p_ht,key,key_type)) == -1){
		fprintf(stderr,"(%s): record not found.\n",prog);
		destroy_hasht(p_ht);
		return STATUS_ERROR;
	}

	if(offset == KEY_NOT_FOUND){
		fprintf(stderr,"(%s): record not found.\n",prog);
		destroy_hasht(p_ht);
		return KEY_NOT_FOUND;
	}
	rec->offset = offset;
	destroy_hasht(p_ht);
	if(!e){
		if (find_record_position(fds[1], offset) == -1) {
			__er_file_pointer(F, L - 1);
			return STATUS_ERROR;
		}

		if(read_file(fds[1], file_name, rec, *(hd.sch_d)) == -1) {
			printf("read record failed, %s:%d.\n",__FILE__, __LINE__ - 1);
			return STATUS_ERROR;
		}
		file_offset update_rec_pos = 0; 
		struct Record_f *temp = NULL;
		temp = rec;
		while ((update_rec_pos = get_update_offset(fds[1])) > 0) {
			struct Record_f *n = (struct Record_f*)malloc(sizeof(struct Record_f));
			if(!n){		
				fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
				return STATUS_ERROR;
			}

			memset(n,0,sizeof *n);
			if (find_record_position(fds[1], update_rec_pos) == -1) {
				__er_file_pointer(F, L - 1);
				free(n);
				return STATUS_ERROR;
			}

			n->offset = update_rec_pos;
			if(read_file(fds[1], file_name,n,*hd.sch_d) == -1) {
				printf("read record failed, %s:%d.\n",__FILE__, __LINE__ - 2);
				return STATUS_ERROR;
			}

			temp->next = n;
			while(temp->next) temp = temp->next; 
			rec->count++;
		}

		if (update_rec_pos == -1) {
			return STATUS_ERROR;
		}
	} else{
		/*RAM FILE mode*/
		file_offset pos_after_read = 0;
		ram.offset = offset;
		if(( pos_after_read = read_ram_file(file_name,&ram, rec,*(hd.sch_d))) == -1){
			fprintf(stderr,"cannot read from ram file '%s'.\n",file_name);
			clear_ram_file(&ram);
			return STATUS_ERROR;
		}

		ui64 up_r_pos_ne = 0; 
		struct Record_f *temp = NULL;
		temp = rec;
		do{
			memcpy(&up_r_pos_ne,&ram.mem[pos_after_read],sizeof(ui64));
			if(up_r_pos_ne == 0) break;

			struct Record_f *n = (struct Record_f*)malloc(sizeof(struct Record_f));
			if(!n){		
				fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
				clear_ram_file(&ram);
				return STATUS_ERROR;
			}

			memset(n,0,sizeof *n);

			file_offset update_rec_pos = swap64(up_r_pos_ne);
			n->offset = update_rec_pos;
			ram.offset = update_rec_pos;
			if(( pos_after_read = read_ram_file(file_name,&ram, n,*(hd.sch_d))) == -1){
				fprintf(stderr,"cannot read from ram file '%s'.\n",file_name);
				clear_ram_file(&ram);
				return STATUS_ERROR;
			}

			temp->next = n;
			while(temp->next) temp = temp->next; 
			rec->count++;
			
		}while(up_r_pos_ne != 0); 
	}
	clear_ram_file(&ram);
	return 0;
}

int get_all_records(char *file_name,file_t *fds,struct Record_f ***recs,struct Header_d hd,size_t *all_size)
{
	if(get_all_record(fds[1],&ram) == -1){
		fprintf(stderr,"cannot get all the record from '%s'.\n",file_name);
		return -1;
	}

	
	*all_size = (ram.size/sizeof(struct Record_f*)) +1;
	*recs = (struct Record_f**)malloc(sizeof(struct Record_f*)*(*all_size));
	if(!recs){ 
		fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		clear_ram_file(&ram);
		return STATUS_ERROR;
	}

	memset(*recs,0,*all_size * sizeof *recs);
	i32 i = 0;
	do{
		file_offset pos_after_read = 0;
		struct Record_f* rec = (struct Record_f*)malloc(sizeof(struct Record_f));
		memset(rec,0,sizeof(struct Record_f));

		if(( pos_after_read = read_ram_file(file_name,&ram, rec,*(hd.sch_d))) == -1){
			fprintf(stderr,"cannot read from ram file '%s', %s:%d.\n",file_name,__FILE__,__LINE__);
			clear_ram_file(&ram);
			return STATUS_ERROR;
		}

		ui64 up_r_pos_ne = 0; 
		struct Record_f *temp = NULL;
		temp = rec;
		memcpy(&up_r_pos_ne,&ram.mem[pos_after_read],sizeof(ui64));
		ram.offset += sizeof(ui64);
		(*recs)[i] = rec;
		i++;
		if(up_r_pos_ne == 0){
			if(ram.size > ram.offset){
				continue;
			}else{
				break;
			}
		}

		struct Record_f *n = (struct Record_f*)malloc(sizeof(struct Record_f));
		if(!n){		
			fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
			clear_ram_file(&ram);
			return STATUS_ERROR;
		}

		memset(n,0,sizeof *n);
		file_offset update_rec_pos = swap64(up_r_pos_ne);
		n->offset = update_rec_pos;
		ram.offset = update_rec_pos;
		if(( pos_after_read = read_ram_file(file_name,&ram, n,*(hd.sch_d))) == -1){
			fprintf(stderr,"cannot read from ram file '%s'.\n",file_name);
			clear_ram_file(&ram);
			return STATUS_ERROR;
		}

		temp->next = n;
		while(temp->next) temp = temp->next; 
		rec->count++;
	}while(ram.size != ram.offset); 

	clear_ram_file(&ram);
	return 0;
}

int is_db_file(struct Header_d *hd, file_t *fds)
{
	if (!read_header(fds[2], hd)) return STATUS_ERROR;
	return 0;
}

int check_data(char *file_path,char *data_to_add,
		file_t *fds, 
		char files[][MAX_FILE_PATH_LENGTH], 
		struct Record_f *rec,
		struct Header_d *hd,
		int *lock_f,
		int option,
		int update)
{
	int fields_count = 0;
	unsigned char check = 0;
	int pos[200];
	memset(pos,-1,sizeof(int)*200);

	if(__IMPORT_EZ || __UTILITY) find_delim_in_fields(":",data_to_add,pos,*(hd->sch_d)); 

	ui8  cnt = (ui8) count_fields(data_to_add,":");
	if(cnt >= 3) cnt -= 1; 	

	if((cnt == hd->sch_d->fields_num) && strstr(data_to_add,"{") && __UTILITY){
		fprintf(stderr,"input different than file definition\n");
		return -1;
	}

	int mode = check_handle_input_mode(data_to_add, FWRT) | WR;

	if(mode == -1){
		fprintf(stderr,"(%s): check the input, value might be missng.\ndata_to_add:%s\n",prog,data_to_add);
		return -1;
	}
	/*check schema*/
	if(mode == TYPE_WR){
		fields_count = count_fields(data_to_add,NULL);
		if(fields_count == 0){
			fprintf(stderr,"(%s):check input syntax.\n",prog);
			return STATUS_ERROR;
		}
		if (fields_count > MAX_FIELD_NR) {
			printf("Too many fields, max %d each file definition.", MAX_FIELD_NR);
			return STATUS_ERROR;
		}

		check = perform_checks_on_schema(mode,data_to_add, fields_count,file_path, rec, hd,pos,option,update);
	} else {
		check = perform_checks_on_schema(mode,data_to_add, -1,file_path, rec, hd, pos,option,update);
	}

	if (check == SCHEMA_ERR || check == 0) return STATUS_ERROR;

	/*
	 * save the schema file for 
	 * SCHEMA_NW 
	 * SCHEMA_EQ_NT 
	 * SCHEMA_NW_NT 
	 * SCHEMA_CT_NT
	 * */	
	if (check == SCHEMA_NW ||
			check == SCHEMA_NW_NT ||
			check == SCHEMA_CT_NT ||
			check == SCHEMA_EQ_NT){
		/*
		 * if the schema is one between 
		 * SCHEMA_NW 
		 * SCHEMA_EQ_NT 
		 * SCHEMA_NW_NT 
		 * SCHEMA_CT_NT
		 * we update the header
		 * */
		/* aquire lock */
		if(*lock_f){	
			if(acquire_lock(fds,*lock_f) == -1){
				fprintf(stderr,"can't acquire lock on file.\n");
				return STATUS_ERROR;
			}
		}

		close_file(1,fds[2]);
		/*open with O_TRUNCATE*/
		if(open_file(files[2],1,&fds[2]) == -1){
			file_error_handler(1,fds[2]); 
			release_lock(fds,*lock_f);
			*lock_f = 0;
			return STATUS_ERROR;
		}

		if(!write_header(fds[2], hd)) {
			__er_write_to_file(F, L - 1);
			release_lock(fds,*lock_f);
			*lock_f = 0;
			return STATUS_ERROR;
		}

		close_file(1,fds[2]);
		/*open in regular mode*/
		if(open_file(files[2],0,&fds[2]) == -1){
			file_error_handler(1,fds[2]);
			release_lock(fds,*lock_f);
			*lock_f = 0;
			return STATUS_ERROR;
		}
	} 

	return check;
}

int write_record(file_t *fds,
		void *key,
		int key_type,
		struct Record_f *rec, 
		int update,
		char files[3][MAX_FILE_PATH_LENGTH],
		int *lock_f,
		int mode,
		struct Schema *sch)
{
	if(mode == IMPORT){
		if(!__IMPORT_EZ) return -1;
		/*allocate memory buffer for index and data file*/
		/*don't write to disk all the time*/
		if(!g_ht){
			/* load al indexes in memory */
			if (!read_all_index_file(fds[0], &g_ht, p_gi)) {
				fprintf(stderr,"read index file failed. %s:%d.\n", F, L - 2);
				return 1;
			}
		}

		if(set_tbl(g_ht,key,(file_offset)ram.size,key_type,0) == -1) return 0;

		if(write_ram_record(&ram,rec,0,0,0) == -1){
			fprintf(stderr,"write_ram_record() failed. %s:%d.\n",__FILE__,__LINE__-1);
			free_ht_array(g_ht,g_index);
			return STATUS_ERROR;
		}

		return 0;
	}

	if((*lock_f) == 0){
		/* aquire lock */
		if(acquire_lock(fds,*lock_f) == -1){
			fprintf(stderr,"can't acquire or release proper lock.\n");
			return STATUS_ERROR;
		}
	}

	file_offset eof = go_to_EOF(fds[1]);
	if (eof == -1) {
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		__er_file_pointer(F, L - 1);
		return -1;
	}

	HashTable *ht = NULL;
	int index = 0;
	int *p_index = &index;
	/* load al indexes in memory */
	if (!read_all_index_file(fds[0], &ht, p_index)) {
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		fprintf(stderr,"read index file failed. %s:%d.\n", F, L - 1);
		return STATUS_ERROR;
	}

	if(check_const_unique(sch,rec, &ht,eof) == -1){
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		free_ht_array(ht, index);
		return STATUS_ERROR;
	}
	
	if(set_tbl(ht,key,eof,key_type,0) == -1){
		free_ht_array(ht, index);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return STATUS_ERROR;
	}

	rec->offset = eof;
/*int buffered_write(int fd, struct Record_f *rec, int update, file_offset rec_ram_file_pos, file_offset offset)*/
	if(buffered_write(&fds[1],rec,update,eof,0) == -1){
		fprintf(stderr,"write to file failed, %s:%d.\n", F,L - 1);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return STATUS_ERROR;
	}

	if(write_index(fds,index,ht,files[0]) == -1){ 
		if(*lock_f){
			release_lock(fds,-1);
			*lock_f = 0;
		}
		return -1;
	}

	free_ht_array(ht, index);
	if(*lock_f){
		release_lock(fds,*lock_f);
		*lock_f = 0;
	}
	return 0;
}

int check_const_unique(struct Schema *sch, struct Record_f *rec, HashTable **ht, file_offset eof)
{
	int inx[sch->fields_num];
	memset(inx,-1,sizeof(int)*sch->fields_num);
	if(sch->has_unique(sch->constraints,sch->fields_num,inx)){
		int i;
		for(i = 0; i < sch->fields_num; i++){
			if(inx[i] == -1 || !rec->field_set[inx[i]])continue;

			switch(rec->fields[inx[i]].type){
			case TYPE_INT:
#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(*ht,(void*)&rec->fields[inx[i]].data.i,eof,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(*ht,(void*)&rec->fields[inx[i]].data.i,eof,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[inx[i]].field_name);
					return STATUS_ERROR;
				}
				break;
			case TYPE_LONG:
			{
				if(rec->fields[inx[i]].data.l > (long)UINT_MAX){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					return STATUS_ERROR;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(*ht,(void*)&rec->fields[inx[i]].data.l,eof,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(*ht,(void*)&rec->fields[inx[i]].data.l,eof,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[inx[i]].field_name);
					return STATUS_ERROR;
				}
				break;
			}
			case TYPE_FLOAT:
			{
				ui32 f = htonf(rec->fields[inx[i]].data.f);
				if( f > (ui32)UINT_MAX){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					return STATUS_ERROR;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(*ht,(void*)&f,eof,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(*ht,(void*)&f,eof,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[inx[i]].field_name);
					return STATUS_ERROR;
				}
				break;
			}
			case TYPE_DOUBLE:
			{
				ui64 d = htonf(rec->fields[inx[i]].data.d);
				if( d > (ui64)UINT_MAX){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					return STATUS_ERROR;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(*ht,(void*)&d,eof,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(*ht,(void*)&d,eof,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[inx[i]].field_name);
					return STATUS_ERROR;
				}
				break;
			}
			case TYPE_STRING:
			{
#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(*ht,(void*)rec->fields[inx[i]].data.s,eof,STR,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(*ht,(void*)rec->fields[inx[i]].data.s,eof,STR_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[inx[i]].field_name);
					return STATUS_ERROR;
				}
				break;
			}
			default:
			break;
			}
		}
	}
	return 0;
}
int write_index(file_t *fds, int index, HashTable *ht, char *file_name)
{
	close_file(1, fds[0]);
	/*opening with o_trunc*/
	if(open_file(file_name, 1,&fds[0]) == -1){
		file_error_handler(1,fds[0]);
		return STATUS_ERROR;
	}

	/* write the new indexes to file */
	if (!write_index_file_head(fds[0], index)) {
		fprintf(stderr,"write index head to file failed, %s:%d.\n", F,L - 1);
		return -1;	
	}

	int i;
	for (i = 0; i < index; i++) {
		if (!write_index_body(fds[0], i, &ht[i])) {
			printf("write to file failed. %s:%d.\n", F, L - 2);
			return -1;
		}
	}

	close_file(1, fds[0]);
	/* opening in regular mode */
	if(open_file(file_name, 0,&fds[0]) == -1){
		file_error_handler(1,fds[0]);
		return STATUS_ERROR;
	}
	return 0;
}

int open_files(char *file_name, file_t *fds, char files[3][MAX_FILE_PATH_LENGTH], int option)
{
	if(three_file_path(file_name, files) == EFLENGTH){
		fprintf(stderr,"(%s): file name or path '%s' too long",prog,file_name);
		return STATUS_ERROR;
	}

	int err = 0;
	switch(option){
		case CREATE_ONLY_SCHEMA:
			{
				if(create_file(files[2],&fds[2]) == -1){
					err = file_error_handler(1,fds[2]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif

					return STATUS_ERROR;
				}	
				break;
			}
		case CREATE_ONLY_DATA:
			{
				if(create_file(files[1],&fds[1]) == -1 ||
					create_file(files[2],&fds[2]) == -1){
					err = file_error_handler(2, fds[1],fds[2]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
					return STATUS_ERROR;
				}	
				break;
			}
		case ONLY_SCHEMA:
			{
				if( open_file(files[2], 0,&fds[2]) == -1){
					err = file_error_handler(1,fds[2]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
					return STATUS_ERROR;
				}

				break;
			}	
		case ONLY_INDEX:
			{
				if(open_file(files[0], 0,&fds[0]) == -1){
					err = file_error_handler(1,fds[0]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
					return STATUS_ERROR;
				}
				return 0;
			}
		case ONLY_DATA:
			{
				if(open_file(files[1],0,&fds[1]) == -1){
					err = file_error_handler(1,fds[1]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
					return STATUS_ERROR;
				}
				return 0;
			}
		case CREATE_FILE:
			{
				if(create_file(files[0],&fds[0]) == -1 
					|| create_file(files[1],&fds[1]) == -1
					|| create_file(files[2],&fds[2]) == -1) {
					err = file_error_handler(3, fds[0],fds[1],fds[2]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
					return STATUS_ERROR;
				}	


				struct Schema sch;
				memset(&sch,0,sizeof(struct Schema));
				struct Header_d hd = {HEADER_ID_SYS, VS, &sch};
				if (!write_empty_header(fds[2], &hd)) {
					fprintf(stderr,"%s:%d.\n", F, L - 1);
					close_file(3,fds[0],fds[1],fds[2]);
					return STATUS_ERROR;
				}
				/*  write the index file */
				if (!write_index_file_head(fds[0], 5)) {
					close_file(3,fds[0],fds[1],fds[2]);
					return STATUS_ERROR;
				}

				int i = 0;
				for (i = 0; i < 5; i++) {
					HashTable ht = {0};
					ht.size = 7;
					ht.write = write_ht;
					if (!write_index_body(fds[0], i, &ht)) {
						destroy_hasht(&ht);
						close_file(3,fds[0],fds[1],fds[2]);
						return STATUS_ERROR;
					}
					destroy_hasht(&ht);
				}
				break;
			}
		default:
			if(open_file(files[0], 0, &fds[0]) == -1
				|| 	open_file(files[1], 0, &fds[1]) == -1
				||	open_file(files[2], 0, &fds[2]) == -1){

			/* file_error_handler will close the file descriptors if there are issues */
				err = file_error_handler(3, fds[0],fds[1],fds[2]);
#if defined(__linux__) || defined(__APPLE__)
					switch(err){
					case ENOENT: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case EEXIST: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#elif defined(_WIN32) || defined(_WIN64)
					switch(err){
					case ERROR_FILE_NOT_FOUND: fprintf(stderr,"(%s): File '%s' doesn't exist.\n",prog,file_name);break;
					case ERROR_SHARING_VIOLATION: fprintf(stderr,"(%s): File '%s' already in use.\n",prog,file_name);break;
					case ERROR_ACCESS_DENIED: fprintf(stderr,"(%s): You don't have permissions to open file '%s' .\n",prog,file_name);break;
					case ERROR_ALREADY_EXISTS: fprintf(stderr,"(%s): File '%s' already exist.\n",prog,file_name);break;
					default: printf("(%s): Error in creating or opening files, %s:%d.\n",prog, F, L - 2);break;
					}
#endif
				return STATUS_ERROR;
			}
			break;
	}

	return 0;
}

int update_rec(char *file_path,
		file_t *fds,
		void *key,
		int key_type,
		struct Record_f *rec,
		struct Header_d hd,
		int check,
		int *lock_f, 
		char *options,
		int index)
{
	struct Record_f rec_old;
	memset(&rec_old,0,sizeof(struct Record_f));
	if(*lock_f){
		if(acquire_lock(fds,*lock_f) == -1){
			fprintf(stderr,"can't acquire or release proper lock.\n");
			return -1;
		}
	}

	i8 err = 0;
	if((err = get_record(-1,file_path,&rec_old,key,key_type,hd,fds,index)) == -1){
		free_record(&rec_old, rec_old.fields_num);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return -1;
	}

	if(err == KEY_NOT_FOUND){
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return KEY_NOT_FOUND;
	}

	struct Record_f *recs[rec_old.count];
	memset(recs,0,sizeof(struct Record_f*)*rec_old.count);

	if (rec_old.count == 1 ) {
		recs[0] = &rec_old;
	}else{
		recs[0] = &rec_old;
		int i = 1;
		struct Record_f *temp = rec_old.next;
		recs[i] = temp;
		while(temp->next){
			temp = temp->next;
			i++;
			recs[i] = temp;
		}
	}

	/*used to return proper value in case of nothing to update*/
	int no_updates = 0;
	if(rec_old.count > 1){
		int option = 0;
		if(options){
			option = convert_options(options);
			if(option == -1){
				fprintf(stderr,"invalid option: '%s'\n",options);
				goto clean_on_error;
			}
		}


		/* here we have the all record in memory and we have
		   to check which fields in the record we have to update*/

		char positions[rec_old.count];
		memset(positions,0,rec_old.count);


		/* 
		 * find_fields_to_update:
		 * 	check all records from the file
		 * 	against the new record, setting the position of the field 
		 * 	that we have to update in  the char array positions. 
		 * 	
		 * 	If an element contain 'y', you have to update the field  
		 * 	at that index position of 'y'.
		 * */

		find_fields_to_update(recs, positions, rec, option,hd,fds,file_path);

		if (positions[0] != 'n' && positions[0] != 'y' && positions[0] != 'e') {
			printf("check on fields failed, %s:%d.\n", F, L - 1);
			goto clean_on_error;
		}

		/* write the update records to file */
		int changed = 0;
		int j;
		for (j = 0; j < (int)rec_old.count; j++) {
			if (positions[j] == 'n') continue;

			if (positions[j] == 'e'){
				no_updates = 1;
				continue;
			}


			no_updates = 0;
			changed = 1;
			if (find_record_position(fds[1], recs[j]->offset) == -1) {
				__er_file_pointer(F, L - 1);
				goto clean_on_error;
			}

			file_offset right_update_pos = 0;
			if ((rec_old.count - j) > 1)
				right_update_pos = recs[j+1]->offset;


			/*the 1 is a flag that make the program know that is an update operation*/
			if(buffered_write(&fds[1], recs[j], 1,recs[j]->offset,right_update_pos) == -1){
				printf("error write file, %s:%d.\n", F, L - 1);
				goto clean_on_error;
			}

		}

		if((check == SCHEMA_CT  ||  check == SCHEMA_CT_NT) && !changed) {
			if(no_updates) {
				fprintf(stderr,"nothing to update\n");
				goto clean_on_error;
			}

			file_offset eof = go_to_EOF(fds[1]); /* file pointer to the end*/
			if (eof == -1) {
				__er_file_pointer(F, L - 1);
				goto clean_on_error;
			}

			/*writing the new part of the schema to the file*/
			if(buffered_write(&fds[1], rec, 0,0,0) == -1){
				printf("write to file failed, %s:%d.\n", F, L - 1);
				goto clean_on_error;
			}

			if(find_record_position(fds[1],recs[rec_old.count - 1]->offset) == -1){
				__er_file_pointer(F, L - 1);
				goto clean_on_error;
			}
			if(buffered_write(&fds[1], recs[rec_old.count-1], 1,recs[rec_old.count-1]->offset,eof) == -1){
				printf("error write file, %s:%d.\n", F, L - 1);
				goto clean_on_error;
			}
		}

		free_record(&rec_old, rec_old.fields_num);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return 0;
	} /*end of if(update_pos > 0) */

	/*updated_rec_pos is 0, THE RECORD IS ALL IN ONE PLACE */
	int op = -1;
	if(options){
		op = convert_options(options);
		if(op == -1){
			fprintf(stderr,"invalid option: '%s'\n",options);
			goto clean_on_error;
		}
	}

	unsigned char comp_rr = compare_old_rec_update_rec(recs, rec, check,op,hd,fds,file_path);
	if (comp_rr == 0) {
		fprintf(stderr,"(%s):compare records failed, %s:%d.\n",prog, F, L -2);
		goto clean_on_error;
	}

	if(comp_rr == UPDATE_NOT){
		no_updates = 1;
		goto clean_on_error;
	}

	if (rec_old.count == 1 && comp_rr == UPDATE_OLD) {
		/* set the position back to the record */
		if (find_record_position(fds[1], recs[0]->offset) == -1) {
			__er_file_pointer(F, L - 1);
			goto clean_on_error;
		}

		/* buffered_write(int fd, struct Record_f *rec, int update, file_offset rec_ram_file_pos, file_offset offset)*/

		/* write the updated record to the file*/
		if(buffered_write(&fds[1], recs[0], 1,recs[0]->offset,0) == -1){
			printf("error write file, %s:%d.\n", F, L - 1);
			goto clean_on_error;
		}

		if(rec_old.count == 1){
			int i;
			for(i = 0; i < rec->fields_num;i++){
				if(rec_old.field_set[i]) 
					rec->field_set[i] = rec_old.field_set[i];
			}
		}
		free_record(&rec_old, rec_old.fields_num);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return 0;
	}

	/*updating the record but we need to write some data in another place in the file*/
	if (rec_old.count == 1 && comp_rr == UPDATE_OLDN && 
			( check == SCHEMA_EQ || 
			  check == SCHEMA_EQ_NT ||
			  check == 1 || 
			  check == SCHEMA_CT ||
			  check == SCHEMA_CT_NT)) {

		file_offset eof = 0;
		if ((eof = go_to_EOF(fds[1])) == -1) {
			__er_file_pointer(F, L - 1);
			goto clean_on_error;
		}

		/* put the position back to the record */
		if (find_record_position(fds[1], recs[0]->offset) == -1) {
			__er_file_pointer(F, L - 1);
			goto clean_on_error;
		}

		/* update the old record */
		if(buffered_write(&fds[1], recs[0], 1,recs[0]->offset,eof) == -1){
			printf("error write file, %s:%d.\n", F, L - 1);
			goto clean_on_error;
		}

		eof = go_to_EOF(fds[1]);
		if (eof == -1) {
			__er_file_pointer(F, L - 1);
			goto clean_on_error;
		}

		/*passing update as 0 becuase is a "new_rec", (right most paramaters) */
		if(buffered_write(&fds[1], rec, 0,0,0) == -1){
			printf("error write file, %s:%d.\n", F, L - 1);
			goto clean_on_error;
		}

		free_record(&rec_old, rec_old.fields_num);
		if(*lock_f){
			release_lock(fds,*lock_f);
			*lock_f = 0;
		}
		return 0;
	}

clean_on_error:
	if(rec_old.count == 1){
		int i;
		for(i = 0; i < rec->fields_num;i++){
			if(rec_old.field_set[i]) 
				rec->field_set[i] = rec_old.field_set[i];
		}
	}
	free_record(&rec_old, rec_old.fields_num);
	if(*lock_f){
		release_lock(fds,*lock_f);
		*lock_f = 0;
	}
	if(no_updates)
		return 0;
	else
		return STATUS_ERROR;

}

int set_tbl(struct HashTable *ht, void *key, file_offset offset, int key_type,int indexing)
{
#if defined(__linux__) || defined(__APPLE__)
	if(key_type == UINT)
#elif defined(_WIN32) || defined(_WIN64)
	if(key_type == UINT_KEY)
#endif
	{
		if(!set(key, key_type, offset, indexing >= 1 ? &ht[indexing] : &ht[0])) return -1;

		return 0;
	}
#if defined(__linux__) || defined(__APPLE__)
	 else if (key_type == STR) 
#elif defined(_WIN32) || defined(_WIN64)
	 else if (key_type == STR_KEY) 
#endif
	{
		if(!set((void *)key, key_type, offset,indexing >= 1 ? &ht[indexing] : &ht[0])) return -1;

		return 0;
	}

	void *key_conv = key_converter((char*)key, &key_type);
#if defined(__linux__) || defined(__APPLE__)
	if (key_type == UINT && !key_conv) 
#elif defined(_WIN32) || defined(_WIN64)
	if (key_type == UINT_KEY && !key_conv) 
#endif
	{
		fprintf(stderr, "error to convert key");
		return -1;
	}
#if defined(__linux__) || defined(__APPLE__)
	else if (key_type == UINT)
#elif defined(_WIN32) || defined(_WIN64)
	else if (key_type == UINT_KEY)
#endif
	{
		if (key_conv) {
			if (!set(key_conv, key_type, offset,indexing >= 1 ? &ht[indexing] : &ht[0])){
				free(key_conv);
				return -1;
			}
			free(key_conv);
		}
	} 
#if defined(__linux__) || defined(__APPLE__)
	else if (key_type == STR) 
#elif defined(_WIN32) || defined(_WIN64)
	else if (key_type == STR_KEY) 
#endif
	{
		/*create a new key value pair in the hash table*/
		if (!set((void *)key, key_type, offset, indexing >= 1 ? &ht[indexing]: &ht[0])) return -1;
	}
	return 0;
}

/*
   struct Cache{
   HashTable *index_file;
   int indexes;
   char *file_name;
   struct Ram_file data_file;
   struct Schema sch;
   time_t ts;
   time_t used;
   };
   */

int write_cache_to_disk(struct Cache *c){
	file_t fds[3];
	memset(fds,-1,3*sizeof(int));
	char file_names[3][MAX_FILE_PATH_LENGTH];
	memset(file_names,0,3*MAX_FILE_PATH_LENGTH);

	if(open_files(c->file_name,fds,file_names,-1) == -1) return -1;

	if(acquire_lock(fds,STD_LOCK | LOCK_FROM_LUA) == -1){
		fprintf(stderr,"can't acquire or release proper lock.%s:%d\n",__FILE__,__LINE__-2);
		close_file(3,fds[0],fds[1],fds[2]);
		return -1;
	}

	if(write_index(fds,c->indexes,c->index_file,file_names[0]) == -1)goto release_lock_on_failure;

	close_file(1,fds[1]);
	if(open_file(file_names[1],1,&fds[1]) == -1) {/*OPEN WITH O_TRUNC*/
		file_error_handler(1,fds[1]);
		goto release_lock_on_failure;
	}

	if(os_write(fds[1],c->data_file.mem,c->data_file.size) == -1) goto release_lock_on_failure;

	/*TODO: write the schema to file*/

	release_lock(fds,-1);
	close_file(3,fds[0],fds[1],fds[2]);
	return 0;

release_lock_on_failure:
	release_lock(fds,-1);
	close_file(3,fds[0],fds[1],fds[2]);
	return -1;			
}

static file_offset get_rec_position(struct HashTable *ht, void *key, int key_type)
{
	file_offset offset = 0;
	void *key_conv =NULL;
	if(key_type == -1){
		key_conv = key_converter(key, &key_type);
#if defined(__linux__) || defined(__APPLE__)
		if (key_type == UINT && !key_conv) 
#elif defined(_WIN32) || defined(_WIN64)
		if (key_type == UINT_KEY && !key_conv) 
#endif
		{
			fprintf(stderr, "(%s): error to convert key",prog);
			return STATUS_ERROR;
		} 
#if defined(__linux__) || defined(__APPLE__)
		else if (key_type == UINT) 
#elif defined(_WIN32) || defined(_WIN64)
		else if (key_type == UINT_KEY) 
#endif
		{
			if (key_conv) {
				offset = get(key_conv, ht, key_type); /*look for the key in the ht */
				free(key_conv);
				if(offset == -1) 
					return KEY_NOT_FOUND;

				return offset;
			}
		} 
#if defined(__linux__) || defined(__APPLE__)
		else if (key_type == STR) 
#elif defined(_WIN32) || defined(_WIN64)
		else if (key_type == STR_KEY) 
#endif
		{
			offset = get((void *)key, ht, key_type); /*look for the key in the ht */
			if(offset == -1) return KEY_NOT_FOUND;
			return offset;
		}
	}
	offset = get((void *)key, ht, key_type); /*look for the key in the ht */
	if(offset == -1) return KEY_NOT_FOUND;
	return offset;
}
