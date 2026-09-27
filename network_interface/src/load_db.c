#include <stdio.h>
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>              /* Definition of O_* constants */
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

/* these are my library */
#include "db_types.h"
#include "crud.h"
#include "lock.h"
#include "file.h"
#include "str_op.h"
#include "common.h"
#include "load_db.h"
#include "request.h"
#include "end_points.h"
#include "string_utilities.h"

static char prog[] = "db_interface";
static char *convert_json(char* body);

const int EIGTH_Kib = 1024 * 8;
int load_resource_db(struct Request *req, struct Content *cont,int data_sock)
{
	int resource = map_end_point(req->resource); 
	if(resource == -1) return -1;

	switch(req->method){
	case POST:
	{
		switch(resource){
		case NEW_CUST:
		{
			/*convert json in db_string*/
			char *db = 0x0;
			if(req->req_body.d_cont)
				db = convert_json(req->req_body.d_cont);
			else
				db = convert_json(req->req_body.content);

			assert(db != NULL);
			if(db[0] == '\0') return -1;

			/*2 stands for  1 '\0', and 1 for the operation*/
			size_t size_buffer = strlen(db) + 2;
			char *buffer = (char *) malloc(size_buffer);

			buffer[0] = resource + '0';
			strncpy(&buffer[1],db,size_buffer-1);

			/*send data to the worker process*/
			if(write(data_sock,buffer,strlen(buffer)) == -1) return -1;

			/*TODO: refactor the socket comunication so that you read once with 
			 * the size of the next message then you allocate a buffer accordangly so 
			 * you can be eficient*/
			char read_buffer[MAX_CONT_SZ];
			if(read(data_sock,read_buffer,MAX_CONT_SZ) == -1) return -1;

			if(read_buffer[0] == '\0') return -1;

			if(copy_to_string(cont->cnt_st,1024,"%s",read_buffer) == -1){
				/*log error*/
				return -1;
			}
			cont->size = strlen(cont->cnt_st);
			return 0;
		}
		case NEW_SORD:
		case UPDATE_SORD:
		{

			/*save the sales order in the db */
			char *db = NULL;
			if(req->req_body.d_cont)
				db = convert_json(req->req_body.d_cont);
			else
				db = convert_json(req->req_body.content);

			assert(db != NULL);

			if(db[0] == '\0') return -1;

			/*process the string and separate the two file sintax*/

			char *lines_start = strstr(db,"sales_orders_lines");
			if(!lines_start) return -1;


			size_t lines_len = strlen((lines_start + strlen("sales_orders_lines:")));
			char orders_line[lines_len+1];
			memset(orders_line,0,lines_len+1);
			strncpy(orders_line,lines_start + strlen("sales_orders_lines:"),lines_len);

			char orders_head[((lines_start - db) - strlen("sales_orders_head:")) + 1];
			memset(orders_head,0,((lines_start - db) -strlen("sales_orders_head:")) +1);
			strncpy(orders_head,&db[strlen("sales_orders_head:")],((lines_start - db)-strlen("sales_orders_head:")));


			size_t size_buffer = 0;
			char *buffer = NULL;
			if(resource == NEW_SORD){
			/* 3 is 
			 *  1 for '^'
			 *  1 for instruction byte
			 *  1 for '\0';
			 * */
				size_buffer = sizeof(orders_head) + sizeof(orders_line)+3;
				buffer = (char*)malloc(size_buffer);
				if(!buffer) return -1;

				memset(buffer,0,size_buffer);

				/*parse data to buffer*/
				buffer[0] = resource + '0';
				strncpy(&buffer[1],orders_head,strlen(orders_head));
				strncpy(&buffer[1+strlen(orders_head)],"^",2);
				strncpy(&buffer[1+strlen(orders_head)+1],orders_line,strlen(orders_line));
			}else{
				/*parse a buffer for the update operation*/
				char *p = req->resource;
				p += strlen(UPDATE_ORDERS) + 1;

			/* 4 is 
			 *  2 for '^'
			 *  1 for instruction byte
			 *  1 for '\0';
			 * */
				
				size_buffer = sizeof(orders_head) + sizeof(orders_line)+ strlen(p) +4;
				buffer = (char*)malloc(size_buffer);
				if(!buffer) return -1;

				memset(buffer,0,size_buffer);

				buffer[0] = resource + '0';
				strncpy(&buffer[1],p,strlen(p));
				int position = 1+strlen(p);
				strncpy(&buffer[position],"^",2);
				position += 1;
				strncpy(&buffer[position],orders_head,strlen(orders_head));
				position += strlen(orders_head);
				strncpy(&buffer[position],"^",2);
				position += 1;
				strncpy(&buffer[position],orders_line,strlen(orders_line));
			}

			/*send data to the worker process*/
			if(write(data_sock,buffer,strlen(buffer)) == -1){
				return -1;
			}

			char read_buffer[MAX_CONT_SZ];
			if(read(data_sock,read_buffer,MAX_CONT_SZ) == -1) return -1;

			if(read_buffer[0] == '\0') return -1;

			if(copy_to_string(cont->cnt_st,1024,"%s",read_buffer) == -1){
				/*log error*/
				return -1;
			}
			cont->size = strlen(cont->cnt_st);
			return 0;
		}
		case S_ORD:
		{
			break;
		}
		default:
		break;
		}
		break;
	}
	case GET:
	{
		switch(resource){
		case CUSTOMER_GET:
		{

			/*get the Key from the request*/
			char *p = req->resource;
			p += strlen(CUSTOMERS) + 1;

			size_t key_size = strlen(p);
			char buffer[key_size+2];
			memset(buffer,0,key_size+2);

			buffer[0] = resource + '0';
			strncpy(&buffer[1],p,key_size);

			if(write(data_sock,buffer,sizeof(buffer)) == -1){
				return -1;
			}

			char *read_buffer = (char*)malloc(EIGTH_Kib*4);
			if(!read_buffer) return -1;

			/*read data from worker proc*/

			memset(read_buffer,0,EIGTH_Kib*4);
			ssize_t bread = 0;
			if((bread = read(data_sock,read_buffer,(EIGTH_Kib*4)-1)) == -1) return -1;

			if(bread == ((EIGTH_Kib * 4) - 1)){
					fprintf(stderr,"code refactor neened %s:%d\n",__FILE__,__LINE__-1);
					return -1;
			}

			if(read_buffer[0] == '\0') return -1;

			size_t mem_size = strlen(read_buffer) + 1;
			cont->cnt_dy = (char*) malloc(mem_size);
			if(!cont->cnt_dy) return -1;

			cont->capacity =mem_size;
			cont->size = mem_size;
			if(copy_to_string(cont->cnt_dy,strlen(read_buffer)+1,"%s",read_buffer) == -1) return -1;
			return 0;
		}
		case CUSTOMER_GET_ALL:
		case S_ORD:
		{		
			/*send data to the worker process*/
			char buffer[2];
			memset(buffer,0,2);
			if(resource == S_ORD)
				buffer[0] = S_ORD + '0';
			else
				buffer[0] = CUSTOMER_GET_ALL + '0';

			if(write(data_sock,buffer,sizeof(buffer)) == -1){
				return -1;
			}

			char *read_buffer = (char*)malloc(EIGTH_Kib*4);
			if(!read_buffer) return -1;

			/*read data from worker proc*/

			memset(read_buffer,0,EIGTH_Kib*4);
			ssize_t bread = 0;
			if((bread = read(data_sock,read_buffer,(EIGTH_Kib*4)-1)) == -1) return -1;

			if(bread == ((EIGTH_Kib * 4) - 1)){
					fprintf(stderr,"code refactor neened %s:%d\n",__FILE__,__LINE__-1);
					return -1;
			}

			if(read_buffer[0] == '\0') return -1;

			size_t mem_size = strlen(read_buffer) + 1;
			cont->cnt_dy = (char*) malloc(mem_size);
			if(!cont->cnt_dy) return -1;

			cont->capacity =mem_size;
			cont->size = mem_size;
			if(copy_to_string(cont->cnt_dy,strlen(read_buffer)+1,"%s",read_buffer) == -1) return -1;

			return 0;
		}
		case S_ORD_GET:
		{
			char *p = req->resource;
			p += strlen(SALES_ORDERS) + 1;

			size_t key_size = strlen(p);
			char buffer[key_size+2];
			memset(buffer,0,key_size+2);

			buffer[0] = resource + '0';
			strncpy(&buffer[1],p,key_size);

			if(write(data_sock,buffer,strlen(buffer)) == -1) return -1;

			char *read_buffer = (char*) malloc(EIGTH_Kib);
			if(!read_buffer) return -1;

			ssize_t bread = 0;
			if((bread = read(data_sock,read_buffer,EIGTH_Kib) == -1)) return -1;

			if(bread == EIGTH_Kib){
				/*code refactor*/					
				fprintf(stderr,"code refactor needed %s:%d\n",__FILE__,__LINE__-4);
				return -1;
			}

			strncpy(cont->cnt_st,read_buffer,strlen(read_buffer));		
			cont->size = strlen(read_buffer);
			return 0;
#if 0

			ui32 k = 0;
			ui8 type = is_num(p);

			char *key = NULL;
			switch(type){
				case UINT:
					{
						/*convert to number */	
						errno = 0;
						long l = string_to_long(p);
						if(errno == EINVAL){
							/*log error*/
							return -1;
						}
						k = (ui32) l;

						int fds[3];
						memset(fds,-1,sizeof(int)*3);
						char files[3][1024];
						memset(files,0,3*1024);
						struct Record_f rec;
						memset(&rec,0,sizeof(struct Record_f));
						struct Schema sch;
						memset(&sch,0,sizeof(struct Schema));
						struct Header_d hd = {0, 0, &sch};

						if(open_files(SALES_ORDERS_H,fds, files, -1) == -1) return -1;
						if(is_db_file(&hd,fds) == -1) goto s_ord_get_exit_error; 

						if(get_record(-1,SALES_ORDERS_H,&rec,(void *)&k,type, hd,fds) == -1) goto s_ord_get_exit_error; 

						int field_ix = 0;
						int rec_index = 0;
						struct Record_f *r = &rec;
						if(!get_index_rec_field("lines_nr", &r,1, &field_ix, &rec_index)) goto s_ord_get_exit_error;

						long lines = rec.fields[field_ix].data.l;

						/*stringfy the orders head here*/
						cont->cnt_dy = (char*)malloc(PAGE_SIZE);
						if(!cont->cnt_dy){
							/*log error*/	
							goto s_ord_get_exit_error;
						}

						/*message formatting*/
						size_t position_in_the_message = strlen("{ ");
						strncpy(cont->cnt_dy,"{ ",strlen("{ ")+1);
						strncpy(&cont->cnt_dy[position_in_the_message],"\"message\" : { ",strlen("\"message\" : { ") + 1);
						position_in_the_message += strlen("\"message\" : { ");

						strncpy(&cont->cnt_dy[position_in_the_message],"\"sales_orders_head\"",strlen("\"sales_orders_head\"") + 1);
						position_in_the_message += strlen("\"sales_orders_head\"");
						strncpy(&cont->cnt_dy[position_in_the_message]," : { ",strlen(" : { ")+1);
						position_in_the_message += strlen(" : { ");

						if(data_to_json(&cont->cnt_dy,&rec,S_ORD_GET) == -1) goto s_ord_get_exit_error;

						position_in_the_message = strlen(cont->cnt_dy);
						strncpy(&cont->cnt_dy[position_in_the_message],", ",3);
						position_in_the_message += 2;

						strncpy(&cont->cnt_dy[position_in_the_message],"\"sales_orders_lines\"",strlen("\"sales_orders_lines\"")+1);
						position_in_the_message += strlen("\"sales_orders_lines\"");
						strncpy(&cont->cnt_dy[position_in_the_message]," : { ",strlen(" : { ")+1);
						position_in_the_message += strlen(" : { ");

						free_record(&rec,rec.fields_num);
						close_file(3,fds[0],fds[1],fds[2]);
						memset(&rec,0,sizeof(struct Record_f));
						memset(&sch,0,sizeof(struct Schema));
						memset(&hd,0,sizeof(struct Header_d));
						memset(files,0,3*1024);
						memset(fds,-1,sizeof(int)*3);
						hd.sch_d = &sch;

						if(open_files(SALES_ORDERS_L,fds, files, -1) == -1) goto s_ord_get_exit_error;
						if(is_db_file(&hd,fds) == -1) goto s_ord_get_exit_error;

						long i;	
						for(i = 0;i < lines;i++){

							size_t l = number_of_digit(k) + number_of_digit(i+1)+1;
							key = (char*) malloc(l+1);
							if(!key){
								fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
								goto s_ord_get_exit_error;
							}

							char *line_title = (char*)malloc(23);
							if(!line_title){
								fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
								goto s_ord_get_exit_error;
							}
							if(copy_to_string(key,l+1,"%d/%ld",k,i+1) == -1) goto s_ord_get_exit_error;

							if(copy_to_string(line_title,23,"\"line_%ld\"",i+1) == -1)goto s_ord_get_exit_error;


							if(get_record(-1,SALES_ORDERS_L,&rec,
										(void *)key,STR, hd,fds) == -1) goto s_ord_get_exit_error; 

							position_in_the_message = strlen(cont->cnt_dy);
							strncpy(&cont->cnt_dy[position_in_the_message],line_title,strlen(line_title));
							position_in_the_message += strlen(line_title);
							strncpy(&cont->cnt_dy[position_in_the_message]," : { ",strlen(" : { ")+1);
							position_in_the_message += strlen(" : { ");


							/*put the line in the bigger string */
							if(data_to_json(&cont->cnt_dy,&rec,S_ORD_GET) == -1) goto s_ord_get_exit_error;

							free_record(&rec,rec.fields_num);
							memset(&rec,0,sizeof(struct Record_f));
							cancel_memory(NULL,key,l+1);

							if(lines - i > 1){
								position_in_the_message = strlen(cont->cnt_dy);
								strncpy(&cont->cnt_dy[position_in_the_message],", ",strlen(", ") + 1);
							}
						}

						close_file(3,fds[0],fds[1],fds[2]);
						/*close the message*/
						position_in_the_message = strlen(cont->cnt_dy);
						strncpy(&cont->cnt_dy[position_in_the_message],"}}}",strlen("}}}")+1);
						position_in_the_message+=2;
						memset(&cont->cnt_dy[strlen(cont->cnt_dy)],0,(1024*4) - strlen(cont->cnt_dy));	
						cont->size = strlen(cont->cnt_dy);
						/*printf("%s\nsize is %ld\nlast char is '%c'\n",message,strlen(message),message[strlen(message)-1]);*/

						return 0;
s_ord_get_exit_error:
						free_record(&rec,rec.fields_num);
						close_file(3,fds[0],fds[1],fds[2]);
						return -1;

					}
				default:
					return -1;
			}	

#endif
			break;
		}
		default:
		break;
		}
		break;
		}
		default:
		break;	
	}
	return 0;
	}

	}	

