#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include "record.h"
#include "string_utilities.h"
#include "debug.h"
#include "str_op.h"
#include "file.h"
#include "parse.h"
#include "lock.h"
#include "common.h"
#include "input.h"
#include "db_types.h"
#include "date.h"

  
 
static char prog[] = "db";
#define ERR_MSG_PAR prog,__FILE__,__LINE__

static void display_data(struct Record_f rec,int max,int tab);
static void clean_input(char *value);

static char str_type[512];
char *type_to_str(int type)
{
	memset(str_type,0,512);

	switch(type){
	case TYPE_INT:
		memcpy(str_type,"TYPE_INT",strlen("TYPE_INT"));
		return &str_type[0];
	case TYPE_LONG:
		memcpy(str_type,"TYPE_LONG",strlen("TYPE_LONG"));
		return &str_type[0];
	case TYPE_FLOAT:
		memcpy(str_type,"TYPE_FLOAT",strlen("TYPE_FLOAT"));
		return &str_type[0];
	case TYPE_STRING:
		memcpy(str_type,"TYPE_STRING",strlen("TYPE_STRING"));
		return &str_type[0];
	case TYPE_BYTE:
		memcpy(str_type,"TYPE_BYTE",strlen("TYPE_BYTE"));
		return &str_type[0];
	case TYPE_PACK:
		memcpy(str_type,"TYPE_PACK",strlen("TYPE_PACK"));
		return &str_type[0];
	case TYPE_DOUBLE:
		memcpy(str_type,"TYPE_DOUBLE",strlen("TYPE_DOUBLE"));
		return &str_type[0];
	case TYPE_ARRAY_INT:
		memcpy(str_type,"TYPE_ARRAY_INT",strlen("TYPE_ARRAY_INT"));
		return &str_type[0];
	case TYPE_SET_INT:
		memcpy(str_type,"TYPE_SET_INT",strlen("TYPE_SET_INT"));
		return &str_type[0];
	case TYPE_ARRAY_LONG:
		memcpy(str_type,"TYPE_ARRAY_LONG",strlen("TYPE_ARRAY_LONG"));
		return &str_type[0];
	case TYPE_SET_LONG:
		memcpy(str_type,"TYPE_SET_LONG",strlen("TYPE_SET_LONG"));
		return &str_type[0];
	case TYPE_ARRAY_FLOAT:
		memcpy(str_type,"TYPE_ARRAY_FLOAT",strlen("TYPE_ARRAY_FLOAT"));
		return &str_type[0];
	case TYPE_SET_FLOAT:
		memcpy(str_type,"TYPE_SET_FLOAT",strlen("TYPE_SET_FLOAT"));
		return &str_type[0];
	case TYPE_ARRAY_STRING:
		memcpy(str_type,"TYPE_ARRAY_STRING",strlen("TYPE_ARRAY_STRING"));
		return &str_type[0];
	case TYPE_SET_STRING:
		memcpy(str_type,"TYPE_SET_STRING",strlen("TYPE_SET_STRING"));
		return &str_type[0];
	case TYPE_ARRAY_BYTE:
		memcpy(str_type,"TYPE_ARRAY_BYTE",strlen("TYPE_ARRAY_BYTE"));
		return &str_type[0];
	case TYPE_SET_BYTE:
		memcpy(str_type,"TYPE_SET_BYTE",strlen("TYPE_SET_BYTE"));
		return &str_type[0];
	case TYPE_ARRAY_DOUBLE:
		memcpy(str_type,"TYPE_ARRAY_DOUBLE",strlen("TYPE_ARRAY_DOUBLE"));
		return &str_type[0];
	case TYPE_SET_DOUBLE:
		memcpy(str_type,"TYPE_SET_DOUBLE",strlen("TYPE_SET_DOUBLE"));
		return &str_type[0];
	case TYPE_FILE:
		memcpy(str_type,"TYPE_FILE",strlen("TYPE_FILE"));
		return &str_type[0];
	case TYPE_DATE:
		memcpy(str_type,"TYPE_DATE",strlen("TYPE_DATE"));
		return &str_type[0];
	case TYPE_KEY:
		memcpy(str_type,"TYPE_KEY",strlen("TYPE_KEY"));
		return &str_type[0];
	default:
		memcpy(str_type,"unrecognized type",strlen("unrecognized type"));
		return &str_type[0];
	}
	return &str_type[0];
}

int write_field_to_record(char *field_name,struct Record_f *rec,void *data, int type)
{
	int field_index = 0;
	int rec_index = 0;	
	if(!get_index_rec_field(field_name,rec,&field_index,&rec_index)) return -1;

	if(type != rec->fields[field_index].type && rec->fields[field_index].type != TYPE_STRING) return -1;

	rec->field_set[field_index] = 1;
	switch(rec->fields[field_index].type){
	case TYPE_INT:
		rec->fields[field_index].data.i = *(int *)data;
		break;
	case TYPE_KEY:
		rec->fields[field_index].data.k = *(ui32 *)data;
		break;
	case TYPE_LONG:
		rec->fields[field_index].data.l = *(long *)data;
		break;
	case TYPE_BYTE:
		rec->fields[field_index].data.b = *(unsigned char *)data;
		break;
	case TYPE_DOUBLE:
		rec->fields[field_index].data.d = *(double *)data;
		break;
	case TYPE_FLOAT:
		rec->fields[field_index].data.f = *(float *)data;
		break;
	case TYPE_DATE:
		break;
	case TYPE_STRING:
	{
		if(is_number_type(type)){
			switch(type){
			case TYPE_INT:
			{
				int l = *(int*)data;
				size_t size = 20;				
				char number[size];
				memset(number,0,size);
				long_to_string(l,number);
				rec->fields[field_index].data.s = (char *)malloc(size);
				if(!rec->fields[field_index].data.s) 
					return -1;
				memset(rec->fields[field_index].data.s,0,size);
				strncpy(rec->fields[field_index].data.s,number,size-1);
				return 0;
			}
			case TYPE_LONG:
			case TYPE_BYTE:
			{
				long l = *((long*)data);
				size_t size = 20;
				char number[size];
				memset(number,0,size);
				long_to_string(l,number);
				rec->fields[field_index].data.s = (char *)malloc(size);
				if(!rec->fields[field_index].data.s) 
					return -1;

				memset(rec->fields[field_index].data.s,0,size);
				strncpy(rec->fields[field_index].data.s,number,size-1);
				return 0;
			}
			case TYPE_DATE:
			case TYPE_KEY:
			{
				ui32 l = 0;
				if(type == TYPE_DATE){
					l = *(ui32*)data;
					if(((ui8)((l >> 16) & 0xff) == 0x2d) || ((ui8)((l >> 8) & 0xff) == 0x2d)){
						if(!is_integer(data)){
							l = convert_date_to_number(-1,(char*)data);	
						}
					}else{
						l = *((ui32*)data);
					}
				}else{
					l = *((ui32*)data);
				}
				size_t size = 20;
				char number[size];
				memset(number,0,size);
				long_to_string(l,number);
				rec->fields[field_index].data.s = (char *)malloc(size);
				if(!rec->fields[field_index].data.s){
					fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
					return -1;
				}
				memset(rec->fields[field_index].data.s,0,size);
				strncpy(rec->fields[field_index].data.s,number,size-1);
				return 0;
			}
			case TYPE_FLOAT:
			case TYPE_DOUBLE:
			{
				double d = *(double *)data;
				size_t size = 25;
				char number[size];
				memset(number,0,size);
				double_to_string(d,number);

				size_t number_size = strlen(number);
				rec->fields[field_index].data.s = (char *)malloc(number_size+1);
				if(!rec->fields[field_index].data.s) 
					return -1;
				memset(rec->fields[field_index].data.s,0,number_size+1);
				strncpy(rec->fields[field_index].data.s,number,number_size);
				return 0;
			}
			default:
			break;
			}
		}

		size_t data_size = strlen((char *)data);
		rec->fields[field_index].data.s = (char *) malloc(data_size+1);
		if(!rec->fields[field_index].data.s)
			return -1;
		
		memset(rec->fields[field_index].data.s,0,data_size+1);
		strncpy(rec->fields[field_index].data.s,data,data_size);
		break;
	}
	/*TODO add arrays and file type*/
	default:
	return -1;
	}

	return 0;
}

int copy_fields(struct Field *src,struct Field *dest)
{
	if(src->type != dest->type) return -1;
	switch(src->type){
		case TYPE_INT:
			dest->data.i = src->data.i;
			return 0;
		case TYPE_LONG:
			dest->data.l = src->data.l;
			return 0;
		case TYPE_BYTE:
			dest->data.b = src->data.b;
			return 0;
		case TYPE_FLOAT:
			dest->data.f = src->data.f;
			return 0;
		case TYPE_DOUBLE:
			dest->data.d = src->data.d;
			return 0;
		case TYPE_DATE:
			dest->data.date = src->data.date;
			return 0;
		case TYPE_STRING:
			if(!dest->data.s){
				dest->data.s = duplicate_str(src->data.s);
				if(!dest->data.s){
					fprintf(stderr,"(%s): duplicate_str() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				return 0;
			}
			free(dest->data.s);
			dest->data.s = duplicate_str(src->data.s);
			if(!dest->data.s){
				fprintf(stderr,"(%s): duplicate_str() failed, %s:%d.\n",ERR_MSG_PAR-2);
				return -1;
			}
			return 0;
		default:
			fprintf(stderr, "type not supported.\n");
			return -1;
	}
}
int create_record(char *file_name, struct Schema sch, struct Record_f *rec)
{
	strncpy(rec->file_name,file_name,strlen(file_name));
	rec->fields_num = sch.fields_num;
	rec->count = 1;
	rec->offset = 0;
	rec->fields = (struct Field*)malloc((sizeof(struct Field)*sch.fields_num));
	rec->field_set = (ui8*)malloc(sizeof(ui8)*sch.fields_num);
	if(!rec->fields || !rec->field_set){
		if(rec->field_set)
			free(rec->field_set);
		if(rec->fields) 
			free(rec->fields);
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}
	memset(rec->fields,0,sizeof(struct Field) * sch.fields_num);
	memset(rec->field_set,0,sch.fields_num);

	ui16 i;
	for(i = 0; i < sch.fields_num;i++){
		strncpy(rec->fields[i].field_name,sch.fields_name[i],strlen(sch.fields_name[i]));
		memset(&rec->fields[i].field_name[strlen(rec->fields[i].field_name)],0,MAX_FILED_LT-strlen(rec->fields[i].field_name));
		rec->fields[i].type = sch.types[i];
		rec->fields[i].is_dropped = sch.is_dropped[i];
	}

	return 0;
}

int copy_schema(struct Schema *src,struct Schema *dest)
{
	dest->fields_num = src->fields_num;
	dest->fields_name = (char**)malloc(sizeof(char*) * src->fields_num);
	if(!dest->fields_name)
		return -1;
	dest->types = (int*)malloc(sizeof(int)*src->fields_num);
	if(!dest->types){
		free(dest->fields_name);
		return -1;
	}
	dest->is_dropped = (ui8*)malloc(sizeof(ui8)*src->fields_num);
	if(!dest->is_dropped){
		free(dest->fields_name);
		free(dest->types);
		return -1;
	}

	dest->constraints = (ui8*)malloc(sizeof(ui8)*src->fields_num);
	if(!dest->constraints){
		free(dest->fields_name);
		free(dest->types);
		free(dest->is_dropped);
		return -1;
	}

	dest->defaults = (void**)malloc(sizeof(void*)*src->fields_num);
	if(!dest->defaults){
		free(dest->fields_name);
		free(dest->types);
		free(dest->is_dropped);
		free(dest->constraints);
		return -1;
	}
	
	memcpy(dest->types,src->types,sizeof(int)*src->fields_num);
	memcpy(dest->is_dropped,src->is_dropped,sizeof(ui8)*src->fields_num);
	memcpy(dest->constraints,src->constraints,sizeof(ui8)*src->fields_num);
	dest->has_unique = src->has_unique;
	memset(dest->defaults,0,sizeof(void*)*src->fields_num);

	int i;
	for(i = 0; i < dest->fields_num; i++){
		size_t sz = strlen(src->fields_name[i]);
		dest->fields_name[i] = (char*)malloc(sz+1);
		dest->fields_name[i][sz] = '\0';
		strncpy(dest->fields_name[i],src->fields_name[i],sz);
		if(src->constraints[i] == CONST_DEFAULT 
				|| ((src->constraints[i] & CONST_DEFAULT) == CONST_DEFAULT)){

			switch(src->types[i]){
			case TYPE_INT:
			case TYPE_LONG:
			{

				if (src->types[i] == TYPE_INT) {
					dest->defaults[i] = (void*)malloc(sizeof(int));
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}

					*(int*)dest->defaults[i] = *(int*)src->defaults[i];
				}else{
					dest->defaults[i] = (void*)malloc(sizeof(long));
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}

					*(long*) dest->defaults[i] = *(long*)src->defaults[i];
				}
				break;
			}
			case TYPE_BYTE:
				{
					dest->defaults[i] = (void*)malloc(sizeof(unsigned char));
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}
					*(unsigned char*)dest->defaults[i] = *(unsigned char*)src->defaults[i];
					break;
				}
				case TYPE_DOUBLE:
				case TYPE_FLOAT:
				{
					if(src->types[i] == TYPE_FLOAT){
						dest->defaults[i] = (void*)malloc(sizeof(float));
						if(!dest->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(dest);
							return -1;
						}
						*(float*)dest->defaults[i] = *(float*)src->defaults[i];

					}else{
						dest->defaults[i] = (void*)malloc(sizeof(double));
						if(!dest->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(dest);
							return -1;
						}
						*(double*)dest->defaults[i] = *(double*)src->defaults[i];
					}
					break;
				}
				case TYPE_DATE:
				{
					dest->defaults[i] = (void*)malloc(sizeof(ui32));
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}

					*(ui32*)dest->defaults[i] = *(ui32*) src->defaults[i]; 
					break;
				}
				case TYPE_KEY:
				{
					/*
					 * this is intended to be another file primary key
					 * so it must be an integer (uint32_t) aka ui32 */


					dest->defaults[i] = (void*)malloc(sizeof(ui32));
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}

					*(ui32*)dest->defaults[i] = *(ui32*)src->defaults[i];
					break;
				}
				case TYPE_STRING:
				{
					dest->defaults[i] = (void*)duplicate_str((char*)src->defaults[i]);
					if(!dest->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(dest);
						return -1;
					}
					break;
				}
				case TYPE_ARRAY_INT:
				case TYPE_SET_INT:
					break;
				case TYPE_ARRAY_LONG:
				case TYPE_SET_LONG:
					break;
				case TYPE_ARRAY_BYTE:
				case TYPE_SET_BYTE:
					break;
				case TYPE_ARRAY_FLOAT:
				case TYPE_SET_FLOAT:
					break;
				case TYPE_ARRAY_DOUBLE:
				case TYPE_SET_DOUBLE:
					break;
				default:
					break;
				}
			}

			if(src->constraints[i] & CONST_UNIQUE){
				dest->defaults[i] = (void*)malloc(sizeof(int));
				if(!dest->defaults[i]){
					fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
					free_schema(dest);
					return -1;
				}

				memset(dest->defaults[i],0,sizeof(int));

				/*this is the index nr that the DB 
				 * will use to enforce the UNIQUE constraints */
				int v = *(int*)src->defaults[i];
				*(int*)dest->defaults[i] = v;
			}
	}
	return 0;
}

