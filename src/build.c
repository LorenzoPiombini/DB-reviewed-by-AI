#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "db_types.h"
#include "common.h"
#include "build.h"
#include "file.h"
#include "lock.h"
#include "helper.h"
#include "str_op.h"
#include "debug.h"
#include "crud.h"
#include "globals.h"
#include "string_utilities.h"
#if defined(_WIN32)
	#include <windows.h>
#endif

static char prog[] = "db";
int get_number_value_from_txt_file(FILE *fp)
{

	char buffer[250];
	ui16 i = 0;
	ui8 is_num = 1;
	if (fgets(buffer, sizeof(buffer), fp))
	{
		buffer[complementary_span(buffer, "\n")] = '\0';

		for (i = 0; buffer[i] != '\0'; i++)
		{
			if (!isdigit(buffer[i]))
			{
				is_num = 0;
				break;
			}
		}
	}

	if (!is_num)
	{
		printf("first line is not a number, %s:%d.\n", __FILE__, __LINE__ - 1);
		return 0;
	}

	long l = string_to_long(buffer);
	if(error_value == INVALID_VALUE) 
		return 0;
	else 
		return l;
}

unsigned char create_system_from_txt_file(char *txt_f)
{
	FILE *fp = fopen(txt_f, "r");
	if (!fp)
	{
		printf("failed to open %s. %s:%d.", txt_f, F, L - 3);
		return 0;
	}
	int lines = 0;
	int i = 0;
	int size = return_bigger_buffer(fp, &lines);
	char buffer[size];
	char **files_n = (char**)malloc(lines * sizeof(char *));
	char **schemas = (char**)malloc(lines * sizeof(char *));
	if(!files_n || !schemas){
		fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
		if(files_n)
			free(files_n);
		if(schemas)
			free(schemas);
		return 0;
	}

	memset(files_n,0,lines*sizeof(char*));
	memset(schemas,0,lines*sizeof(char*));
	int buckets[lines];
	int indexes[lines];
	int file_field[lines];
	memset(buckets,0,lines);
	memset(indexes,0,lines);
	memset(file_field,0,lines);


	while (fgets(buffer, sizeof(buffer), fp))
	{
		buffer[complementary_span(buffer, "\n")] = '\0';
		if (buffer[0] == '\0' || buffer[0] == '#' || buffer[0] == ' ' || buffer[0] == '\t')continue;

		char *t = tok(buffer, "|"); 
		if(t){ 
			files_n[i] = duplicate_str(t);
			if(!files_n[i]){
				free_strs(lines, 2, files_n, schemas);
				fprintf(stderr,"wrong file or syntax error\n");
				return 0;
			}
		}else{
			free_strs(lines, 2, files_n, schemas);
			fprintf(stderr,"wrong file or syntax error\n");
			return 0;
		}
			
		
		t = tok(NULL, "|");
		if(t){
			schemas[i] = duplicate_str(t);
			if(!schemas[i]){
				free_strs(lines, 2, files_n, schemas);
				fprintf(stderr,"wrong file or syntax error\n");
				return 0;
			}
		}else{
			free_strs(lines, 2, files_n, schemas);
			fprintf(stderr,"wrong file or syntax error\n");
			return 0;
		}

		t = tok(NULL,"|");
		long l = string_to_long(t);
		if(error_value == INVALID_VALUE)
			buckets[i] = 0;
		else 
			buckets[i] = (int) l; 


		t = tok(NULL, "|");

		l = string_to_long(t);
		if(error_value == INVALID_VALUE)
			indexes[i] = 0; 
		else 
			indexes[i] = (int) l; 

		t = tok(NULL,"|");

		l = string_to_long(t);
		if(error_value == INVALID_VALUE)
			file_field[i] = 0; 
		else 
			file_field[i] = (int) l; 

		i++;
	}

	fclose(fp);

	/* create file */
	int j = 0;
	for (j = 0; j < i; j++)
	{
		char files[3][MAX_FILE_PATH_LENGTH] = {0};
		three_file_path(files_n[j],files);
		file_t fd_schema,fd_index,fd_data;

		if(file_field[j]){
			if(create_file(files[2],&fd_schema) == -1){
				file_error_handler(1,fd_schema);
				fprintf(stderr, "system already exist!\n");
				free_strs(lines, 2, files_n, schemas);
				return 0;
			}
		}else{
			if(create_file(files[0],&fd_index) == -1
					|| create_file(files[1],&fd_data) == -1
					|| create_file(files[2],&fd_schema) == -1){
					file_error_handler(3,fd_schema,fd_data,fd_index);
				fprintf(stderr, "system already exist!\n");
				free_strs(lines, 2, files_n, schemas);
				return 0;
			}
		}


		if (create_file_with_schema(fd_schema, fd_index, schemas[j], buckets[j], indexes[j],file_field[j]) == -1){
			if(file_field[j]){
				delete_file(1,files[2]);
				close_file(1,fd_schema);
			}else{
				delete_file(3, files[0], files[1],files[2]);
				close_file(3, fd_data, fd_index,fd_schema);
			}
			free_strs(lines, 2, files_n, schemas);
			return 0;
		}

		if(file_field[j])
			close_file(1,fd_schema);
		else
			close_file(3, fd_data, fd_index,fd_schema);
	}

	free_strs(lines, 2, files_n, schemas);
	return 1;
}

