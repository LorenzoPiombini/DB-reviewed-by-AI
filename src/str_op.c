#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "str_op.h"
#include "db_types.h"
#include "string_utilities.h"
#include "common.h"
#include "globals.h"
#include "debug.h"

static char prog[] = "db";
static char *strstr_last(char *src, char delim);
static int is_target_db_type(char *target);
static int search_string_for_types(char *str, int *types);

#define TO_LOWER(c) do { \
						if((int)*c >= 65 && (int)*c <= 90) *c = (int)*c + 32;\
					}while(0);

#define BASE 247
static const char *base_247[] = {"_A","_B","_C" ,"_D","_E","_F","_G","_H","_I","_J","_K","_L","_M","_N","_O","_P","_Q","_R","_S","_T","_U",
				"_V","_W","_X","_Y","_Z","_[","_\\","_]","_^","__"," ","!","\"","#","$","%","&","'","(",")","*","+",
				",","-",".","0","1","2","3","4","5","6","7","8","9",":",";","<","=","?","@","A","B","C","D","E","F",
				"G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","[","]","_","`","a",
				"b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z",
				"_x7f","_x80","_x81","_x82","_x83","_x84","_x85","_x86","_x87","_x88","_x89","_x8a","_x8b","_x8c","_x8d",
				"_x8e","_x8f","_x90","_x91","_x92","_x93","_x94","_x95","_x96","_x97","_x98","_x99","_x9a","_x9b","_x9c",
				"_x9d","_x9e","_x9f","_xa0","_xa1","_xa2","_xa3","_xa4","_xa5","_xa6","_xa7","_xa8","_xa9","_xaa","_xab",
				"_xac","_xad","_xae","_xaf","_xb0","_xb1","_xb2","_xb3","_xb4","_xb5","_xb6","_xb7","_xb8","_xb9","_xba",
				"_xbb","_xbc","_xbd","_xbe","_xbf","_xc0","_xc1","_xc2","_xc3","_xc4","_xc5","_xc6","_xc7","_xc8","_xc9",
				"_xca","_xcb","_xcc","_xcd","_xce","_xcf","_xd0","_xd1","_xd2","_xd3","_xd4","_xd5","_xd6","_xd7","_xd8",
				"_xd9","_xda","_xdb","_xdc","_xdd","_xde","_xdf","_xe0","_xe1","_xe2","_xe3","_xe4","_xe5","_xe6","_xe7",
				"_xe8","_xe9","_xea","_xeb","_xec","_xed","_xee","_xef","_xf0","_xf1","_xf2","_xf3","_xf4","_xf5","_xf6",
				"_xf7","_xf8","_xf9","_xfa","_xfb","_xfc","_xfd","_xfe","_xff"};

char *exclude_sub_str(char *start_delim,char *end_delim, char *src,int loop)
{
	char *start_d = 0x0;
	char *end_d = 0x0;
	static char ex_s_str[1024] = {0};
	memset(ex_s_str,0,1024);

	if(loop){
		
		int i = 0;
		while(*src){
			while(*src && *src != '['){
				ex_s_str[i] = *src;
				i++,src++;
			}

			if(!(*src))
				break;

			ex_s_str[i] = *src;

			if(!(*src))
				break;

			src++, i++;
			while(*src && *src != ']') src++;

			ex_s_str[i] = *src;
			if(!(*src))
				break;
			src++, i++;
		}
		if(ex_s_str[0])
			return &ex_s_str[0];
		else
			return NULL;
	} else{
		if((start_d = strstr(src,start_delim))){
			int l = start_d - src;
			strncpy(ex_s_str,src,l+1);
			if((end_d = strstr(src,end_delim))){
				int e = end_d - src;
				strncpy(&ex_s_str[l+1],&src[e],(strlen(src)-e)+1);
				return &ex_s_str[0];
			}
		}
	}
	return 0x0;
}

char *get_sub_str(char *start_delim, char *end_delim, char *str, int loop)
{
	char *start_d = 0x0;
	char *end_d = 0x0;
	static char sub_str[1024] = {0};
	memset(sub_str,0,1024);

	if(loop){
		int i = 0;
		while(*str){
			while(*str && *str != *start_delim) str++;

			str++;
			while(*str && *str != *end_delim){
				sub_str[i] = *str;
				i++,str++;
			}

			str++;
			if(!(*str))
				break;

			sub_str[i] = ',';
			i++;
		}
		if(sub_str[0])
			return &sub_str[0];
		else
			return NULL;


	}else{
		if((start_d = strstr(str,start_delim))){
			if((end_d = strstr(str,end_delim))){
				if(loop){
					*start_d = '@';
					*end_d = '&';
				}
				int sx = start_d - str;
				int size = ((--end_d - str) - sx)+1;

				strncpy(sub_str,&str[sx + 1],size-1);
				return &sub_str[0];
			}
		}

	}
	return 0x0;
}

int check_handle_input_mode(char *buffer, int op)
{

	/*look for TYPE_FILE*/
	int av_ct = 0;
	int av_cT = 0;
	ui8 file = 0;
	/*
	 * __IMPORT_EZ is a global variable that will be true
	 * only if we import from an EZgen system
	 * in that case we do not need to check for file type
	 * */

	if(!__IMPORT_EZ){
		char *sub_str = get_sub_str("[", "]", buffer, 0);
		if(sub_str){
			/*count T_ and TYPE_ that we have to ignore*/
			av_ct = count_fields(sub_str,T_); 
			av_cT = count_fields(sub_str,TYPE_);
			file = 1;
		}
	}
	int c_t = count_fields(buffer,T_) - av_ct; 
	int c_T = count_fields(buffer,TYPE_) - av_cT; 
	int c_CT = count_fields(buffer,CON_);
	int c_ct = count_fields(buffer,C_);
	
	char *excluded_file_input = exclude_sub_str("[","]",buffer,0);
	int c_delim = 0;
	if(excluded_file_input){
		c_delim = count_fields(excluded_file_input,":");
	}else if(!excluded_file_input && file){
		fprintf(stderr,"(%s): error in excluding file data from input. %s:%d\n",prog,__FILE__,__LINE__-5);
		return -1;
	}else{
		c_delim = count_fields(buffer,":");
	}
		


	
	if(c_T == 0 && c_t == 0 && c_delim > 0) return NO_TYPE;
	
	/*for each t_ or TYPE_ or C_ or CONST_ delim we need to account for one ':' */
	int v = c_T  + c_t + c_ct + c_CT ;
	switch(op){ 
	case FCRT:
	{

		if(c_ct == 0 
				&& c_CT == 0 
				&& c_T == 0 
				&& c_t == 0
				&& c_delim == 0 
				&& (buffer && *buffer != '\0'))
			return NO_TYPE;

		int additional_c_delim_count = v > 1 ? v-1 : 0;
		if((c_delim - v - additional_c_delim_count - c_CT - c_ct) > 0) return HYB;
		if((c_delim - v - additional_c_delim_count - c_CT - c_ct) == 0) return TYPE;
		if(c_delim - c_CT - c_ct  <= v
				|| c_delim - c_CT - c_ct >= v) return TYPE;
		break;
	}
	case FWRT:
	{
		if(c_T == 0 && c_t == 0 && c_delim == 0) return -1;
		int additional_c_delim_count = v == 1 ? 2 : v *2;
		if((c_delim - v -additional_c_delim_count) > 0 ) return HYB;
		if((c_delim - v -additional_c_delim_count) == 0) return TYPE;
		if((c_delim - v -additional_c_delim_count) < 0) return TYPE;
		break;
	}
	default:
		break;	
	}

	return -1;
}