int set_schema(char names[][MAX_FIELD_LT], int *types_i, struct Schema *sch, int fields_c, int *constraints, char **def_value)
{
	sch->has_unique = has_constrain_unique;
	sch->types = (int*)malloc(sizeof(int)*fields_c);
	if(!sch->types){
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}

	sch->fields_name = (char**)malloc((sizeof(char*)*MAX_FIELD_LT)*fields_c);

	if(!sch->fields_name){
		free(sch->types);
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}
		
	sch->is_dropped = (ui8*) malloc(fields_c);
	if(!sch->is_dropped){
		free(sch->types);
		free(sch->fields_name);
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}

	sch->constraints = (ui8*) malloc(fields_c);
	if(!sch->constraints){
		free(sch->types);
		free(sch->fields_name);
		free(sch->is_dropped);
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}

	sch->defaults = (void**) malloc(sizeof(void*)*fields_c);
	if(!sch->defaults){
		free(sch->types);
		free(sch->fields_name);
		free(sch->is_dropped);
		free(sch->constraints);
		fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
		return -1;
	}

	memset(sch->fields_name,0,sizeof(char*)*MAX_FIELD_LT*fields_c);
	memset(sch->types,-1,sizeof(int)*fields_c);
	memset(sch->is_dropped,0,fields_c);
	memset(sch->constraints,0,fields_c);
	memset(sch->defaults,0,sizeof(void*)*fields_c);

	sch->fields_num = (ui16)fields_c;
	int i;
	for(i = 0; i < fields_c; i++){
		sch->fields_name[i] = (char*)malloc(strlen(names[i])+1);
		if(!sch->fields_name[i]){
			free_schema(sch);
			return -1;
		}

		memset(sch->fields_name[i],0,sizeof(char)*strlen(names[i])+1);

		strncpy(sch->fields_name[i],names[i],strlen(names[i]));
		sch->types[i] = types_i[i];

		if(constraints){
			sch->constraints[i] = constraints[i];
			if(constraints[i] == CONST_DEFAULT 
					|| ((constraints[i] & CONST_DEFAULT) == CONST_DEFAULT)){

				if(types_i[i] == -1){
					/*you have to infere the type from the default value*/
					sch->types[i] = assign_type(def_value[i]);
					types_i[i] = sch->types[i];
				}
				switch(types_i[i]){
				case TYPE_INT:
				case TYPE_LONG:
				{
					if (!is_integer(def_value[i])) {
						if(types_i[i] == TYPE_INT)
							fprintf(stderr,"(%s): invalid value for integer type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						else
							fprintf(stderr,"(%s): invalid value for long integer type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						free_schema(sch);
						return -1;
					}

					int range = 0;
					if ((range = is_number_in_limits(def_value[i])) == 0) {
						if(types_i[i] == TYPE_INT)
							printf("integer value not allowed in this system.\n");
						else
							printf("long integer value not allowed in this system.\n");
						free_schema(sch);
						return -1;
					}

					if (types_i[i] == TYPE_INT && range == IN_INT) {
						/*convert the value to long and then cast it to int */
						long n = string_to_long(def_value[i]);
						if (error_value == INVALID_VALUE) {
							printf("conversion ERROR type int %s:%d.\n", F, L - 2);
							free_schema(sch);
							return -1;
						}
						sch->defaults[i] = (void*)malloc(sizeof(int));
						if(!sch->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(sch);
							return -1;
						}


						int num = (int)n;
						*(int*)sch->defaults[i] = num;
					}else if(range == IN_INT || range == IN_LONG){
						long n = string_to_long(def_value[i]);
						if (error_value == INVALID_VALUE) {
							printf("conversion ERROR type long %s:%d.\n", F, L - 2);
							free_schema(sch);
							return -1;
						}
						sch->defaults[i] = (void*)malloc(sizeof(long));
						if(!sch->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(sch);
							return -1;
						}


						*(long*) sch->defaults[i] = n;
					}else{
						if(types_i[i] == TYPE_INT)
							fprintf(stderr,"(%s): invalid value for integer type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						else
							fprintf(stderr,"(%s): invalid value for long integer type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						free_schema(sch);
						return -1;
					}
					break;
				}
				case TYPE_BYTE:
				{
					sch->defaults[i] = (void*)malloc(sizeof(unsigned char));
					if(!sch->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(sch);
						return -1;
					}


					if(strlen(def_value[i]) == 1){
						char c = *def_value[i];
						if(isalpha(c)){	
							char p = tolower(c);
							if(p == 'n'){
								*(unsigned char*)sch->defaults[i] = 0;
								break;
							}
							if(p == 'y'){
								*(unsigned char*)sch->defaults[i] = 1;
								break;
							}
						}
					}else{
						char *p = &def_value[i][0];
						for(; *p != '\0'; p++){
							*p = tolower(*p);
						}

						if(strncmp(def_value[i],"false",strlen(p)) == 0){
							*(unsigned char*)sch->defaults[i] = 0;
							break;
						}

						if(strncmp(def_value[i],"true",strlen(p)) == 0){
							*(unsigned char*)sch->defaults[i] = 1;
							break;
						}
					}

					if (!is_integer(def_value[i])){
						fprintf(stderr,"(%s): invalid value for byte type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						return 0;
					}

					long l = string_to_long(def_value[i]);
					if (error_value == INVALID_VALUE){
						printf("conversion ERROR type float %s:%d.\n", F, L - 2);
						return 0;
					}

					if (l > UCHAR_MAX || l < 0) {
						printf("byte value not allowed in this system.\n");
						return 0;
					}
					*(unsigned char*)sch->defaults[i] = (unsigned char)l;
					break;
				}
				case TYPE_DOUBLE:
				case TYPE_FLOAT:
				{
					if (!is_floaintg_point(def_value[i])) {
						if(is_integer(def_value[i])){
							char *decimal = ".00";
							size_t vs = strlen(def_value[i]);
							size_t ds = strlen(decimal);
							size_t s = vs + ds + 1; 
							char cpy[s];
							memset(cpy,0,s);
							strncpy(cpy,def_value[i],vs);
							strncat(cpy,decimal,ds);

							int range = 0;
							if ((range = is_number_in_limits(cpy)) == 0){
								if(types_i[i] == TYPE_FLOAT)
									printf("float value not allowed in this system.\n");
								else
									printf("double value not allowed in this system.\n");

								free_schema(sch);
								return -1;
							}

							if (types_i[i] == TYPE_FLOAT && range == IN_FLOAT){
								float f = (float)string_to_double(cpy);
								if (error_value == INVALID_VALUE){
									printf("conversion ERROR type float %s:%d.\n", F, L - 2);
									free_schema(sch);
									return -1;
								}
								sch->defaults[i] = (void*)malloc(sizeof(float));
								if(!sch->defaults[i]){
									fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
									free_schema(sch);
									return -1;
								}
								*(float*)sch->defaults[i] = f;
								break;
							}else if (types_i[i] == TYPE_DOUBLE && (range == TYPE_DOUBLE || range == TYPE_FLOAT)){
								double f = (float)string_to_double(cpy);
								if (error_value == INVALID_VALUE){
									printf("conversion ERROR type float %s:%d.\n", F, L - 2);
									free_schema(sch);
									return -1;
								}

								sch->defaults[i] = (void*)malloc(sizeof(double));
								if(!sch->defaults[i]){
									fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
									free_schema(sch);
									return -1;
								}
								*(double*)sch->defaults[i] = f;
								break;

							}else{
								if(types_i[i] == TYPE_FLOAT)
									fprintf(stderr,"float value '%s' is out of range.\n",cpy);
								else
									fprintf(stderr,"double value '%s' is out of range.\n",cpy);
								free_schema(sch);
								return -1;
							}
						}
						fprintf(stderr,"(%s): invalid value for float type: %s. Field: '%s'\n",prog, def_value[i],sch->fields_name[i]);
						free_schema(sch);
						return -1;
					}

					int range = 0;
					if ((range = is_number_in_limits(def_value[i])) == 0){
						if (types_i[i] == TYPE_FLOAT)
							printf("float value not allowed in this system.\n");
						else
							printf("double value not allowed in this system.\n");
						free_schema(sch);
						return -1;
					}

					if (types_i[i] == TYPE_FLOAT && range == IN_FLOAT){
						float f = (float)string_to_double(def_value[i]);
						if (error_value == INVALID_VALUE){
							printf("conversion ERROR type float %s:%d.\n", F, L - 2);
							free_schema(sch);
							return -1;
						}

						sch->defaults[i] = (void*)malloc(sizeof(float));
						if(!sch->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(sch);
							return -1;
						}
						*(float*)sch->defaults[i] = f;
					}else if (types_i[i] == TYPE_DOUBLE && (range == TYPE_DOUBLE || range == TYPE_FLOAT)){
						double f = string_to_double(def_value[i]);
						if (error_value == INVALID_VALUE){
							printf("conversion ERROR type float %s:%d.\n", F, L - 2);
							free_schema(sch);
							return -1;
						}

						sch->defaults[i] = (void*)malloc(sizeof(double));
						if(!sch->defaults[i]){
							fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
							free_schema(sch);
							return -1;
						}

						*(double*)sch->defaults[i] = f;
					}else{
						if(types_i[i] == TYPE_FLOAT)
							fprintf(stderr,"float value '%s' is out of range.\n",def_value[i]);
						else
							fprintf(stderr,"double value '%s' is out of range.\n",def_value[i]);
						free_schema(sch);
						return -1;
					}
					break;
				}
				case TYPE_DATE:
				{
					sch->defaults[i] = (void*)malloc(sizeof(ui32));
					if(!sch->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(sch);
						return -1;
					}

					if((*(ui32*)sch->defaults[i] = convert_date_to_number(-1,def_value[i])) == 0){
						fprintf(stderr,"cannot convert date\n");
						free_schema(sch);
						return -1;
					}
					break;
				}
				case TYPE_KEY:
				{
					/*
					 * this is intended to be another file primary key
					 * so it must be an integer (uint32_t) aka ui32 */

					if(!is_integer(def_value[i])){
						printf("invalid value for key type: %s.\n", def_value[i]);
						free_schema(sch);
						return -1;
					}

					long l = string_to_long(def_value[i]);
					if(error_value == INVALID_VALUE){
						printf("conversion ERROR type KEY value:%s, %s:%d.\n",def_value[i],__FILE__, __LINE__ - 2);
						free_schema(sch);
						return -1;
					}

					if(l > MAX_KEY){
						printf("KEY exceed the limit value %s:%d.\n",__FILE__, __LINE__ - 2);
						free_schema(sch);
						return -1;
					}

					sch->defaults[i] = (void*)malloc(sizeof(ui32));
					if(!sch->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(sch);
						return -1;
					}

					*(ui32*)sch->defaults[i] = (ui32)l;
					break;
				}
				case TYPE_STRING:
				{
					sch->defaults[i] = (void*)duplicate_str(def_value[i]);
					if(!sch->defaults[i]){
						fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
						free_schema(sch);
						return -1;
					}
					break;
				}
				case TYPE_ARRAY_INT:
				case TYPE_SET_INT:
					break;
				case TYPE_ARRAY_LONG:
				case TYPE_SET_LONG:
					break;
				case TYPE_ARRAY_BYTE:
				case TYPE_SET_BYTE:
					break;
				case TYPE_ARRAY_FLOAT:
				case TYPE_SET_FLOAT:
					break;
				case TYPE_ARRAY_DOUBLE:
				case TYPE_SET_DOUBLE:
					break;
				default:
					break;
				}
			}

			if(constraints[i] & CONST_UNIQUE){
				sch->defaults[i] = (void*)malloc(sizeof(int));
				if(!sch->defaults[i]){
					fprintf(stderr,"malloc() failed, %s:%d\n",__FILE__,__LINE__-2);
					free_schema(sch);
					return -1;
				}

				/*this is the index nr that the DB 
				 * will use to enforce the UNIQUE constraints */
				*(int*)sch->defaults[i] = 1;
			}
		}
	}

	return 0;
}

int free_schema(struct Schema *sch)
{
	ui16 i;
	for(i = 0; i < sch->fields_num;i++){
		free(sch->fields_name[i]);
		if(sch->defaults[i]){
			free(sch->defaults[i]);
		}
	}

	free(sch->types);
	free(sch->is_dropped);
	free(sch->constraints);
	free(sch->fields_name);
	free(sch->defaults);
	return 0;
}

unsigned char set_field(
				struct Record_f *rec, 
				int index, 
				char *field_name, 
				enum ValueType type, 
				char *value,
				ui8 field_bit)
{
	strncpy(rec->fields[index].field_name,field_name,strlen(field_name));
	rec->fields[index].type = type;
	rec->field_set[index] = field_bit;

	/*this prevent a fake failure when the TYPE_FILE is not active in the current record*/
	if(!value && (field_bit == 0)) return 1;

	int t = (int)type;
	switch (t) {
		case -1:
			break;
		case TYPE_DATE:
			if((rec->fields[index].data.date = convert_date_to_number(-1,value)) == 0){
				fprintf(stderr,"cannot convert date\n");
				return 0;
			}
			break;
		case TYPE_KEY:
			if(!is_integer(value)){
				printf("invalid value for key type: %s.\n", value);
				return 0;
			}

			long l = string_to_long(value);
			if(error_value == INVALID_VALUE){
				printf("conversion ERROR type KEY value:%s, %s:%d.\n",value,__FILE__, __LINE__ - 2);
				return 0;
			}

			if(l > MAX_KEY){
				printf("KEY exceed the limit value %s:%d.\n",__FILE__, __LINE__ - 2);
				return 0;
			}

			rec->fields[index].data.k = (ui32)l;
			break;
		case TYPE_FILE:
			{	
				/*validate the input*/

				if(!strstr(value,"|")){
					fprintf(stderr,"(%s): syntax error for type file.\nUsage field_name:[w|fields in the file]\n",prog);
					return 0;
				}

				/*
				 * understand how many records you have to create
				 * */
				char *close_c = NULL;
				size_t cp_l = strlen(value) + 1;
				char cpy[cp_l];
				memset(cpy,0,cp_l);
				strncpy(cpy,value,cp_l);

				int count = 0;
				while((close_c = strstr(cpy,"]"))) {
					*close_c = '@';
					count++;	
				}

				/*if count is 0 it means the value of the 
				 * file field has already been stripped of []*/
				if(count == 0)
					count++;

				char values[count][500];
				memset(values,0,count*500);
				if(count > 1){
					int i = 0;
					int stop = 0;
					while((close_c = strstr(value,"]"))){
						close_c++;
						if(*close_c == ',' || *close_c == '\0') {
							if(stop != 0){
								int start = stop;
								stop = close_c - value;

								if(stop > 500){
									/* TODO: handle this hedge case*/

								}
								if(i < count)
									strncpy(values[i],&value[start+1],stop-start-1);

								i++;
								*(--close_c) = '@';
								continue;

							}
							stop = close_c - value;
							if(stop > 500){
								/* TODO handle this hedge case record.c l 166*/

							}
							if(i < count)
								strncpy(values[i],value,stop-1);

							i++;
							*(--close_c) = '@';
							continue;
						} 
						/* should be unreachable BUT is a safety feature
						 * if we reach here we sub ']' with '@' 
						 * to avoid an infinite loop
						 * */
						*(--close_c) = '@';

					}
				}
				/*
				 * if schema file exist => read schema file-> check input match with schema
				 * 	else :
				 * 	create the schema file -> create_file_with_no_value (src/parse)
				 * */
				int i;
				for(i = 0; i < count; i++){
					if(count > 1 )
						clean_input(values[i]);
					else
						clean_input(value);		

				}


				struct Schema sch;
				memset(&sch,0,sizeof(struct Schema));

				/* 
				 * we need to guarantee that the file field gets created
				 * in the same directory as the main file.
				 * */


				/* this make sure we create the right path for the file*/
				char *p = rec->file_name;
				char *f = NULL;
				i = 0;
				char *last = NULL;
				for(; (f = strstr(p,"/")) != NULL; i++, p[f - p] = '@', last = f);

				replace('@','/',rec->file_name);
				int partial_path = 0;
				if (i > 0) partial_path = (last - p) +1;

				char *sfx = ".sch"; 
				size_t sfxl = strlen(sfx);
				size_t fl = strlen(rec->fields[index].field_name); 
				size_t l = 0; 
				if(partial_path > 0)
					l = fl + sfxl + partial_path + 1;
				else 
					l = fl + sfxl + 1;

				char file_name[l];
				memset(file_name,0,l);
				if(partial_path > 0){
					strncpy(file_name,rec->file_name,partial_path);
					strncat(file_name,rec->fields[index].field_name,fl);
					strncat(file_name,sfx,sfxl);
				}else{
					strncpy(file_name,rec->fields[index].field_name,l);
					strncat(file_name,sfx,sfxl);
				}

				file_t fd_schema;
				int cr = 0;
				if(open_file(file_name,0,&fd_schema) == -1){
					file_error_handler(1,fd_schema);
					if(create_file(file_name,&fd_schema) == -1){
						fprintf(stderr,"(%s): cannot create file %s:%d.\n",ERR_MSG_PAR-1);
						return 0; 
					}
					cr = 1;
				}

				if(cr){
					/*file is new*/	
					int mode = 0;
					if(count == 1){
						if(value[0] == 'w')
							mode = check_handle_input_mode(&value[2], FCRT) | WR;
						/*you can implement other modes*/
					}else{
						int i;
						for(i = 0; i < count;i++){
							if(values[i][0] == 'w')
								mode = check_handle_input_mode(&values[0][2], FCRT) | WR;
						}
					}

					switch(mode){
						case NO_TYPE_WR:
						case HYB_WR:
							int fields_count = 0;
							char names[MAX_FIELD_NR][MAX_FILED_LT];
							memset(names,0,MAX_FILED_LT*MAX_FIELD_NR*sizeof(char));
							int types_i[MAX_FIELD_NR];
							memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);
							char **values_in = NULL;

							if(count == 1){
								switch (mode){
									case NO_TYPE_WR:			
										{	
											values_in = extract_fields_value_types_from_input(&value[2],names,types_i,&fields_count);
											if(!values_in){
												fprintf(stderr,"(%s): cannot extract value from input,%s:%d.\n",prog,__FILE__,__LINE__-1);
												close_file(1,fd_schema);	
												return 0;
											}
											break;
										}
									case HYB_WR:			
										{
											if((fields_count = get_name_types_hybrid(mode,&value[2],names,types_i)) == -1){ 
												free_strs(fields_count,1,values_in);
												close_file(1,fd_schema);	
												return 0;
											}

											if(get_values_hyb(value,&values_in,fields_count) == -1){
												free_strs(fields_count,1,values_in);
												close_file(1,fd_schema);	
												return 0;
											}

											int j;
											for(j = 0; j < fields_count; j++){
												if(types_i[j] == -1) types_i[j] = assign_type(values_in[j]);		
											}

											break;
										}	
									default:
										close_file(1,fd_schema);	
										return 0;
								}
								/*TODO:*/
								set_schema(names,types_i,&sch,fields_count,NULL,NULL);	
								

								rec->fields[index].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
								rec->fields[index].data.file.count = 1;
								if(!rec->fields[index].data.file.recs){
									fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-3);
									free_strs(fields_count,1,values_in);
									close_file(1,fd_schema);	
									return 0;
								}
								memset(rec->fields[index].data.file.recs,0,sizeof(struct Record_f));
								if(parse_input_with_no_type(rec->fields[index].field_name,fields_count, names, 
											types_i, values_in,&sch,0,rec->fields[index].data.file.recs,0) == -1){
									fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
									free_strs(fields_count,1,values_in);
									close_file(1,fd_schema);	
									return 0;
								}


								free_strs(fields_count,1,values_in);
							}else{

								int i;
								for(i = 0; i < count;i++){
									switch (mode){
										case NO_TYPE_WR:			
											{	
												values_in = extract_fields_value_types_from_input(&values[i][2],names,types_i,&fields_count);
												if(!values_in){
													fprintf(stderr,"(%s): cannot extract value from input,%s:%d.\n",prog,__FILE__,__LINE__-1);
													close_file(1,fd_schema);	
													return 0;
												}
												break;
											}
										case HYB_WR:			
											{
												if((fields_count = get_name_types_hybrid(mode,&values[i][2],names,types_i)) == -1){ 
													free_strs(fields_count,1,values_in);
													close_file(1,fd_schema);	
													return 0;
												}

												if(get_values_hyb(&values[i][2],&values_in,fields_count) == -1){
													free_strs(fields_count,1,values_in);
													close_file(1,fd_schema);	
													return 0;
												}
												int j;
												for(j = 0; j < fields_count; j++){
													if(types_i[j] == -1) types_i[j] = assign_type(values_in[j]);		
												}
												break;
											}	
										default:
											close_file(1,fd_schema);	
											return 0;
									}
									/*TODO*/
									set_schema(names,types_i,&sch,fields_count,NULL,NULL);

									if(!rec->fields[index].data.file.recs){
										rec->fields[index].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
										rec->fields[index].data.file.count = 1;
										if(!rec->fields[index].data.file.recs){
											fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-3);
											free_strs(fields_count,1,values_in);
											close_file(1,fd_schema);	
											return 0;
										}

										memset(rec->fields[index].data.file.recs,0,sizeof(struct Record_f));
										if(parse_input_with_no_type(rec->fields[index].field_name,fields_count, names, 
													types_i, values_in,&sch,0,&rec->fields[index].data.file.recs[0],0) == -1){
											fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
											free_strs(fields_count,1,values_in);
											close_file(1,fd_schema);	
											return 0;
										}


									}else{
										int new_count = rec->fields[index].data.file.count + 1;
										struct Record_f *new_rec = (struct Record_f*)realloc(
												rec->fields[index].data.file.recs,
												new_count * sizeof(struct Record_f));

										if(!new_rec){
											fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
											free_strs(fields_count,1,values_in);
											close_file(1,fd_schema);	
											return 0;
										}

										rec->fields[index].data.file.count = new_count;
										rec->fields[index].data.file.recs = new_rec;


										if(parse_input_with_no_type(rec->fields[index].field_name,
													fields_count, 
													names, 
													types_i, 
													values_in,
													&sch,
													0,
													&rec->fields[index].data.file.recs[new_count-1],
													0) == -1){
											fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
											free_strs(fields_count,1,values_in);
											close_file(1,fd_schema);	
											return 0;
										}
									}
									free_strs(fields_count,1,values_in);
									values_in = NULL;
								}
							}
							break;
						case TYPE_WR:
							{
								int f_count = 0; 
								if(count == 1){		
									f_count = count_fields(value,NULL);

									if (f_count == 0) {
										fprintf(stderr,"(%s): type syntax might be wrong.\n",prog);
										close_file(1,fd_schema);	
										return 0;
									}

									if (f_count > MAX_FIELD_NR) {
										fprintf(stderr,"(%s): too many fields, max %d each file definition.",prog, MAX_FIELD_NR);
										close_file(1,fd_schema);	
										return 0;
									}
									rec->fields[index].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
									rec->fields[index].data.file.count = 1;
									if(!rec->fields[index].data.file.recs){
										fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
										close_file(1,fd_schema);
										return 0;
									}

									memset(rec->fields[index].data.file.recs,0,sizeof(struct Record_f));
									if(parse_d_flag_input(rec->fields[index].field_name, 
												f_count,
												value,
												&sch, 
												0,
												&rec->fields[index].data.file.recs[0],
												NULL,0) == -1) {
										fprintf(stderr,"(%s): error creating the record, %s:%d.\n",ERR_MSG_PAR- 1);
										close_file(1,fd_schema);	
										return 0;
									}

								}else{
									int i;
									for(i = 0; i < count;i++){
										f_count = count_fields(values[i],NULL);

										if (f_count == 0) {
											fprintf(stderr,"(%s): type syntax might be wrong.\n",prog);
											close_file(1,fd_schema);	
											return 0;
										}

										if (f_count > MAX_FIELD_NR) {
											fprintf(stderr,"(%s): too many fields, max %d each file definition.",prog, MAX_FIELD_NR);
											close_file(1,fd_schema);	
											return 0;
										}

										if(!rec->fields[index].data.file.recs){
											rec->fields[index].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
											rec->fields[index].data.file.count = 1;

											if(!rec->fields[index].data.file.recs){
												fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
												close_file(1,fd_schema);	
												return 0;
											}

											memset(rec->fields[index].data.file.recs,0,sizeof(struct Record_f));
											if(parse_d_flag_input(rec->fields[index].field_name, 
														f_count,
														values[i],
														&sch, 
														0,
														&rec->fields[index].data.file.recs[0],
														NULL,0) == -1) {
												fprintf(stderr,"(%s): error creating the record, %s:%d.\n",ERR_MSG_PAR- 1);
												close_file(1,fd_schema);	
												return 0;
											}
										}else{
											int new_count = rec->fields[index].data.file.count + 1;
											struct Record_f *new_rec = (struct Record_f*) realloc(
													rec->fields[index].data.file.recs,
													new_count * sizeof(struct Record_f));

											if(!new_rec){
												fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
												close_file(1,fd_schema);	
												return 0;
											}

											rec->fields[index].data.file.count = new_count;
											rec->fields[index].data.file.recs = new_rec;

											if(parse_d_flag_input(rec->fields[index].field_name, 
														f_count,
														values[i],
														&sch, 
														0,
														&rec->fields[index].data.file.recs[new_count-1],
														NULL,0) == -1) {
												fprintf(stderr,"(%s): error creating the record, %s:%d.\n",prog, __FILE__, __LINE__ - 1);
												close_file(1,fd_schema);	
												return 0;
											}
										}
									}
								}
								break;
							}
						default:
							fprintf(stderr,"(%s): mode not supported, %s:%d.\n",ERR_MSG_PAR);
							close_file(1,fd_schema);	
							return 0;
					}

					/*write the schema to the file*/
					struct Header_d hd = {0, 0, &sch};

					hd.id_n = HEADER_ID_SYS;
					hd.version = VS;
					if (!write_header(fd_schema, &hd)) {
						__er_write_to_file(F, L - 1);
						close_file(1,fd_schema);
						return 0;
					}

					close_file(1,fd_schema);


				}else {
					/*file exist*/
					/* init the Schema structure*/
					struct Schema sch;
					memset(&sch,0,sizeof(struct Schema));
					struct Header_d hd = {0, 0, &sch};

					/* ensure the file is a db file */
					if (!read_header(fd_schema, &hd)) {
						close_file(1,fd_schema);
						return 0;
					}

					int fields_count = 0;
					unsigned char check = 0;
					int mode = 0; 
					if(count == 1){
						mode = check_handle_input_mode(value, FWRT) | WR;
					}else{
						int i;
						for(i = 0; i < count; i++){
							mode = check_handle_input_mode(values[i], FWRT) | WR;

							if(mode == TYPE_WR){
								fields_count = count_fields(&values[i][2],NULL);

								if(fields_count == 0){
									fprintf(stderr,"(%s):check input syntax.\n",prog);
									close_file(1,fd_schema);
									free_schema(hd.sch_d);
									return 0;
								}

								if (fields_count > MAX_FIELD_NR) {
									printf("Too many fields, max %d each file definition.", MAX_FIELD_NR);
									close_file(1,fd_schema);
									free_schema(hd.sch_d);
									return 0;
								}

								if(!rec->fields[index].data.file.recs){
									rec->fields[index].data.file.recs = (struct Record_f*)malloc(sizeof(struct Record_f));
									rec->fields[index].data.file.count++;
									if(!rec->fields[index].data.file.recs){
										fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-3);
										free_schema(hd.sch_d);
										return 0;
									}

									memset(rec->fields[index].data.file.recs,0,sizeof(struct Record_f));
									check = perform_checks_on_schema(mode,&values[i][2], fields_count,
											rec->fields[index].field_name,
											&rec->fields[index].data.file.recs[0],
											&hd,NULL,-1,0);

								}else{
									int new_count = rec->fields[index].data.file.count + 1;
									struct Record_f *new_rec = (struct Record_f*)realloc(
											rec->fields[index].data.file.recs,
											new_count * sizeof(struct Record_f));

									if(!new_rec){
										fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
										close_file(1,fd_schema);	
										free_schema(hd.sch_d);
										return 0;
									}

									rec->fields[index].data.file.count = new_count;
									rec->fields[index].data.file.recs = new_rec;



									check = perform_checks_on_schema(mode,&values[i][2], fields_count,
											rec->fields[index].field_name,
											&rec->fields[index].data.file.recs[new_count-1],
											&hd,NULL,-1,0);
								}
							}else{
								if(!rec->fields[index].data.file.recs){
									rec->fields[index].data.file.recs = (struct Record_f*)malloc(count *sizeof(struct Record_f));
									rec->fields[index].data.file.count++;
									if(!rec->fields[index].data.file.recs){
										fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-2);
										return 0;
									}

									memset(rec->fields[index].data.file.recs,0,count * sizeof(struct Record_f));
									check = perform_checks_on_schema(mode,&values[i][2], -1,
											rec->fields[index].field_name,
											&rec->fields[index].data.file.recs[0],
											&hd,NULL,-1,0);

								}else{
									int new_count = rec->fields[index].data.file.count + 1;
									struct Record_f *new_rec = (struct Record_f*)realloc(
											rec->fields[index].data.file.recs,
											new_count * sizeof(struct Record_f));

									if(!new_rec){
										fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
										close_file(1,fd_schema);	
										free_schema(hd.sch_d);
										return 0;
									}

									rec->fields[index].data.file.count = new_count;
									rec->fields[index].data.file.recs = new_rec;

									check = perform_checks_on_schema(mode,&values[i][2], fields_count,
											rec->fields[index].field_name,
											&rec->fields[index].data.file.recs[new_count-1],
											&hd,NULL,-1,0);
								}

							}

							if (check == SCHEMA_ERR || check == 0) {
								close_file(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}

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

								/* aquire lock 
								 * TODO: I am not using this feature of the system as for now 07-06-26
								if(acquire_lock(fds,LOCK_SCHEMA_FILE) == -1){
									fprintf(stderr,"can't acquire or release proper lock.\n");
									free_schema(hd.sch_d);
									close_file(1,fd_schema);
									return 0;
								}
								*/
								close_file(1,fd_schema);
								if(open_file(file_name,1,&fd_schema) == -1){ /*open with O_TRUNCATE*/
									file_error_handler(1,fd_schema);
									free_schema(hd.sch_d);
									return 0;
								}

								if (!write_header(fd_schema, &hd)) {
									__er_write_to_file(F, L - 1);
									free_schema(hd.sch_d);
									close_file(1,fd_schema);
									return 0;
								}

								/*release_lock(fds,LOCK_SCHEMA_FILE);*/
								free_schema(hd.sch_d);
							}
						}
					}

					/*check schema*/
					if(count == 1){
						if(mode == TYPE_WR){
							fields_count = count_fields(value,NULL);

							if(fields_count == 0){
								fprintf(stderr,"(%s):check input syntax.\n",prog);
								close_file(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}

							if (fields_count > MAX_FIELD_NR) {
								printf("Too many fields, max %d each file definition.", MAX_FIELD_NR);
								close_file(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}

							if(!rec->fields[index].data.file.recs){
								rec->fields[index].data.file.recs = (struct Record_f*) malloc(count*sizeof(struct Record_f));
								rec->fields[index].data.file.count++;
								if(!rec->fields[index].data.file.recs){
									fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
									free_schema(hd.sch_d);
									return 0;
								}

								memset(rec->fields[index].data.file.recs,0,count*sizeof(struct Record_f));
								check = perform_checks_on_schema(mode,&value[2], fields_count,
										rec->fields[index].field_name,
										&rec->fields[index].data.file.recs[0],
										&hd,NULL,-1,0);
							}
						} else {
							if(!rec->fields[index].data.file.recs){
								rec->fields[index].data.file.recs = (struct Record_f*)malloc(count*sizeof(struct Record_f));
								rec->fields[index].data.file.count++;
								if(!rec->fields[index].data.file.recs){
									fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-3);
									return 0;
								}
								memset(rec->fields[index].data.file.recs,0,count*sizeof(struct Record_f));
								check = perform_checks_on_schema(mode,&value[2], -1,rec->fields[index].field_name,
										&rec->fields[index].data.file.recs[0],
										&hd,NULL,-1,0);

							}

						}			

						if (check == SCHEMA_ERR || check == 0) {
							close_file(1,fd_schema);
							free_schema(hd.sch_d);
							return 0;
						}

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

							/*AS FORE july 26 we are not using this part of the software*/
							/* aquire lock */

							/*
							if(acquire_lock(fds,LOCK_SCHEMA_FILE) == -1){
								fprintf(stderr,"can't acquire or release proper lock.\n");
								close_file(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}
							*/

							close_file(1,fd_schema);
							if(open_file(file_name,1,&fd_schema) == -1){ /*open with O_TRUNCATE*/
								file_error_handler(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}

							if (!write_header(fd_schema, &hd)) {
								__er_write_to_file(F, L - 1);
								close_file(1,fd_schema);
								free_schema(hd.sch_d);
								return 0;
							}

							/*release_lock(fds,LOCK_SCHEMA_FILE);*/
							free_schema(hd.sch_d);
							close_file(1,fd_schema);
							break;
						} /* end of update schema branch*/
					}
					free_schema(hd.sch_d);
					close_file(1,fd_schema);
				}
				break;
			}
		case TYPE_INT:
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
			{
				if (type == TYPE_ARRAY_INT || type == TYPE_SET_INT) {
					if (!rec->fields[index].data.v.elements.i)
					{
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_INT)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t)
					{

						if (!is_integer(t)) {
							printf("invalid value for integer type: %s.\n", value);
							return 0;
						}

						int range = 0;
						if ((range = is_number_in_limits(t)) == 0) {
							printf("integer value not allowed in this system.\n");
							return 0;
						}

						if (range == IN_INT) {
							/*convert the value to long and then cast it to int */
							long n = string_to_long(t);
							if (error_value == INVALID_VALUE) {
								printf("conversion ERROR type int %s:%d.\n", F, L - 2);
								return 0;
							}

							int num = (int)n;
							rec->fields[index].data.v.insert((void*)&num,
									&(rec->fields[index].data.v),
									type);
						}else{
							printf("the integer value is not valid for this system.\n");
							return 0;
						}

						t = tok(NULL, ",");
					}
				} else {

					if (!is_integer(value)){
						fprintf(stderr,"(%s): invalid value for integer type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
						return 0;
					}

					int range = 0;
					if ((range = is_number_in_limits(value)) == 0) {
						printf("integer value not allowed in this system.\n");
						return 0;
					}

					if (range == IN_INT){
						/*convert the value to long and then cast it to int */
						/*i do not want to use atoi*/
						long n = string_to_long(value);
						if (error_value == INVALID_VALUE) {
							printf("conversion ERROR type int %s:%d.\n", F, L - 2);
							return 0;
						}

						rec->fields[index].data.i = (int)n;
					}else {
						printf("the integer value is not valid for this system.\n");
						return 0;
					}
				}
				break;
			}
		case TYPE_LONG:
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
			{

				if (type == TYPE_ARRAY_LONG || type == TYPE_SET_LONG){
					if (!rec->fields[index].data.v.elements.l) {
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_LONG)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t){
						if (!is_integer(t)){
							fprintf(stderr,"(%s): invalid value for long integer type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
							return 0;
						}

						int range = 0;
						if ((range = is_number_in_limits(t)) == 0) {
							printf("long value not allowed in this system.\n");
							return 0;
						}

						if (range == IN_INT || range == IN_LONG) {
							long n = string_to_long(t);
							if (error_value == INVALID_VALUE){
								printf("conversion ERROR type long %s:%d.\n", F, L - 2);
								return 0;
							}
							rec->fields[index].data.v.insert((void *)&n,
									&(rec->fields[index].data.v),type);
						}
						t = tok(NULL, ",");
					}
				} else {

					if (!is_integer(value)) {
						fprintf(stderr,"(%s): invalid value for long integer type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
						return 0;
					}

					int range = 0;
					if ((range = is_number_in_limits(value)) == 0) {
						printf("long value not allowed in this system.\n");
						return 0;
					}

					if (range == IN_INT || range == IN_LONG) {
						long n = string_to_long(value);
						if (error_value == INVALID_VALUE) {
							printf("conversion ERROR type long %s:%d.\n", F, L - 2);
							return 0;
						}
						rec->fields[index].data.l = n;
					}
				}
				break;
			}
		case TYPE_FLOAT:
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
			{
				if (type == TYPE_ARRAY_FLOAT || type == TYPE_SET_FLOAT)
				{
					if (!rec->fields[index].data.v.elements.f){
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_FLOAT)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t){
						if (!is_floaintg_point(t)) {
							if(is_integer(t)){
								char *decimal = ".00";
								size_t vs = strlen(t);
								size_t ds = strlen(decimal);
								size_t s = vs + ds + 1; 
								char cpy[s];
								memset(cpy,0,s);
								strncpy(cpy,t,vs);
								strncat(cpy,decimal,ds);

								int range = 0;
								if ((range = is_number_in_limits(cpy)) == 0){
									printf("float value not allowed in this system.\n");
									return 0;
								}

								if (range == IN_FLOAT){
									float f = (float)string_to_double(cpy);
									if (error_value == INVALID_VALUE){
										printf("conversion ERROR type float %s:%d.\n", F, L - 2);
										return 0;
									}
									rec->fields[index].data.v.insert((void *)&f,
											&(rec->fields[index].data.v),type);

									t = tok(NULL, ",");
									continue;
								}else{
									fprintf(stderr,"float value '%s' is out of range.\n",cpy);
									return 0;
								}
							}

							fprintf(stderr,"(%s): invalid value for float type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
							return 0;
						}

						int range = 0;
						if ((range = is_number_in_limits(t)) == 0)
						{
							printf("float value not allowed in this system.\n");
							return 0;
						}

						if (range == IN_FLOAT){
							float f = (float)string_to_double(t);
							if (error_value == INVALID_VALUE){
								printf("conversion ERROR type float %s:%d.\n", F, L - 2);
								return 0;
							}

							rec->fields[index].data.v.insert((void *)&f,
									&(rec->fields[index].data.v),type);
						}
						t = tok(NULL, ",");
					}
				} else {

					if (!is_floaintg_point(value)) {
						if(is_integer(value)){
							char *decimal = ".00";
							size_t vs = strlen(value);
							size_t ds = strlen(decimal);
							size_t s = vs + ds + 1; 
							char cpy[s];
							memset(cpy,0,s);
							strncpy(cpy,value,vs);
							strncat(cpy,decimal,ds);

							int range = 0;
							if ((range = is_number_in_limits(cpy)) == 0){
								printf("float value not allowed in this system.\n");
								return 0;
							}

							if (range == IN_FLOAT){
								float f = (float)string_to_double(cpy);
								if (error_value == INVALID_VALUE){
									printf("conversion ERROR type float %s:%d.\n", F, L - 2);
									return 0;
								}
								rec->fields[index].data.f = f;
								break;
							}else{
								fprintf(stderr,"float value '%s' is out of range.\n",cpy);
								return 0;
							}
						}
						fprintf(stderr,"(%s): invalid value for float type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
						return 0;
					}

					int range = 0;
					if ((range = is_number_in_limits(value)) == 0)
					{
						printf("float value not allowed in this system.\n");
						return 0;
					}

					if (range == IN_FLOAT)
					{
						float f = (float)string_to_double(value);
						if (error_value == INVALID_VALUE)
						{
							printf("conversion ERROR type float %s:%d.\n", F, L - 2);
							return 0;
						}

						rec->fields[index].data.f = f;
					}
				}
				break;
			}
		case TYPE_STRING:
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
			{
				if (type == TYPE_ARRAY_STRING || type == TYPE_SET_STRING)
				{
					if (!rec->fields[index].data.v.elements.s)
					{
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_STRING)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t) {
						rec->fields[index].data.v.insert((void *)t,&(rec->fields[index].data.v),type);
						t = tok(NULL, ",");
					}
				} else {
					rec->fields[index].data.s = duplicate_str(value);
					if (!rec->fields[index].data.s) {
						fprintf(stderr,"(%s): duplicate_str() failed, %s:%d.\n",ERR_MSG_PAR-2);
						return 0;
					}
				}
				break;
			}
		case TYPE_BYTE:
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
			{
				if (type == TYPE_ARRAY_BYTE || type == TYPE_SET_BYTE)
				{
					if (!rec->fields[index].data.v.elements.b)
					{
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_BYTE)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t)
					{
						if(strlen(t) == 1){
							char c = *t;
							if(isalpha(c)){	
								char p = tolower(c);
								if(p == 'n') t = "0";
								if(p == 'y') t = "1";
							}
						}

						if (!is_integer(t))
						{
							fprintf(stderr,"(%s): invalid value for byte type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
							return 0;
						}


						long l = string_to_long(t);
						if (error_value == INVALID_VALUE)
						{
							printf("conversion ERROR type float %s:%d.\n", F, L - 2);
							return 0;
						}

						if (l > UCHAR_MAX || l < 0)
						{
							printf("byte value not allowed in this system.\n");
							return 0;
						}

						unsigned char num = (unsigned char)l;
						rec->fields[index].data.v.insert((void *)&num,&(rec->fields[index].data.v),type);
						t = tok(NULL, ",");
					}
				}
				else
				{
					if(strlen(value) == 1){
						char c = *value;
						if(isalpha(c)){	
							char p = tolower(c);
							if(p == 'n') value = "0";
							if(p == 'y') value = "1";
						}
					}else{

						char *p = &value[0];
						int sz = (int)strlen(value);
						for(; *p != '\0'; p++){
							*p = tolower(*p);
						}

						if(strncmp(value,"false",sz) == 0){
							value[0] = '0';
							value[1] = '\0';
						}

						if(strncmp(value,"true",sz) == 0){
							value[0] = '1';
							value[1] = '\0';
						}
					}


					if (!is_integer(value))
					{
						fprintf(stderr,"(%s): invalid value for byte type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
						return 0;
					}

					long l = string_to_long(value);
					if (error_value == INVALID_VALUE)
					{
						printf("conversion ERROR type float %s:%d.\n", F, L - 2);
						return 0;
					}
					if (l > UCHAR_MAX || l < 0)
					{
						printf("byte value not allowed in this system.\n");
						return 0;
					}
					rec->fields[index].data.b = (unsigned char)l;
				}
				break;
			}
		case TYPE_PACK:
			{
				if (!is_integer(value)){
					fprintf(stderr,"(%s): invalid value for pack type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
					return 0;
				}

				long p = string_to_long(value);
				if (error_value == INVALID_VALUE){
					printf("conversion ERROR type pack %s:%d.\n", F, L - 2);
					return 0;
				}

				if (p > MAX_KEY){
					printf("pack value not allowed on this system.\n");
					return 0;
				}
				rec->fields[index].data.p = (ui32)p;

				break;
			}
		case TYPE_DOUBLE:
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
			{
				if (type == TYPE_ARRAY_DOUBLE || type == TYPE_SET_DOUBLE)
				{
					if (!rec->fields[index].data.v.elements.d)
					{
						rec->fields[index].data.v.insert = insert_element;
						rec->fields[index].data.v.destroy = free_dynamic_array;
					}

					if(type == TYPE_SET_DOUBLE)
						rec->fields[index].data.v.is_set = 1;

					char *t = tok(value, ",");
					while (t)
					{
						if (!is_floaintg_point(t)) {
							if(is_integer(t)){
								char *decimal = ".00";
								size_t vs = strlen(t);
								size_t ds = strlen(decimal);
								size_t s = vs + ds + 1; 
								char cpy[s];
								memset(cpy,0,s);
								strncpy(cpy,value,vs);
								strncat(cpy,decimal,ds);

								int range = 0;
								if ((range = is_number_in_limits(cpy)) == 0){
									printf("double value not allowed in this system.\n");
									return 0;
								}

								if (range == IN_DOUBLE || range == IN_FLOAT) {
									double d = string_to_double(cpy);
									if (error_value == INVALID_VALUE) {
										printf("conversion ERROR type double %s:%d.\n", F, L - 2);
										return 0;
									}

									rec->fields[index].data.v.insert((void *)&d,
											&(rec->fields[index].data.v),
											type);
									t = tok(NULL, ",");
									continue;
								} else {
									fprintf(stderr,"value '%s', it's out of range for double.\n",t);
									return 0;
								}
							}

							fprintf(stderr,"(%s): invalid value for double type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
							return 0;
						}

						int range = 0;
						if ((range = is_number_in_limits(t)) == 0) {
							printf("float value not allowed in this system.\n");
							return 0;
						}

						if (range == IN_DOUBLE || range == IN_FLOAT) {
							double d = string_to_double(t);
							if (error_value == INVALID_VALUE) {
								printf("conversion ERROR type double %s:%d.\n", F, L - 2);
								return 0;
							}

							rec->fields[index].data.v.insert((void *)&d,
									&(rec->fields[index].data.v),
									type);
						}

						t = tok(NULL, ",");
					}
				}
				else
				{

					if (!is_floaintg_point(value)){
						if(is_integer(value)){
							char *decimal = ".00";
							size_t vs = strlen(value);
							size_t ds = strlen(decimal);
							size_t s = vs + ds + 1; 
							char cpy[s];
							memset(cpy,0,s);
							strncpy(cpy,value,vs);
							strncat(cpy,decimal,ds);

							int range = 0;
							if ((range = is_number_in_limits(cpy)) == 0){
								printf("double value not allowed in this system.\n");
								return 0;
							}

							if (range == IN_DOUBLE || range == IN_FLOAT) {
								double d = string_to_double(cpy);
								if (error_value == INVALID_VALUE) {
									printf("conversion ERROR type double %s:%d.\n", F, L - 2);
									return 0;
								}
								rec->fields[index].data.d = d;
								break;

							} else {
								fprintf(stderr,"value '%s', is out of range for double.\n",value);
								return 0;
							}
							fprintf(stderr,"(%s): invalid value for double type: %s. Field: '%s'\n",prog, value,rec->fields[index].field_name);
							return 0;
						}
					}
					int range = 0;
					if ((range = is_number_in_limits(value)) == 0)
					{
						printf("float value not allowed in this system.\n");
						return 0;
					}

					if (range == IN_DOUBLE || range == IN_FLOAT) {
						double d = string_to_double(value);
						if (error_value == INVALID_VALUE)
						{
							printf("conversion ERROR type double %s:%d.\n", F, L - 2);
							return 0;
						}
						rec->fields[index].data.d = d;
					}
				}
				break;
			}
		default:
			printf("invalid type! type -> %d.", type);
			return 0;
	}

	return 1;
}

/*
 * if optimized is 1 the main allocation
 * wont be freed, because we need that allocation again
 * so instead of free, and reallocating, we just zeroed out 
 * the memory and we use to store new data
 * */

void free_type_file(struct Record_f *rec,int optimized)
{
	int index = -1;
	int i;
	for(i = 0; i < rec->fields_num; i++){
		if(rec->field_set[i] == 1 && rec->fields[i].type == TYPE_FILE) index = i;	
	}

	if(index == -1) return;

	rec->field_set[index] = 0;
	if(!rec->fields[index].data.file.recs) return;

	ui32 k;
	for(k = 0; k < rec->fields[index].data.file.count;k++){
		if(!optimized)
			free_record(&rec->fields[index].data.file.recs[k],rec->fields[index].data.file.recs[k].fields_num);
		else
			memset(&rec->fields[index].data.file.recs[k],0,sizeof(struct Record_f));
	}

	if(!optimized) 
		free(rec->fields[index].data.file.recs);
}

void free_record(struct Record_f *rec, int fields_num)
{
	if(!rec) return;
	int i;
	for (i = 0; i < fields_num; i++){

		if (rec->field_set[i] == 0) continue;
		int t = (int)rec->fields[i].type;
		switch (t) {
			case -1:
			case TYPE_INT:
			case TYPE_KEY:
			case TYPE_LONG:
			case TYPE_FLOAT:
			case TYPE_BYTE:
			case TYPE_PACK:
			case TYPE_DATE:
			case TYPE_DOUBLE:
				break;
			case TYPE_STRING:
				if(rec->fields[i].data.s){
					free(rec->fields[i].data.s);
				}
				break;
			case TYPE_FILE:
				free_type_file(rec,0);
				break;
			case TYPE_ARRAY_INT:
			case TYPE_ARRAY_LONG:
			case TYPE_ARRAY_FLOAT:
			case TYPE_ARRAY_STRING:
			case TYPE_ARRAY_BYTE:
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_INT:
			case TYPE_SET_LONG:
			case TYPE_SET_FLOAT:
			case TYPE_SET_STRING:
			case TYPE_SET_BYTE:
				if(rec->fields[i].data.v.destroy)
					rec->fields[i].data.v.destroy(&rec->fields[i].data.v, 
							rec->fields[i].type);
				break;
			default:
				fprintf(stderr, "type not supported.\n");
				return;
		}
	}

	if(rec->count > 1){
		struct Record_f *temp = rec->next;
		if(temp){
			while(rec->count > 1){
				rec->next = temp->next;
				temp->next = NULL;
				free_record(temp,temp->fields_num);
				free(temp);
				temp = rec->next;  
				rec->count--;
				if(!temp)break; 
			}
		}
	}

	free(rec->fields);
	free(rec->field_set);
}

void print_record(int count, struct Record_f recs)
{

	printf("#################################################################\n\n");
	printf("the Record data are: \n");
	display_data(recs,0,0);
	if(count > 1){
		while(recs.next){
			struct Record_f rec = *recs.next;
			display_data(rec,0,0);
			recs.next =recs.next->next;
		}
	}
	printf("\n#################################################################\n\n");
}

static void display_data(struct Record_f rec, int max,int tab)
{
	int i;
	for (i = 0; i < rec.fields_num; i++){
		if (max < (int)strlen(rec.fields[i].field_name))
		{
			max = (int)strlen(rec.fields[i].field_name);
		}
	}

	for (i = 0; i < rec.fields_num; i++)
	{
		if(rec.field_set[i] == 0) continue;

		strip('"', rec.fields[i].field_name);

		if(rec.fields[i].type == TYPE_FILE)
			if(rec.fields[i].data.file.count == 0) continue;

		if(rec.fields[i].is_dropped)
			continue;

		if(tab)
			printf("\t");

		printf("%-*s\t", 10+(max - (int)strlen(rec.fields[i].field_name)), rec.fields[i].field_name);

		int t = (int)rec.fields[i].type;
		switch (t){
			case -1:
				break;
			case TYPE_INT:
				printf("%d\n", rec.fields[i].data.i);
				break;
			case TYPE_KEY:
				printf("%u\n", rec.fields[i].data.k);
				break;
			case TYPE_LONG:
				printf("%ld\n", rec.fields[i].data.l);
				break;
			case TYPE_FLOAT:
				printf("%.2f\n", rec.fields[i].data.f);
				break;
			case TYPE_STRING:
				strip('"', rec.fields[i].data.s);
				printf("%s\n", rec.fields[i].data.s);
				break;
			case TYPE_BYTE:
				printf("%u\n", rec.fields[i].data.b);
				break;
			case TYPE_PACK:
				{
					ui8 packed[5];
					pack(rec.fields[i].data.p,packed);
					print_pack_str(packed);
					printf("\n");
					break;
				}
			case TYPE_DATE:
				{
					char date_buff[11];
					memset(date_buff,0,11);
					convert_number_to_date(date_buff,(int)rec.fields[i].data.date);
					printf("%s\n",date_buff);
					break;
				}
			case TYPE_DOUBLE:
				printf("%.2f\n", rec.fields[i].data.d);
				break;
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++){
						if (rec.fields[i].data.v.size - k > 1) {
							if (!rec.fields[i].data.v.elements.i[k])
								continue;

							printf("%d, ", rec.fields[i].data.v.elements.i[k]);
						}else{
							printf("%d.\n", rec.fields[i].data.v.elements.i[k]);
						}
					}

					break;
				}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++)
					{
						if (rec.fields[i].data.v.size - k > 1)
						{
							printf("%ld, ", rec.fields[i].data.v.elements.l[k]);
						}
						else
						{
							printf("%ld.\n", rec.fields[i].data.v.elements.l[k]);
						}
					}
					break;
				}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++)
					{
						if (rec.fields[i].data.v.size - k > 1)
						{
							printf("%.2f, ", rec.fields[i].data.v.elements.f[k]);
						}
						else
						{
							printf("%.2f.\n", rec.fields[i].data.v.elements.f[k]);
						}
					}
					break;
				}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++)
					{
						if (rec.fields[i].data.v.size - k > 1)
						{
							strip('"', rec.fields[i].data.v.elements.s[k]);
							printf("%s, ", rec.fields[i].data.v.elements.s[k]);
						}
						else
						{
							printf("%s.\n", rec.fields[i].data.v.elements.s[k]);
						}
					}
					break;
				}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++)
					{
						if (rec.fields[i].data.v.size - k > 1)
						{
							printf("%u, ", rec.fields[i].data.v.elements.b[k]);
						}
						else
						{
							printf("%u.\n", rec.fields[i].data.v.elements.b[k]);
						}
					}
					break;
				}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
				{
					int k;
					for (k = 0; k < rec.fields[i].data.v.size; k++)
					{
						if (rec.fields[i].data.v.size - k > 1)
						{
							printf("%.2f, ", rec.fields[i].data.v.elements.d[k]);
						}
						else
						{
							printf("%.2f.\n", rec.fields[i].data.v.elements.d[k]);
						}
					}
					break;
				}
			case TYPE_FILE:

				if(rec.fields[i].data.file.count == 0) break;
				printf("\n");
				ui32 x;
				for(x = 0; x < rec.fields[i].data.file.count; x++){
					/*the last parameters is 1,will serve for formatting reason*/
					display_data(rec.fields[i].data.file.recs[x],max,1);
				}

				break;
			default:
				break;
		}
	}
}

