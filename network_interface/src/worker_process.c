#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <stdarg.h>
#include <sys/time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

/*my libs*/
#include <crud.h>
#include "durable.h"
#include <key.h>
#include <str_op.h>
#include <types.h>
#include <file.h>
#include "end_points.h"
#include "common.h"
#include "lua_start.h"
#include "string_utilities.h"

static volatile sig_atomic_t shutdown_requested;
static int mutation_reply, storage_failed;


static void request_shutdown(int signo)
{
    (void)signo;
    shutdown_requested = 1;
}

static char prog[] = "worker_process";

#define read64(n) (((ui64)(n)[0])	| ((ui64)(n)[1]<<8)\
			| ((ui64)(n)[2]<<16)  	| ((ui64)(n)[3]<<24)\
			| ((ui64)(n)[4]<<32)  	| ((ui64)(n)[5]<<40)\
			| ((ui64)(n)[6]<<48)  	| ((ui64)(n)[7]<<56))

#define LUA_VALUE_ERROR -20
#define LUA_SALES_ORDER_HEAD_WRITE_FAILED -21
#define LUA_SALES_ORDER_LINES_WRITE_FAILED -22
#define LUA_GET_NUMERIC_KEY_FAILED -23
#define LUA_NEW_ITEM_WRITE_ERROR -24
#define LUA_ORDER_TRANSACTION_FAILED -25
#define GENERAL_ERROR -1
#define NO_ERROR 0

static int format_reply(char *dst, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args,format);
    int n=vsnprintf(dst,size,format,args);
    va_end(args);
    return n<0 || (size_t)n>=size ? -1 : 0;
}

static void item_reply(char *dst, size_t size, const char *name)
{
    /* Preserve a useful message without letting item names break JSON. */
    char escaped[900];
    size_t j=0;
    for(const unsigned char *p=(const unsigned char *)name; *p; ++p){
        if(j+6>=sizeof(escaped)) goto generic;
        if(*p<' '){
            snprintf(escaped+j,sizeof(escaped)-j,"\\u%04x",*p);
            j+=6;
        } else {
            if(*p=='"' || *p=='\\') escaped[j++]='\\';
            escaped[j++]=*p;
        }
    }
    escaped[j]=0;
    if(format_reply(dst,size,"{\"message\":\"'%s' added!\"}",escaped)==0) return;
generic:
    format_reply(dst,size,"%s","{\"message\":\"Item added!\"}");
}

static ssize_t send_reply(int fd, const void *data, size_t size)
{
    ssize_t n;
    short code;
    short failed=GENERAL_ERROR;
    if(mutation_reply){
        mutation_reply=0;
        memcpy(&code,data,sizeof(code));
        if(code==0){
            if(commit_lua_caches()<0){
                fprintf(stderr,"database: durable commit failed; stopping worker, retain journal for recovery\n");
                storage_failed=1; shutdown_requested=1;
                data=&failed; size=sizeof(failed);
            }
        } else if(discard_lua_caches()<0){
            storage_failed=1; shutdown_requested=1;
        }
        db_durable_request(0);
    }
    do { n=send(fd,data,size,MSG_NOSIGNAL); } while(n<0 && errno==EINTR && !shutdown_requested);
    return n == (ssize_t)size ? n : -1;
}