int get_name_types_hybrid(int mode,char *buffer, char names[][MAX_FILED_LT],int *types_i)
{
	size_t size = strlen(buffer) + 1;
	char cbuf[size];
	memset(cbuf,0,size);
	strncpy(cbuf,buffer,size);


	replace('@','^',cbuf);
	int i = 0;

	/* detect files types*/	
	char *file_start = 0x0;
	if(!__IMPORT_EZ){
		while((file_start = strstr(cbuf,"["))){
			char *end_file = strstr(file_start,"]");
			if(end_file){ 
				int end = end_file - cbuf;
				char *delim = 0x0;
				while((delim = strstr(file_start,":"))){
					if((delim - cbuf) < end) 
						*delim = '@';
					else
						break;
				}
			}
			*file_start = '@';
		}
	}

	int reset_mistype_index[10];
	memset(reset_mistype_index,-1,sizeof(int)*10);
	int rmi = 0;
	if(strstr(cbuf,T_)){
		char *pos_t = 0x0;
		while((pos_t = strstr(cbuf,T_))){
			if(is_target_db_type(pos_t) == -1){
				/* this is not a type*/
				*pos_t = '@';			
				reset_mistype_index[rmi] = pos_t - cbuf;
				rmi++;
				continue;
			}
			int start = pos_t - cbuf;

			/*this change the delimeter so it won't
			 * be found in the next iteration*/
			*pos_t= '@';
			char *end_t = (strstr(pos_t,":"));
			if(!end_t){
				size_t sz = strlen(++pos_t) + 1;
				char type[sz];
				memset(type,0,sz);
				strncpy(type,pos_t,sz);
				types_i[i] = get_type(type);

				char *p = &cbuf[start] - 1;
				while(*p !=':' && p != &cbuf[0] && *p != '@') p--;

				sz = (--pos_t - cbuf) - (p - cbuf) + 1;
				if(sz-2 > MAX_FIELD_LT) return -1;

				if(p != &cbuf[0]){
					strncpy(names[i],++p, sz -2);
					memset(p,0x20,sz-2);
				}else{
					strncpy(names[i],p, sz -1);
					memset(p,0x20,sz-1);
				}

				i++;
				break;
			}


			/*get the type*/
			int l = end_t - pos_t;
			char type[l];
			memset(type,0,l);
			strncpy(type,++pos_t,l-1);
			types_i[i] = get_type(type);

			char *p = &cbuf[start] - 1;
			while(*p !=':' && p != &cbuf[0] && *p != '@') p--;
			
			l = pos_t - p;
			if(l-2 > MAX_FIELD_LT) return -1;

			if(p != &cbuf[0]){
				strncpy(names[i],++p, l -2);
				memset(p,0x20,l-2);
			}else{
				strncpy(names[i],p, l -1);
				memset(p,0x20,l-1);
			}
				
			if(types_i[i] == TYPE_FILE){
				char *file_block = 0x0;
				if((file_block = strstr(p,"["))){
					char *d = 0x0;
					char *end = strstr(file_block,"]");
					while((d = strstr(&file_block[1],T_)) && 
							(((d = strstr(&file_block[1],T_)) - cbuf) < (end - cbuf))){ 
						*d = '@';	
					}
				}
			}

			i++;
		}
	}

	
	rmi = 0;
	while(reset_mistype_index[rmi] != -1){
		cbuf[reset_mistype_index[rmi]] = ':';
		rmi++;
	}
	if (strstr(cbuf,TYPE_)){
		char *pos_T = 0x0;
		while((pos_T = strstr(cbuf,TYPE_))){
			int start = pos_T - cbuf;
			/*this change the delimeter so it won't
			 * be found in the next iteration*/
			*pos_T= '@';
			char *end_T = (strstr(pos_T,":"));
			if(!end_T){
				size_t sz = strlen(++pos_T) + 1;
				char type[sz];
				memset(type,0,sz);
				strncpy(type,pos_T,sz);
				types_i[i] = get_type(type);

				char *p = &cbuf[start] - 1;
				while(*p !=':' && p != &cbuf[0] && *p != '@') p--;

				sz = pos_T - p;
				if(sz-2 > MAX_FIELD_LT) return -1;

				if(p != &cbuf[0]){
					strncpy(names[i],++p, sz -2);
					memset(p,0x20,sz-2);
				}else{
					strncpy(names[i],p, sz -1);
					memset(p,0x20,sz-1);
				}
				i++;
				break;
			}

			/*get the type*/
			int l = end_T - pos_T;
			char type[l];
			memset(type,0,l);
			strncpy(type,++pos_T,l-1);
			types_i[i] = get_type(type);


			char *p = &cbuf[start] - 1;
			while(*p !=':' && p != &cbuf[0] && *p != '@') p--;
			
			l = pos_T - p;
			if(l-2 > MAX_FIELD_LT) return -1;

			if(p != &cbuf[0]){
				strncpy(names[i],++p, l -2);
				memset(p,0x20,l-2);
			}else{
				strncpy(names[i],p, l -1);
				memset(p,0x20,l-1);
			}

			if(types_i[i] == TYPE_FILE){
				char *file_block = 0x0;
				if((file_block = strstr(p,"["))){
					char *d = 0x0;
					char *end = strstr(file_block,"]");
					while((d = strstr(&file_block[1],TYPE_)) && 
							(((d = strstr(&file_block[1],TYPE_)) - cbuf) < (end - cbuf))){ 
						*d = '@';	
					}
				}

			}
			i++;
		}
	} 
	char names_s[MAX_FIELD_NR - i][MAX_FILED_LT];
	memset(names_s,0,(MAX_FIELD_NR - i)*MAX_FILED_LT);
	

	int s = 0;
	switch(mode){
	case HYB_DF:
	{
		if(search_string_for_types(cbuf,types_i) == -1){
			return -1;
		}

		char buf[strlen(cbuf)+1];
		memset(buf,0,strlen(cbuf)+1);

		char *p_buf = &cbuf[0];
		int i;
		for(i = 0; *p_buf != '\0'; p_buf++){
			if(*p_buf == ' ') continue;
			
			if(i == 0 && *p_buf == ':') continue;
			if(i > 0){
				if(buf[i-1] == ':' && *p_buf == ':')continue;
			}
			buf[i] = *p_buf;
			i++;
		}

		s = get_fields_name_with_no_type(buf, names_s);
		break;
	}
	case HYB_WR:	
	{ 		
		if(search_string_for_types(cbuf,types_i) == -1){
			return -1;
		}
		char buf[strlen(cbuf)+1];
		memset(buf,0,strlen(cbuf)+1);

		char *p_buf = &cbuf[0];
		int i;
		for(i = 0; *p_buf != '\0'; p_buf++){
			if(*p_buf == ' ') continue;
			
			if(i == 0 && *p_buf == ':') continue;
			if(i > 0){
				if(buf[i-1] == ':' && *p_buf == ':')continue;
			}
			buf[i] = *p_buf;
			i++;
		}

		s = get_names_with_no_type_skip_value(buf,names_s);
			
		break;
	}
	default:
		fprintf(stderr,"mode not supported, %s:%d.\n",__FILE__,__LINE__);
		return -1;
	}
	
	int skip = -1;
	int j,x;
	for(j = 0; j < s; j++){
		int b = 0;
		for(x = 0; x < i; x++){
			size_t sz = strlen(names_s[j]);
			if(sz != strlen(names[x])) continue;
			if(strncmp(names_s[j],names[x],sz) == 0){
				skip = j;
				b = 1;
				break;
			}
		}
		if (b) break;
	}

	for(x = 0; names_s[x][0] != '\0'; x++){
		if (x == skip) continue;
		strncpy(names[i],names_s[x],strlen(names_s[x]));
		i++;
	}
	return i;
}