void free_record_array(int len, struct Record_f **recs)
{
	int i;
	for (i = 0; i < len; i++) {
		free_record(&(*recs)[i],  (*recs)[i].fields_num);
	}
}

/* this parameters are:
   - int len => the length of the array() Record_f***
   - Record_f*** array => the arrays of record arrays
   - int* len_ia => length inner array,  an array that keeps track of all the found records array sizes
   - int size_ia => the actual size of len_ia,
   */
#if 0
void free_array_of_arrays(int len, struct Record_f ****array, int *len_ia, int size_ia)
{
	if (!*array)
		return;

	if (!size_ia)
	{
		free(NULL,*array);
		return;
	}

	int i = 0;
	for (i = 0; i < len; i++)
	{
		if ((*array)[i])
		{
			if (i < size_ia)
			{
				if (len_ia[i] != 0)
					free_record_array(len_ia[i], (*array)[i]);
			}
			else
			{
				free(NULL,(*array)[i]);
				return;
			}
		}
	}

	free(NULL,*array);
}

#endif
unsigned char copy_rec(struct Record_f *src, struct Record_f *dest, struct Schema *sch)
{
	if(sch)
		create_record(src->file_name, *sch, dest);

	/*if(!sch){
	  if(dest->fields_num == 0) return 0;
	  if(dest->fields[0].field_name[0] == '\0') return 0;
	  }*/


	strncpy(dest->file_name,src->file_name,strlen(src->file_name));
	dest->fields_num = src->fields_num;

	char data[30];
	ui16 i;
	for (i = 0; i < src->fields_num; i++)
	{
		switch (src->fields[i].type)
		{
			case -1:
				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, NULL,src->field_set[i])) {
					printf("set_fields() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_INT:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%d", src->fields[i].data.i) < 0) {
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, (*dest).fields_num);
					return 0;
				}

				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])) {
					printf("set_fields() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_STRING:
				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, src->fields[i].data.s,src->field_set[i])){
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_LONG:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%ld", src->fields[i].data.l) < 0) {
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}

				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])){
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_FLOAT:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%.2f", src->fields[i].data.f) < 0)
				{
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])) {
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_BYTE:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%d", src->fields[i].data.b) < 0) {
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])) {
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_DATE:
				dest->fields[i].data.date = src->fields[i].data.date;
				break;
			case TYPE_DOUBLE:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%.2f", src->fields[i].data.d) < 0)
				{
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])) {
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}
				break;
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
				memset(data, 0, 30);
				if (copy_to_string(data, 30, "%d", src->fields[i].data.v.elements.i[0]) < 0)
				{
					printf("copy_to_string failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}

				if (!set_field(dest, i, src->fields[i].field_name,
							src->fields[i].type, data,src->field_set[i])) {
					printf("set_field() failed %s:%d.\n", F, L - 2);
					free_record(dest, dest->fields_num);
					return 0;
				}

				int j;
				for (j = 1; j < src->fields[i].data.v.size; j++) {
					dest->fields[i].data.v.insert((void *)&src->fields[i].data.v.elements.i[j],
							&dest->fields[i].data.v,
							src->fields[i].type);
				}

				break;
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
				{
					memset(data, 0, 30);
					if (copy_to_string(data, 30, "%d", src->fields[i].data.v.elements.b[0]) < 0) {
						printf("copy_to_string failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					if (!set_field(dest, i, src->fields[i].field_name,
								src->fields[i].type, data,src->field_set[i])) {
						printf("set_field() failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					int j;
					for (j = 1; j < src->fields[i].data.v.size; j++) {
						dest->fields[i].data.v.insert((void *)&src->fields[i].data.v.elements.b[j],
								&dest->fields[i].data.v,
								src->fields[i].type);
					}
					break;
				}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
				{
					memset(data, 0, 30);
					if (copy_to_string(data, 30, "%ld", src->fields[i].data.v.elements.l[0]) < 0) {
						printf("copy_to_string failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					if (!set_field(dest, i, src->fields[i].field_name,
								src->fields[i].type, data,src->field_set[i])){
						printf("set_field() failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					int j;
					for (j = 1; j < src->fields[i].data.v.size; j++) {
						dest->fields[i].data.v.insert((void *)&src->fields[i].data.v.elements.l[j],
								&dest->fields[i].data.v,
								src->fields[i].type);
					}
					break;
				}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
				{
					memset(data, 0, 30);
					if (copy_to_string(data, 30, "%.2f", src->fields[i].data.v.elements.d[0]) < 0) {
						printf("copy_to_string failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					if (!set_field(dest, i, src->fields[i].field_name,
								src->fields[i].type, data,src->field_set[i])){
						printf("set_field() failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					int j;
					for (j = 1; j < src->fields[i].data.v.size; j++) {
						dest->fields[i].data.v.insert((void *)&src->fields[i].data.v.elements.d[j],
								&dest->fields[i].data.v,
								src->fields[i].type);
					}
					break;
				}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
				{
					memset(data, 0, 30);
					if (copy_to_string(data, 30, "%.2f", src->fields[i].data.v.elements.f[0]) < 0)
					{
						printf("copy_to_string failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					if (!set_field(dest, i, src->fields[i].field_name,
								src->fields[i].type, data,src->field_set[i])) {
						printf("set_field() failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					int j;
					for (j = 1; j < src->fields[i].data.v.size; j++) {
						dest->fields[i].data.v.insert((void *)&src->fields[i].data.v.elements.f[j],
								&dest->fields[i].data.v,
								src->fields[i].type);
					}
					break;
				}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
				{
					if (!set_field(dest, i, src->fields[i].field_name,
								src->fields[i].type, 
								src->fields[i].data.v.elements.s[0],
								src->field_set[i])) {
						printf("set_field() failed %s:%d.\n", F, L - 2);
						free_record(dest, dest->fields_num);
						return 0;
					}

					int j;
					for (j = 1; j < src->fields[i].data.v.size; j++) {
						dest->fields[i].data.v.insert((void *)src->fields[i].data.v.elements.s[j],
								&dest->fields[i].data.v,
								src->fields[i].type);
					}
					break;
				}
			case TYPE_FILE:
				{
					/*need to understand this better*/
					dest->field_set[i] = 1;
					dest->fields[i].data.file.count = src->fields[i].data.file.count;

					if(!dest->fields[i].data.file.recs){
						dest->fields[i].data.file.recs = (struct Record_f*)malloc(src->fields[i].data.file.count * sizeof(struct Record_f));

						if(!dest->fields[i].data.file.recs){
							fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__-3);
							free_record(dest, dest->fields_num);
							return 0;
						}	

						memset(dest->fields[i].data.file.recs,0,src->fields[i].data.file.count * sizeof(struct Record_f));
					}else{
						struct Record_f *new_rec = (struct Record_f*)realloc(dest->fields[i].data.file.recs,
								src->fields[i].data.file.count * sizeof(struct Record_f));

						if(!new_rec){
							fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__-3);
							free_record(dest, dest->fields_num);
							return 0;
						}

						dest->fields[i].data.file.recs = new_rec;
						dest->fields[i].data.file.count = src->fields[i].data.file.count;
					}

					ui32 j;
					for (j = 0; j < src->fields[i].data.file.count; j++) {
						if(!copy_rec(&src->fields[i].data.file.recs[j],&dest->fields[i].data.file.recs[j],sch)){
							fprintf(stderr,"copy_rec() failed, %s:%d.\n",F,L-1);
							free_record(dest, dest->fields_num);
							return 0;
						}	
					}
					break;
				}
			default:
				printf("type not supported, %s:%d.\n", F, L - 27);
				return 0;
		}
	}

	return 1;
}

unsigned char get_index_rec_field(char *field_name, struct Record_f *rec,int *field_i_r, int *rec_index)
{
	size_t field_name_len = strlen(field_name);
	int i,j;
	if(rec->count > 1){
		i = 0;
		struct Record_f *r = rec;
		while(r){
			for (j = 0; j < r->fields_num; j++){
				if (strncmp(field_name, r->fields[j].field_name, field_name_len) == 0){
					*field_i_r = j;
					*rec_index = i;
					return 1;
				}
			}
			i++;
			r = r->next;
		}
		return 0;
	}

	for (j = 0, i = 0; j < rec->fields_num; j++){
		if (strncmp(field_name, rec->fields[j].field_name, field_name_len) == 0){
			*field_i_r = j;
			*rec_index = i;
			return 1;
		}
	}

	return 0;
}

int init_array(struct array **v, enum ValueType type)
{
	(*(*v)).size = DEF_SIZE;
	switch (type){
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
			{
				(*(*v)).elements.i = (int*)malloc(DEF_SIZE * sizeof(int));
				if (!(*(*v)).elements.i){
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.i,0,sizeof(int)*DEF_SIZE);

				break;
			}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
			{
				(*(*v)).elements.l = (long*)malloc(DEF_SIZE * sizeof(long));
				if (!(*(*v)).elements.l)
				{
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.l,0,sizeof(long) * DEF_SIZE);
				break;
			}
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
			{
				(*(*v)).elements.f = (float*)malloc(DEF_SIZE * sizeof(float));
				if (!(*(*v)).elements.f){
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.l,0,sizeof(float) * DEF_SIZE);
				break;
			}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
			{
				(*(*v)).elements.s = (char**)malloc(DEF_SIZE*sizeof(char *));
				if (!(*(*v)).elements.s)
				{
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.s,0,sizeof(char*) * DEF_SIZE);
				break;
			}
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
			{
				(*(*v)).elements.b = (unsigned char*)malloc(DEF_SIZE * sizeof(unsigned char));
				if (!(*(*v)).elements.b)
				{
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.b,0,sizeof(unsigned char) * DEF_SIZE);
				break;
			}
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
			{
				(*(*v)).elements.d = (double*)malloc(DEF_SIZE * sizeof(double));
				if (!(*(*v)).elements.d)
				{
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				memset((*(*v)).elements.d,0,sizeof(double) * DEF_SIZE);
				break;
			}
		default:
			fprintf(stderr, "type not supported.\n");
			return -1;
	}
	return 0;
}

int is_element_in_set(void *element,struct array *v,enum ValueType type)
{

	int i;
	switch(type){
		case TYPE_SET_INT:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.i[i] == *(int*) element)
					return 1;
			}
			break;
		case TYPE_SET_BYTE:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.b[i] == *(unsigned char*) element)
					return 1;
			}
			break;
		case TYPE_SET_LONG:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.l[i] == *(long*) element)
					return 1;
			}
			break;
		case TYPE_SET_FLOAT:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.f[i] == *(float*) element)
					return 1;
			}
			break;
		case TYPE_SET_DOUBLE:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.d[i] == *(double*) element)
					return 1;
			}
			break;
		case TYPE_SET_STRING:
			for(i = 0; i < (*v).size; i++){
				if((*v).elements.s[i]){
					if(strncmp((*v).elements.s[i],(char*)element,strlen((char*)element)) == 0)
						return 1;
				}
			}
			break;
		default:
			fprintf(stderr, "(db): array type not supperted\n");
			return 1;
	}

	return 0;
}
int insert_element(void *element, struct array *v, enum ValueType type)
{

	switch (type){
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.i)
				{
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}
				/*check if there is enough space for new item */
				if (!(*v).elements.i[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++)
					{
						if ((*v).elements.i[i])
							continue;

						((*v).elements.i[i]) = *(int *)element;
						return 0;
					}
				}

				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				int *elements_new = (int*)realloc((*v).elements.i,new_size * sizeof(int));
				if (!elements_new){
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}

				(*v).size = new_size;

				(*v).elements.i = elements_new;
				((*v).elements.i[(*v).size - 1]) = *(int *)element;
				return 0;
			}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.l)
				{
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}

				/*check if there is enough space for new item */
				if (!(*v).elements.l[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++)
					{
						if ((*v).elements.l[i])
							continue;

						((*v).elements.l[i]) = *(long *)element;
						return 0;
					}
				}
				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				long *elements_new = (long*)realloc((*v).elements.l,new_size * sizeof(long));
				if (!elements_new){
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}

				(*v).size = new_size;

				(*v).elements.l = elements_new;
				((*v).elements.l[(*v).size - 1]) = *(long *)element;
				return 0;
			}
		case TYPE_ARRAY_FLOAT:
		case TYPE_SET_FLOAT:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.f)
				{
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}


				/*check if there is enough space for new item */
				if (!(*v).elements.f[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++)
					{
						if ((*v).elements.f[i])
							continue;

						((*v).elements.f[i]) = *(float *)element;
						return 0;
					}
				}
				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				float *elements_new = (float*)realloc((*v).elements.f,new_size * sizeof(float));
				if (!elements_new){
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}
				(*v).size = new_size;
				(*v).elements.f = elements_new;
				((*v).elements.f[(*v).size - 1]) = *(float *)element;
				return 0;
			}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.s){
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}


				/*check if there is enough space for new item */
				if (!(*v).elements.s[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++){
						if ((*v).elements.s[i])
							continue;

						size_t l = strlen((char *)element) + 1;

						(*v).elements.s[i] = (char*)malloc(l * sizeof(char));
						if (!(*v).elements.s[(*v).size - 1]){
							fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
							return -1;
						}

						memset((*v).elements.s[i],0,sizeof(char) * l);
						strncpy((*v).elements.s[i], (char *)element, l-1);

						return 0;
					}
				}

				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				char **elements_new = (char**)realloc((*v).elements.s, new_size * sizeof(char *));
				if (!elements_new){
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}

				(*v).size = new_size;
				(*v).elements.s = elements_new;

				size_t l = strlen((char *)element) + 1;
				(*v).elements.s[(*v).size - 1] = (char*)malloc(l * sizeof(char));
				if (!(*v).elements.s[(*v).size - 1]){
					fprintf(stderr,"(%s): malloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}


				memset((*v).elements.s[(*v).size - 1],0,sizeof(char) * l);
				strncpy((*v).elements.s[(*v).size - 1], (char *)element, l);
				return 0;
			}
		case TYPE_ARRAY_BYTE:
		case TYPE_SET_BYTE:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.b)
				{
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}

				/*check if there is enough space for new item */
				if (!(*v).elements.b[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++)
					{
						if ((*v).elements.b[i])
							continue;

						((*v).elements.b[i]) = *(unsigned char *)element;
						return 0;
					}
				}

				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				unsigned char *elements_new = (unsigned char*)realloc((*v).elements.b,
						new_size * sizeof(unsigned char));
				if (!elements_new)
				{
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}

				(*v).size = new_size;
				(*v).elements.b = elements_new;
				((*v).elements.b[(*v).size - 1]) = *(unsigned char *)element;
				return 0;
			}
		case TYPE_ARRAY_DOUBLE:
		case TYPE_SET_DOUBLE:
			{
				/*check if the array has been initialized */
				if (!(*v).elements.d)
				{
					if (init_array(&v, type) == -1)
					{
						fprintf(stderr, "init array failed.\n");
						return -1;
					}
				}

				if((*v).is_set){
					if(is_element_in_set(element,v,type))
						return -1;
				}


				/*check if there is enough space for new item */
				if (!(*v).elements.d[(*v).size - 1])
				{
					int i;
					for (i = 0; i < (*v).size; i++)
					{
						if ((*v).elements.d[i])
							continue;

						((*v).elements.d[i]) = *(double *)element;
						return 0;
					}
				}

				/*not enough space, increase the size */
				int new_size = (*v).size + 1;
				double *elements_new = (double*)realloc((*v).elements.d,
						new_size * sizeof(double));
				if (!elements_new){
					fprintf(stderr,"(%s): realloc() failed, %s:%d.\n",ERR_MSG_PAR-2);
					return -1;
				}

				(*v).elements.d = elements_new;
				(*v).size = new_size;
				((*v).elements.d[(*v).size - 1]) = *(double *)element;
				return 0;
			}
		default:
			fprintf(stderr, "type not supperted");
			return -1;
	}

	return 0; /*success*/
}