#define EIGTH_Kib 1024*8
int work_process(int sock)
{
	char err[1024] = {0};
	char succ[1024] = {0};
	int data_sock = -1;
	char buffer[EIGTH_Kib + 1] = {0};
	char *d_buff = NULL;

    struct sigaction action = {0}, old_term, old_int;
    int listener_flags = fcntl(sock,F_GETFL);
    if(listener_flags == -1) return -1;
    shutdown_requested = 0;
    mutation_reply=storage_failed=0;
    action.sa_handler = request_shutdown;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    if(sigaction(SIGTERM,&action,&old_term) == -1) return -1;
    if(sigaction(SIGINT,&action,&old_int) == -1){
        sigaction(SIGTERM,&old_term,NULL);
        return -1;
    }
    int result = -1;
#ifdef __linux__
    /* Replace WSER's inherited SIGKILL before Lua/cache initialization. */
    pid_t parent = getppid();
    int old_parent_signal = 0;
    if(prctl(PR_GET_PDEATHSIG,&old_parent_signal) == -1) goto restore_handlers;
    if(prctl(PR_SET_PDEATHSIG,(long)SIGTERM,0L,0L,0L) == -1) goto restore_handlers;
    if(parent == 1 || getppid() != parent) shutdown_requested = 1;
#endif
    if(fcntl(sock,F_SETFL,listener_flags | O_NONBLOCK) == -1) goto restore_parent;
    if(shutdown_requested) { result = 0; goto restore_listener; }
    if(init_durable_lua(LUA_CONFIG_FILE) == -1) goto restore_listener;

    while(!shutdown_requested){
        check_config_file();
        if(db_durable_failed()){ storage_failed=1; shutdown_requested=1; }
        if(shutdown_requested) break;
        /* Bounded poll avoids the signal-before-blocking race. Accept must
         * also be nonblocking if a queued connection disappears. */
        struct pollfd event = {.fd=sock, .events=POLLIN};
        int ready = poll(&event,1,250);
        if(shutdown_requested) break;
        if(ready < 0){ if(errno == EINTR) continue; break; }
        if(ready == 0) continue;
        if(!(event.revents & POLLIN)) break;
        if((data_sock = accept(sock,NULL,NULL)) == -1){
            if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }
        if(shutdown_requested){ close(data_sock); break; }
        /* One client must not indefinitely stall this single database worker. */
        struct timeval timeout = {.tv_sec=5};
        if(setsockopt(data_sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) ||
           setsockopt(data_sock,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))){
            close(data_sock);
            continue;
        }
        clear_lua_stack();
        memset(err,0,sizeof(err));
        memset(succ,0,sizeof(succ));
        memset(buffer,0,sizeof(buffer));
        ssize_t r;
        do { r=recv(data_sock,buffer,EIGTH_Kib,MSG_TRUNC); } while(r<0 && errno==EINTR && !shutdown_requested);
        if(r < 2 || r > EIGTH_Kib){ close(data_sock); continue; }
        if(shutdown_requested){ close(data_sock); break; }
        buffer[r] = '\0';
        ui16 operation;
        memcpy(&operation,buffer,sizeof(operation));
        int operation_to_perform = operation;
        if(operation==NEW_CUST || operation==N_ITEM || operation==NEW_SORD || operation==UPDATE_SORD){
            /* operation + total length + at least the field-count word */
            if(r < 12){
                short code=GENERAL_ERROR;
                send_reply(data_sock,&code,sizeof(code));
                close(data_sock);
                continue;
            }
        }

        if(operation==NEW_CUST || operation==N_ITEM || operation==NEW_SORD){
            mutation_reply=1;
            db_durable_request(1);
        }
		switch(operation_to_perform){
		case NEW_CUST:
		{

			char *p = &buffer[2];
			size_t data_size = read64((ui8*)p);
			if(data_size != (size_t)r) goto new_cust_error;

			/*this is the data from ssl_process*/
			ui8 *data = (ui8*)&buffer[10];

			long long res = -1, key = -1;
			if(execute_lua_function("write_customers","t>ll",data,data_size-10,CUSTOMER_FILE,&res,&key) == -1 || res != 0 ){
				/*send error and resume*/
				short int err_code = (short int)res;
				memcpy(&err[0],&err_code,sizeof(short int));
				switch(err_code){
				case LUA_VALUE_ERROR:
					if(format_reply(&err[2],1024-2,"%s",
								"{\"message\":\"values are wrong!\"}") == -1){
						fprintf(stderr,"(%s):format_reply() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
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
			if(format_reply(&succ[2],1024-2,"{\"message\":\"customer nr %lld, created!\"}",key) == -1) goto new_cust_error;

			if(send_reply(data_sock,succ,strlen(&succ[2])+2) == -1) goto new_cust_error;

			close(data_sock);
			data_sock = -1;
			continue;

new_cust_error:	
			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				send_reply(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				send_reply(data_sock,err,2);
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
			if(get_function_signature(function_to_execute,sig,sizeof(sig)) == -1)
				goto report_error;
            /* Reports take no C arguments and return exactly one string. */
            if(strcmp(sig, ">s") != 0) goto report_error;

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
			if(size_json > limit){
				fprintf(stderr,"refactor needed in RPT protocol %s:%d\n",__FILE__,__LINE__);
				free(msg);
				goto report_error;
			}
			ui32 sz = (ui32)size_json;
			if(send_reply(data_sock,&sz,sizeof(ui32)) == -1){
				free(msg);
				goto report_error;
			}

			/* The receive timeout also bounds the report handshake. */
			char ok = 0;
			if(recv(data_sock,&ok,1,0) != 1){
				free(msg);
				goto report_error;
			}

			if(ok == '\001'){
				if(send_reply(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto report_error;
				}
			}

			free(msg);
			close(data_sock);
			continue;

report_error:
			memset(err,0,1024);
			send_reply(data_sock,err,sizeof(err));
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
			if(execute_lua_function("write_item","t>ls",data,data_size-10,ITEM_FILE,&res,&item_name) == -1 || res != 0){
				short int err_code = (short int)res;
				memcpy(&err[0],&err_code,sizeof(short int));
				switch(err_code){
					case LUA_NEW_ITEM_WRITE_ERROR:
						if(format_reply(&err[2],1024-2,"%s",
									"{\"message\":\"Cannot Write to database, call your admin.\"}") == -1){
							fprintf(stderr,"(%s):format_reply() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					case LUA_VALUE_ERROR:
						if(format_reply(&err[2],1024-2,"%s",
									"{\"message\":\"Check values like price_level and unit_price, values are wrong!\"}") == -1){
							fprintf(stderr,"(%s):format_reply() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					default:
						break;
				}
				clear_lua_stack();
				goto n_item_error;
			}

			memset(succ,0,1024);
            item_reply(&succ[2],sizeof(succ)-2,item_name);

			clear_lua_stack();
			item_name = NULL;
			
			if(send_reply(data_sock,succ,strlen(&succ[2]) + 2) == -1) goto n_item_error;

			close(data_sock);
			data_sock = -1;
			continue;
n_item_error:
			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				send_reply(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				send_reply(data_sock,err,2);
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

			if(operation_to_perform == UPDATE_SORD) goto new_up_ords_err; /* not implemented */

			long long key_ord = -1, order_status = -1;
			if(operation_to_perform == NEW_SORD){
				if(execute_lua_function("write_orders","t>ll",data,data_size-10,"data",&key_ord,&order_status) == -1 || order_status != 0){
					/*send error and resume*/
					/*key ord contain the error code*/
					short int err_code = (short int)key_ord;
					memcpy(&err[0],&err_code,sizeof(short int));
					switch(err_code){
					case LUA_ORDER_TRANSACTION_FAILED:
					case LUA_SALES_ORDER_LINES_WRITE_FAILED:
					case LUA_SALES_ORDER_HEAD_WRITE_FAILED:
						if(format_reply(&err[2],1024-2,"%s",
									"{\"message\":\"Cannot Write to database, call your admin.\"}") == -1){
							fprintf(stderr,"(%s):format_reply() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
						}
						break;
					case LUA_VALUE_ERROR:
						if(format_reply(&err[2],1024-2,"%s",
								"{\"message\":\"Check values like Quantity,Discount and so on, some values are wrong!\"}") == -1){
							fprintf(stderr,"(%s):format_reply() failed to write error,%s:%d\n",prog,__FILE__,__LINE__);
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
				if(format_reply(&succ[2],1022,"{ \"message\" : \"order nr %lld, created!\"}",key_ord) == -1){
					/*log error*/
					close(data_sock);
					data_sock = -1;
					continue;
				}

				size_t l = strlen(&succ[2])+ 3;
				if(send_reply(data_sock,succ,l) == -1) goto new_up_ords_err;

			}else if(operation_to_perform == UPDATE_SORD){
#if 0
				memset(succ,0,1024);
				if(format_reply(&succ[2],1022,"{ \"message\" : \"order nr %s, updated!\"}",key_up) == -1){
					/*log error*/
					goto new_up_ords_err;
				}
				size_t l = strlen(&succ[2])+ 3;

				if(send_reply(data_sock,succ,l) == -1) goto new_up_ords_err;
#endif
			}

			close(data_sock);
			data_sock = -1;
			continue;

new_up_ords_err:

			if(err[2] != '\0'){
				size_t l = strlen(&err[2]) + 3; /* 2 is for short int  and 1 for '\0'*/
				send_reply(data_sock,err,l);
			}else{
				short int e = GENERAL_ERROR;
				memcpy(&err[0],&e,sizeof(short int));
				send_reply(data_sock,err,2);
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
				send_reply(data_sock,err,sizeof(err));
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
				if(format_reply(d_buff,mes_l+1,"{ \"message\" : %s}",keys) == -1) {
					free(d_buff);
					goto error_s_ord;
				}

				if(send_reply(data_sock,d_buff,strlen(d_buff)) == -1) {
					free(d_buff);
					goto error_s_ord;
				}

				close(data_sock);
				free(d_buff);
				continue;
			}else{

				memset(succ,0,1024);
				if(format_reply(succ,sizeof(succ),"{ \"message\" : %s}",keys) == -1) goto error_s_ord;

				if(send_reply(data_sock,succ,strlen(succ)) == -1) goto error_s_ord;

				close(data_sock);
				continue;
			}
error_s_ord:
			memset(err,0,1024);
			send_reply(data_sock,err,sizeof(err));
            close(data_sock);
			continue;
		}
		case S_ORD_CUSTOMER_GET:
		case CUSTOMER_GET:
		case S_ORD_GET:
		case ITEM_GET:
		{
			ui32 k = 0;
			ui8 type = buffer[2] && strspn(&buffer[2],"0123456789") == strlen(&buffer[2]) ? UINT : STR;
			switch(type){
			case UINT:
			{
				/*convert to number */	
				errno = 0;
				unsigned long l = strtoul(&buffer[2],NULL,10);
				if(errno == ERANGE || l > UINT32_MAX){
					/*log error*/
					memset(err,0,1024);
					send_reply(data_sock,err,sizeof(err));
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

				if(send_reply(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto s_ord_get_exit_error;
				}

				/*printf("%s\nsize is %ld\nlast char is '%c'\n",message,strlen(message),message[string_length(message)-1]);*/

				free(msg);
				close(data_sock);
				continue;
s_ord_get_exit_error:
				memset(err,0,1024);
				send_reply(data_sock,err,2);
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

				if(send_reply(data_sock,msg,size_json) == -1 ) {
					free(msg);
					goto s_ord_get_exit_error;
				}

				free(msg);
				close(data_sock);
				continue;
			}
			default:
				memset(err,0,1024);
				send_reply(data_sock,err,sizeof(err));
				close(data_sock);
				continue;
			}
		}
		default:
			memset(err,0,1024);
			send_reply(data_sock,err,sizeof(err));
			close(data_sock);
			continue;
		}
	}
    result = shutdown_requested ? 0 : -1;
    if(storage_failed || db_durable_failed()) result=-1;
    else if(flush_lua_caches() == -1) result = -1;
    close_lua();
restore_listener:
    if(fcntl(sock,F_SETFL,listener_flags) == -1) result = -1;
restore_parent:
#ifdef __linux__
    if(prctl(PR_SET_PDEATHSIG,(long)old_parent_signal,0L,0L,0L) == -1) result = -1;
#endif
restore_handlers:
    sigaction(SIGINT,&old_int,NULL);
    sigaction(SIGTERM,&old_term,NULL);
    return result; /* WSER owns the listener and the worker's final exit. */
}
