#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

/*my libs*/
#include <crud.h>
#include <key.h>
#include <str_op.h>
#include <types.h>
#include <file.h>
#include "end_points.h"
#include "common.h"
#include "lua_start.h"
#include "string_utilities.h"

static char prog[] = "worker_process";
static int data_to_json(char **buffer, struct Record_f *rec,int end_point);

#define read64(n) (((ui64)(n)[0])	| ((ui64)(n)[1]<<8)\
			| ((ui64)(n)[2]<<16)  	| ((ui64)(n)[3]<<24)\
			| ((ui64)(n)[4]<<32)  	| ((ui64)(n)[5]<<40)\
			| ((ui64)(n)[6]<<48)  	| ((ui64)(n)[7]<<56))

#define LUA_VALUE_ERROR -20
#define LUA_SALES_ORDER_HEAD_WRITE_FAILED -21
#define LUA_SALES_ORDER_LINES_WRITE_FAILED -22
#define LUA_GET_NUMERIC_KEY_FAILED -23
#define LUA_NEW_ITEM_WRITE_ERROR -24
#define GENERAL_ERROR -1
#define NO_ERROR 0

#define EIGTH_Kib 1024*8
int work_process(int sock)
{
	char err[1024];
	char succ[1024];
	int data_sock = -1;
	char buffer[EIGTH_Kib] = {0};
	char *d_buff = NULL;

	/*start the Lua interpreter*/
	if(init_lua(LUA_CONFIG_FILE) == -1){
		/**/
		return -1;
	}
	

	for(;;){
		/*accept connection*/
		check_config_file();
		if((data_sock = accept(sock,NULL,NULL)) == -1){
			break;
		}

		memset(buffer,0,EIGTH_Kib);
		int r = 0;
		if((r=read(data_sock,buffer,sizeof(buffer))) == -1){
			close(data_sock);
			break;
		}

		if(r == 0){
			close(data_sock);
			continue;
		}

		if(r == EIGTH_Kib){
			fprintf(stderr,"REFACTOR NEEDED,SOCKET READING BUFFER %s:%d\n",__FILE__,__LINE__-14);
			close(data_sock);
			continue;
		}

		buffer[sizeof(buffer) - 1] = '\0';
		int operation_to_perform = (int)(*((ui16*)buffer));	

		switch(operation_to_perform){
		case NEW_CUST:
		{

			char *p = &buffer[2];
			size_t data_size = read64((ui8*)p);
			if(data_size != (size_t)r) goto new_cust_error;

			/*this is the data from ssl_process*/
			ui8 *data = (ui8*)&buffer[10];

			long long res = -1, key = -1;
			if(execute_lua_function("write_customers","t>ll",data,data_size-10,CUSTOMER_FILE,&res,&key) == -1 || res == 2 ){
				/*send error and resume*/
				short int err_code = (short int)res;
				memcpy(&err[0],&err_code,sizeof(short int));
				switch(err_code){
				case LUA_VALUE_ERROR:
					if(copy_to_string(&err[2],1024-2,"%s",
								"{\"message\":\"values are wrong!\"}") == -1){
						fprintf(stderr,"(%s):copy_to_string() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
					}
					break;
				default:
					break;
				}
				clear_lua_stack();
				goto new_cust_error;
			}
			clear_lua_stack();

			memset(succ,0,1024);
			if(copy_to_string(&succ[2],1024-2,"{\"message\":\"customer nr %d, created!\"}",key) == -1) goto new_cust_error;

			if(write(data_sock,succ,strlen(&succ[2])+2) == -1) goto new_cust_error;

			close(data_sock);
			data_sock = -1;
			continue;

new_cust_error:	
			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				write(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				write(data_sock,err,2);
			}
			memset(err,0,sizeof(err));
			close(data_sock);
			data_sock = -1;
			continue;
		}
		case RPT:
		{
			char *function_to_execute = &buffer[2];

			char sig[20] = {0};
			if(get_function_signature(function_to_execute,sig) == -1)
				goto report_error;

			char *json = NULL;
			if(execute_lua_function(function_to_execute,sig,&json) == -1){
				/*send error and resume*/
				goto report_error;
			}

			/*copy the json string from lua to memory*/
			size_t size_json = strlen(json);
			char *msg = (char*) malloc(size_json+1);
			if(!msg){
				fprintf(stderr,"malloc() failed. %s:%d.\n",__FILE__,__LINE__-2);
				clear_lua_stack();
				goto report_error;
			}

			memset(msg,0,size_json+1);
			memcpy(msg,json,size_json);

			clear_lua_stack();
			json = NULL;

			
			ui32 limit = 0 | 0xFFFFFFFF;
			if(size_json > (ui16)limit){
				fprintf(stderr,"refactor needed in RPT protocol %s:%d\n",__FILE__,__LINE__);
				free(msg);
				goto report_error;
			}
			ui32 sz = (ui32)size_json;
			if(write(data_sock,&sz,sizeof(ui32)) == -1){
				free(msg);
				goto report_error;
			}

			/*do I NEED A TIMER ????*/
			char ok = 0;
			if(read(data_sock,&ok,1) == -1){
				free(msg);
				goto report_error;
			}

			if(ok == '\001'){
				if(write(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto report_error;
				}
			}

			free(msg);
			close(data_sock);
			continue;

report_error:
			memset(err,0,1024);
			write(data_sock,err,sizeof(err));
			close(data_sock);
			data_sock = -1;
			continue;
		}
		case N_ITEM: /*NEW ITEM*/
		{
			char *p = &buffer[2];
			size_t data_size = read64((ui8*)p);
			if(data_size != (size_t)r) goto n_item_error;

			ui8 *data = (ui8*)&buffer[10];

			long long res = -1;
			char *item_name = NULL;
			if(execute_lua_function("write_item","t>ls",data,data_size-10,ITEM_FILE,&res,&item_name) == -1){
				short int err_code = (short int)res;
				memcpy(&err[0],&err_code,sizeof(short int));
				switch(err_code){
					case LUA_NEW_ITEM_WRITE_ERROR:
						if(copy_to_string(&err[2],1024-2,"%s",
									"{\"message\":\"Cannot Write to database, call your admin.\"}") == -1){
							fprintf(stderr,"(%s):copy_to_string() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					case LUA_VALUE_ERROR:
						if(copy_to_string(&err[2],1024-2,"%s",
									"{\"message\":\"Check values like price_level and unit_price, values are wrong!\"}") == -1){
							fprintf(stderr,"(%s):copy_to_string() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					default:
						break;
				}
				clear_lua_stack();
				goto n_item_error;
			}

			memset(succ,0,1024);
			if(copy_to_string(&succ[2],1024-2,"{\"message\":\"'%s' added!\"}",item_name) == -1){ 
				goto n_item_error;
			}

			clear_lua_stack();
			item_name = NULL;
			
			if(write(data_sock,succ,strlen(&succ[2]) + 2) == -1) goto n_item_error;

			close(data_sock);
			data_sock = -1;
			continue;
n_item_error:
			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				write(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				write(data_sock,err,2);
			}
			memset(err,0,sizeof(err));
			close(data_sock);
			data_sock = -1;
			continue;
		}
		case NEW_SORD:
		case UPDATE_SORD:
		{
			char *p = &buffer[2];
			size_t data_size = read64((ui8*)p);
			if(data_size != (size_t)r) goto new_up_ords_err;

			ui8 *data = (ui8*)&buffer[10];

			if(operation_to_perform == UPDATE_SORD){
				/*get the key of the record that we have to update*/
			}

			long long key_ord = -1;
			if(operation_to_perform == NEW_SORD){
				if(execute_lua_function("write_orders","t>l",data,data_size-10,"data",&key_ord) == -1){
					/*send error and resume*/
					/*key ord contain the error code*/
					short int err_code = (short int)key_ord;
					memcpy(&err[0],&err_code,sizeof(short int));
					switch(err_code){
					case LUA_SALES_ORDER_LINES_WRITE_FAILED:
					case LUA_SALES_ORDER_HEAD_WRITE_FAILED:
						if(copy_to_string(&err[2],1024-2,"%s",
									"{\"message\":\"Cannot Write to database, call your admin.\"}") == -1){
							fprintf(stderr,"(%s):copy_to_string() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					case LUA_VALUE_ERROR:
						if(copy_to_string(&err[2],1024-2,"%s",
								"{\"message\":\"Check values like Quantity,Discount and so on, some values are wrong!\"}") == -1){
							fprintf(stderr,"(%s):copy_to_string() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					default:
						break;
					}
					clear_lua_stack();
					goto new_up_ords_err;
				}
			}else{
#if 0
				int key_type = is_num(key_up);
				switch(key_type){
				case UINT:
				{
					ui8 *data = (ui8*)&buffer[2];

					error_value = -1;
					long l = string_to_long(key_up);
					if(error_value == INVALID_VALUE) goto new_up_ords_err;

					ui32 key = (ui32)l;
					long long res = -1;
					if(execute_lua_function("update_orders",
								"ttI>l",
								data,
								SALES_ORDERS_H,
								SALES_ORDERS_L,
								key, &res) == -1 || res != 0){
						/*send error and resume*/
						clear_lua_stack();
						goto new_up_ords_err;
					}
					break;	
				}
				case STR:
				{
					long long res = -1;
					if(execute_lua_function("update_orders","tts>l",
								data,
								SALES_ORDERS_H,
								SALES_ORDERS_L,
								key_up,&res) == -1 || res != 0){
						/*send error and resume*/
						clear_lua_stack();
						goto new_up_ords_err;
					}
					break;
				}
				default:
					goto new_up_ords_err;
				}
#endif
			}


			clear_lua_stack();
			if(operation_to_perform == NEW_SORD){
				memset(succ,0,1024);
				if(copy_to_string(&succ[2],1022,"{ \"message\" : \"order nr %d, created!\"}",key_ord) == -1){
					/*log error*/
					close(data_sock);
					data_sock = -1;
					continue;
				}

				size_t l = strlen(&succ[2])+ 3;
				if(write(data_sock,succ,l) == -1) goto new_up_ords_err;

			}else if(operation_to_perform == UPDATE_SORD){
#if 0
				memset(succ,0,1024);
				if(copy_to_string(&succ[2],1022,"{ \"message\" : \"order nr %s, updated!\"}",key_up) == -1){
					/*log error*/
					goto new_up_ords_err;
				}
				size_t l = strlen(&succ[2])+ 3;

				if(write(data_sock,succ,l) == -1) goto new_up_ords_err;
#endif
			}

			close(data_sock);
			data_sock = -1;
			continue;

new_up_ords_err:

			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				write(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				write(data_sock,err,2);
			}
			close(data_sock);
			memset(err,0,sizeof(err));
			data_sock = -1;
			continue;
		}	
		case ITEM_GET_ALL:
		case CUSTOMER_GET_ALL: 
		case S_ORD:
		{
			/*get the all keys for the sales order file or the CUSTOMER*/
			char *keys = 0x0;
			int index = 0,mode = 0;
			switch(operation_to_perform){
			case S_ORD:
				if(execute_lua_function("g_all_key","sii>s",SALES_ORDERS_H,index,mode,&keys) == -1){
					clear_lua_stack();
					goto error_s_ord;
				}
				break;
			case CUSTOMER_GET_ALL:
				index = 2, mode = MAKE_KEY_JS_STRING;
				if(execute_lua_function("g_all_key","sii>s",CUSTOMER_FILE,index,mode,&keys) == -1){
					clear_lua_stack();
					goto error_s_ord;
				}
				break;
			case ITEM_GET_ALL:
				index = 1, mode = MAKE_KEY_JS_STRING;
				if(execute_lua_function("g_all_key","sii>s",ITEM_FILE,index,mode,&keys) == -1){
					clear_lua_stack();
					goto error_s_ord;
				}	
				break;
			default:
				goto error_s_ord;
			}


			if(!keys){
				/*log errors*/	
				char *erro_message = 0x0;
				switch(operation_to_perform){
				case S_ORD:
					erro_message = "{\"message\": \"there are no orders\"}";
					break;
				case CUSTOMER_GET_ALL:
					erro_message = "{\"message\": \"there are no customers\"}";
					break;
				case ITEM_GET_ALL:
					erro_message = "{\"message\": \"there are no items\"}";
					break;
				default:
					goto error_s_ord;
				}

				if(operation_to_perform == S_ORD)
					erro_message = "{\"message\": \"there are no orders\"}";
				else if(operation_to_perform == CUSTOMER_GET_ALL)
					erro_message = "{\"message\": \"there are no customers\"}";

				memset(err,0,1024);
				strncpy(err,erro_message,strlen(erro_message));
				write(data_sock,err,sizeof(err));
				close(data_sock);
				continue;
			}

			size_t l = strlen(keys);
			size_t mes_l = strlen("{\"message\" : ") + l + strlen(" }");
			if((mes_l) >= 1024) {

				d_buff = (char *)malloc(mes_l+1);
				if(!d_buff){
					fprintf(stderr,"malloc() failed. %s:%d.\n",__FILE__,__LINE__-2);
					goto error_s_ord;
				}

				memset(d_buff, 0,mes_l+1);
				if(copy_to_string(d_buff,mes_l+1,"{ \"message\" : %s}",keys) == -1) {
					free(d_buff);
					goto error_s_ord;
				}

				if(write(data_sock,d_buff,strlen(d_buff)) == -1) {
					free(d_buff);
					goto error_s_ord;
				}

				close(data_sock);
				free(d_buff);
				continue;
			}else{

				memset(succ,0,1024);
				if(copy_to_string(succ,mes_l,"{ \"message\" : %s}",keys) == -1) goto error_s_ord;

				if(write(data_sock,succ,strlen(succ)) == -1) goto error_s_ord;

				close(data_sock);
				continue;
			}
error_s_ord:
			memset(err,0,1024);
			write(data_sock,err,sizeof(err));
			continue;
		}
		case S_ORD_CUSTOMER_GET:
		case CUSTOMER_GET:
		case S_ORD_GET:
		case ITEM_GET:
		{
			ui32 k = 0;
			ui8 type = is_num(&buffer[2]);
			switch(type){
			case UINT:
			{
				/*convert to number */	
				long l = string_to_long(&buffer[2]);
				if(error_value == INVALID_VALUE){
					/*log error*/
					memset(err,0,1024);
					write(data_sock,err,sizeof(err));
					close(data_sock);
					continue;
				}

				k = (ui32) l;

				char *json = NULL;
				switch(operation_to_perform){
				case ITEM_GET:
					if(execute_lua_function("get_item","I>s",k,&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				case S_ORD_GET:
					if(execute_lua_function("get_order","I>s",k,&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				case CUSTOMER_GET:
					if(execute_lua_function("get_customer","I>s",k,&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				case S_ORD_CUSTOMER_GET:
					if(execute_lua_function("get_customer_for_new_sales_order","I>s",k,&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				default:
					fprintf(stderr,"database endpoint not supported\n");
					goto s_ord_get_exit_error;
				}

				/*copy the json string from lua to memory*/
				size_t size_json = strlen(json);
				char *msg = (char*) malloc(size_json+1);
				if(!msg){
					fprintf(stderr,"malloc() failed. %s:%d.\n",__FILE__,__LINE__-2);
					clear_lua_stack();
					goto s_ord_get_exit_error;
				}
				memset(msg,0,size_json+1);
				memcpy(msg,json,size_json);
				clear_lua_stack();
				json = NULL;

				if(write(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto s_ord_get_exit_error;
				}

				/*printf("%s\nsize is %ld\nlast char is '%c'\n",message,strlen(message),message[string_length(message)-1]);*/

				free(msg);
				close(data_sock);
				continue;
s_ord_get_exit_error:
				memset(err,0,1024);
				write(data_sock,err,2);
				close(data_sock);
				continue;
			}
			case STR:
			{
				char *json = NULL;
				switch(operation_to_perform){
				case ITEM_GET:
					if(execute_lua_function("get_item","s>s",&buffer[2],&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				case S_ORD_GET:
					if(execute_lua_function("get_order","s>s",&buffer[2],&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				case CUSTOMER_GET:
					if(execute_lua_function("get_customer","s>s",&buffer[2],&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
				}
					break;
				case S_ORD_CUSTOMER_GET:
					if(execute_lua_function("get_customer_for_new_sales_order","s>s",&buffer[2],&json) == -1){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					if(!json){
						clear_lua_stack();
						goto s_ord_get_exit_error;
					}
					break;
				default:
					fprintf(stderr,"database endpoint not supported\n");
					goto s_ord_get_exit_error;
				}

				/*copy the json string from lua to memory*/
				size_t size_json = strlen(json);
				char *msg = (char*) malloc(size_json+1);
				if(!msg){
					fprintf(stderr,"malloc() failed. %s:%d.\n",__FILE__,__LINE__-2);
					clear_lua_stack();
					goto s_ord_get_exit_error;
				}
				memset(msg,0,size_json+1);
				memcpy(msg,json,size_json);
				clear_lua_stack();
				json = NULL;

				if(write(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto s_ord_get_exit_error;
				}

				free(msg);
				close(data_sock);
				continue;
			}
			default:
				memset(err,0,1024);
				write(data_sock,err,sizeof(err));
				close(data_sock);
				continue;
			}
		}
		default:
			memset(err,0,1024);
			write(data_sock,err,sizeof(err));
			close(data_sock);
			continue;
		}
	}
	close_lua();
	pid_t p = getppid();
	if(p != -1)
		kill(p,SIGINT);
	exit(1);
	return 0;/*unrechable*/
}

static int data_to_json(char **buffer, struct Record_f *rec,int end_point)
{
	switch(end_point){	
		case S_ORD_GET: 
			{
				if(parse_record_to_json(rec,buffer) == -1) return -1;
				break;
			}
		default:
			return -1;
	}
	return 0;
}	