void free_dynamic_array(struct array *v, enum ValueType type)
{
	switch (type)
	{
		case TYPE_ARRAY_INT:
		case TYPE_SET_INT:
			{
				free(v->elements.i);
				v->elements.i = NULL;
				v->size = 0;
				break;
			}
		case TYPE_ARRAY_LONG:
		case TYPE_SET_LONG:
			{
				free(v->elements.l);
				v->elements.l = NULL;
				v->size = 0;
				break;
			}
		case TYPE_SET_FLOAT:
		case TYPE_ARRAY_FLOAT:
			{
				free(v->elements.f);
				v->elements.f = NULL;
				v->size = 0;
				break;
			}
		case TYPE_ARRAY_STRING:
		case TYPE_SET_STRING:
			{
				int i;
				for (i = 0; i < v->size; i++){
					if (v->elements.s[i])
						free(v->elements.s[i]);
				}

				free(v->elements.s);
				v->elements.s = NULL;
				v->size = 0;
				break;
			}
		case TYPE_SET_BYTE:
		case TYPE_ARRAY_BYTE:
			{
				free(v->elements.b);
				v->elements.s = NULL;
				v->size = 0;
				break;
			}
		case TYPE_SET_DOUBLE:
		case TYPE_ARRAY_DOUBLE:
			{
				free(v->elements.d);
				v->elements.d = NULL;
				v->size = 0;
				break;
			}
		default:
			fprintf(stderr, "array type not suported.\n");
			return;
	}
}