static int search_string_for_types(char *str, int *types)
{
	int i;
	for(i = 0;*str != '\0';str++,i++){
		if(types[i] == -1) break;
		switch(types[i]){
		case TYPE_KEY:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_ky:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			while((p = strstr(str,"@TYPE_KEY:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			break;

		}
		case TYPE_INT:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_i:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			while((p = strstr(str,"@TYPE_INT:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			break;
		}
		case TYPE_DATE:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_dt:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p);
					int k;
					for(k = 0; k < p_size; k++,p++)
						*p = ' ';

					break;/* break from switch */
				}
				int p_size = (int)(p_end - str) - (p - str);
				if(p_size < 0)return -1;

				int k;
				for(k = 0; k < p_size; k++,p++)
					*p = ' ';

				break;
			}

			while((p = strstr(str,"@TYPE_DATE:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p);
					int k;
					for(k = 0; k < p_size; k++,p++)
						*p = ' ';

					break;/* break from switch */
				}
				int p_size = (int)(p_end - str) - (p - str);
				if(p_size < 0)return -1;

				int k;
				for(k = 0; k < p_size; k++,p++)
					*p = ' ';

				break;
			}
			break;
		}
		case TYPE_LONG:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_l:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			while((p = strstr(str,"@TYPE_LONG:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;

				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			break;

		}
		case TYPE_FLOAT:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_f:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			while((p = strstr(str,"@TYPE_FLOAT:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;

		}
		case TYPE_DOUBLE:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_d:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,"@TYPE_DOUBLE:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_BYTE:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_b:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			while((p = strstr(str,"@TYPE_BYTE:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_STRING:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_s:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,"@TYPE_STRING:"))){

				char *p_end = strstr(p,":");
				if(!p_end) return -1;

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_INT ? "@t_ai:" : "@t_si:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,types[i] == TYPE_ARRAY_INT ? "@TYPE_ARRAY_INT:" : "@TYPE_SET_INT"))){
				char *p_end = strstr(p,":");
				if(!p_end) return -1;

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_LONG ? "@t_al:" : "@t_sl:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,types[i] == TYPE_ARRAY_LONG ? "@TYPE_ARRAY_LONG:" : "@TYPE_SET_LONG"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_BYTE ? "@t_ab:" : "@t_sb:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}


				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str, types[i] == TYPE_ARRAY_BYTE ? "@TYPE_ARRAY_BYTE:" : "@TYPE_SET_BYTE"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_FLOAT ? "@t_af:" : "@t_sf:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,types[i] == TYPE_ARRAY_FLOAT ? "@TYPE_ARRAY_FLOAT:" : "@TYPE_SET_FLOAT"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_DOUBLE ? "@t_ad:":"@t_sd:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,types[i] == TYPE_ARRAY_DOUBLE ? "@TYPE_ARRAY_DOUBLE:" : "@TYPE_SET_DOUBLE"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;	
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
		{
			char *p = 0x0;
			while((p = strstr(str,types[i] == TYPE_ARRAY_STRING ? "@t_as:" : "@t_ss:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,types[i] == TYPE_ARRAY_STRING ? "@TYPE_ARRAY_STRING:" : "@TYPE_SET_STRING"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		case TYPE_FILE:
		{
			char *p = 0x0;
			while((p = strstr(str,"@t_fl:"))){
				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}

			while((p = strstr(str,"@TYPE_FILE:"))){

				char *p_end = strstr(p,":");
				if(!p_end){
					int p_size = (int)strlen(p); 

					int k;
					for(k = 0; k < p_size; k++){
						*p = ' ';
						p++;
					}
					break;
				}

				int p_size =(int) (p_end - str) - (p - str); 
				if(p_size < 0) return -1;
		
				int k;
				for(k = 0; k < p_size; k++){
					*p = ' ';
					p++;
				}
			}
			break;
		}
		default:
			fprintf(stderr,"type not valid. %s:%d.\n",__FILE__,__LINE__);
			return -1;
		}	
	}

	return 0;
}
int is_num(char *key)
{
	unsigned char uint = 2;
	unsigned char str = 1;

	for (; *key != '\0'; key++)
		if (isalpha(*key) || ispunct(*key))
			return str;

	return uint;
}

void *key_converter(char *key, int *key_type)
{
	void *converted = 0x0;
	*key_type = is_num(key);
	switch (*key_type) {
	case 2:
	{
		long l = string_to_long(key);
		if (l == INVALID_VALUE) return 0x0;

		if (l == MAX_KEY) {
			fprintf(stderr, "key value out fo range.\n");
			return 0x0;
		}

		converted = (ui32*)malloc(sizeof(ui32));
		if (!converted) {
			fprintf(stderr,"malloc failed, %s:%d.\n",F, L - 2);
			return 0x0;
		}

		memset(converted,0,sizeof *converted);
		memcpy(converted, (ui32 *)&l, sizeof(ui32));
		break;
	}
	case 1:
		return 0x0;
	default:
		return 0x0;
	}

	return converted;
}

static char *strstr_last(char *src, char delim)
{

	int last = 0 ;
	char *p = src;
	for(; *p != '\0'; p++){
		if(*p == delim) last = p - src;		
	}
	
	if(last == 0) return 0x0;
	return &src[last];
}
int get_names_with_no_type_skip_value(char *buffer, char names[][MAX_FIELD_LT])
{
	/*copy the buffer*/
	size_t l = strlen(buffer);
	char cpy[l+1];
	memset(cpy,0,l+1);
	strncpy(cpy,buffer,l);
	

	if(strstr(buffer,"@") && !strstr(buffer,"@@")) replace('@','^',cpy);

	/*exclude file syntax*/
	if(!__IMPORT_EZ){
		char *f_st = 0x0;
		char *f_en = 0x0;
		while((f_st = strstr(cpy,"[")) && (f_en = strstr(cpy,"]"))){
			*f_st = '\n';
			for(; *f_st != ']'; f_st++){
				if(*f_st == ':') *f_st = '@';
			}
			*f_en = '\r';
		}

		if(strstr(cpy,"\n")) replace('\n','[',cpy);
		if(strstr(cpy,"\r")) replace('\r',']',cpy);
	}
	

	char *delim = ":";
	char *first = 0x0;
	char *last = 0x0;
	
	int i = 0;
	char *p = cpy;
	char *p2 = cpy;
	while(((first = strstr(p2,delim)) != 0x0) && ((last=strstr(&p[(first+1) - cpy],delim)) != 0x0)){
		char *move_back = (first - 1); 
		while(move_back != &cpy[0] && *move_back != '@' && *move_back != ':') --move_back;   
			
		int size = 0;
		if(move_back == &cpy[0])
			size = (first - cpy) - ( move_back - cpy) +1;
		else
			size = (first - cpy) - ( move_back - cpy);
		
		int next_start = last - p2;
		if(move_back == &cpy[0])
			strncpy(names[i],move_back,size-1);
		else
			strncpy(names[i],++move_back,size-1);

		p2 += next_start+1;
		i++;
	}

	if(first){
		if(*(first + 1) != '\0') {
			char *cf = (first - 1);		
			while(*cf != ':' && cf != &cpy[0] && *cf != '@') cf--;
			int size = 0; 
			if(cf != &cpy[0])	 
				size = first - (++cf) +1;
			else 
				size = first - cf +1;	
			
			strncpy(names[i],cf,size-1);
			i++;
		}
	}else if(strstr(cpy,delim) == 0x0 && !strstr(cpy,"@")){
		if(buffer){
			strncpy(names[i],cpy,strlen(buffer));
			i++;
		}
	}else if ((first = strstr(cpy,delim))){
		if(first[1] == '['){
			if((last = strstr(&first[1],delim))){
				if(last[strlen(last)-1] == ']') return i;
			}
		}

	}else if((last = strstr_last(cpy,'@'))){
		++last;
		strncpy(names[i],last,strlen(last));
	}else {
		return -1;
	}

	return i;
}


int three_file_path(char *file_path, char files[][MAX_FILE_PATH_LENGTH])
{
	char *dat = ".dat";
	char *ind = ".inx";
	char *sch = ".sch";

	size_t file_p_l= strlen(file_path);

	if(file_p_l > MAX_FILE_PATH_LENGTH) return EFLENGTH;
	if(file_p_l + strlen(dat) > MAX_FILE_PATH_LENGTH) return EFLENGTH;
	
	strncpy(files[0],file_path,file_p_l);
	strncpy(&files[0][file_p_l],ind,strlen(ind));

	strncpy(files[1],file_path,file_p_l);
	strncpy(&files[1][file_p_l],dat,strlen(dat));

	strncpy(files[2],file_path,file_p_l);
	strncpy(&files[2][file_p_l],sch,strlen(dat));
	
	return 0;
}

int count_fields(char *fields, const char *user_target)
{
	int c = 0;
	const char *p = fields;
	if(user_target){
		while ((p = strstr(p, user_target)) != 0x0) {
			c++;
			p++;
		}
		return c;
	}
	

	char *target = TYPE_;
	if(strstr(p,target) == 0x0){
		char *new_target = T_;
		if(strstr(p,new_target) != 0x0){
			while ((p = strstr(p, new_target)) != 0x0) {
				if(is_target_db_type((char *)p) == -1){
					p++;
					continue;
				}
				c++;
				p++;
			}
		}
	} else {
		char *second_target = T_;
		if(strstr(p,second_target) != 0x0){

			while ((p = strstr(p, second_target)) != 0x0) {
				if(is_target_db_type((char *)p) == -1){
					p++;
					continue;
				}
				c++;
				p++;
			}
		}

		p = fields;
		char *r = NULL;
		while ((r = strstr(p, target)) != 0x0) {
				c++;
				p = ++r;
		}

		while((p = strstr(p,":")) != 0x0){
			if(p[1] != '\0') c++;
			p++;
		}
	}

	if(c == 0){
		p = fields;
		int pos = 0;
		char *new_target = ":";
		while((p = strstr(p,new_target))){
			pos = p - fields;
			c++;
			p += strlen(new_target);
		}

		if(fields[pos+1] != '\0') c++;

		int cos = count_fields(fields,CON_) + count_fields(fields,C_);
		if(cos){
			char *def = NULL;
			replace('@','^',fields);
			while((def = strstr(fields,":CONST_DEFAULT")) 
					|| (def = strstr(fields,":c_df"))) {
				c--; 
				*def = '@';
			}
		
			replace('@',':',fields);
			replace('^','@',fields);
			c -= cos;
		}
	}
	return c;
}

int get_type(char *s){

	int sz = (int) strlen(s);

	if (sz == 8 && strncmp(s, "TYPE_INT",8) == 0) {
		return 0;
	} else if (sz == 9 && strncmp(s, "TYPE_LONG",9) == 0){
		return 1;
	} else if (sz == 10 && strncmp(s, "TYPE_FLOAT",10) == 0) {
		return 2;
	}
	else if (sz == 11 && strncmp(s, "TYPE_STRING",11) == 0)
	{
		return 3;
	}
	else if (sz == 9 && strncmp(s, "TYPE_BYTE",9) == 0)
	{
		return 4;
	}
	else if(sz == 9 && strncmp(s, "TYPE_PACK",9) == 0)
	{
		return 5;
	}
	else if (sz == 11 && strncmp(s, "TYPE_DOUBLE",11) == 0)
	{
		return 6;
	}else if (sz == 8 && strncmp(s, "TYPE_KEY",8) == 0)
	{
		return 15;
	}
	else if (sz == 3 && strncmp(s, "t_i",3) == 0)
	{
		return 0;
	}
	else if (sz == 3 && strncmp(s, "t_l",3) == 0)
	{
		return 1;
	}
	else if (sz == 3 && strncmp(s, "t_f",3) == 0)
	{
		return 2;
	}
	else if (sz == 3 && strncmp(s, "t_s",3) == 0)
	{
		return 3;
	}
	else if (sz == 3 && strncmp(s, "t_b",3) == 0)
	{
		return 4;
	}
	else if (sz == 3 && strncmp(s, "t_pk",4) == 0)
	{
		return 5;
	}
	else if (sz == 3 && strncmp(s, "t_d",3) == 0)
	{
		return 6;
	}
	else if (sz == 14 && strncmp(s, "TYPE_ARRAY_INT",14) == 0)
	{
		return 7;
	}
	else if (sz == 15 && strncmp(s, "TYPE_ARRAY_LONG",15) == 0)
	{
		return 8;
	}
	else if (sz == 16 && strncmp(s, "TYPE_ARRAY_FLOAT",16) == 0)
	{
		return 9;
	}
	else if (sz == 17 && strncmp(s, "TYPE_ARRAY_STRING",17) == 0)
	{
		return 10;
	}
	else if (sz == 15 && strncmp(s, "TYPE_ARRAY_BYTE",15) == 0)
	{
		return 11;
	}
	else if (sz == 17 && strncmp(s, "TYPE_ARRAY_DOUBLE",17) == 0)
	{
		return 12;
	}
	else if (sz == 9 && strncmp(s, "TYPE_FILE",9) == 0)
	{
		return 13;
	}
	else if (sz == 9 && strncmp(s, "TYPE_DATE",9) == 0){
		return 14;
	}else if (sz == 12 && strncmp(s, "TYPE_SET_INT",12) == 0){
		return 16;
	}else if (sz == 13 && strncmp(s, "TYPE_SET_LONG",13) == 0){
		return 16;
	}else if (sz == 14 && strncmp(s, "TYPE_SET_FLOAT",14) == 0){
		return 18;
	}else if (sz == 12 && strncmp(s, "TYPE_SET_BYTE",12) == 0){
		return 19;
	}else if (sz == 15 && strncmp(s, "TYPE_SET_STRING",15) == 0){
		return 20;
	}else if (sz == 15 && strncmp(s, "TYPE_SET_DOUBLE",15) == 0){
		return 21;
	}else if (sz == 4 && strncmp(s, "t_ai",4) == 0)	{
		return 7;
	}
	else if (sz == 4 && strncmp(s, "t_al",4) == 0)
	{
		return 8;
	}
	else if (sz == 4 && strncmp(s, "t_af",4) == 0)
	{
		return 9;
	}
	else if (sz == 4 && strncmp(s, "t_as",4) == 0)
	{
		return 10;
	}
	else if (sz == 4 && strncmp(s, "t_ab",4) == 0)
	{
		return 11;
	}
	else if (sz == 4 && strncmp(s, "t_ad",4) == 0)
	{
		return 12;
	}
	else if (sz == 4 && strncmp(s, "t_fl",4) == 0)
	{
		return 13;
	}else if (sz == 4 && strncmp(s, "t_dt",4) == 0){
		return 14;
	}else if (sz == 4 && strncmp(s, "t_ky",4) == 0){
		return 15;
	}else if (sz == 4 && strncmp(s, "t_si",4) == 0)	{
		return 16;
	}
	else if (sz == 4 && strncmp(s, "t_sl",4) == 0)
	{
		return 17;
	}
	else if (sz == 4 && strncmp(s, "t_sf",4) == 0)
	{
		return 18;
	}
	else if (sz == 4 && strncmp(s, "t_ss",4) == 0)
	{
		return 19;
	}
	else if (sz == 4 && strncmp(s, "t_sb",4) == 0)
	{
		return 20;
	}
	else if (sz == 4 && strncmp(s, "t_sd",4) == 0)
	{
		return 21;
	}


	return -1;
}

int get_fields_name_with_no_type(char *fields_name, char names[][MAX_FILED_LT])
{
	
	char *p = fields_name;
	char *delim = 0x0;
	int j = 0;
	int pos = 0;
	if(strstr(p,":") == 0x0){
		if(p){
			strncpy(names[j],p,strlen(p));
			j++;
			return j;
		}
	}
		
	while((delim = strstr(p,":"))){
		pos = delim - fields_name;
		*delim = '@';
		delim--;
		int i = 0;
		for( i = 1;*delim != ':' && *delim != '@' && delim != &p[0]; i++, delim--);
		
		if(delim != &p[0]) delim++, i--;

		strncpy(names[j],delim,i);
		p += (i+1);
		j++;
	}

	if(fields_name[pos + 1] != '\0' && strstr(fields_name,"@@") == 0x0) {
		strncpy(names[j],&fields_name[pos+1],strlen(&fields_name[pos + 1]));
		j++;
	}

	return j;
}

int map_constraints(char *c)
{
	if(strncmp(c,"CONST_NOT_NULL",14) == 0 
			|| strncmp(c,"c_n_n",5) == 0 )
		return CONST_NOT_NULL;

	if(strncmp(c,"CONST_UNIQUE",11) == 0
			|| strncmp(c,"c_un",4) == 0)
		return CONST_UNIQUE;

	if(strncmp(c,"CONST_DEFAULT",13) == 0
			|| strncmp(c,"c_df",4) == 0)
		return CONST_DEFAULT;

	if(strncmp(c,"CONST_FOREIGN_KEY",17) == 0
			|| strncmp(c,"c_fky",5) == 0)
		return CONST_FOREIGN_KEY;

	return -1;
}


int get_constrains(char *buff, int field_count,int **cnstr,char ***value)
{
	/*
	 * if field_count is 0, it means the data does not have type
	 * */
	if(!cnstr)
		return -1;

	char *c = NULL;
	if(!(c = strstr(buff,CON_)))
			return 0; /*no constraints in the input*/

	replace('@','^',buff);

	ui8 no_type = 0;
	if(!field_count){
		field_count = count_fields(buff,NULL);
		no_type = 1;
	}

	char **val = NULL; 
	int field_pos[field_count];
	memset(field_pos,-1,field_count * sizeof(int));

	/* 
	 * if the buff has types, we nee to know the fields position
	 * to properly store the costraints
	 * */

	if(!no_type){
		int sz = (int)strlen(buff);
		char cpy[sz+1];
		memset(cpy,0,sz+1);
		strncpy(cpy,buff,sz);

		int i = 0;
		char *t = NULL;
		while((t = strstr(cpy,T_))){
			assert(i < field_count);
			*t = ' ';
			while(t != &cpy[0] && *t && *t != ':') t--;
			if(t == &cpy[0]){
				field_pos[i++] = 0;
				continue;
			}

			if(*t == ':')
				field_pos[i++] = (int) (++t - cpy);

		}

		while((t = strstr(cpy,TYPE_))){
			assert(i < field_count);

			*t = ' ';
			while(t != &cpy[0] && *t && *t != ':') t--;
			if(t == &cpy[0]){
				field_pos[i++] = 0;
				continue;
			}

			if(*t == ':')
				field_pos[i++] = (int) (++t - cpy);
		}
	} else {
		/*understand the field position even with no type*/
		int sz = (int)strlen(buff);
		char cpy[sz+1], *p = &cpy[0];
		memset(cpy,0,sz+1);
		strncpy(cpy,buff,sz);
		

		int i =0;
		field_pos[i++] = p - cpy;		
		if(field_count > 1){
			do{
				char *f = strstr(p,":");
				if(!f)
					break;

				*f++ = ' ';
				p = f; 
				f = strstr(p,":");
				if(!f)
					break;

				int s = f - p;

				char ch[s+1];
				ch[s] = '\0';
				strncpy(ch,p,s);

				if(!strstr(ch,CON_) && !strstr(ch,C_)){
					field_pos[i++] = p - cpy;	
					p += s;
					continue;
				}
			}while(*p);
		}
	}

#if defined(_WIN32)
	*cnstr = array_init(field_count,ARR_INT);
#else
	*cnstr = array_init(field_count,INT);
#endif
	if(!(*cnstr)){
		fprintf(stderr,"array_init() failed, %s:%d.\n",F, L - 2);
		return -1;
	}

	int f; 
	for(f = 0; f < field_count; f++){
		char *next = NULL;
		int	or_c = 0, exit = 0;
		do{
			if(!no_type){
				next = strstr(c,T_);
				if(!next)
					next = strstr(c,TYPE_);

				int position = (int)(c - buff);
				if(field_pos[f] != position){
					int i;
					for(i = 0; i < field_count; i++){
						if(position > field_pos[i])
							f = i;	

						if(position < field_pos[i])
							break;
					}
				}
			}
			if(c){
				*c++ = ' ';
				int position = (int)(c - buff);
				if(field_pos[f] != position){
					int i;
					for(i = 0; i < field_count; i++){
						if(position > field_pos[i])
							f = i;	

						if(position < field_pos[i])
							break;
					}
				}
			}else{
				break;
			}

			char *p = c;
			while(*p && *p != ':') p++;

			int l = p - c;
			char constraint[l+1];
			constraint[l] = '\0';
			strncpy(constraint,c,l);

			int m = map_constraints(constraint);
			if(m == -1){
				array_free(*cnstr);
				return -1;
			}

			or_c |= m;
			if(m == CONST_DEFAULT){
				while(*c && *c != ':') *c++ = ' ';
				if(*c)
					*c++ = ' ';

				int j;
				p = c;
				for(j = 0; *p && *p != ':'; j++,p++);

				char b[j+1];
				b[j] = '\0';
				strncpy(b,c,j);

				while(*c && *c != ':') *c++ = ' ';

				if(!val){
					val = (char**)array_init(10,STRING);
					if(!val){
						fprintf(stderr,"array_init failed, %s:%d.\n",F, L - 2);
						array_free(*cnstr);
						return -1;
					}
				}
				array_insert_at(f,(void **)&val,b);
			}else{

				while(*c && *c != ':') *c++ = ' ';
				/* 
				 * understand if there are more constraint for 
				 * this field
				 * */
				if(no_type){
					char *more = NULL;
					if((more = strstr(c,CON_))){
						int s =  more - c;
						if(s > 1){
							c = strstr(buff,CON_);
								break;
						}
					}
				}
			}
			
			
			if(!(c = strstr(buff,CON_)) && !(c = strstr(buff,C_))){
				exit = 1;
				break;
			}

		}while( (!no_type && next && c < next) || no_type);
		array_insert_at(f,(void**)cnstr,&or_c);
		if(exit)
			break;
	}

	if(val)
		*value = val;

	replace('@',':',buff);
	replace('^','@',buff);
	/*change the buffer to hold only the fields */
	int sz = (int)strlen(buff);
	char cpy[sz+1];
	memset(cpy,0,sz+1);

	int i,j;
	for(i = 0, j = 0; i < sz; i++){
		if(buff[i] != ' '){
			if(j < sz){
				cpy[j] = buff[i];
				j++;
			}
		}
	}

	memset(buff,0,strlen(buff));
	strncpy(buff,cpy,i);
	return 0;
}

int get_fileds_name(char *fields_name, int fields_count, int steps, char names[][MAX_FILED_LT])
{
	if (fields_count == 0) return -1;

	int j = 0;
	char *s = 0x0;

	while ((s = tok(fields_name, ":")) != 0x0 && j < fields_count) {
		strncpy(names[j],s,strlen(s));

		j++;

		tok(0x0, ":");

		if (steps == 3 && j < fields_count)
			tok(0x0, ":");
	}

	return 0;
}

unsigned char check_fields_integrity(char names[][MAX_FILED_LT], int fields_count)
{
	int c = 0;

	if(fields_count == 0) return 0;

	int i,j;
	for (i = 0; i < fields_count; i++) {
		if ((fields_count - i) == 1)
			break;

		for (j = 0; j < fields_count; j++) {
			if (((fields_count - i) > 1) && ((fields_count - j) > 1))
			{
				if (j == i)
					continue;

				int l_i = strlen(names[i]);
				int l_j = strlen(names[j]);
				if(names[j][0] == '\0') c++;
				if(c == fields_count) return 0;

				if (l_i == l_j)
				{
					if (strncmp(names[i], names[j], l_j) == 0)
						return 0;
				}
			}
		}
	}
	return 1;
}

int check_array_input(char *value)
{
	size_t size = strlen(value) + 1;
	char cbuff[size];
	memset(cbuff,0,size);
	strncpy(cbuff,value,size);

	char *p = 0x0;
	int elements = 0;
	int d = 0;
	int i = 0;
	int pos = 0;
	while((p = strstr(cbuff,","))){
		if(p[1] == '\0') break;

		elements++;
		int start = p - cbuff;
		while((p != &cbuff[0] && *p != '@')) p--;

		if(*p == '@') ++p;

		int l = (&cbuff[start] - p)+1;
		pos = start ;
		char cpy[l];
		memset(cpy, 0, l);

		strncpy(cpy,p,l-1);

		if(is_floaintg_point(cpy)){
			if(is_number_in_limits(cpy)) d++;
		}else if(is_integer(cpy)){
			if(is_number_in_limits(cpy)) i++;
		}

		cbuff[start] = '@';
	}

	if(cbuff[pos] != '\0' && p == 0x0){
		elements++;
		char *last = &cbuff[pos];
		size_t size = strlen(++last) + 1;
		char n[size];
		memset(n,0,size);

		strncpy(n,last,size-1);
		if(is_floaintg_point(n)){
			if(is_number_in_limits(n)) d++;
		}else if(is_integer(n)){
			if(is_number_in_limits(n)) i++;
		}

	}

	if(i == elements) return TYPE_ARRAY_LONG;
	if(d == elements) return TYPE_ARRAY_DOUBLE;

	return -1;


}
int assign_type(char *value)
{

	if(!value) return -1;

	if(value[0] == '[' && value[strlen(value) - 1] == ']') return TYPE_FILE;

	if(strstr(value,",")){
		/*ARRAY case*/
		int result = 0;
		if((result = check_array_input(value)) == - 1) return TYPE_ARRAY_STRING;

		return result;	
	}


	if(is_floaintg_point(value)) {
		if(is_number_in_limits(value)) return TYPE_DOUBLE;

		return -1;
	}

	if(is_integer(value)){
		if(is_number_in_limits(value)) return TYPE_LONG;

		return -1;
	}

	if(strlen(value) > 1){

		size_t sz = strlen(value);
		char cpy[sz+1];
		memset(cpy,0,sz+1);
		memcpy(cpy,value,sz);

		char *p = &cpy[0];
		for(;*p != '\0';p++){
			TO_LOWER(p)			
		}
		if(strncmp(cpy,"false",strlen(cpy)) == 0) return TYPE_BYTE;
		if(strncmp(cpy,"true",strlen(cpy)) == 0) return TYPE_BYTE;
	}else{
		char c = *value;
		char *p = &c;
		TO_LOWER(p)
			if(c == 'n' || c == 'y') return TYPE_BYTE;
	}
	return TYPE_STRING;
}



int get_value_types(char *fields_input, int fields_count, int steps, int *types)
{

	if (fields_count == 0) return -1;

	int i = steps == 3 ? 0 : 1;
	int j = 0;

	char *s = 0x0;
	s = tok(fields_input, ":");

	s = tok(0x0, ":");

	if (s){
		int result = get_type(s);
		if (result == -1) return -1;
		types[j] = result;
		i++;
	}else {
		if(fields_count > 1){
			fprintf(stderr,"type token not found in get_value_types().\n");
			return -1;
		}
		return 0;
	}

	while ((s = tok(0x0, ":")) != 0x0) {
		if ((i % 3 == 0) && (j < fields_count - 1)) {
			j++;
			int r  = get_type(s);
			if (r == -1){
				fprintf(stderr,"check input format: fieldName:TYPE:value.\n");
				return -1;
			}
			types[j] = r;
		}

		if (steps != 3 && i == 2) {
			i++;
		}else if (steps != 3 && i == 3) {
			i--;
		}else{
			i++;
		}
	}
	return 0;
}

int get_values_hyb(char *buff,char ***values,  int fields_count)
{	
	size_t size = strlen(buff)+1;
	char cbuff[size];
	memset(cbuff,0,size);
	strncpy(cbuff,buff,size);

	*values = (char**)malloc(fields_count * sizeof(char *));
	if (!(*values)) {
		fprintf(stderr,"malloc() failed, %s:%d.\n",F,L-2);
		return -1;
	}

	memset(*values,0,fields_count * sizeof(char*));
	/* detect files types - ONLY IF NOT IMPORTING FROM EZgen*/	
	char *file_start = 0x0;
	int count = 0;
	if(!__IMPORT_EZ){

		while((file_start = strstr(cbuff,"["))){
			char *end_file = strstr(file_start,"]");
			if(end_file){ 
				int end = end_file - cbuff;
				char *delim = 0x0;
				while((delim = strstr(file_start,":"))){
					if((delim - cbuff) < end) 
						*delim = '&';
					else
						break;
				}
			}

			*file_start = '@';
			count++;
		}
		if(count){
			replace('@','[',cbuff);
			replace('&','@',cbuff);
		}
	}

	int i = 0;
	int reset_mistype_index[10];
	memset(reset_mistype_index,-1,sizeof(int)*10);
	int rmi = 0;
	if(strstr(cbuff,T_)){
		char *pos_t = 0x0;
		while((pos_t = strstr(cbuff,T_))){
			/*this change the delimeter so it won't
			 * be found in the next iteration*/
			if(is_target_db_type(pos_t) == -1){
				*pos_t = '@';
				reset_mistype_index[rmi] = pos_t - cbuff;
				rmi++;
				continue;
			}
			*pos_t= '@';
			char *end_t = (strstr(pos_t,":"));
			assert(end_t != 0x0); 

			*end_t = '@';
			char *next = strstr(end_t,":");
			if(!next){
				++end_t;
				if(count > 0){
					/*clean the value*/
					replace('@',':',end_t);
				}
				(*values)[i] = duplicate_str(end_t);
				if(!(*values)[i]){
					fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
					if(i == 0)
						free(*values);
					else
						free_strs(i,1,values);

					return -1;
				}
				i++;
				break;
			}
			*next = '@';
			size_t len = (next - cbuff) - (++end_t - cbuff) + 1;
			char cpy[len];
			memset(cpy,0,len);
			strncpy(cpy,end_t,len-1);

			if(count > 0){
				/*clean the value*/
				replace('@',':',cpy);
			}
			(*values)[i] = duplicate_str(cpy);
			if(!(*values)[i]){
				fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
				if(i == 0)
					free(*values);
				else
					free_strs(i,1,values);
				return -1;
			}
			i++;
		}		
	}
	rmi = 0;
	while(reset_mistype_index[rmi] != -1){
		cbuff[reset_mistype_index[rmi]] = ':';
		rmi++;
	}


	if(strstr(cbuff,TYPE_)){
		char *pos_T = 0x0;
		while((pos_T = strstr(cbuff,TYPE_))){
			/*this change the delimeter so it won't
			 * be found in the next iteration*/
			*pos_T= '@';
			char *end_T = (strstr(pos_T,":"));
			assert(end_T != 0x0); 

			*end_T = '@';
			char *next = strstr(end_T+1,":");
			if(!next){
				++end_T;
				if(count > 0){
					/*clean the value*/
					replace('@',':',end_T);
				}
				(*values)[i] = duplicate_str(end_T);
				if(!(*values)[i]){
					fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
					if(i == 0)
						free(*values);
					else
						free_strs(i,1,values);

					return -1;
				}
				i++;
				break;
			}

			*next = '@';
			size_t len = (next - cbuff) - (++end_T- cbuff) + 1;
			char cpy[len];
			memset(cpy,0,len);
			strncpy(cpy,end_T,len-1);

			if(count > 0){
				/*clean the value*/
				replace('@',':',cpy);
			}
			(*values)[i] = duplicate_str(cpy);
			if(!(*values)[i]){
				fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
				if(i == 0)
					free(*values);
				else
					free_strs(i,1,values);

				return -1;
			}
			i++;
		}		
	}

	if(strstr(cbuff,":")){
		char *pos_d = 0x0;
		while((pos_d = strstr(cbuff,":"))){
			/*this change the delimeter so it won't
			 * be found in the next iteration*/
			*pos_d= '@';
			char *end_d = strstr(pos_d,":");
			if(!end_d){
				++pos_d;		
				if(count > 0){
					/*clean the value*/
					replace('@',':',pos_d);
				}
				(*values)[i] = duplicate_str(pos_d);
				if(!(*values)[i]){
					fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
					if(i == 0)
						free(*values);
					else
						free_strs(i,1,values);

					return -1;
				}
				break;
			}

			size_t len = end_d - (++pos_d) + 1;
			char cpy[len];
			memset(cpy,0,len);
			*end_d = '@';
			strncpy(cpy,pos_d,len-1);

			if(count > 0){
				/*clean the value*/
				replace('@',':',cpy);
			}
			(*values)[i] = duplicate_str(cpy);
			if(!(*values)[i]){
				fprintf(stderr,"duplicate_str() failed, %s:%d\n",__FILE__,__LINE__-2);
				free_strs(i,1,values);
				return -1;
			}
			i++;
		}		
	}

	return 0;
}
char ** get_values_with_no_types(char *buff,int fields_count)
{
	size_t l = strlen(buff);
	char cpy[l+1];
	memset(cpy,0,l+1);
	strncpy(cpy,buff,l);

	char *delim = ":";
	char *first = 0x0;
	char *last = 0x0;

	char **values = (char**)malloc(fields_count * sizeof(char *));
	if (!values) {
		fprintf(stderr,"memory get values %s:%d\n",__FILE__,__LINE__-2);
		return 0x0;
	}

	memset(values,0,fields_count * sizeof(char*));
	if(strstr(buff,"@")) replace('@','^',cpy);

	/*exclude file syntax*/
	char *f_st = 0x0;
	char *f_en = 0x0;
	while((f_st = strstr(cpy,"[")) && (f_en = strstr(cpy,"]"))){
		*f_st = '\n';
		for(; *f_st != ']'; f_st++){
			if(*f_st == ':') *f_st = '@';
		}
		*f_en = '\r';
	}

	if(strstr(cpy,"\n")) replace('\n','[',cpy);
	if(strstr(cpy,"\r")) replace('\r',']',cpy);


	int i = 0;
	char *p2 = cpy;
	char *p = cpy;
	while((first = strstr(p2,delim)) != 0x0 && (last = strstr(&p[(first+1) - cpy],delim)) != 0x0){
		/*	if(first[1] == '[' && ((file_block = strstr(&first[1],"]")))){
			int start = (first + 1) - buff;
			int size = (file_block - buff) - start +1;
			char cpy[size+1];
			memset(cpy,0,size+1);
			strncpy(cpy,&buff[start],size);
			values[i] = duplicate_str(cpy);
			if(!values[i]){
			fprintf(stderr,"duplicate_str() failed, %s:%d",__FILE__,__LINE__-2);
			return 0x0;
			}
			p2 = (first + 1) + size+1;
			i++;
			continue;
			}
			*/
		int start = (first + 1) - cpy;
		int size = (last - cpy) - start;
		char bpy[size+1];
		memset(bpy,0,size+1);
		strncpy(bpy,&buff[start],size);

		values[i] = duplicate_str(bpy);
		if(!values[i]){
			fprintf(stderr,"duplicate_str() failed, %s:%d",__FILE__,__LINE__-2);
			return 0x0;
		}

		if(strstr(values[i],"@")) replace('@',':',values[i]);
		if(strstr(values[i],"^")) replace('^','@',values[i]);

		p2 = (first + 1) + size+1;
		i++;
	}

	if(first){
		if(*(first + 1) != '\0') {
			values[i] = duplicate_str(&first[1]);
			if(!values[i]){
				fprintf(stderr,"duplicate_str() failed, %s:%d",__FILE__,__LINE__-2);
				return 0x0;
			}
			if(strstr(values[i],"@")) replace('@',':',values[i]);
			if(strstr(values[i],"^")) replace('^','@',values[i]);
		}
	}

	return values;
}

char **get_values(char *fields_input, int fields_count)
{
	int i = 0, j = 0;

	char **values = (char**)malloc(fields_count * sizeof(char *));
	if (!values) {
		fprintf(stderr,"memory get values %s:%d",__FILE__,__LINE__-2);
		return 0x0;
	}

	memset(values,0,fields_count * sizeof(char*));

	/*detect file type*/

	char *s = 0x0;
	char *excluded_input =  exclude_sub_str("[","]",fields_input,1);

	if(excluded_input){
		tok(excluded_input, ":");
		tok(0x0, ":");
	}else{
		tok(fields_input, ":");
		tok(0x0, ":");
	}

	s = tok(0x0,":");

	if (s){
		if(strncmp(s,"[]",2) == 0){
			values[j] = duplicate_str(get_sub_str("[","]",fields_input,0));		
			if (!values[j]){
				free(values);
				return 0x0;
			}
			i++;
		}else{
			values[j] = duplicate_str(s);
			if (!values[j]){
				free(values);
				return 0x0;
			}
			i++;
		}
	}
	else
	{
		fprintf(stderr,"value token not found in get_values();\n");
		free(values);
		return 0x0;
	}

	while ((s = tok(0x0, ":")) != 0x0)
	{
		if ((i % 3 == 0) && (fields_count - j >= 1)){
			j++;
			values[j] = duplicate_str(s);
		}


		if (!values[j]){
			free_strs(j,i,values);
			return 0x0;
		}
		i++;
		s = 0x0;
	}

	return values;
}

void free_strs(int fields_num, int count, ...)
{
	va_list args;
	va_start(args, count);

	int i,j;
	for (i = 0; i < count; i++){
		char **str = va_arg(args, char **);
		if(!str)
			continue;

		for (j = 0; j < fields_num; j++){
			if (str[j]){
				free(str[j]);
			}
		}
		free(str);
	}
}

int is_file_name_valid(char *str)
{
	size_t l = strlen(str);
	size_t i;
	for (i = 0; i < l; i++)
	{
		if (ispunct(str[i]))
			if ((str[i] != '-' && str[i] != '/'&& str[i] != '\\' && str[i] != '_' && str[i] != '.')) return 0;

		if (isspace(str[i]))
			return 0;
	}
	return 1;
}

void strip(const char c, char *str)
{
	size_t l = strlen(str);
	char cpy[l];
	memset(cpy,0,l);

	size_t i,j;
	for (i = 0, j = 0; i < l; i++){
		if (str[i] == c)continue;

		cpy[j] = str[i];
		j++;
	}

	memset(str,0,l);
	strncpy(str,cpy,l);
}

void replace(const char c, const char with, char *str)
{
	size_t l = strlen(str);
	size_t i;
	for (i = 0; i < l; i++)
		if (str[i] == c)
			str[i] = with;
}


char return_last_char(char *str)
{
	size_t l = strlen(str);
	return str[l - 1];
}

size_t digits_with_decimal(float n)
{
	char buf[20];
	if (copy_to_string(buf, 20, "%.2f", n) < 0)
	{
		fprintf(stderr,"copy_to_string failed %s:%d.\n", F, L - 2);
		return -1;
	}

	size_t i = 0;
	for (i = 0; i < 20; i++)
	{
		if (buf[i] == '.')
			return ++i;
	}

	return (size_t)-1;
}

unsigned char is_floaintg_point(char *str)
{
	size_t l = strlen(str);
	int point = 0;
	size_t i;
	for (i = 0; i < l; i++)
	{
		if (isdigit(str[i]))
		{
			if (str[i] == '.')
				return 1;
		}
		else if (str[i] == '.')
		{
			point++;
		}
		else if (str[i] == '-' && i == 0)
		{
			continue;
		}
		else
		{
			return 0;
		}
	}

	return point == 1;
}

unsigned char is_integer(char *str)
{
	size_t l = strlen(str);
	size_t i;
	for (i = 0; i < l; i++){
		if (!isdigit(str[i])){
			if (str[i] == '-' && i == 0){
				continue;
			}else{
				return 0;
			}
		}
	}
	return 1;
}


float __round_alt(float n)
{
	char buff[20];
	if (copy_to_string(buff, 20,"%f", n) < 0)
		return -1;

	size_t digit = 0;

	if ((digit = digits_with_decimal(n) + 2) > 20)
		return -1;

	int num = (int)((int)buff[digit] - '0');
	if (num > 5){
		n += 0.01;
	}

	return n;
}

unsigned char is_number_in_limits(char *value)
{
	if (!value)
		return 0;

	size_t l = strlen(value);
	unsigned char negative = value[0] == '-';
	int ascii_value = 0;

	if (is_integer(value)){

		size_t i;
		for (i = 0; i < l; i++)
			ascii_value += (int)value[i];

		if (negative){
			if (-1 * ascii_value < -1 * ASCII_INT_MIN){
				if (-1 * ascii_value < -1 * ASCII_LONG_MIN)
					return 0;
				else
					return IN_LONG;
			}

			return IN_INT;
		}

		if (ascii_value > ASCII_INT_MAX){
			if (ascii_value > ASCII_LONG_MAX)
				return 0;
			else
				return IN_LONG;
		}

		return IN_INT;
	}

	if (is_floaintg_point(value)){
		size_t i;
		for (i = 0; i < l; i++)
			ascii_value += (int)value[i];

		if (negative){
			if (-1 * ascii_value < -1 * ASCII_FLT_MAX_NEGATIVE){
				if (-1 * ascii_value < -1 * ASCII_DBL_MAX_NEGATIVE)
					return 0;
				else
					return IN_DOUBLE;
			}

			return IN_FLOAT;
		}

		if (ascii_value > ASCII_FLT_MAX){
			if (ascii_value > ASCII_DBL_MAX)
				return 0;
			else
				return IN_DOUBLE;
		}
		return IN_FLOAT;
	}

	return 0;
}

int find_last_char(const char c, char *src)
{
	size_t l = strlen(src);
	int last = -1;
	size_t i;
	for (i = 0; i < l; i++){
		if (src[i] == c)
			last = i;
	}

	return last;
}

int count_delim(char *delim, char *str)
{
	size_t l = strlen(str) + 1;
	char buf[l];
	memset(buf,0,l);
	strncpy(buf,str,l-1);

	int c = 0;
	char *d = 0x0;

	while((d = strstr(buf,delim))){
		c++;
		*d = '@';	
	}

	return c;
}

int find_double_delim(char *delim, char *str, int *pos, struct Schema sch)
{

	char *c = 0x0;
	int f = 0;
	while((c = strstr(str,delim))){
		f = 1;
		if(find_delim_in_fields(":",str,pos,sch) == 0) return 0;
		if(*(c + 2) == '\0'){
			c++;
			*c = '\0';
			break;	
		}
		c++;
		*c = '{';
		if(pos){
			*pos = c - str;
			pos++;
		}
	}
	if(f) return 0;

	return -1;
}

int find_delim_in_fields(char *delim, char *str, int *pos, struct Schema sch)
{
	if(strstr(str, delim) == 0x0) return -1;
	size_t l = strlen(str) + 1;
	char buf[l];
	memset(buf,0,l);
	strncpy(buf,str,l-1);

	/* in the case of UTILITY usage we need to exclude the type file syntax
	 * field_name:[file data]*/
	i16 f_start[50];
	i16 f_end[50];
	memset(f_start,-1,sizeof(i16)*50);
	memset(f_end,-1,sizeof(i16)*50);

	if(__UTILITY){
		char *fs = 0x0;
		char *fe = 0x0;
		int i = 0;
		for(; (fs = strstr(buf,"[")) && (fe = strstr(buf,"]")); i++){
			if (i == 50){
				fprintf(stderr,"(%s): code refactor needed for %s() %s:%d\n",prog,__func__,__FILE__,__LINE__-10);
				break;
			}

			f_start[i] = fs - buf;
			f_end[i] = fe - buf;
			*fs = '@';
			*fe = '&';
		}

		if (i > 0) {
			replace('@','[',buf);
			replace('&',']',buf);
		}
	}
	/*
	 * -step 1;
	 * erase the field name from the buf string and the attached ':'*/
	int i;
	for(i = 0; i < sch.fields_num;i++){
		char *f = 0x0;
		char single_char_field[3] = {0};
		if(strlen(sch.fields_name[i]) == 1){
			single_char_field[0] = *sch.fields_name[i];
			single_char_field[1] = ':';
			if(!(f = strstr(buf,single_char_field)))
				continue;
		} else{
			if(!(f = strstr(buf,sch.fields_name[i]))) 
				continue;
		}

		if(f == &buf[0]) {
			while( *f != ':'){
				*f = ' '; 
				f++;
			}
			if(*f == ':')
				*f = ' ';

			continue;
		}
		f--;
		/* 
		 * if f is not ':' it means that 
		 * this is not a field  or a similar field, i.e name and last_name 
		 * so we need to skip and look again
		 * for the field 
		 * */

		/* 
		 * check for a similir field
		 * example name and last_name
		 * */
		if(*f == ':'){
			*f = ' ';
		}else if(f != &buf[0]){
			while(*f != ':' && f != &buf[0]) f--;
			if(f == &buf[0] || *f == ':'){

				if (*f == ':') *f = ' ';
				int size = 0;
				if(f == &buf[0])
					size = strstr(f,":") - f;
				else 
					size = strstr(&f[1],":") - f;

				if (size < 0) continue;

				char cpy[size+1];
				memset(cpy,0,size+1);
				strncpy(cpy,f,size);
				if(strlen(sch.fields_name[i]) == (size_t)size){
					if(strncmp(cpy,sch.fields_name[i],size) == 0){
						while(*f != ':'){
							*f = ' ';
							f++;
						}
						if(*f == ':') *f = ' ';
						continue;
					}
				}

				continue;
			}
		}else{
			f++;
			*f = '@';
			i--;/* this is to recheck on the same field in the next iteration*/
			continue;
		}

		while( *f != ':' && *f != '\0'){
			*f = ' '; 
			f++;
		}
		if(*f == ':') *f = ' ';
	}

	i = 0;
	if(strstr(buf," ") == 0x0) return -1;
	char *start = 0x0;
	while((start = strstr(buf,"TYPE_"))){
		*start = '@';
		while(*start != ':' && *start != '\0') start++;

		if(*start == ':') *start = ' ';
	}

	while((start = strstr(buf,"t_"))){
		if(is_target_db_type(start) == -1)break;

		*start = '@';
		while(*start != ':' && *start != '\0') start++;

		if(*start == ':') *start = ' ';
	}


	/* at this point only the ':' inside the fields are left*/
	char *delim_in_fields = 0x0;
	i8 cont = 0;
	while((delim_in_fields = strstr(buf,delim))){

		int p = delim_in_fields - buf;
		if(f_start[0] > -1 && f_end[0] > -1){
			int i;
			for(i = 0; f_start[i] != -1 && f_end[i] != -1; i++){
				if (p > f_start[i] && p < f_end[i]){
					*delim_in_fields = '@';
					cont = 1;	
					break;
				}
			}
			if(cont){
				cont = 0;
				continue;
			}
		}

		str[p] = '{';
		if(i < 200){
			pos[i] = p;
			i++;
		}
		*delim_in_fields = '@';
	}

	return 0;
}

char *find_field_to_reset_delim(int *pos, char *buffer)
{
	static char field[MAX_FILED_LT] = {0};
	int i;	
	for(i = 0; i < 200; i++){
		if (pos[i] == -1) break;

		char *p = &buffer[pos[i]];
		int count = 0;
		int start = 0;
		int end = 0;
		int variable_steps = 2;
		while(count < variable_steps) {
			if(p == &buffer[0]) break;
			p--;
			if (*p == ':') {
				if(count == 0) end = p - buffer;
				if(count == 1) start = p - buffer;
				if(count == 2) start = p - buffer;

				if(count == 1){
					char *type = 0x0;
					if((type = strstr(p,TYPE_))){
						if(p == type){ 		
							end = p - buffer;
							variable_steps = 3;
						}
					}
				}
				count++;
			}
		}

		if(p == &buffer[0])
			strncpy(field, &buffer[start],end - start -1);
		else
			strncpy(field, &buffer[start+1],end - start -1);
	}

	return field;
}

static int is_target_db_type(char *target)
{
	int size = 0;
	size_t l = strlen(target);
	if(l > 4){
		/*extract the real target*/
		if(*target == ':'){
			char *e = strstr(&target[1],":");
			if(e) size = e - target;
		}else{
			char *e = strstr(target,":");
			if(e) size = e - target;
		}

	}

	if(size > 5) return -1;

	int b_size = 0;
	if(*target == ':') b_size += size + 1;
	if(*target != ':') b_size += (size + 2);


	char tg[b_size];
	memset(tg,0,b_size);

	if(size > 0){
		if(*target == ':') {
			strncpy(tg,target,size);
		}else{
			tg[0] = ':';
			strncpy(&tg[1],target,size);
			size++;
		}

		if(strlen(tg) == strlen(":t_s")) if(strncmp(tg,":t_s",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_l")) if(strncmp(tg,":t_l",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_b")) if(strncmp(tg,":t_b",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_d")) if(strncmp(tg,":t_d",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_i")) if(strncmp(tg,":t_i",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_f")) if(strncmp(tg,":t_f",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_p")) if(strncmp(tg,":t_p",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_ai")) if(strncmp(tg,":t_ai",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_al")) if(strncmp(tg,":t_al",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_af")) if(strncmp(tg,":t_af",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_ad")) if(strncmp(tg,":t_ad",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_as")) if(strncmp(tg,":t_as",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_ab")) if(strncmp(tg,":t_ab",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_fl")) if(strncmp(tg,":t_fl",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_dt")) if(strncmp(tg,":t_dt",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_si")) if(strncmp(tg,":t_si",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_sl")) if(strncmp(tg,":t_sl",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_sf")) if(strncmp(tg,":t_sf",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_sd")) if(strncmp(tg,":t_sd",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_ss")) if(strncmp(tg,":t_ss",size) == 0) return 0;
		if(strlen(tg) == strlen(":t_sb")) if(strncmp(tg,":t_sb",size) == 0) return 0;


		return -1;

	}

	if(strstr(target,":t_s")) return 0;
	if(strstr(target,":t_l")) return 0;
	if(strstr(target,":t_b")) return 0;
	if(strstr(target,":t_d")) return 0;
	if(strstr(target,":t_i")) return 0;
	if(strstr(target,":t_f")) return 0;
	if(strstr(target,":t_p")) return 0;
	if(strstr(target,":t_ai")) return 0;
	if(strstr(target,":t_al")) return 0;
	if(strstr(target,":t_as")) return 0;
	if(strstr(target,":t_ad")) return 0;
	if(strstr(target,":t_af")) return 0;
	if(strstr(target,":t_ab")) return 0;
	if(strstr(target,":t_fl")) return 0;
	if(strstr(target,":t_si")) return 0;
	if(strstr(target,":t_sl")) return 0;
	if(strstr(target,":t_ss")) return 0;
	if(strstr(target,":t_sd")) return 0;
	if(strstr(target,":t_sf")) return 0;
	if(strstr(target,":t_sb")) return 0;


	return -1;

}

void pack(ui32 n, ui8 *digits_indexes)
{
	if((n <= (BASE-1))) {
		memset(digits_indexes,255,sizeof(ui8) * 5);
		digits_indexes[4] = n;
		return;
	}

	int i = 4;	
	memset(digits_indexes,255,sizeof(ui8) * 5);

	ui32 rm = 0;
	while(n > 0){
		if (i < 0){
			memset(digits_indexes,255,sizeof(ui8) * 5);
			return;
		}
		rm = n % BASE;
		n /= BASE;
		digits_indexes[i] = rm;
		i--;
	}
}

void print_pack_str(ui8 *digits_index)
{
	int i;
	for(i = 0; i < 5; i++){
		if(digits_index[i] == 255) continue;
		fprintf(stderr,"%s",base_247[digits_index[i]]);
	}
}

/* this is good for internal unpack*/
long long unpack(ui8 *digits_index)
{
	long long unpacked = 0;
	int pow = 0;
	int i;
	for(i = 4; i >= 0; i--){
		if(digits_index[i] == 255) break;
		unpacked += (digits_index[i] * power(BASE,pow));
		pow++;
	}

	return unpacked;
}



struct tok_handler{
	char *original_tok;
	char *last;
	char delim[300];
	char result[1024];
	ui8 finish;
};

static struct tok_handler t_hndl;

void clear_tok()
{
	t_hndl.finish = 0;
	free(t_hndl.original_tok);
	t_hndl.original_tok = 0x0;
}
char *tok(char *str, char *delim)
{

	if(t_hndl.finish){
		if(!str){
			free(t_hndl.original_tok);
			t_hndl.original_tok = 0x0;
			t_hndl.finish = 0;
			return 0x0;
		}


		size_t string_size = strlen(str);	
		if(string_size != strlen(t_hndl.original_tok)){
			free(t_hndl.original_tok);
			t_hndl.original_tok = 0x0;
			t_hndl.finish = 0;
			goto tok_process;
		}else{
			replace('\n',*delim,t_hndl.original_tok);	
			if(strncmp(str,t_hndl.original_tok,string_size) == 0){
				free(t_hndl.original_tok);
				t_hndl.original_tok = 0x0;
				t_hndl.finish = 0;
				return 0x0;
			} else {
				free(t_hndl.original_tok);
				t_hndl.original_tok = 0x0;
				t_hndl.finish = 0;
				goto tok_process;
			}
		}
		t_hndl.finish = 0;
		if(!t_hndl.original_tok)
			goto tok_process;

		return 0x0;
	}

tok_process:
	if(!t_hndl.original_tok && !str) return 0x0;

	size_t len = 0;
	if(str) len = strlen(str);	

	if(!t_hndl.original_tok){
		memset(&t_hndl,0,sizeof(struct tok_handler));
		t_hndl.original_tok = (char*)malloc(len + 1);		

		if(!t_hndl.original_tok){
			fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
			return 0x0;
		}

		memset(t_hndl.original_tok,0,len+1);
		strncpy(t_hndl.original_tok,str,len);
		strncpy(t_hndl.delim,delim,strlen(delim));
		t_hndl.last = t_hndl.original_tok;
	}

	if(strncmp(t_hndl.delim,delim,strlen(t_hndl.delim)) != 0) {
		free(t_hndl.original_tok);
		t_hndl.original_tok = 0x0;
		return 0x0;
	}

	char *found = 0x0;
	if((found = strstr(t_hndl.original_tok,t_hndl.delim))){
		if(*(found + 1) == '\0'){
			memset(t_hndl.result,0,1024);
			strncpy(t_hndl.result,t_hndl.last,strlen(t_hndl.last)-1);				
			t_hndl.finish =  1;
			return &t_hndl.result[0];
		}
		ui32 end = found - t_hndl.original_tok;
		ui32 start = t_hndl.last - t_hndl.original_tok;
		memset(t_hndl.result,0,1024);
		strncpy(t_hndl.result,t_hndl.last,end - start);				
		*found = '\n';
		t_hndl.last += (end - start) + 1;
	} else {
		memset(t_hndl.result,0,1024);
		strncpy(t_hndl.result,t_hndl.last,strlen(t_hndl.last));				
		t_hndl.finish =  1;
		return &t_hndl.result[0];
	}

	return &t_hndl.result[0];
}

char *duplicate_str(char *str)
{
	if(!str)
		return NULL;

	size_t l = strlen(str) +1 ;
	char *dup = (char*)malloc(l);
	if(!dup){
		fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
		return 0x0;
	}

	memset(dup,0,l);
	strncpy(dup,str,l-1);
	return dup;
}