static char *convert_json(char* body)
{
	static char db_entry[1024] = {0};
	memset(db_entry,0,1024);
	int array = 0;
	int n_array = 0;
	int n_obj_arr = 0;
	int n_obj = 0;
	int string = 0;
	int i = 0;
	for(char *p = &body[1]; *p != '\0'; p++){
		if(*p == ']'){
			if(n_array) 
				n_array = 0;
			else
				array = 0;

			continue;
		}

		if(*p == ',' && !string) {
			db_entry[i] = ':';
			i++;
			continue;
		}

		if(*p == '}'){
			if(n_obj_arr){
				n_obj_arr = 0;
				db_entry[i] = ']';
				i++;
				/* 
				 * the following if statment check if we have more
				 * than one object in the array
				 * and format the db_entry accordingly
				 * */
				if(*(p + 1) == ','){
					db_entry[i] = ',';
					i++;
					p++;
				}
			}else if (n_obj){
				n_obj = 0;
			}
			continue;
		}

		if(*p == '{'){
			if(array){
				n_obj_arr = 1;
				/*file as a field syntax*/
				db_entry[i] = '[';
				i++;
				db_entry[i] = 'w';
				i++;
				db_entry[i] = '|';
				i++;
			}else{	
				n_obj = 1;
			}
		}

		if(*p == '['){
			if(array)
				n_array = 1;
			else
				array = 1;
			continue;
		}

		if(*p == ':' && string == 0){
			db_entry[i] = *p;
			i++;
			continue;
		}

		if(*p == ' ' && !string) continue;
		if(*p == '"') {
			if(string)
				string = 0;
			else
				string = 1;
			continue;
		}

		if(string){
			db_entry[i] = *p;
			i++;
			continue;
		}	

		if(isdigit(*p)){
			db_entry[i] = *p;
			i++;
			continue;
		}
	}

	return &db_entry[0];
}