int schema_has_type(struct Header_d *hd)
{
	ui16 i;
	for(i = 0;i < hd->sch_d->fields_num;i++)
		if(hd->sch_d->types[i] == -1) return 0;

	return 1;
}


int compare_rec(struct Record_f *src, struct Record_f *dest,int option)
{
	if (src->fields_num > dest->fields_num) return -1;
	if (src->fields_num < dest->fields_num) return 1;
	if (src->fields_num == dest->fields_num){
		int c = 0;
		int active = 0;	
		int ai = 0;
		int al = 0;
		int ab = 0;
		int af = 0;
		int ad = 0;
		int as = 0;
		ui32 tf = 0;
		int dif = 0;

		int i;
		for(i = 0; i < src->fields_num; i++){
			if(src->field_set[i] == 1 && dest->field_set[i] == 1){
				active++;
				switch(src->fields[i].type){
					case TYPE_INT:
						if(src->fields[i].data.i == dest->fields[i].data.i) c++;

						if(c == active) break;
						return i;
					case TYPE_LONG:
						if(src->fields[i].data.l == dest->fields[i].data.l) c++;
						if(c == active) break;
						return i;
					case TYPE_BYTE:
						if(src->fields[i].data.b == dest->fields[i].data.b) c++;
						if(c == active) break;
						return i;
					case TYPE_FLOAT:
						if(src->fields[i].data.f == dest->fields[i].data.f) c++;
						if(c == active) break;
						return i;
					case TYPE_DATE:
						if(src->fields[i].data.date == dest->fields[i].data.date) c++;
						if(c == active) break;
						return i;
					case TYPE_DOUBLE:
						if(src->fields[i].data.d == dest->fields[i].data.d) c++;
						if(c == active) break;
						return i;
					case TYPE_STRING:
						{
							size_t l = strlen(src->fields[i].data.s);
							if (l != strlen(dest->fields[i].data.s)) return i;

							if(strncmp(src->fields[i].data.s,dest->fields[i].data.s,l) == 0) c++;
							if(c == active) break;
							return i;
						}
					case TYPE_ARRAY_INT:
					case TYPE_SET_INT:
						{	
							if(option == AAR){
								return i;
							}


							if (dest->fields[i].data.v.elements.i){
								if (dest->fields[i].data.v.size == 1 && dest->fields[i].data.v.elements.i[0] == 0){
									if(src->fields[i].data.v.size == 1 && 
											src->fields[i].data.v.elements.i[0] == 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (src->fields[i].data.v.elements.i[a] == 
												dest->fields[i].data.v.elements.i[a]) ai++;

									}
								}
								if(dest->fields[i].data.v.size == ai) c++;

								if(c == active) break;
								return i;
							}
							break;
						}
					case TYPE_ARRAY_LONG:
					case TYPE_SET_LONG:
						{
							if(option == AAR){
								return i;
							}

							if (dest->fields[i].data.v.elements.l){
								if (dest->fields[i].data.v.size == 1 && dest->fields[i].data.v.elements.l[0] == 0){
									if(src->fields[i].data.v.size == 1 && 
											src->fields[i].data.v.elements.l[0] == 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (src->fields[i].data.v.elements.l[a] == 
												dest->fields[i].data.v.elements.l[a]) al++;
									}
								}
								if(dest->fields[i].data.v.size == al) c++;

								if(c == active) break;
								return i;
							}
							break;
						}
					case TYPE_ARRAY_BYTE:
					case TYPE_SET_BYTE:
						{
							if(option == AAR){
								return i;
							}
							if (dest->fields[i].data.v.elements.b){
								if (dest->fields[i].data.v.size == 1 && dest->fields[i].data.v.elements.b[0] == 0){
									if(src->fields[i].data.v.size == 1 && 
											src->fields[i].data.v.elements.b[0] == 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (src->fields[i].data.v.elements.b[a] == 
												dest->fields[i].data.v.elements.b[a]) ab++;

									}
								}
								if(dest->fields[i].data.v.size == ab) c++;

								if(c == active) break;
								return i;
							}
							break;
						}
					case TYPE_SET_FLOAT:
						{
							if(option == AAR){
								return i;
							}
							if (dest->fields[i].data.v.elements.f){
								if (dest->fields[i].data.v.size == 1 && dest->fields[i].data.v.elements.f[0] == 0.00){
									if(src->fields[i].data.v.size == 1 && 
											src->fields[i].data.v.elements.f[0] == 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (src->fields[i].data.v.elements.f[a] == 
												dest->fields[i].data.v.elements.f[a]) af++;
									}
								}
								if(dest->fields[i].data.v.size == af) c++;

								if(c == active) break;
								return i;
							}

							break;
						}
					case TYPE_ARRAY_DOUBLE:
					case TYPE_SET_DOUBLE:
						{
							if(option == AAR){
								return i;
							}
							if (dest->fields[i].data.v.elements.d){
								if (dest->fields[i].data.v.size == 1 && dest->fields[i].data.v.elements.d[0] == 0.0){
									if(src->fields[i].data.v.size == 1 && 
											src->fields[i].data.v.elements.d[0] == 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (src->fields[i].data.v.elements.d[a] == 
												dest->fields[i].data.v.elements.d[a]) ad++;

									}
								}
								if(dest->fields[i].data.v.size == ad) c++;

								if(c == active) break;
								return i;
							}
							break;
						}
					case TYPE_ARRAY_STRING:
					case TYPE_SET_STRING:
						{
							if(option == AAR){
								return i;
							}
							if (dest->fields[i].data.v.elements.s){
								if (dest->fields[i].data.v.size == 1 && 
										strncmp(dest->fields[i].data.v.elements.s[0],
											"null",strlen("null"))== 0){
									if(src->fields[i].data.v.size == 1 && 
											strncmp(dest->fields[i].data.v.elements.s[0],
												"null",strlen("null"))== 0) c++;

									if(c == active) break;
									return i;
								}

								/*check the values*/
								if (dest->fields[i].data.v.size == src->fields[i].data.v.size) {
									int a;
									for (a = 0; a < dest->fields[i].data.v.size; a++){
										if (strncmp(src->fields[i].data.v.elements.s[a], 
													dest->fields[i].data.v.elements.s[a],
													strlen(src->fields[i].data.v.elements.s[a])) == 0) as++;
									}
								}
								if(dest->fields[i].data.v.size == as) c++;

								if(c == active) break;
								return i;
							}
							break;
						}
					case TYPE_FILE:
						{
							if(src->fields[i].data.file.recs){
								/*if the files have the same count*/
								if(src->fields[i].data.file.count == dest->fields[i].data.file.count){
									ui32 k;
									for(k = 0; k < src->fields[i].data.file.count; k++){
										if(compare_rec(&src->fields[i].data.file.recs[k],
													&dest->fields[i].data.file.recs[k],option) == -1){ 
											tf++;
											continue;
										} 
									}
									if(tf == src->fields[i].data.file.count)c++;

									if(c == active) break;

									return i;
								}

								if(src->fields[i].data.file.count > dest->fields[i].data.file.count){
									/*compare the records*/
									ui32 u,y;
									for(u = 0; u < src->fields[i].data.file.count;u++){
										for(y = 0;y < dest->fields[i].data.file.count;y++){
											if(compare_rec(&src->fields[i].data.file.recs[u],
														&dest->fields[i].data.file.recs[y],option) == -1){
												tf++;
												continue;
											}
										}
									}

									if(tf == dest->fields[i].data.file.count)c++;
									if(c == active) break;

									return i;
								}
								break;
							}

							/*files have a different count*/
							if(src->fields[i].data.file.count > dest->fields[i].data.file.count) return i;
							if(src->fields[i].data.file.count < dest->fields[i].data.file.count) return i;
							break;
						}
					default:
						fprintf(stderr,"type not supported %s:%d.\n",__FILE__,__LINE__);
						return E_RCMP;
				}

			}else{
				dif++;
			} 
		}

		if(dif > 0)
			if(dif == src->fields_num) return DIF;

		if(c == active) return -1;
	}
	return E_RCMP;	
}