int return_bigger_buffer(FILE *fp, int *lines)
{
	char buffer[5000];
	int max = 0;
	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		(*lines)++;
		int temp = 0;
		if ((temp = (complementary_span(buffer, "\n") + 1)) > max)
			max = temp;
	}
	rewind(fp);
	return max + 10;
}

/* 
 * import_data_to_system 
 * is to be use to import old EZgen systems
 * */
int import_data_to_system(char *data_file)
{
	FILE *fp = fopen(data_file,"r"); 
	if(!fp){
		fprintf(stderr,"cannot open %s.\n",data_file);
		return -1;
	}
	
	if(fseek(fp,0,SEEK_END) == -1) return -1;
	
	file_offset size = ftell(fp);
	rewind(fp);	

	char *content = (char*)malloc((size+1)*sizeof(char));
	if(!content){
		fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
		return -1;
	}

	memset(content,0,size+1);
	if(fread(content,size,1,fp) != 1){
		fprintf(stderr,"fread() failed, %s:%d.\n",F,L-1);
		free(content);
		fclose(fp);
		return -1;
	}

	fclose(fp);

	/*  global variable that change the behavior of the inner
	 *  working of the check_data functions
	 *  it affects how the string functions detect types */
	__IMPORT_EZ = 1;

#if defined(_WIN32)
	HANDLE fds[3];
	memset(fds,0,sizeof(HANDLE)*3);
#else
	int fds[3];
	memset(fds,-1,sizeof(int)*3);
#endif
	char files[3][MAX_FILE_PATH_LENGTH] = {0};  
	/* init the Schema structure*/
	struct Schema sch;
	memset(&sch,0,sizeof(struct Schema));
	struct Record_f rec;
	memset(&rec,-1,sizeof(struct Record_f));
	struct Header_d hd = {0, 0, &sch};


	char *delim = NULL;
	file_offset start = 0;
	char file_name[MAX_FILE_PATH_LENGTH] = {0};
	int lock_f = 1;/*avoid to lock files in inner funcions*/
	while((delim = strstr(&content[start],"\n"))){
		size_t l = 0;
		file_offset end = delim - content;		
		if(start != 0) 
			l = end - start + 1; 
		else
			l = end + 1;

		char buf[l];				
		memset(buf,0,l);
		strncpy(buf,&content[start],l-1);
		/* set the new start */
		start = (delim +1) - content;
	        *delim = ' ';	
		

		if(buf[0] == '@'){
			memset(file_name,0,MAX_FILE_PATH_LENGTH);
			if(open_files(&buf[1],fds,files,0) == -1){
				free(content);
				return STATUS_ERROR;
			}
			if(is_db_file(&hd,fds) == -1){
				free(content);
				return STATUS_ERROR;
			}
			strncpy(file_name,&buf[1],strlen(&buf[1]));
			printf("importing '%s' ...\n",file_name);
			continue;
		}

		if(buf[0] == '='){

			if(os_write(fds[1],ram.mem,ram.size) == -1)
			{
				close_file(3,fds[0],fds[1],fds[2]);
				if(g_ht) free_ht_array(g_ht,g_index);
				close_ram_file(&ram);
				free(content);
				return STATUS_ERROR;
			}

			if(write_index(fds,g_index,g_ht,files[0]) == -1){
				close_file(3,fds[0],fds[1],fds[2]);
				close_ram_file(&ram);
				free(content);
				return STATUS_ERROR;
			}

			g_index = 0;
			g_ht = NULL;
			memset(&sch,0,sizeof(struct Schema));
			clear_ram_file(&ram);
			close_file(3,fds[0],fds[1],fds[2]);
			continue;
		}

		if(buf[0] == ' ' || buf[0] == '\0' || buf[0] == '\n' || buf[0] == '#') continue;

		/*write to file*/
		char *d = strstr(buf,":{@");
		if(!d){
			fprintf(stderr,"(%s): delim ':{@' not found, import of '%s' aborted.\n",prog,file_name);
			close_file(3,fds[0],fds[1],fds[2]);
			free(content);
			close_ram_file(&ram);
			return -1;
		}

		/* separating the data from the key*/		
		char *dd = d;
		d += 3;
		*dd = '\0';
		size_t sz = strlen(buf);
		char cpy[sz+1];
		memset(cpy,0,sz+1);
		strncpy(cpy,buf,sz);

		
		size_t key_sz = strlen(d) + 1;
		char key[key_sz];
		memset(key,0,key_sz);
		strncpy(key,d,key_sz -1);
		
		printf("key: '%s' - '%s'\n",key,file_name);
		/*check data (schema) and writing to file*/
		if(check_data(file_name,cpy,fds,files,&rec,&hd,&lock_f,-1,0) == -1) {
			printf("key value: %s\n",key);
			free_record(&rec,rec.fields_num);
			free(content);
			close_ram_file(&ram);
			close_file(3,fds[0],fds[1],fds[2]);
			return STATUS_ERROR;
		}

	
#if defined(_WIN32)
		int key_type = STR_KEY;
#else
		int key_type = STR;
#endif
		if(is_integer(key)) key_type = -1;

		if(write_record(fds,(void*)key,key_type,&rec, 0,files,&lock_f,IMPORT,hd.sch_d) == -1) {
			free_record(&rec,rec.fields_num);
			memset(&rec,0,sizeof(struct Record_f));
			continue;
		}
	
		free_record(&rec,rec.fields_num);
		memset(&rec,0,sizeof(struct Record_f));
	}
	free(content);
	close_ram_file(&ram);
	__IMPORT_EZ = 0;
	return 0;
}
