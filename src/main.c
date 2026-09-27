#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "key.h"
#include "journal.h"
#include "string_utilities.h"
#include "globals.h"
#include "common.h"
#include "file.h"
#include "record.h"
#include "input.h"
#include "hash_tbl.h"
#include "str_op.h"
#include "lock.h"
#include "parse.h"
#include "debug.h"
#include "build.h"
#include "crud.h"


char prog[] = "db";
int main(int argc, char *argv[])
{
	if (argc < 2) {
		print_usage(argv);
		return 1;
	}


	__UTILITY = 1;
	/*----------- bool values-------------------*/

	unsigned char new_file = 0;
	unsigned char del = 0;
	unsigned char update = 0;
	unsigned char list_def = 0;
	unsigned char del_file = 0;
	unsigned char del_field = 0;
	unsigned char build = 0;
	unsigned char list_keys = 0;
	unsigned char create = 0;
	unsigned char import_from_data = 0;
	unsigned char options = 0;
	unsigned char index_add = 0;
	unsigned char file_field = 0;
	unsigned char journal_display = 0;
	unsigned char nr_of_record_display = 0;
	unsigned char modify_schema = 0;
	unsigned char swap_index = 0;
	/*------------------------------------------*/

	/* parameters populated with the flag from getopt()*/
	int c = 0;
	struct String file_path;
	init(&file_path,NULL);
	struct String data_to_add;
	init(&data_to_add,NULL);
	struct String key;
	init(&key,NULL);
	struct String schema_def;
	init(&schema_def,NULL);
	struct String constrains_def;
	init(&constrains_def,NULL);
	struct String txt_f;
	init(&txt_f,NULL);

	char *option = NULL;
	int bucket_ht = 0;
	int indexes = 0;
	int index_nr = 0;
	int only_dat = 0;

	while ((c = getopt(argc, argv, "jnItAf:F:a:k:d:D:R:uleB:b:s:x:c:C:i:o:X:NMS")) != -1)
	{
		switch (c){
		case 'S':
		{
			swap_index = 1;
			break;
		}
		case 'C':
		{
			init(&constrains_def,optarg);
			break;
		}
		case 'M':
		{
			modify_schema = 1;
			break;
		}
		case 'd':
		{
			del_field = 1;
			init(&data_to_add,optarg);
			break;
		}
		case 'N':
		{
			nr_of_record_display = 1;
			break;
		}
		case 'j':
		{		
			journal_display = 1;
			break;
		}
		case 'a':
		{
			init(&data_to_add,optarg);
			break;
		}
		case 'n':
		{
			new_file = 1; 
			break;
		}
		case 'f':
		{
			init(&file_path,optarg);
			break;
		}
		case 'F':
		{
			init(&file_path,optarg);
			file_field = 1;
			break;
		}
		case 'k':
		{
			init(&key,optarg);
			break;
		}
		case 'D':
		{
			del = 1;
			errno = 0;
			long l = string_to_long(optarg);
			if(error_value == INVALID_VALUE || l < 0){
				fprintf(stderr,"option -i value is not a valid number.\n");
				return -1;
			}
			index_nr = (int) l;
			break;
		}
		case 't':
			print_types();
			return 0;
		case 'R':
		{
			init(&schema_def,optarg);
			break;
		}
		case 'u':
			update = 1;
			break;
		case 'l':
			list_def = 1;
			break;
		case 'e':
			del_file = 1;
			break;
		case 'b':
		{
			build = 1;
			init(&txt_f,optarg);
			break;
		}
		case 'B':
		{
			import_from_data = 1;
			init(&txt_f,optarg);
			break;
		}
		case 's':
		{
			errno = 0;
			long l = string_to_long(optarg);
			if(error_value == INVALID_VALUE){
				fprintf(stderr,"option -s value is not a valid number.\n");
				return -1;
			}
			bucket_ht = (int)l;
			break;

		}
		case 'X':
		{
			error_value = 0;
			long l = string_to_long(optarg);
			if(error_value == INVALID_VALUE){
				fprintf(stderr,"option -X value is not a valid number.\n");
				return -1;
			}
			index_nr = (int)l;
			break;
		}
		case 'x':
		{
			list_keys = 1;
			error_value = 0;
			long l = string_to_long(optarg);
			if(error_value == INVALID_VALUE){
				fprintf(stderr,"option -x value is not a valid number.\n");
				return -1;
			}
			index_nr = (int)l;
			break;
		}
		case 'c':
		{
			create = 1;
			init(&txt_f,optarg);
			break;
		}
		case 'i':
		{
			errno = 0;
			long l = string_to_long(optarg);
			if(error_value == INVALID_VALUE){
				fprintf(stderr,"option -i value is not a valid number.\n");
				return -1;
			}
			indexes = (int)l;
			break;
		}
		case 'o':
			options = 1, option = optarg;
			break;
		case 'A':
			index_add = 1;
			break;
		case 'I':
			only_dat = 1;
			break;
		default:
			fprintf(stderr,"Unknow option -%c\n", c);
			return 1;
		}
	}

	if (!check_input_and_values(schema_def,file_path, 
				data_to_add,
				key,
				argv, 
				del, 
				list_def, 
				new_file,
				update, 
				del_file,	
				build, 
				create, 
				options, 
				index_add,
				file_field,
				import_from_data,
				journal_display,
				nr_of_record_display,
				del_field,
				modify_schema,
				swap_index)) {
		return -1;
	}

	if(journal_display){
		if(show_journal() == -1){
			fprintf(stderr,"show_journal() failed, %s:%d.\n",__FILE__,__LINE__-1);
			return -1;
		}
		return 0;
	}

	if (create){
		if(txt_f.str){
			if (!create_system_from_txt_file(txt_f.str)) {
				return STATUS_ERROR;
			}
		}else{
			if (!create_system_from_txt_file(txt_f.base)) {
				return STATUS_ERROR;
			}

		}
		fprintf(stderr,"system created!\n");
		return 0;
	}

	if (build) {
		/*this is not valid as for now*/
		/*
		if (build_from_txt_file(file_path, txt_f)) {
			return 0;
		}

		*/
		return STATUS_ERROR;
	}

	if(import_from_data){
		if(txt_f.str){
			if(import_data_to_system(txt_f.str) == -1) {
				fprintf(stderr,"(%s): could not import data from '%s'.\n",prog,txt_f.str);
				return -1;
			}
		}else{
			if(import_data_to_system(txt_f.base) == -1) {
				fprintf(stderr,"(%s): could not import data from '%s'.\n",prog,txt_f.base);
				return -1;
			}
		}

		return 0;
	}

	if (new_file) {
		/*creates three name from the file_path  "str_op.h" */
		char cpy_fp[file_path.size + 1];
		memset(cpy_fp,0,file_path.size + 1);
		if(file_path.str)
			strncpy(cpy_fp,file_path.str,file_path.size);
		else
			strncpy(cpy_fp,file_path.base,file_path.size);

		file_path.close(&file_path);


		char cpy_sd[schema_def.size + 1];
		memset(cpy_sd,0,schema_def.size + 1);
		if(!schema_def.is_empty(&schema_def)){
			if(schema_def.str)
				strncpy(cpy_sd,schema_def.str,schema_def.size);
			else
				strncpy(cpy_sd,schema_def.base,schema_def.size);
			schema_def.close(&schema_def);
		}

		char cpy_cd[constrains_def.size+1];
		memset(cpy_cd,0,constrains_def.size +1);
		if(!constrains_def.is_empty(&constrains_def)){
			if(constrains_def.str)
				strncpy(cpy_cd,constrains_def.str,constrains_def.size);
			else
				strncpy(cpy_cd,constrains_def.base,constrains_def.size);

			constrains_def.close(&constrains_def);
		}

		char cpy_dta[data_to_add.size + 1];
		memset(cpy_dta,0,data_to_add.size + 1);
		if(!data_to_add.is_empty(&data_to_add)){
			if(data_to_add.str)
				strncpy(cpy_dta,data_to_add.str,data_to_add.size);
			else
				strncpy(cpy_dta,data_to_add.base,data_to_add.size);

			data_to_add.close(&data_to_add);
		}

		char kcpy[key.size+1];
		memset(kcpy,0,key.size+1);
		if(!key.is_empty(&key)){
			if(key.str)
				strncpy(kcpy,key.str,key.size);
			else
				strncpy(kcpy,key.str,key.size);
			key.close(&key);
		}

		file_t fds[3];
		INIT_FILE_T_ARRAY(fds,3);
		char files[3][MAX_FILE_PATH_LENGTH] = {0};  
		if (only_dat) {
			if(open_files(cpy_fp, fds, files,CREATE_ONLY_DATA) == -1)
				return STATUS_ERROR;
		}else if(file_field){
			if(open_files(cpy_fp, fds, files,CREATE_ONLY_SCHEMA) == -1)
				return STATUS_ERROR;
		}else{
			if(open_files(cpy_fp, fds, files,CREATE_FILE) == -1)
				return STATUS_ERROR;
		}

		if (cpy_sd[0] != '\0') { 
			/* case when user creates a file with only file definition*/
			int mode = check_handle_input_mode(cpy_sd, FCRT) | DF;
			if(mode == -1){
				fprintf(stderr,"(%s): check the input, value might be missng.\n",prog);
				goto clean_on_error_1;
			}
			int fields_count = 0; 
			/* init he Schema structure*/
			struct Schema sch;
			memset(&sch,0,sizeof(struct Schema));

			switch(mode){
			case TYPE_DF:
			{
				fields_count = count_fields(cpy_sd,NULL);
				if (fields_count == 0) {
					fprintf(stderr,"(%s): type syntax might be wrong.\n",prog);
					goto clean_on_error_1;
				}

				if (fields_count > MAX_FIELD_NR) {
					fprintf(stderr,"(%s): too many fields, max %d fields each file definition.\n"
							,prog, MAX_FIELD_NR);
					goto clean_on_error_1;
				}

				if (!create_file_definition_with_no_value(mode,fields_count, cpy_sd, &sch)) {
					fprintf(stderr,"(%s): can't create file definition %s:%d.\n",prog, F, L - 1);
					goto clean_on_error_1;
				}
				break;
			}
			case HYB_DF:
			case NO_TYPE_DF	:	
			{
				if (!create_file_definition_with_no_value(mode,fields_count, cpy_sd, &sch)) {
					fprintf(stderr,"(%s): can't create file definition %s:%d.\n",prog, F, L - 1);
					goto clean_on_error_1;
				}
				break;
			}
			default:
				fprintf(stderr,"(%s):invalid input.\n",prog);
				goto clean_on_error_1;
			}

			struct Header_d hd = {0, 0, &sch};

			if (!create_header(&hd)) goto clean_on_error_1;

			if (!write_header(fds[2], &hd)) {
				fprintf(stderr,"(%s): write schema failed, %s:%d.\n",prog, F, L - 1);
				goto clean_on_error_1;
			}


			if (only_dat) {
				fprintf(stderr,"(%s): File created successfully!\n",prog);
				free_schema(&sch);
				close_file(2, fds[2], fds[1]);
				return 0;
			}

			if(file_field){
				fprintf(stderr,"(%s): File created successfully!\n",prog);
				close_file(1, fds[2]);
				free_schema(&sch);
				return 0;
			}

			/*  write the index file */
			int bucket = bucket_ht > 0 ? bucket_ht : 7;
			int index_num = indexes > 0 ? indexes : 5;

			if (!write_index_file_head(fds[0], index_num)) {
				fprintf(stderr,"(%s) write index file head failed, %s:%d",prog, F, L - 2);
				goto clean_on_error_1;
			}

			int i = 0;
			for (i = 0; i < index_num; i++) {
				HashTable ht = {0};
				ht.size = bucket;
				ht.write = write_ht;

				if (!write_index_body(fds[0], i, &ht)) {
					fprintf(stderr,"write to file failed. %s:%d.\n", F, L - 2);
					destroy_hasht(&ht);
					goto clean_on_error_1;
				}

				destroy_hasht(&ht);
			}

			fprintf(stderr,"(%s): File created successfully!\n",prog);

			close_file(3,fds[0], fds[1],fds[2]);
			if(free_schema(&sch) == -1){
				fprintf(stderr,"could not free the schema, %s:%d\n",__FILE__,__LINE__-1);
				goto clean_on_error_1;
			}
			return 0;

			clean_on_error_1:
			close_file(3,fds[0], fds[1],fds[2]);
			delete_file(3, files[0], files[1], files[2]);
			if(free_schema(&sch) == -1){
				fprintf(stderr,"could not free the schema, %s:%d\n",__FILE__,__LINE__-1);
			}
			return STATUS_ERROR;
		}

		if (cpy_dta[0] != '\0') { 
			/* creates a file with full definitons (fields and value)*/
			int mode = check_handle_input_mode(cpy_dta, FWRT) | WR;

			if(mode == -1){
				fprintf(stderr,"(%s): check the input, value might be missng.\n",prog);
				goto clean_on_error_2;
			}

			int fields_count = 0; 

			/* init the Schema structure*/
			struct Schema sch;
			memset(&sch,0,sizeof(struct Schema));

			struct Record_f rec;
			memset(&rec,0,sizeof(struct Record_f));
			
			switch(mode){
			case NO_TYPE_WR:	
			case HYB_WR:
			{
				char names[MAX_FIELD_NR][MAX_FILED_LT] ={0};
				int types_i[MAX_FIELD_NR];
				memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);
				char **values = NULL;

				switch (mode){
				case NO_TYPE_WR:			
				{	
					values = extract_fields_value_types_from_input(cpy_dta,names,
							types_i,
							&fields_count);
					if(!values){
						fprintf(stderr,"(%s): cannot extract value from input,%s:%d.\n",prog,
								__FILE__,__LINE__-1);
						goto clean_on_error_2;
					}
					break;
				}
				case HYB_WR:			
				{
					if((fields_count = get_name_types_hybrid(mode,cpy_dta
								,names,
								types_i)) == -1) goto clean_on_error_2;
					if(get_values_hyb(cpy_dta,
							&values,
							fields_count) == -1) goto clean_on_error_2;

					int i;
					for(i = 0; i < fields_count; i++){
						if(types_i[i] == -1) types_i[i] = assign_type(values[i]);		
					}
					break;
				}	
				default:
					goto clean_on_error_2;
				}

				set_schema(names,types_i,&sch,fields_count,NULL,NULL);	

				if(parse_input_with_no_type(
							cpy_fp,
							fields_count, 
							names,
							types_i, 
							values,
							&sch,
							0,
							&rec,
							0) == -1){
					fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
					free_strs(fields_count,1,values);
					goto clean_on_error_2;
				}
				free_strs(fields_count,1,values);
				break;
			}
			case TYPE_WR:			
			{ 
				fields_count = count_fields(cpy_dta,NULL);
				if (fields_count > MAX_FIELD_NR) {
					fprintf(stderr,"(%s): too many fields, max %d each file definition.",prog, MAX_FIELD_NR);
					goto clean_on_error_2;
				}

				if(parse_d_flag_input(cpy_fp, fields_count,cpy_dta, &sch, 0,&rec,NULL,0) == -1) {
					fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
					goto clean_on_error_2;
				}

				break;
			}
			default:
				goto clean_on_error_2;
			}

			struct Header_d hd = {0, 0, &sch};
			if (!create_header(&hd)) {
				fprintf(stderr,"%s:%d.\n", F, L - 1);
				goto clean_on_error_2;
			}

			if (!write_header(fds[2], &hd)) {
				fprintf(stderr,"write to file failed, %s:%d.\n", __FILE__, __LINE__ - 1);
				goto clean_on_error_2;
			}

			if (only_dat) {
				fprintf(stderr,"File created successfully!\n");
				free_record(&rec, fields_count);
				close_file(2, fds[1],fds[2]);
				if(free_schema(&sch) == -1){
					fprintf(stderr,"could not free the schema, %s:%d\n",__FILE__,__LINE__-1);
				}
				return 0;
			}

			/* write the index file */
			int bucket = bucket_ht > 0 ? bucket_ht : 7;
			int index_num = indexes > 0 ? indexes : 5;

			if (!write_index_file_head(fds[0],index_num)) {
				fprintf(stderr,"write to file failed, %s:%d", F, L - 2);
				goto clean_on_error_2;
			}


			int i = 0;
			for (i = 0; i < index_num; i++) {
				HashTable ht;
				memset(&ht,0,sizeof(ht));
				ht.size = bucket;
				ht.write = write_ht;

				if (i == 0) {
					file_offset offset = get_file_offset(fds[1]);
					if (offset == -1) {
						__er_file_pointer(F, L - 3);
						goto clean_on_error_2;
					}

					if(kcpy[0] == '\0'){
						
						/*create a new key value pair in the hash table*/
						i64 k = generate_numeric_key(fds,INCREM,-1,NULL);

						if(k == -1){
							fprintf(stderr,"increment key failed. %s:%d\n",__FILE__,__LINE__-2);
							destroy_hasht(&ht);
							goto clean_on_error_2;
						}

						if(k > (i64)(MAX_KEY-1)){
							fprintf(stderr,"(%s): key is out of range.\n",prog);
							destroy_hasht(&ht);
							goto clean_on_error_2;
						}

						ui32 n = (ui32)k;

#if defined(__linux__) || defined(__APPLE__)
						if (!set((void *)&n, UINT, offset, &ht)) 
#elif defined(_WIN32) || defined(_WIN64)
						if (!set((void *)&n, UINT_KEY, offset, &ht)) 
#endif
						{
							destroy_hasht(&ht);
							goto clean_on_error_2;
						}

					} else {
						int key_type = 0;
						void *key_conv = key_converter(kcpy, &key_type);
#if defined(__linux__) || defined(__APPLE__)
						if (key_type == UINT && !key_conv) 
#elif defined(_WIN32) || defined(_WIN64)
						if (key_type == UINT_KEY && !key_conv) 
#endif
						{
							fprintf(stderr, "(%s): error to convert key.\n",prog);
							goto clean_on_error_2;
						}
#if defined(__linux__) || defined(__APPLE__)
						 else if (key_type == UINT) 
#elif defined(_WIN32) || defined(_WIN64)
						 else if (key_type == UINT_KEY) 
#endif
						 {
							if (key_conv) {
								if (!set(key_conv, key_type, offset, &ht)) {
									free(key_conv);
									goto clean_on_error_2;
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
							if (!set((void *)kcpy, key_type, offset, &ht)) {
								destroy_hasht(&ht);
								goto clean_on_error_2;
							}
						}
					}

					if (!write_file(fds[1], &rec, 0, update)) {
						fprintf(stderr,"write to file failed, %s:%d.\n", F, L - 1);
						destroy_hasht(&ht);
						goto clean_on_error_2;
					}
				}

				if (!write_index_body(fds[0], i, &ht)) {
					fprintf(stderr,"write to file failed. %s:%d.\n", F, L - 2);
					destroy_hasht(&ht);
					goto clean_on_error_2;
				}

				destroy_hasht(&ht);
			}

			fprintf(stderr,"(%s): File created successfully.\n",prog);
			free_record(&rec, fields_count); 
			close_file(3,fds[0], fds[1],fds[2]);
			free_schema(&sch);
			return 0;
			
			clean_on_error_2:
			close_file(3,fds[0], fds[1],fds[2]);
			delete_file(3, files[0], files[1], files[2]);
			free_record(&rec, fields_count);
			free_schema(&sch);
			return STATUS_ERROR;

		}else {
			print_usage(argv);
			fprintf(stderr,"(%s): no data to write to file %s.\n",prog,cpy_fp );
			fprintf(stderr,"(%s): %s has been created, you can add to the file using option -a.\n",prog, cpy_fp);

			/* init the Schema structure*/
			struct Schema sch;
			memset(&sch,0,sizeof(struct Schema));
			struct Header_d hd = {HEADER_ID_SYS, VS, &sch};

			if (!write_empty_header(fds[1], &hd)) {
				fprintf(stderr,"%s:%d.\n", F, L - 1);
				goto clean_on_error_3;
			}

			/*  write the index file */
			int bucket = bucket_ht > 0 ? bucket_ht : 7;
			int index_num = indexes > 0 ? indexes : 5;

			if (!write_index_file_head(fds[0], index_num)) {
				fprintf(stderr,"write to file failed, %s:%d", F, L - 2);
				goto clean_on_error_3;
			}

			int i = 0;
			for (i = 0; i < index_num; i++) {
				HashTable ht = {0};
				ht.size = bucket;
				ht.write = write_ht;

				if (!write_index_body(fds[0], i, &ht)) {
					fprintf(stderr,"write to file failed. %s:%d.\n", F, L - 2);
					goto clean_on_error_3;
				}

				destroy_hasht(&ht);
			}

			fprintf(stderr,"File created successfully.\n");

			close_file(3, fds[0], fds[1],fds[2]);
			return 0;

			clean_on_error_3:
			close_file(3, fds[0], fds[1],fds[2]);
			delete_file(3, files[0], files[1], files[2]);
			return STATUS_ERROR;

		}

	} else { /*file already exist. we can perform CRUD operation*/

		/*freeing the String used for the input*/
		char cpy_fp[file_path.size + 1];
		memset(cpy_fp,0,file_path.size + 1);
		if(file_path.str)
			strncpy(cpy_fp,file_path.str,file_path.size);
		else
			strncpy(cpy_fp,file_path.base,file_path.size);


		char kcpy[key.size+1];
		memset(kcpy,0,key.size+1);
		if(!key.is_empty(&key)){
			if(key.str)
				strncpy(kcpy,key.str,key.size);
			else
				strncpy(kcpy,key.base,key.size);

		}

		char cpy_sd[schema_def.size + 1];
		memset(cpy_sd,0,schema_def.size + 1);
		if(!schema_def.is_empty(&schema_def)){
			if(schema_def.str)
				strncpy(cpy_sd,schema_def.str,schema_def.size);
			else
				strncpy(cpy_sd,schema_def.base,schema_def.size);

		}

		char cpy_dta[data_to_add.size + 1];
		memset(cpy_dta,0,data_to_add.size + 1);
		if(!data_to_add.is_empty(&data_to_add)){
			if(data_to_add.str)
				strncpy(cpy_dta,data_to_add.str,data_to_add.size);
			else
				strncpy(cpy_dta,data_to_add.base,data_to_add.size);

		}


		/* ====================================================	*/
		/* we open the file here and we acquire the lock, if we */
		/* if we cannot aquire the lock the program will exit	*/
		/* ====================================================	*/
		
		file_t fds[3];
		INIT_FILE_T_ARRAY(fds,3);
		char files[3][MAX_FILE_PATH_LENGTH] = {0};  
		/* init the Schema structure*/
		struct Schema sch;
		memset(&sch,0,sizeof(struct Schema));

		struct Header_d hd = {0, 0, &sch};

		if (list_def) {
			
			if(open_files(cpy_fp,fds,files,ONLY_SCHEMA) == -1) {
				return STATUS_ERROR;
			}

			if(acquire_lock(fds,LOCK_SCHEMA_FILE) == -1){
				fprintf(stderr,"(%s): File '%s' is locked by another process\n",prog,cpy_fp);
				close_file(1,fds[2]);
				return STATUS_ERROR;
			}

			/* ensure the file is a db file */
			if (is_db_file(&hd, fds) == -1) {
				release_lock(fds,LOCK_SCHEMA_FILE);
				close_file(1,fds[2]);
				return STATUS_ERROR;
			}
		} else if (swap_index){
			if(open_files(cpy_fp,fds,files,ONLY_INDEX) == -1) {
				return STATUS_ERROR;
			}

			if(acquire_lock(fds,-1) == -1){
				fprintf(stderr,"(%s): File '%s' is locked by another process\n",prog,cpy_fp);
				close_file(1,fds[0]);
				return STATUS_ERROR;
			}
		}else {
			if(open_files(cpy_fp,fds,files,-1) == -1) {
				return STATUS_ERROR;
			}

			if(acquire_lock(fds,-1) == -1){
				fprintf(stderr,"(%s): File '%s' is locked by another process\n",prog,cpy_fp);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			/* ensure the file is a db file */
			if (is_db_file(&hd, fds) == -1) {
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				fprintf(stderr,"(%s): file '%s', is not a database file.\n",prog,cpy_fp);
				return STATUS_ERROR;
			}
		}

		/*FROM HERE THE LOCK IS ALWAYS ACQUIRED */
		if(swap_index){
			HashTable *ht = NULL;
			int index = 0;
			int *p_index = &index;
			if(!read_all_index_file(fds[0], &ht, p_index)) {
				fprintf(stderr,"(%s): cannot read index file, %s:%d\n",prog,__FILE__,__LINE__-1);
				free_ht_array(ht,index);
				release_lock(fds,-1);
				close_file(1,fds[0]);
				return -1;
			}

			if(swap_indexes(bucket_ht,indexes,ht) == -1){
				fprintf(stderr,"(%s): cannot swap indexes, %s:%d.\n",prog,__FILE__,__LINE__-1);
				free_ht_array(ht,index);
				release_lock(fds,-1);
				close_file(1,fds[0]);
				return -1;
			}

			
			if(write_index(fds,index,ht,files[0]) == -1){
				fprintf(stderr,"(%s): cannot swap indexes, %s:%d.\n",prog,__FILE__,__LINE__-1);
				free_ht_array(ht,index);
				release_lock(fds,-1);
				close_file(1,fds[0]);
				return -1;
			}

			free_ht_array(ht,index);
			release_lock(fds,-1);
			close_file(1,fds[0]);
			fprintf(stdout,"(%s): index %d swapped with %d.\n",prog,bucket_ht,indexes);
			return 0;
		}

		if (del_field){
			if(drop_field(hd.sch_d,cpy_dta) == -1){
				fprintf(stderr,"field(s) not found in the file. ->(%s)\n",cpy_dta);
				free_schema(hd.sch_d);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			close_file(1,fds[2]);
			if(open_file(files[2],1,&fds[2]) == -1){ /* truncate*/
				int err = 0;
				if((err = file_error_handler(1,fds[2])) > 0){
#if defined(__linux__) || defined(__APPLE__)
					if(err == ENOENT)
						fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#elif defined(_WIN32) || defined(_WIN64)
					if(err == ERROR_FILE_NOT_FOUND)
						fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#endif
				}
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			if (!write_header(fds[2], &hd)) {
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			free_schema(hd.sch_d);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return 0;
		}

		if (index_add) {
			free_schema(hd.sch_d);

			/*  write the index file
			 *  if the user does not specify the indexes number
			 *  that they want to add, then only one will
			 *  be added.
			 *  */
			int bucket = bucket_ht > 0 ? bucket_ht : 7;
			int index_num = indexes > 0 ? indexes : 1;
			
			if (add_index(index_num, cpy_fp, bucket) == -1) {
				fprintf(stderr, "can't add index %s:%d",F, L - 2);
				goto clean_on_error_4;
			}

			char *mes = (index_num > 1) ? "indexes" : "index";
			fprintf(stderr,"%d %s added.\n", index_num, mes);

			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return 0;

			clean_on_error_4:
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return STATUS_ERROR;
		}

		if (del_file) {/*delete file*/
			free_schema(hd.sch_d);

			/* we can safely delete the files, here, this process is the only one owning locks
				for both the index and the data file */
			
#if defined(__linux__) || defined(__APPLE__)
			struct stat st;
			if(fstat(fds[0],&st) != 0){
				fprintf(stderr,"(%s): delete file '%s' failed.\n",prog,cpy_fp);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}
			ui64 file_id = st.st_ino;
#elif defined(_WIN32) || defined(_WIN64)
			BY_HANDLE_FILE_INFORMATION file_info;
			if (!GetFileInformationByHandle(fds[0],&file_info)) {
				fprintf(stderr,"(%s): delete file '%s' failed.\n",prog,cpy_fp);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return -1;
			}
			ui64 file_id = ((ui64)file_info.nFileIndexHigh << 32) | file_info.nFileIndexLow;
#endif
			close_file(3, fds[0], fds[1],fds[2]);
			delete_file(3, files[0], files[1],files[2]);
			fprintf(stderr,"file %s, deleted.\n", cpy_fp);

			/*release the lock*/
			size_t l = number_of_digit(file_id) + strlen(".lock") + 1;
			char buf[l];
			memset(buf,0,l);
			
			if(copy_to_string(buf,l,"%ld.lock",file_id) < 0){
				fprintf(stderr,"cannot release the lock");
				return STATUS_ERROR;
			}

			unlink(buf);
			return 0;
		} /* end of delete file path*/

		if (cpy_sd[0] != '\0'){ 
			if(modify_schema){
				if(change_fields_name(cpy_sd,hd.sch_d) == -1){
					fprintf(stderr,"(%s): cannot change fields name\n",prog);
					goto clean_on_error_5;
				}
			}else{
				/* add field to schema */
				int mode = check_handle_input_mode(cpy_sd,FCRT) | WR;
				if(mode == -1){
					fprintf(stderr,"(%s): check the input, value might be missng.\n",prog);
					goto clean_on_error_5;
				}
				int fields_count = 0; 
				switch(mode){
					case NO_TYPE_WR:
						if (!add_fields_to_schema(mode, fields_count, cpy_sd,hd.sch_d)) {
							goto clean_on_error_5;
						}
						break;
					case TYPE_WR:
						fields_count = count_fields(cpy_sd,NULL);
						if (fields_count > MAX_FIELD_NR || hd.sch_d->fields_num + fields_count > MAX_FIELD_NR) {
							fprintf(stderr,"Too many fields, max %d each file definition.", MAX_FIELD_NR);
							goto clean_on_error_5;
						}

						/*add field provided to the schema*/
						if (!add_fields_to_schema(mode, fields_count, cpy_sd, hd.sch_d)) {
							fprintf(stderr,"(%s): add_fields_to_schema() failed, %s:%d\n",prog,F,L-1);
							goto clean_on_error_5;
						}

						break;
					case HYB_WR:
						if (!add_fields_to_schema(mode, fields_count,cpy_sd, hd.sch_d)) {
							fprintf(stderr,"(%s): add_fields_to_schema() failed, %s:%d\n",prog,F,L-1);
							goto clean_on_error_5;
						}
						break;
					default:
						goto clean_on_error_5;
				}
			}


			close_file(1,fds[2]);
			if( open_file(files[2],1,&fds[2]) == -1){
				int err = 0;
				if((err=file_error_handler(1,fds[2])) > 0){
#if defined(__linux__) || defined(__APPLE__)
					if(err == ENOENT)
						fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#elif defined(_WIN32) || defined(_WIN64)
					if(err == ERROR_FILE_NOT_FOUND)
						fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#endif
				}
				goto clean_on_error_5;
			}

			if (!write_header(fds[2], &hd)) {
				fprintf(stderr,"write to file failed, %s:%d.\n", F, L - 2);
				goto clean_on_error_5;
			}


			release_lock(fds,-1);
			fprintf(stderr,"data added to schema!\n");
			close_file(3, fds[0], fds[1],fds[2]);
			free_schema(&sch);
			return 0;

			clean_on_error_5:
			fprintf(stderr,"cannot add data to the schema!\n");
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			free_schema(hd.sch_d);
			return STATUS_ERROR;
			
		} /* end of add new fields to schema path*/

		if (del) { 
			/* del a record in a file or the all content in the file */

			free_schema(hd.sch_d);
			if (options) {
				if (option) {
					switch (convert_options(option)) {
					case ALL:
					{
						int ind = 0, *p_i_nr = &ind;
						if (!indexes_on_file(fds[0], p_i_nr)) {
							fprintf(stderr,"er index nr,%s:%d.\n", F, L - 4);
							goto option_clean_on_error;
						}

						int buc_t = 0, *pbuck = &buc_t;
						if (!nr_bucket(fds[0], pbuck)) {
							fprintf(stderr,"er index nr,%s:%d.\n", F, L - 4);
							goto option_clean_on_error;
						}
						/* create *p_i_nr of ht and write them to file*/
						HashTable *ht = (HashTable*)malloc(*p_i_nr * sizeof(HashTable));
						if (!ht) {
							fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
							goto option_clean_on_error;
						}
						memset(ht,0,*p_i_nr * sizeof *ht);
						int i;
						for (i = 0; i < *p_i_nr; i++) {
							HashTable ht_n;
							memset(&ht_n,0,sizeof(HashTable));
							ht_n.size = *pbuck;
							ht_n.write = write_ht;

							ht[i] = ht_n;
						}

						close_file(1, fds[0]);
						/*opening with O_TRUNC*/
						if(open_file(files[0], 1,&fds[0]) == -1){
							int err = 0;
							if((err=file_error_handler(1,fds[2])) > 0){
#if defined(__linux__) || defined(__APPLE__)
								if(err == ENOENT)
									fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#elif defined(_WIN32) || defined(_WIN64)
								if(err == ERROR_FILE_NOT_FOUND)
									fprintf(stderr,"(%s): '%s' does not exist",prog,file_path.base);
#endif
							}

							goto option_clean_on_error;
						}

						/*  write the index file */

						if (!write_index_file_head(fds[0], *p_i_nr)) {
							fprintf(stderr,"write to file failed,%s:%d",F, L - 2);
							free_ht_array(ht,*p_i_nr);
							goto option_clean_on_error;
						}

						for (i = 0; i < *p_i_nr; i++) {
							if (!write_index_body(fds[0], i, &ht[i])) {
								fprintf(stderr,"write to file failed. %s:%d.\n", F, L - 2);
								free_ht_array(ht,*p_i_nr);
								goto option_clean_on_error;
							}
						}

						/* release the lock */
						release_lock(fds,-1);

						close_file(3, fds[0], fds[1],fds[2]);
						free_ht_array(ht,*p_i_nr);
						fprintf(stderr,"all record deleted from %s file.\n", cpy_fp);
						return 0;

						option_clean_on_error:
						release_lock(fds,-1);
						close_file(3, fds[0], fds[1],fds[2]);
						return STATUS_ERROR;
					}
					case AAR:
					case FRC:
						fprintf(stderr,"option '%s' not valid for delete path,\n",option);
						release_lock(fds,-1);
						close_file(3, fds[0], fds[1],fds[2]);
						return STATUS_ERROR;
					default:
						fprintf(stderr,"options not valid.\n");
						release_lock(fds,-1);
						close_file(3, fds[0], fds[1],fds[2]);
						return STATUS_ERROR;
					}
				}
			}/*end of option path*/

			/* delete the record specified by the -D option, in the index file*/
			HashTable *ht = NULL;
			int index = 0;
			int *p_index = &index;
			/* load all indexes in memory */
			if (!read_all_index_file(fds[0], &ht, p_index)) {
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}

			int indexes = 0;
			if (!indexes_on_file(fds[0], &indexes)) {
				fprintf(stderr,"indexes_on_file() failed, %s:%d.\n", F, L - 2);
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}

			if (index_nr >= indexes) {
				fprintf(stderr,"index out of bound.\n");
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}

			int key_type = 0;
			void *key_conv = key_converter(kcpy, &key_type);

			Node *record_del = NULL;
			if (key_conv) {
				record_del = ht_delete(key_conv, &ht[index_nr], key_type);
				free(key_conv);

			}
#if defined(__linux__) || defined(__APPLE__)
			else if (key_type == STR)
#elif defined(_WIN32) || defined(_WIN64)
			else if (key_type == STR_KEY)
#endif
			{
					record_del = ht_delete((void *)kcpy, &ht[index_nr], key_type);
			} else {
				fprintf(stderr,"error key_converter().\n");
				free_ht_array(ht, index);
				goto clean_on_error_6;
			}


			if (!record_del) {
				fprintf(stderr,"record %s not found.\n", kcpy);
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}
			
			/*this will save the record that we deleted,
			 * so we can undo this operations */
			/*TODO: fix this system!!!*/
#if defined(__linux__) || defined(__APPLE__)
			if(record_del->key.type == STR)
#elif defined(_WIN32) || defined(_WIN64)
			if(record_del->key.type == STR_KEY)
#endif
			{
 				if(journal(fds[0], 
						record_del->value, 
						(void*)record_del->key.k.s, 
						record_del->key.type, 
						J_DEL) == -1){
					fprintf(stderr,"(%s): failed to save del data.\n",prog);
				}
			}else{
				ui32 kn = record_del->key.k.n;
				if(journal(fds[0], 
						record_del->value, 
						(void*)&kn, 
						record_del->key.type, 
						J_DEL) == -1){
					fprintf(stderr,"(%s): failed to save del data.\n",prog);
				}
			}

			fprintf(stderr,"record %s deleted!.\n", kcpy);
			

			free_ht_node(record_del);
			close_file(1, fds[0]);
			/*opening with o_trunc*/
			if(open_file(files[0], 1,&fds[0]) == -1){
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}
			/*  write the index file */
			if (!write_index_file_head(fds[0], index)) {
				fprintf(stderr,"write to file failed, %s:%d", F, L- 2);
				free_ht_array(ht,index);
				goto clean_on_error_6;
			}

			int i = 0;
			for (i = 0; i < index; i++){

				if (!write_index_body(fds[0], i, &ht[i])) {
					fprintf(stderr,"write to file failed. %s:%d.\n", F, L - 2);
					free_ht_array(ht,index);
					goto clean_on_error_6;
				}
			}

			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			free_ht_array(ht,index);
			return 0;
			
			clean_on_error_6:
			free_schema(hd.sch_d);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return STATUS_ERROR;
		} /*end of del path (either del the all content or a record )*/

		if(!update && cpy_dta[0] != '\0') { 
			/* append data to the specified file*/
			struct Record_f rec;
			memset(&rec,0,sizeof(struct Record_f));

			int lock_f = 0;/*we already have a lock*/
			int check = 0;
			int option_value= -1;
			if(option){
				option_value= convert_options(option);
				if(option_value == -1){
					fprintf(stderr,"option '%s' not valid\n",option);
					goto clean_on_error_7;
				}
			}
			if(( check = check_data(cpy_fp,cpy_dta,fds,files,&rec,&hd,&lock_f,option_value,0)) == -1) {
				fprintf(stderr,"(%s): schema different than file definition or worng syntax\n",prog);
				goto clean_on_error_7;
			}


			ui32 n = 0;
			if(kcpy[0] == '\0'){
				i64 k = generate_numeric_key(fds,INCREM,-1,NULL);
				if(k == -1){
					fprintf(stderr,"increment key failed. %s:%d\n",__FILE__,__LINE__-2);
					goto clean_on_error_7;
				}

				if(k > (i64)(MAX_KEY-1)){
					fprintf(stderr,"(%s): key is out of range.\n",prog);
					goto clean_on_error_7;
				}

				n = (ui32)k;
				
#if defined(__linux__) || defined(__APPLE__)
			if(write_record(fds,(void *)&n, UINT, &rec, update, files, &lock_f, -1,hd.sch_d) == -1)
#elif defined(_WIN32) || defined(_WIN64)
			if(write_record(fds,(void *)&n, UINT_KEY, &rec, update, files, &lock_f, -1,hd.sch_d) == -1)
#endif
			{
					fprintf(stderr, "write_record failed %s:%d.\n",__FILE__,__LINE__-1);
					goto clean_on_error_7;
				}

			} else {
				if(write_record(fds,(void *)kcpy, -1, &rec, update, files, &lock_f, -1,hd.sch_d) == -1){
					fprintf(stderr, "write_record failed %s:%d.\n",__FILE__,__LINE__-1);
					goto clean_on_error_7;
				}
			}

			free_record(&rec, rec.fields_num);
			free_schema(&sch);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			if(kcpy[0] == '\0')
				fprintf(stderr,"record %u wrote succesfully.\n", n);
			else
				fprintf(stderr,"record %s wrote succesfully.\n", kcpy);
			return 0;

			clean_on_error_7:
			free_record(&rec, rec.fields_num);
			free_schema(&sch);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return STATUS_ERROR;
		}

		if (update && cpy_dta[0] != '\0' && kcpy[0] != '\0') { 
			/* updating an existing record */
			struct Record_f rec;
			memset(&rec,0,sizeof(struct Record_f));

			int lock_f = 0; /*we already have a lock*/
			int check = 0;
			int option_value = -1;
			if(option){
				option_value = convert_options(option);
				if(option_value == -1){
					fprintf(stderr,"option '%s' not valid\n",option);
					goto clean_on_error_7;
				}
			}

			if((check = check_data(cpy_fp,cpy_dta,fds,files,&rec,&hd,&lock_f,option_value,update)) == -1) goto clean_on_error;

			if(update_rec(cpy_fp,fds,kcpy,-1,&rec,hd,check,&lock_f,option,index_nr) == -1) goto clean_on_error;

			fprintf(stderr,"record %s updated!\n", kcpy);
			free_record(&rec, rec.fields_num);
			free_schema(hd.sch_d);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return 0;

			clean_on_error:
			free_record(&rec, rec.fields_num);
			free_schema(hd.sch_d);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return STATUS_ERROR;

		} /*end of update path*/
				
		/* reading the file to show data */

		if (list_def) { /* show file definitions */
			print_schema(*hd.sch_d);
			free_schema(hd.sch_d);
			release_lock(fds,LOCK_SCHEMA_FILE);
			close_file(1,fds[2]);
			return 0;
		}

		if (list_keys) { /*display the keys in the file*/
			
			HashTable ht = {0};
			HashTable *p_ht = &ht;
			if (!read_index_nr(index_nr, fds[0], &p_ht)) {
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return STATUS_ERROR;
			}

			struct Keys_ht keys_data;
			memset(&keys_data,0,sizeof(struct Keys_ht));
			int er = 0;
			if((er = keys(p_ht,&keys_data)) == -1){
				fprintf(stderr,"(%s): cannot get all keys from index file.\n",prog);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return STATUS_ERROR;
			}

			if(er == NO_ELEMENT){
				fprintf(stderr,"(%s): file is empty.\n",prog);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return 0;
			}

			char keyboard = '0';
			int end = len(ht), i = 0, j = 0;
			for (i = 0, j = i; i < end; i++) {
				switch (keys_data.keys[i].type){
#if defined(__linux__) || defined(__APPLE__)
				case STR:
#elif defined(_WIN32) || defined(_WIN64)
				case STR_KEY:
#endif
					fprintf(stderr,"%d. %s\n", ++j, keys_data.keys[i].k.s);
					break;
#if defined(__linux__) || defined(__APPLE__)
				case UINT:
#elif defined(_WIN32) || defined(_WIN64)
				case UINT_KEY:
#endif
				{
					if(keys_data.keys[i].size == 16)
						fprintf(stderr,"%d. %u   ", ++j, keys_data.keys[i].k.n16);
					else
						fprintf(stderr,"%d. %u   ", ++j, keys_data.keys[i].k.n);
					print_pack_str(keys_data.keys[i].paked_k);
					fprintf(stderr,"\n");
					break;
				}
				default:
					break;
				}

				if (i > 0 && (i % 20 == 0)){
					fprintf(stderr,"press return key. . .\nenter q to quit . . .\n");
					keyboard = (char)getc(stdin);
				}

				if (keyboard == 'q')
					break;
			}

			release_lock(fds,-1);
			destroy_hasht(p_ht);
			close_file(3, fds[0], fds[1],fds[2]);
			free_keys_data(&keys_data);
			free_schema(hd.sch_d);
			return 0;
		}

		if(nr_of_record_display){

			HashTable ht = {0};
			HashTable *p_ht = &ht;
			if (!read_index_nr(index_nr, fds[0], &p_ht)) {
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return STATUS_ERROR;
			}

			struct Keys_ht keys_data;
			memset(&keys_data,0,sizeof(struct Keys_ht));
			int er = 0;
			if((er = keys(p_ht,&keys_data)) == -1){
				fprintf(stderr,"(%s): cannot get all keys from index file.\n",prog);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return STATUS_ERROR;
			}

			if(er == NO_ELEMENT){
				fprintf(stderr,"(%s): file is empty.\n",prog);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				free_schema(hd.sch_d);
				return 0;
			}

			fprintf(stdout,"(%s): %d records in '%s'.\n",prog,keys_data.length,cpy_fp);

			destroy_hasht(p_ht);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			free_keys_data(&keys_data);
			free_schema(hd.sch_d);
			return 0;




		}

		if (kcpy[0] != '\0') { 
			/*display record*/

			struct Record_f rec;
			memset(&rec,0,sizeof(struct Record_f));

			int err = 0;
			if((err = get_record(-1,cpy_fp,&rec,(void *)kcpy,-1, hd,fds,index_nr >= 0 ? index_nr : -1)) == -1){
				free_record(&rec,rec.fields_num);
				free_schema(hd.sch_d);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			if( err == KEY_NOT_FOUND){
				free_schema(hd.sch_d);
				release_lock(fds,-1);
				close_file(3, fds[0], fds[1],fds[2]);
				return STATUS_ERROR;
			}

			if (rec.count == 1) {
				print_record(1, rec);
				free_record(&rec, rec.fields_num);
			}else {
				print_record(rec.count, rec);
				free_record(&rec, rec.fields_num);
			}

			if(ram.mem) close_ram_file(&ram);

			free_schema(hd.sch_d);
			release_lock(fds,-1);
			close_file(3, fds[0], fds[1],fds[2]);
			return 0;
		}
		/*ALL THE PATHS RETURN EITHER ERROR OR SUCCSES*/
		/*THIS SHOULD BE UNREACHABLE*/
		release_lock(fds,-1);
		close_file(3, fds[0], fds[1],fds[2]);
		return 0;
	}

	return 0;
} /*-- program end --*/