static void clean_input(char *value)
{
	strip('[',value);
	strip(']',value);
}

int parse_record_to_json(struct Record_f *rec,char **buffer)
{

	size_t buffer_lenght = 1024 * 4;
	size_t bwritten = strlen(*buffer);
	int i;
	struct Record_f *temp = rec;
	while(temp){
		for(i = 0; i < temp->fields_num; i++){
			if(temp->field_set[i] == 0) continue;	

			switch(temp->fields[i].type){
				case TYPE_INT:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  number_of_digit(temp->fields[i].data.i) + \
												  4 + 2; /* 4 '"',  1 ':', 1 ',' */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%d\",",
										temp->fields[i].field_name,
										temp->fields[i].data.i) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%d\",",
										temp->fields[i].field_name,
										temp->fields[i].data.i) == -1){
								return -1;
							}
						}

						bwritten += field_tot_length;
						break;
					}
				case TYPE_LONG:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  number_of_digit(temp->fields[i].data.l) + \
												  4 + 2; /* 4 '"',  1 ':', 1 ',' */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%ld\",",
										temp->fields[i].field_name,
										temp->fields[i].data.l) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%ld\",",
										temp->fields[i].field_name,
										temp->fields[i].data.l
										) == -1){
								return -1;
							}
						}

						bwritten += field_tot_length;
						break;
					}
				case TYPE_BYTE:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  number_of_digit(temp->fields[i].data.b) + \
												  4 + 2; /* 4 '"',  1 ':', 1 ',' */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%d\",",
										temp->fields[i].field_name,
										temp->fields[i].data.b
										) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%d\",",
										temp->fields[i].field_name,
										temp->fields[i].data.b
										) == -1){
								return -1;
							}
						}

						bwritten += field_tot_length;
						break;
					}
				case TYPE_FLOAT:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  digits_with_decimal(temp->fields[i].data.f) + \
												  4 + 2 + 2; /* 4 '"',  1 ':', 1 ',', 2 '00' decimal pleces */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%.2f\",",
										temp->fields[i].field_name,
										temp->fields[i].data.f
										) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%.2f\",",
										temp->fields[i].field_name,
										temp->fields[i].data.f
										) == -1){
								return -1;
							}
						}

						bwritten += field_tot_length;
						break;
					}
				case TYPE_DATE:
					{
						char date_buff[11];
						memset(date_buff,0,11);
						if(convert_number_to_date(date_buff,(int)rec->fields[i].data.date) == -1){
							fprintf(stderr,"cannot convert date");
							return -1;
						}

						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  strlen(date_buff) + \
												  4 + 2; /* 4 '"',  1 ':', 1 ',' */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%s\",",
										temp->fields[i].field_name,
										date_buff
										) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%s\",",
										temp->fields[i].field_name,
										date_buff
										) == -1){
								return -1;
							}
						}
						bwritten += field_tot_length;
						break;


					}	
				case TYPE_DOUBLE:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) +
							digits_with_decimal(temp->fields[i].data.d) + 
							4 + 2 + 2; /* 4 '"',  1 ':', 1 ',', 2 '00' decimal pleces */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%.2f\",",
										temp->fields[i].field_name,
										temp->fields[i].data.d
										) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%.2f\",",
										temp->fields[i].field_name,
										temp->fields[i].data.d
										) == -1){
								return -1;
							}
						}

						bwritten += field_tot_length;
						break;
					}
				case TYPE_STRING:
					{
						size_t field_tot_length = strlen(temp->fields[i].field_name) + 	   \
												  strlen(temp->fields[i].data.s) + \
												  4 + 2; /* 4 '"',  1 ':', 1 ',' */
						if(bwritten == 0){
							if(copy_to_string(*buffer,field_tot_length+1,"\"%s\":\"%s\",",
										temp->fields[i].field_name,
										temp->fields[i].data.s
										) == -1){
								/*log error*/
								return -1;
							}
						}else{
							if((bwritten + field_tot_length) >= buffer_lenght){
								/* reallocate memory*/
								size_t new_size = buffer_lenght * 2;
								char *n_buff = (char*)realloc(*buffer,new_size * sizeof(char));
								if(!n_buff){
									/*log error*/
									fprintf(stderr,"realloc() failed %s:%d.\n",F,L-3);
									return -1;
								}
								*buffer = n_buff;
								buffer_lenght = new_size;
							}

							if(copy_to_string(&(*buffer)[bwritten],field_tot_length+1,"\"%s\":\"%s\",",
										temp->fields[i].field_name,
										temp->fields[i].data.s
										) == -1){
								return -1;
							}
						}
						bwritten += field_tot_length;
						break;
					}
				case TYPE_ARRAY_INT:
				case TYPE_SET_INT:
				case TYPE_ARRAY_LONG:
				case TYPE_SET_LONG:
				case TYPE_ARRAY_BYTE:
				case TYPE_SET_BYTE:
				case TYPE_ARRAY_FLOAT:
				case TYPE_SET_FLOAT:
				case TYPE_ARRAY_DOUBLE:
				case TYPE_SET_DOUBLE:
				case TYPE_FILE:
				default:
					break;
			}
		}
		temp = temp->next;
	}
	if((*buffer)[bwritten-1] == ',') (*buffer)[bwritten-1] = '}';
	return 0;
}

