#include <stdio.h>
#include <string.h>
#include "helper.h"
#include "record.h"
#include "parse.h"
#include "globals.h"
#include "common.h"
#include "file.h"
#include "str_op.h"
#include "crud.h"
#include "debug.h"

static char prog[] = "db";
unsigned char create_empty_file(file_t fd_schema, file_t fd_index, int bucket_ht)
{

	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {HEADER_ID_SYS, VS, &sch};

	if (!write_empty_header(fd_schema, &hd)) {
		printf("helper.c l %d.\n", __LINE__ - 1);
		return 0;
	}

	int bucket = bucket_ht > 0 ? bucket_ht : 7;
	HashTable ht = {0};
	ht.size = bucket;
	ht.write = write_ht;

	if (!ht.write(fd_index, &ht)) {
		printf("write to index file failed, helper.c l %d.\n", __LINE__ - 1);
		return 0;
	}

	destroy_hasht(&ht);
	return 1;
}

unsigned char append_to_file(file_t *fds, char *file_path, char *key,char files[][MAX_FILE_PATH_LENGTH],char *data_to_add, HashTable *ht)
{

	struct Record_f rec;
	memset(&rec,0,sizeof(struct Record_f));
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Header_d hd = {0, 0, &sch};
	int lock_f = 0;
	if(check_data(file_path,data_to_add,fds,files, &rec,&hd,&lock_f,-1,0) == -1) return -1;

	file_offset eof = go_to_EOF(fds[1]);
	if (eof == -1) {
		printf("file pointer failed, helper.c l %d.\n", __LINE__ - 2);
		free_record(&rec, rec.fields_num);
		return 0;
	}

	int key_type = is_num(key);
	if (!set((void *)key, key_type, eof, ht)) {
		free_record(&rec, rec.fields_num);
		return ALREADY_KEY;
	}

	if(!lock_f)
	if (!write_file(fds[1], &rec, 0, 0)) {
		printf("write to file failed, helper.c l %d.\n", __LINE__ - 1);
		free_record(&rec, rec.fields_num);
		return 0;
	}

	free_record(&rec, rec.fields_num);
	return 1;
}

int create_file_with_schema(file_t fd_schema,  file_t fd_index, char *schema_def, int bucket_ht, int indexes, int file_field)
{
	int mode = check_handle_input_mode(schema_def, FCRT) | DF;
	int fields_count = 0; 
	/* init the Schema structure*/
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	switch(mode){
	case TYPE_DF:
	{

		if (!create_file_definition_with_no_value(mode,fields_count, schema_def, &sch)) {
			fprintf(stderr,"(%s): can't create file definition %s:%d.\n",prog, F, L - 1);
			free_schema(&sch);
			return -1;
		}
		break;
	}
	case HYB_DF:
	case NO_TYPE_DF	:	
	{
		if (!create_file_definition_with_no_value(mode,fields_count, schema_def, &sch)) {
			free_schema(&sch);
			fprintf(stderr,"(%s): can't create file definition %s:%d.\n",prog, F, L - 1);
			return -1;
		}
		break;
	}
	default:
		fprintf(stderr,"(%s):invalid input.\n",prog);
		return -1;
	}

	struct Header_d hd = {0, 0, &sch};

	if (!create_header(&hd)) return -1;

	if (!write_header(fd_schema, &hd)) {
		fprintf(stderr,"(%s): write schema failed, %s:%d.\n",prog, F, L - 1);
		return -1;
	}

	free_schema(&sch);
	if(file_field) return 0;

	/*  write the index file */
	int bucket = bucket_ht > 0 ? bucket_ht : 7;
	int index_num = indexes > 0 ? indexes : 5;

	if (!write_index_file_head(fd_index, index_num)) {
		printf("write to file failed, %s:%d", F, L - 2);
		return -1;
	}

	int i = 0;
	for (i = 0; i < index_num; i++)
	{
		HashTable ht; 
		memset(&ht,0,sizeof(HashTable));
		ht.size = bucket;
		ht.write = write_ht;

		if (!write_index_body(fd_index, i, &ht))
		{
			printf("write to file failed. %s:%d.\n", F, L - 2);
			destroy_hasht(&ht);
			return -1;
		}

		destroy_hasht(&ht);
	}

	return 0;
}