int drop_field(struct Schema *s, char *fields)
{
	if(!s)
		return -1;

	int pos[s->fields_num];
	memset(pos,-1,sizeof(int) * s->fields_num);

	char *field = tok(fields,":");
	do{
		size_t field_sz = strlen(field);
		int i;
		for(i = 0; i < s->fields_num; i++){
			if(field_sz != strlen(s->fields_name[i]))
				continue;

			if(strncmp(s->fields_name[i],field,field_sz) == 0){
				if(!s->is_dropped[i]){
					s->is_dropped[i] = 1;
					pos[i] = 1;
				}
			}
		}
	}while((field = tok(NULL,":")));

	int i = 0, dr = 0;
	for(;i < s->fields_num; i++){
		if(pos[i] != -1){
			fprintf(stdout,"field '%s' dropped.\n",s->fields_name[i]);
			dr++;
		}
	}

	if(dr)
		return 0;
	
	return -1;
}
int change_fields_name(char *buffer,struct Schema *sch)
{
	clear_tok();
	if(!buffer)
		return -1;

	if(strstr(buffer, "TYPE_STRING") 
		||	strstr(buffer, "TYPE_LONG") 
		||	strstr(buffer, "TYPE_INT")
		|| 	strstr(buffer, "TYPE_BYTE")
		|| 	strstr(buffer, "TYPE_FLOAT")
		||	strstr(buffer, "TYPE_PACK")
		|| 	strstr(buffer, "TYPE_DOUBLE")
		|| 	strstr(buffer, "TYPE_DATE") 
		|| 	strstr(buffer, "TYPE_KEY")
		|| 	strstr(buffer, "TYPE_FILE")
		||	strstr(buffer, "TYPE_ARRAY_INT") 
		||	strstr(buffer, "TYPE_ARRAY_FLOAT") 
		||	strstr(buffer, "TYPE_ARRAY_LONG") 
		||  strstr(buffer, "TYPE_ARRAY_STRING") 
		||	strstr(buffer, "TYPE_ARRAY_BYTE") 
		||	strstr(buffer, "TYPE_ARRAY_DOUBLE")
		||	strstr(buffer, "TYPE_SET_BYTE") 
		||	strstr(buffer, "TYPE_SET_INT") 
		||	strstr(buffer, "TYPE_SET_LONG") 
		||	strstr(buffer, "TYPE_SET_DOUBLE") 
		||	strstr(buffer, "TYPE_SET_FLOAT") 
		||	strstr(buffer, "TYPE_SET_STRING")){
		fprintf(stderr,"schema different then file definition.\n");
		return -1;
	}

	char *t = tok(buffer,":");
	if(!t)
		return -1;

	ui8 change = 0;
	while(t){
		int sz = (int)strlen(t);
		int i; 
		for(i = 0; i < sch->fields_num; i++){
			if(sz != (int)strlen(sch->fields_name[i]))
				continue;

			if(strncmp(sch->fields_name[i],t,sz) != 0)
				continue;

			free(sch->fields_name[i]);
			sch->fields_name[i] = NULL;
			t = tok(NULL,":");
			if(!t)
				return -1;
			
			change = 1;
			int tsz = strlen(t);
			sch->fields_name[i] = (char *) malloc(tsz+1);
			if(!sch->fields_name[i]){
				fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
				return -1;
			}

			sch->fields_name[i][tsz] = '\0';
			strncpy(sch->fields_name[i],t,tsz);
		}
		t = tok(NULL,":");
	}

	if(change)
		return 0;
	else
		return -1;
}

ui8 has_constrain_unique(ui8 *constraints, int field_num,int *field_unique)
{
	int i,f = 0;
	for(i = 0; i < field_num; i++){
		if(constraints[i] & CONST_UNIQUE){
			field_unique[i] = i;
			f = 1;
		}
	}
	return f;
}

/* This function is used to update the old record*/
int combine_old_and_new_rec(char *file_name,struct Record_f *old, struct Record_f *new,struct Schema sch)
{
	int pos[old->fields_num];
	memset(pos,-1,sizeof(int)*old->fields_num);

	ui8 need_new = 0;
	struct Record_f *o = old;
	do{
		int i;
		for(i = 0; i < o->fields_num; i++){
			if(!new->field_set[i]) continue;

			if(o->field_set[i] && new->field_set[i]){
				if(copy_fields(&new->fields[i],&o->fields[i]) == -1)return -1;
				new->field_set[i] = 0;
				if(new->fields[i].type == TYPE_STRING){
					free(new->fields[i].data.s);
					new->fields[i].data.s = NULL;
				}
				continue;
			}

			if(!o->field_set[i] && new->field_set[i]){
				pos[i] = i;
				need_new = 1;
			}
		}

		if(o->next)
			o = o->next;
		else
			break;

	}while(o);
	
	/*---------------TODO------------------*/
	/* to update a record, we assume that the new record overwrites
	 * whatever fields are already active in the old record from the file, if they are active in the new record as well
	 * i.e :
	  		if(o->field_set[i] && new->field_set[i]){
				if(copy_fields(&new->fields[i],&o->fields[i]) == -1)return -1;
				continue;
			}
	 * if a field is active in the new record but not in the record on file
	 * we need to create a new record in the first free spot in the record linked list, and populated with the new field 
	 * of the updated record.
	 * */

	if(need_new){
		o->next = malloc(sizeof *o);
		if(!o->next){
			fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
			return -1;
		}

		memset(o->next,0,sizeof *o);
		if(create_record(file_name,sch, o->next) == -1){
			fprintf(stderr,"create_record() failed, %s:%d.\n",__FILE__,__LINE__-2);
			return -1;
		}

		int i;
		for(i = 0; i < o->fields_num; i++){
			if(pos[i] == -1)continue; 
			copy_fields(&new->fields[i],&o->next->fields[i]);
			o->next->field_set[i] = 1;
		}

		old->count++;
	}
	return 0;
}

