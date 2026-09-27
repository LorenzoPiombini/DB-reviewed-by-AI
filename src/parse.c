#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "record.h"
#include "file.h"
#include "endian.h"
#include "db_types.h"
#include "str_op.h"
#include "parse.h"
#include "common.h"
#include "sort.h"
#include "debug.h"
#include "input.h"
#include "date.h"
#include "hash_tbl.h"
#include "crud.h"

static char prog[] = "db";
static int schema_check_type(int count,
							int mode,
							struct Schema *sch,
							char names[][MAX_FILED_LT],
							int *types_i,
							char ***values,
							int option);

static int check_double_compatibility(struct Schema *sch, char ***values);
static char *get_def_value(struct Schema sch,int i);
static char *print_constraints(struct Schema sch, int i);
/*this allows for compatibility between different versions*/
static int read_hd_V1(ui8 **buf, long *bread, struct Schema **sch);
/*static int read_hd_V1_1(ui8 **buf, long *bread, struct Schema **sch);*/
static int write_hd_VS1(ui8 **buf, long *bwritten, struct Schema **sch);
/*static int write_hd_VS1_1(ui8 **buf, long *bwritten, struct Schema **sch);*/
static int is_input_correct(char names[][MAX_FIELD_LT],int fields_num);
static int enforce_constraints(struct Schema *sch, int i, int found,struct Record_f *rec);
static int unique_constraint_update(file_t *fds, struct Record_f *rec, struct Record_f *old_rec, int index, char *file_path);

int parse_d_flag_input(	char *file_path, 
						int fields_num, 
						char *buffer, 
						struct Schema *sch, 
						int check_sch,
						struct Record_f *rec,
						int *pos,
						int update)
{

	char names[MAX_FIELD_NR][MAX_FIELD_LT];
	memset(names,0,MAX_FIELD_NR * MAX_FIELD_LT * sizeof(char));

	int types_i[MAX_FIELD_NR];
	memset(types_i,-1,MAX_FIELD_NR*sizeof(int));

	size_t s = strlen(buffer);
	char cpy[s+1];
	memset(cpy,0,s+1);
	strncpy(cpy,buffer,s);

	clear_tok();
	int *constraints = NULL;
	char **def_values = NULL;
	if(get_constrains(cpy,fields_num,&constraints,&def_values) == -1)
		return -1;
	
	clear_tok();
	get_fileds_name(buffer, fields_num, 3, names);
	clear_tok();
	get_value_types(buffer, fields_num, 3, types_i);
	
	/* check if the fields name are correct- if not - input is incorrect */
	if(!is_input_correct(names,fields_num))
		return -1;
	
	if (!check_fields_integrity(names, fields_num)) {
		fprintf(stderr,"(%s): invalid input, one or more fields have the same name, or the value is missing\n",prog);
		fprintf(stderr,"(%s): input syntax: fieldname:type:value\n",prog);
		return -1;
	}

	if (sch && check_sch == 0 && sch->fields_num == 0) {
		/* true when a new file is created */
		if(set_schema(names, types_i, sch, fields_num,constraints,def_values) == -1){
			fprintf(stderr,"(%s): set_schema failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
			return -1;
		}
	}

	char **values = NULL;
	values = get_values(buffer,fields_num);
	if (!values) {
		printf("get_values failed, %s:%d.\n",__FILE__, __LINE__-1);
		return -1;
	}

	
	int reorder_rtl = -1;
	if (check_sch == SCHEMA_EQ){
		reorder_rtl = sort_input_like_header_schema(check_sch, fields_num, sch, names, &values, types_i);

		if (reorder_rtl == -1) {
			printf("sort_input_like_header_schema failed, parse.c l %d.\n", __LINE__ - 4);
			free_strs(fields_num, 1, values);
			return -1;
		}
				
		if(pos){
			if(pos[0] != -1){
				char *field_n =	find_field_to_reset_delim(pos,cpy);
				size_t fd_nl = strlen(field_n);
				int i;
				for(i = 0; i < fields_num; i++){
					if(fd_nl != strlen(names[i])) continue;
					if(strncmp(field_n,names[i], fd_nl) == 0){
						replace('{',':',values[i]);	
						break;
					}
				}
			} 
		}	
	}

#if 0
	if (check_sch == SCHEMA_NW) {
		if(sort_input_like_header_schema(check_sch, fields_num, sch, names, &values, types_i) == -1){
			printf("sort_input_like_header_schema failed, %s:%d.\n", F, L - 4);
			free_strs(fields_num, 1, values);
			return -1;
		}

		if(pos){
			if(pos[0] != -1){
				char *field_n =	find_field_to_reset_delim(pos,cpy);
				size_t fd_nl = strlen(field_n);
				int i;
				for(i = 0; i < fields_num; i++){
					if(fd_nl != strlen(names[i])) continue;
					if(strncmp(field_n,names[i], fd_nl) == 0){
						replace('{',':',values[i]);	
						break;
					}
				}
			}
		}

		int old_fn = sch->fields_num;
		sch->fields_num = fields_num;

		int i;
		for(i = old_fn; i < fields_num; i++) {
			strncpy(sch->fields_name[i],names[i],strlen(names[i]));
			sch->types[i] = types_i[i];
		}

		create_record(file_path, *sch,rec);
	}

#endif /* this is might need to be canceled or reconsidered */

	if (check_sch == SCHEMA_CT || check_sch == SCHEMA_CT_NT) { 
		/*the input contain one or more BUT NOT ALL, fields in the schema*/
		create_record(file_path, *sch,rec);
 		
		if(fields_num > 1){
			if(sort_input_like_header_schema(check_sch, fields_num, sch, names, &values, types_i) == -1){
				printf("sort_input_like_header_schema failed, %s:%d.\n", F, L - 4);
				free_strs(sch->fields_num, 1, values);
				return -1;
			}
		}
		if(pos){
			if(pos[0] != -1){
				if(fields_num == 1) {
					replace('{',':',values[0]);	
				}else{

					char *field_n =	find_field_to_reset_delim(pos,cpy);
					size_t fd_nl = strlen(field_n);
					int i;
					for(i = 0; i < sch->fields_num; i++){
						if(names[i][0] == '\0') continue;

						if(fd_nl != strlen(names[i])) continue;
						if(strncmp(field_n,names[i], fd_nl) == 0){
							replace('{',':',values[i]);	
							break;
						}
					}
				}
			}
		}

		
		int i = 0;
		int j = 0;
		int found = 0;
		while(names[j][0] == '\0') j++;

		int mv_field_value = fields_num + j;
		for (i = 0; i < sch->fields_num; i++){
			found = 0;
			for (; j < mv_field_value; j++) {
				if (strcmp(sch->fields_name[i], names[j]) == 0) {
					if (!set_field(rec, i, names[j], types_i[j], values[j],1)) {
						printf("set_field failed %s:%d.\n", F, L - 2);
						if (fields_num == 1) 
							free_strs(fields_num,1,values);
						else
							free_strs(sch->fields_num, 1, values);
							
						return -1;
					}

					found++;

				}
			}

			j -= fields_num;
			ui8 bitfield = 0; 
			if (found == 0) rec->field_set[i] = bitfield;

			if(!update && (enforce_constraints(sch,i,found,rec) == -1)){
				free_strs(fields_num, 1, values);
				return -1;
			}
		}

		if (fields_num == 1) 
			free_strs(fields_num,1,values);
		else
			free_strs(sch->fields_num, 1, values);

		return 0;
	}else {

		create_record(file_path, *sch,rec);
		int i;
		for (i = 0; i < fields_num; i++) {
			if (!set_field(rec, i, names[i], types_i[i], values[i],1)) {
				printf("set_field failed %s:%d.\n", F, L - 2);
				free_strs(fields_num, 1, values);
				return -1;
			}

		}
	}

	free_strs(fields_num, 1, values);
	return 0;
}

int parse_input_with_no_type(	char *file_path, 
								int fields_num, 
								char names[][MAX_FIELD_LT], 
								int *types_i, 
								char **values,
								struct Schema *sch, 
								int check_sch,
								struct Record_f *rec,
								int update)
{
	/*equal to parse _d_input_flag 
	 * but we already have the types the values and the names */	

	if (check_sch == SCHEMA_CT || check_sch == SCHEMA_CT_NT) { 
		/*the input contain one or more BUT NOT ALL, fields in the schema*/
		create_record(file_path, *sch,rec);

		int i = 0, j = 0, found = 0;

		/* 
		 * when we order the fields in the case of SCHEMA_CT 
		 * can cause issue because in some cases we have only one fields
		 * and it might be a position 3 in the name array and this can cause wrong results
		 *
		 * the loop 
		 *
		 * 		while(names[j][0] == '\0') j++;
		 *
		 * is fixing the issue 
		 * */

		while(names[j][0] == '\0') j++;			

		int start = j;
		for (i = 0; i < sch->fields_num; i++) {
			found = 0;
			for (j = start; j < sch->fields_num ; j++) {
				if (strcmp(sch->fields_name[i], names[j]) == 0) {
					switch(check_sch){
						case SCHEMA_CT_NT:
							if (!set_field(rec, i, names[j], types_i[j], values[j],1)) {
								printf("set_field failed %s:%d.\n", F, L - 2);
								return -1;
							}
							found++;
							break;
						case SCHEMA_CT:
							if (!set_field(rec, i, names[j], types_i[i], values[j],1)) {
								printf("set_field failed %s:%d.\n", F, L - 2);
								return -1;
							}
							found++;
							break;
						default:
							return -1;
					}
				}
			}

			if(!update && (enforce_constraints(sch,i,found,rec) == -1))
				return -1;
#if 0
			char *number = "0";
			char *fp = "0.0";
			char *str = "null";
			ui8 bitfield = 0; 
			if (found == 0) {
				switch (sch->types[i]){
				case -1:
				case TYPE_INT:
				case TYPE_LONG:
				case TYPE_BYTE:
				case TYPE_PACK:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], number,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_STRING:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], str,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_FLOAT:
				case TYPE_DOUBLE:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], fp,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_ARRAY_INT:
				case TYPE_ARRAY_BYTE:
				case TYPE_ARRAY_LONG:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], number,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_ARRAY_FLOAT:
				case TYPE_ARRAY_DOUBLE:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], fp,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_ARRAY_STRING:
					if (!set_field(rec, i, sch->fields_name[i], sch->types[i], str,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				case TYPE_FILE:
					if (!set_field(rec,i,sch->fields_name[i], sch->types[i],NULL,bitfield))
					{
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				default:
					printf("type no supported! %d.\n", sch->types[i]);
					return -1;
				}
			}
#endif /*i think we can safely remove this code*/
		}

		return 0;
	}else {

		create_record(file_path, *sch,rec);
		int i;
		for (i = 0; i < fields_num; i++) {
			if (!set_field(rec, i, names[i], types_i[i], values[i],1)) {
				printf("set_field failed %s:%d.\n", F, L - 2);
				return -1;
			}
		}
	}

	return 0;
}

int create_header(struct Header_d *hd)
{

	if (hd->sch_d->fields_name[0][0] == '\0') {
		printf("\nschema is NULL.\ncreate header failed, parse.c l %d.\n", __LINE__ - 3);
		return 0;
	}

	hd->id_n = HEADER_ID_SYS;
	hd->version = VS;

	return 1;
}

#if defined(_WIN32)
int write_empty_header(HANDLE fd, struct Header_d *hd)
#else
int write_empty_header(int fd, struct Header_d *hd)
#endif
{
	ui32 id = swap32(hd->id_n); /*converting the endianess*/

#if defined(__linux__) || defined(__APPLE__)
	if (write(fd, &id, sizeof(id)) == -1)
#elif defined(_WIN32)
	DWORD written = 0;
	if(!WriteFile(fd,&id,sizeof(id),&written,NULL))
#endif
	{
		perror("write header id.\n");
		printf("parse.c l %d.\n", __LINE__ - 3);
		return 0;
	}

	ui16 vs = swap16(hd->version);
	
	if (os_write(fd, &vs, sizeof(vs)) == -1)
	{
		perror("write header version.\n");
		printf("parse.c l %d.\n", __LINE__ - 3);
		return 0;
	}

	/* important we need to write 0 field_number when the user creates a file with no data*/
	ui16 fn = swap16((ui16)hd->sch_d->fields_num);
	if (os_write(fd, &fn, sizeof(fn)) == -1)
	{
		perror("writing fields number header.");
		printf("parse.c l %d.\n", __LINE__ - 3);
		return 0;
	}
	return 1;
}


static int write_hd_VS1(ui8 **buf, long *bwritten, struct Schema **sch)
{
	const unsigned long EIGTH_Kib = 1024 * 8;
	unsigned long msize =  *(unsigned long*)(*buf - sizeof(unsigned long));
	ui16 i = 0;
	for (i = 0; i < (*sch)->fields_num; i++) {
		if(*bwritten +(sizeof(ui8)*2 + sizeof(ui32)*2 + 32) > EIGTH_Kib){
			ui8* n = realloc(*buf,msize + EIGTH_Kib);
			if(!n){
				fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__ - 2);
				return -1;
			}

			*buf = n;
			msize += EIGTH_Kib;
		}

		size_t l = strlen((*sch)->fields_name[i]) + 1;
		ui32 l_end = swap32((ui32)l);

		memcpy(&(*buf)[*bwritten],&l_end,sizeof(ui32));
		*bwritten += sizeof(ui32);

		memcpy(&(*buf)[*bwritten],(*sch)->fields_name[i], l);
		*bwritten += l;

		ui32 type = swap32((ui32)(*sch)->types[i]);
		memcpy(&(*buf)[*bwritten],&type,sizeof(ui32));
		*bwritten += sizeof(ui32);

		ui8 d = (*sch)->is_dropped[i];
		memcpy(&(*buf)[*bwritten],&d,sizeof(ui8));
		*bwritten += sizeof(ui8);

		ui8 c = (*sch)->constraints[i];
		memcpy(&(*buf)[*bwritten],&c,sizeof(ui8));
		*bwritten += sizeof(ui8);

		if((*sch)->constraints[i] & CONST_DEFAULT){
			switch((*sch)->types[i]){
				case TYPE_INT:
					{
						int n = *(int*)(*sch)->defaults[i];
						n = swap32(n);
						memcpy(&(*buf)[*bwritten],&n,sizeof(int));
						*bwritten += sizeof(int);
						break;
					}
				case TYPE_LONG:
					{
						long n = *(long*)(*sch)->defaults[i];
						n = swap64(n);
						memcpy(&(*buf)[*bwritten],&n,sizeof(long));
						*bwritten += sizeof(long);
						break;
					}
				case TYPE_BYTE:
					{
						ui8 n = *(ui8*) (*sch)->defaults[i];
						memcpy(&(*buf)[*bwritten],&n,sizeof(ui8));
						*bwritten += sizeof(ui8);
						break;
					}
				case TYPE_KEY:
				case TYPE_DATE:
					{
						ui32 n = *(ui32*)(*sch)->defaults[i];
						n = swap32(n);
						memcpy(&(*buf)[*bwritten],&n,sizeof(ui32));
						*bwritten += sizeof(ui32);
						break;
					}
				case TYPE_FLOAT:
					{
						float f = *(float*)(*sch)->defaults[i];
						ui32 n = htonf(f);
						memcpy(&(*buf)[*bwritten],&n,sizeof(ui32));
						*bwritten += sizeof(ui32);
						break;
					}
				case TYPE_DOUBLE:
					{
						double f = *(double*)(*sch)->defaults[i];
						ui64 n = htond(f);
						memcpy(&(*buf)[*bwritten],&n,sizeof(ui64));
						*bwritten += sizeof(ui64);
						break;
					}
				case TYPE_STRING:
					{ 
						char *c = (char*)(*sch)->defaults[i];

						int s = (int)strlen(c)+1;
						ui32 sz = swap32(s);
						memcpy(&(*buf)[*bwritten],&sz,sizeof(ui32));
						*bwritten += sizeof(ui32);

						memcpy(&(*buf)[*bwritten],c,strlen(c)+1);
						*bwritten += s ;
						break;
					}
					/*TODO: arraysss*/
				default:
					fprintf(stderr,"(%s): type not supported.%s:%d.\n",prog,__FILE__,__LINE__);
					return -1;
			}
		}
	}


	return 0;

}

int write_header(file_t fd, struct Header_d *hd)
{
	if (!hd->sch_d->fields_name || !hd->sch_d->fields_name[0]) {
		printf("\nschema is NULL.\ncreate header failed, %s:%d.\n",__FILE__, __LINE__ - 3);
		return 0;
	}

	if(begin_in_file(fd) == -1){
		fprintf(stderr,"error with file_pointer %s:%d.\n",__FILE__,__LINE__);
		return 0;
	}
	const unsigned long EIGTH_Kib = 1024 * 8;
	unsigned long msize = EIGTH_Kib;
	long bwritten = 0;
	ui8* buf = (ui8*) malloc(msize+1+sizeof(unsigned long));
	if(!buf){
		fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__ - 2);
		return 0;
	}

	*(unsigned long*) buf = msize;
	 buf += sizeof(unsigned long);

	memset(buf,0,msize+1);

	ui32 id = swap32(hd->id_n); /*converting the endianness*/
	memcpy(&buf[bwritten],&id,sizeof(ui32));
	bwritten += sizeof(ui32);

	ui16 vs = swap16(hd->version);
	memcpy(&buf[bwritten],&vs,sizeof(ui16));
	bwritten += sizeof(ui16);

	ui16 fn = swap16((ui16)hd->sch_d->fields_num);
	memcpy(&buf[bwritten],&fn,sizeof(ui16));
	bwritten += sizeof(ui16);

	switch(hd->version){
	case VS:
		if(write_hd_VS1(&buf,&bwritten,&hd->sch_d) == -1){
			buf -= sizeof(unsigned long);
			free(buf);
			return 0;
		}
		break;
	default:
		buf -= sizeof(unsigned long);
		free(buf);
		return 0;
	}
	
	if(os_write(fd,buf,bwritten) == -1){
		fprintf(stderr,"cannot write header, %s:%d.\n",__FILE__,__LINE__ - 1);
		buf -= sizeof(unsigned long);
		free(buf);
		return 0;
	}

	buf -= sizeof(unsigned long);
	free(buf);
	return 1; /* succssed */
}

static int read_hd_V1(ui8 **buf, long *bread, struct Schema **sch)
{
	ui16 field_n = 0;
	memcpy(&field_n,&(*buf)[*bread],sizeof(ui16));
	*bread += sizeof(ui16);
	(*sch)->fields_num = swap16(field_n);

	if ((*sch)->fields_num == 0) {
		printf("no schema in this header.Please check data integrety.\n");
		return -1;
	}


	(*sch)->fields_name = (char**)malloc(sizeof(char*) * (*sch)->fields_num);
	(*sch)->types = (int*)malloc(sizeof(int) * (*sch)->fields_num);
	(*sch)->is_dropped = (ui8*)malloc((*sch)->fields_num);
	(*sch)->constraints = (ui8*)malloc((*sch)->fields_num);
	(*sch)->defaults = (void**)malloc((*sch)->fields_num*sizeof(void*));

	memset((*sch)->fields_name,0,sizeof(char*)*(*sch)->fields_num);
	memset((*sch)->types,-1,sizeof(int) * (*sch)->fields_num);
	memset((*sch)->is_dropped,0,(*sch)->fields_num);
	memset((*sch)->constraints,0,(*sch)->fields_num);
	memset((*sch)->defaults,0,(*sch)->fields_num*sizeof(void*));

	if(!(*sch)->fields_name
			|| !(*sch)->types 
			|| !(*sch)->is_dropped
			|| !(*sch)->constraints
			|| !(*sch)->defaults){
		fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-4);
		if((*sch)->fields_name) 
			free((*sch)->fields_name);
		if((*sch)->types)
			free((*sch)->types);
		if((*sch)->is_dropped)
			free((*sch)->is_dropped);
		if((*sch)->constraints)
			free((*sch)->constraints);
		if((*sch)->defaults)
			free((*sch)->defaults);
		return -1;
	} 

	ui16 i;
	for (i = 0; i < (*sch)->fields_num; i++) {
		ui32 l_end = 0;
		memcpy(&l_end,&(*buf)[*bread],sizeof(ui32));
		*bread += sizeof(ui32);

		size_t l = (size_t)swap32(l_end);

		(*sch)->fields_name[i] = (char*)malloc(l);
		if(!(*sch)->fields_name[i]){
			fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
			free_schema(*sch);
			return -1;
		} 

		memset((*sch)->fields_name[i],0,l);
		memcpy((*sch)->fields_name[i],&(*buf)[*bread],l);
		*bread += l;

		ui32 type = 0;
		memcpy(&type,&(*buf)[*bread],sizeof(type));
		*bread += sizeof(type);

		(*sch)->types[i] = swap32(type);

		memcpy(&(*sch)->is_dropped[i],&(*buf)[*bread],sizeof(ui8));
		*bread += sizeof(ui8);

		memcpy(&(*sch)->constraints[i],&(*buf)[*bread],sizeof(ui8));
		*bread += sizeof(ui8);

		if((*sch)->constraints[i] & CONST_DEFAULT){
			switch((*sch)->types[i]){
			case TYPE_INT:
			{
				int n = 0;
				memcpy(&n,&(*buf)[*bread],sizeof(int));
				*bread += sizeof(int);

				n = swap32(n);
				(*sch)->defaults[i] = (void*) malloc(sizeof(int));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(int*)(*sch)->defaults[i] = n;
				break;
			}
			case TYPE_LONG:
			{
				long n = 0;
				memcpy(&n,&(*buf)[*bread],sizeof(long));
				*bread += sizeof(long);

				n = swap64(n);

				(*sch)->defaults[i] = (void*) malloc(sizeof(long));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(long*)(*sch)->defaults[i] = n;
				break;
			}
			case TYPE_BYTE:
			{
				ui8 n = 0;
				memcpy(&n,&(*buf)[*bread],sizeof(ui8));
				*bread += sizeof(ui8);

				(*sch)->defaults[i] = (void*) malloc(sizeof(ui8));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(ui8*)(*sch)->defaults[i] = n;
				break;

			}
			case TYPE_KEY:
			case TYPE_DATE:
			{
				ui32 n = 0;
				memcpy(&n,&(*buf)[*bread],sizeof(ui32));
				*bread += sizeof(ui32);

				n = swap32(n);
				(*sch)->defaults[i] = (void*) malloc(sizeof(ui32));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(ui32*)(*sch)->defaults[i] = n;
				break;

			}
			case TYPE_FLOAT:
			{
				ui32 fne = 0;
				memcpy(&fne,&(*buf)[*bread],sizeof(ui32));
				*bread += sizeof(ui32);

				float f = ntohf(fne);

				(*sch)->defaults[i] = (void*) malloc(sizeof(float));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(float*)(*sch)->defaults[i] = f;
				break;
			}
			case TYPE_DOUBLE:
			{
				ui32 f = 0;
				memcpy(&f,&(*buf)[*bread],sizeof(ui32));
				*bread += sizeof(ui32);

				double d = ntohd(f);

				(*sch)->defaults[i] = (void*) malloc(sizeof(double));
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"malloc failed %s:%d.\n",__FILE__,__LINE__-1);
					free_schema(*sch);
					return -1;
				}
				*(double*)(*sch)->defaults[i] = d;
				break;
			}
			case TYPE_STRING:
			{ 
				ui32 sz = 0;
				memcpy(&sz,&(*buf)[*bread],sizeof(ui32));
				*bread += sizeof(ui32);
				
				sz = swap32(sz);
				char str[sz];	
				str[sz-1] = '\0';

				memcpy(str,&(*buf)[*bread],sz);
				*bread += sz;

				(*sch)->defaults[i] = (void*)duplicate_str(str);
				if(!(*sch)->defaults[i]){
					fprintf(stderr,"duplicate_str failed %s:%d.\n",__FILE__,__LINE__-2);
					free_schema(*sch);
					return -1;
				}
				break;
			}
			/*TODO: arraysss*/
			default:
				fprintf(stderr,"(%s): type not supported.%s:%d.\n",prog,__FILE__,__LINE__);
				return -1;
			}
		}

		if((*sch)->constraints[i] & CONST_UNIQUE){
			(*sch)->defaults[i] = malloc(sizeof(int));
			if(!(*sch)->defaults[i]){
				fprintf(stderr,"malloc() failed %s:%d.\n",__FILE__,__LINE__-2);
				free_schema(*sch);
				return -1;
			}
			*(int*)(*sch)->defaults[i] = 1;/*index use by the system to enforce unique*/ 
		}
	}
	return 0;
}

int read_header(file_t fd, struct Header_d *hd)
{
#if defined(_WIN32)
	file_offset msize = get_file_size(fd);
#else
	file_offset msize = get_file_size(fd, NULL);
#endif

	if(msize <= 0)
		return 0;

	long bread = 0;
	ui8* buf = (ui8*) malloc(msize);
	if(!buf){
		fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__ - 2);
		return 0;
	}

	if (os_read(fd, buf, msize) == -1){
		perror("reading header.\n");
		printf("parse.c %s:%d.\n",__FILE__, __LINE__ - 2);
		free(buf);
		return 0;
	}

	unsigned int id = 0;
	memcpy(&id,&buf[bread],sizeof(id));
	bread += sizeof(id);
	
	hd->id_n = swap32(id); /*changing the bytes to host endianess*/
	if (hd->id_n != HEADER_ID_SYS) {
		printf("this is not a db file.\n");
		free(buf);
		return 0;
	}

	unsigned short vs = 0;
	memcpy(&vs,&buf[bread],sizeof(vs));
	bread += sizeof(vs);

	hd->version = swap16(vs);

	switch(hd->version){
	case VS:
		if(read_hd_V1(&buf,&bread,&hd->sch_d) == -1){
			free(buf);
			return 0;
		}
		break;
	default:
		printf("this file was edited from a different software.\n");
		free(buf);
		return 0;
	}
		

	hd->sch_d->has_unique = has_constrain_unique;
	free(buf);
	return 1; /* successed */
}

unsigned char ck_input_schema_fields(char names[][MAX_FIELD_LT], int *types_i, struct Header_d *hd)
{
	int fields_eq = 0;
	int types_eq = 0;

	char *copy_sch[hd->sch_d->fields_num];
	memset(copy_sch,0,hd->sch_d->fields_num*sizeof(char*));

	int types_cp[hd->sch_d->fields_num];
	memset(types_cp,-1,hd->sch_d->fields_num);

	char *names_array_for_sort[MAX_FIELD_NR];
	memset(names_array_for_sort,0,MAX_FIELD_NR*sizeof(char*));

	ui16 i;
	for (i = 0; i < hd->sch_d->fields_num; i++) {
		copy_sch[i] = hd->sch_d->fields_name[i];
		types_cp[i] = hd->sch_d->types[i];
		names_array_for_sort[i] = names[i];
	}

	/*sorting the name and type arrays  */
	if (hd->sch_d->fields_num > 1) {
		quick_sort(types_i, 0, hd->sch_d->fields_num - 1);
		quick_sort(types_cp, 0, hd->sch_d->fields_num - 1);
		quick_sort_str(names_array_for_sort, 0, hd->sch_d->fields_num - 1);
		quick_sort_str(copy_sch, 0, hd->sch_d->fields_num - 1);
	}

	

	int j;
	for (i = 0, j = 0; i < hd->sch_d->fields_num; i++, j++) {
		if (strncmp(copy_sch[i], names_array_for_sort[j],strlen(names_array_for_sort[i])) == 0)
			fields_eq++;

		if ((int)types_cp[i] == types_i[j]){
			types_eq++;
		}else{
			if(hd->sch_d->types[i] == -1){
				hd->sch_d->types[i] = types_i[j];
				types_eq++;
			}		
		}
	}

	if (fields_eq != hd->sch_d->fields_num || types_eq != hd->sch_d->fields_num) {
		printf("Schema different than file definition.\n");
		return SCHEMA_ERR;
	}

	return SCHEMA_EQ;
}

char **extract_fields_value_types_from_input(char *buffer, char names[][MAX_FILED_LT], int *types_i, int *count)
{
	

	*count = get_names_with_no_type_skip_value(buffer,names);
	char **values = get_values_with_no_types(buffer,*count);
	if(!values) return NULL;

	int i;
	for(i = 0; i < *count; i++){
		if((types_i[i] = assign_type(values[i])) == -1){	
			/* free the values and return NULL*/		
			free_strs(*count,1,values);
			return NULL;	
		}
	}
	
	return values;
}

int check_schema_with_no_types(char names[][MAX_FILED_LT], struct Header_d hd, char **sorted_names){
	int fields_eq = 0;

	char *copy_sch[MAX_FILED_LT];
	memset(copy_sch,0,MAX_FILED_LT*sizeof(char*));

	ui16 i;
	for (i = 0; i < hd.sch_d->fields_num; i++) {
		copy_sch[i] = hd.sch_d->fields_name[i];
		sorted_names[i] = names[i];
	}

	/*sorting the name arrays  */
	if (hd.sch_d->fields_num > 1) {
		quick_sort_str(sorted_names, 0, hd.sch_d->fields_num - 1);
		quick_sort_str(copy_sch, 0, hd.sch_d->fields_num - 1);
	}

	
	int j;
	for (i = 0, j = 0; i < hd.sch_d->fields_num; i++, j++) {
		if (strncmp(copy_sch[i], sorted_names[j],strlen(sorted_names[j])) == 0)
			fields_eq++;

	}

	if (fields_eq != hd.sch_d->fields_num) { 
		printf("Schema different than file definition.\n");
		return SCHEMA_ERR;
	
	}

	return SCHEMA_EQ;
	

}

unsigned char check_schema(int fields_n, char *buffer, struct Header_d *hd)
{
	int types_i[MAX_FIELD_NR];
	memset(types_i,-1,sizeof(int) * MAX_FIELD_NR);

	char names[MAX_FIELD_NR][MAX_FIELD_LT];
	memset(names,0,MAX_FIELD_LT*MAX_FIELD_NR*sizeof(char));

	get_value_types(buffer, fields_n, 3,types_i);
	get_fileds_name(buffer, fields_n, 3,names);

	if (hd->sch_d->fields_num == fields_n) {
		unsigned char ck_rst = ck_input_schema_fields(names, types_i, hd);
		switch (ck_rst) {
		case SCHEMA_ERR:
			return SCHEMA_ERR;
		case SCHEMA_EQ:
			return SCHEMA_EQ;
		default:
			printf("check on Schema failed.\n");
			return 0;
		}
	} else if (fields_n > hd->sch_d->fields_num) {
		/* case where the header needs to be updated */
		if (((fields_n - hd->sch_d->fields_num) + hd->sch_d->fields_num) > MAX_FIELD_NR) {
			printf("cannot add the new fileds, limit is %d fields.\n", MAX_FIELD_NR);
			return SCHEMA_ERR;
		}
		unsigned char ck_rst = ck_input_schema_fields(names, types_i, hd);

		switch (ck_rst){
		case SCHEMA_ERR:
			return SCHEMA_ERR;
		case SCHEMA_EQ:
			return SCHEMA_NW;
		default:
			return 0;
		}
	} else if (hd->sch_d->fields_num > fields_n){ 
		/* case where the fileds are less than the schema 
		if they are in the schema and the types are correct, return SCHEMA_CT
		create a record with only the values provided and set the other values to 0;*/

		int ck_rst = ck_schema_contain_input(names, types_i, hd, fields_n);

		switch (ck_rst) {
		case SCHEMA_ERR:
			return SCHEMA_ERR;
		case SCHEMA_CT:
			return SCHEMA_CT;
		case SCHEMA_CT_NT:
			return SCHEMA_CT_NT;
		default:
			return 0;
		}
	}

	/*this is unreachable*/
	return 1;
}

int sort_input_like_header_schema(int schema_tp, 
					int fields_num, 
					struct Schema *sch, 
					char names[][MAX_FIELD_LT], 
					char ***values, 
					int *types_i)
{

	if(schema_tp == SCHEMA_CT || schema_tp == SCHEMA_CT_NT){
		/*order the data with missing fields*/
		char **n_v = (char**)realloc(*values,sch->fields_num * sizeof(char*));
		if(!n_v){
			fprintf(stderr,"realloc() failed %s:%d.\n",__FILE__,__LINE__-2);
			return -1;
		}

		*values = n_v;

		int value_pos[sch->fields_num];
		memset(value_pos,-1,sizeof(int) * sch->fields_num);

		int i,j;
		for (i = 0; i < fields_num; i++) {
			for (j = 0; j < sch->fields_num; j++) {

				if(strlen(names[i]) != strlen(sch->fields_name[j])) continue;

				if (strncmp(names[i], sch->fields_name[j], strlen(names[i])) == 0){
					value_pos[i] = j;
					break;
				}
			}
		}

		char **temp_val = (char**)malloc(sch->fields_num * sizeof(char *));
		if (!temp_val) {
			fprintf(stderr,"(%s): ask_m failed, %s:%d.\n",prog,__FILE__,__LINE__-2);
			return 0;
		}

		memset(temp_val,0,sch->fields_num * sizeof(char *));

		char temp_name[MAX_FIELD_NR][MAX_FIELD_LT]; 
		memset(temp_name,0,MAX_FIELD_LT*MAX_FIELD_NR);

		int temp_types[MAX_FIELD_NR];
		memset(temp_types,-1,sizeof(int)*MAX_FIELD_NR);

		for (i = 0; i < sch->fields_num; i++) {
			if(value_pos[i] == -1) continue;

			if(!(*values)[i]) continue;
			temp_val[value_pos[i]] = (*values)[i];
			strncpy(temp_name[value_pos[i]],names[i],strlen(names[i]));
			temp_types[value_pos[i]] = types_i[i];
		}

		/*this memset is crucial to maintain the data integrety*/
		memset(names,0,MAX_FIELD_NR*MAX_FIELD_LT);

		for (i = 0; i < sch->fields_num; i++) {
			(*values)[i] = temp_val[i];
			strncpy(names[i],temp_name[i],strlen(temp_name[i]));
			types_i[i] = temp_types[i];
		}

		free(temp_val);
		return 1;

	}

	/* order the data when SCHEM_EQ*/
	int f_n = schema_tp == SCHEMA_NW ? sch->fields_num : fields_num;
	int value_pos[f_n];
	memset(value_pos,-1,sizeof(int) * f_n);

	int i,j;
	for (i = 0; i < f_n; i++) {
		for (j = 0; j < sch->fields_num; j++) {
			if (strncmp(names[i], sch->fields_name[j], strlen(names[i])) == 0){
				value_pos[i] = j;
				break;
			}
		}
		/*if the names in the input are worng we return*/
		if(value_pos[i] == -1 && schema_tp == SCHEMA_EQ) return 0;
	}



	char **temp_val = (char**)malloc(fields_num * sizeof(char *));
	if (!temp_val) {
		fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		return 0;
	}

	memset(temp_val,0,sizeof(char*)*fields_num);
	char temp_name[MAX_FIELD_NR][MAX_FIELD_LT] = {0}; 

	int temp_types[MAX_FIELD_NR];
	memset(temp_types,-1,sizeof(int)*MAX_FIELD_NR);

	for (i = 0; i < f_n; i++) {
		if(value_pos[i] == -1) continue;
		temp_val[value_pos[i]] = (*values)[i];
		strncpy(temp_name[value_pos[i]],names[i],strlen(names[i]));
		temp_types[value_pos[i]] = types_i[i];
	}

	/*this memset is crucial to maintain the data integrety*/
	memset(names,0,MAX_FIELD_NR*MAX_FIELD_LT);
	for (i = 0; i < f_n; i++) {
		if(value_pos[i] == -1) continue;
		(*values)[i] = temp_val[i];
		strncpy(names[i],temp_name[i],strlen(temp_name[i]));
		types_i[i] = temp_types[i];
	}

	free(temp_val);
	return 1;
}

unsigned char ck_schema_contain_input(char names[][MAX_FIELD_LT], int *types_i, struct Header_d *hd, int fields_num)
{
	int found_f = 0;
	int result = SCHEMA_CT;

	int i;
	ui16 j;
	for (i = 0; i < fields_num; i++){
		for (j = 0; j < hd->sch_d->fields_num; j++){
			if (strncmp(names[i], hd->sch_d->fields_name[j],strlen(names[i])) == 0){
				found_f++;
				if(hd->sch_d->types[j] == -1) {
					hd->sch_d->types[j] = (int)types_i[i];
					result = SCHEMA_CT_NT;
				}

				if ((int)types_i[i] != hd->sch_d->types[j]){
					printf("Schema different than file definition.\n");
					return SCHEMA_ERR;
				}
			}
		}
	}
	if (found_f == fields_num) return result;

	printf("Schema different than file definition.\n");
	return SCHEMA_ERR;
}

unsigned char add_fields_to_schema(int mode, int fields_num, char *buffer, struct Schema *sch)
{
	char names[MAX_FIELD_NR][MAX_FIELD_LT];
	memset(names,0,MAX_FIELD_LT*MAX_FIELD_NR*sizeof(char));

	int types_i[MAX_FIELD_NR];
	memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);

	char **def_values = NULL;
	int *constraints = NULL;
	switch(mode){
	case TYPE_WR:
		if(get_constrains(buffer,fields_num,&constraints,&def_values) == -1){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}
		if(get_fileds_name(buffer, fields_num, 2, names) == -1) return 0;

		if(!is_input_correct(names,fields_num)){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		if(get_value_types(buffer, fields_num, 2,types_i) == -1) return 0;
		break;
	case NO_TYPE_WR:
		if(get_constrains(buffer,0,&constraints,&def_values) == -1){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		if((fields_num=get_fields_name_with_no_type(buffer, names)) == 0 ) return 0;

		if(!is_input_correct(names,fields_num)){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		break;
	case HYB_WR:
		if(get_constrains(buffer,0,&constraints,&def_values) == -1){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		if((fields_num = get_name_types_hybrid(mode,buffer,names,types_i)) == -1) return 0;
		
		if(!is_input_correct(names,fields_num)){
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		break;
	default:
		array_free(constraints);
		array_free(def_values);
		return 0;
	}

	ui16 i;
	int j,count = 0;
	for (i = 0; i < sch->fields_num; i++) {
		for (j = 0; j < fields_num; j++) {
			if(names[j][0] == '\0')
				continue;

			int name_size = (int)strlen(names[j]);
			if((int)strlen(sch->fields_name[i]) != name_size)
					continue;

			if (strncmp(sch->fields_name[i], names[j],name_size) == 0) {
				if(sch->is_dropped[i]){
					sch->is_dropped[i] = 0;
					if(fields_num == 1){
						array_free(def_values);
						array_free(constraints);
						return 1;
					}
				}
				names[j][0] = '\0';
				count++;
				continue;
			} 
		}
	}

	if (count == fields_num) {
		printf("fields already exist.\n");
		array_free(def_values);
		array_free(constraints);
		return 0;
	}

	assert((fields_num - count) > 0);
	int number_new_field = fields_num - count;
	char **new_fields = (char**) realloc(sch->fields_name, (sch->fields_num + number_new_field)*sizeof(char*));
	if(!new_fields){
		fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__-2);
		array_free(constraints);
		array_free(def_values);
		return 0;
	}

	sch->fields_name = new_fields;
	memset(&sch->fields_name[sch->fields_num],0,number_new_field);

	int *types = (int*) realloc(sch->types,
			(sch->fields_num + number_new_field) * sizeof(int));
	if(!types){
		fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		array_free(constraints);
		array_free(def_values);
		return 0;
	}
	sch->types = types;
	memset(&sch->types[sch->fields_num],-1,number_new_field);

	ui8 *nd = (ui8*) realloc(sch->is_dropped,(sch->fields_num + number_new_field));
	if(!nd){
		fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		array_free(constraints);
		array_free(def_values);
		return 0;
	}

	sch->is_dropped = nd;
	memset(&sch->is_dropped[sch->fields_num],0,number_new_field);
	ui8* nc = realloc(sch->constraints,sch->fields_num + number_new_field);
	if(!nc){
		fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		array_free(constraints);
		array_free(def_values);
		return 0;
	}
	sch->constraints  = nc;
	memset(&sch->constraints[sch->fields_num],0,number_new_field);

	void** ndf = realloc(sch->defaults,(sch->fields_num+number_new_field) * sizeof(void*));
	if(!ndf){
		fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
		array_free(constraints);
		array_free(def_values);
		return 0;
	}
	sch->defaults = ndf;
	memset(&sch->defaults[sch->fields_num],0,number_new_field * sizeof(void*));

	/* check which fields are already in the schema if any */
	for (i = 0, j = 0; i < fields_num; i++) {
		if(names[i][0] == '\0')
			continue;

		sch->fields_name[sch->fields_num] = (char*) malloc(strlen(names[i])+1);
		if(!sch->fields_name[sch->fields_num]){
			fprintf(stderr," malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
			array_free(constraints);
			array_free(def_values);
			return 0;
		}

		memset(sch->fields_name[sch->fields_num],0,strlen(names[i])+1);
		strncpy(sch->fields_name[sch->fields_num],names[i],strlen(names[i]));
		sch->types[sch->fields_num] = types_i[i];
		sch->is_dropped[sch->fields_num] = 0;
		if(constraints){
			sch->constraints[sch->fields_num] = constraints[j];
			if(constraints[j] & CONST_DEFAULT){
				/* TODO: assaign def*/
			}
			j++;
		}
		sch->fields_num++;
	}

	array_free(constraints);
	array_free(def_values);
	return 1;
}

int create_file_definition_with_no_value(int mode, int fields_num, char *buffer, struct Schema *sch)
{


	char names[MAX_FIELD_NR][MAX_FIELD_LT];
	memset(names,0,MAX_FIELD_LT*MAX_FIELD_NR);
	int types_i[MAX_FIELD_NR];
	memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);

	char **cnts_value = NULL;
	int *cnstr = NULL;
	switch(mode){
		case NO_TYPE_DF:
			{
				if(get_constrains(buffer,0,&cnstr,&cnts_value) == -1){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}

				if((fields_num = get_fields_name_with_no_type(buffer,names)) == -1) return 0;
				if(!is_input_correct(names,fields_num)){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}

				break;
			}
		case TYPE_DF:
			{

				if(fields_num == 0)
					fields_num = count_fields(buffer,NULL);

				if(get_constrains(buffer,fields_num,&cnstr,&cnts_value) == -1){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}

				get_fileds_name(buffer, fields_num, 2,names);
				if(get_value_types(buffer, fields_num, 2,types_i) == -1) {
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}

				if(!is_input_correct(names,fields_num)){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}

				break;
			}
		case HYB_DF:
			{
				if(get_constrains(buffer,0,&cnstr,&cnts_value) == -1){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}


				if((fields_num = get_name_types_hybrid(mode,buffer,names,types_i)) == -1) return 0;
				if(!is_input_correct(names,fields_num)){
					array_free(cnstr);
					array_free(cnts_value);
					return 0;
				}
				break;
			}
		default:
			array_free(cnstr);
			array_free(cnts_value);
			return 0;
	}



	if (!check_fields_integrity(names, fields_num)) {
		printf("invalid input, one or more fields have the same name, or the value is missing\n");
		printf("input syntax: fieldName:TYPE:value\n");
		array_free(cnstr);
		array_free(cnts_value);
		return 0;
	}

	/*parse the constraints*/
	if(set_schema(names, types_i, sch,fields_num,cnstr,cnts_value) == -1){
		fprintf(stderr,"set_schema() failed, %s:%d.\n",__FILE__,__LINE__-1);
		array_free(cnstr);
		array_free(cnts_value);
		return 0;
	}

	array_free(cnts_value);
	array_free(cnstr);
	return 1; /*schema creation succssed*/
}

static int schema_check_type(int count,int mode,struct Schema *sch,
		char names[][MAX_FILED_LT],
		int *types_i,
		char ***values,
		int option)
{
	int name_check = 0;
	char *decimal = ".00";
	switch(mode){
		case SCHEMA_EQ:
			{
				int i;
		for(i = 0; i < sch->fields_num; i++){
			if(strlen(sch->fields_name[i]) != strlen(names[i])) continue;
			if(strncmp(sch->fields_name[i],names[i],strlen(sch->fields_name[i])) == 0) name_check++;

			if(sch->types[i] != types_i[i])	{
				if(is_number_type(sch->types[i])){
					if(sch->types[i] == TYPE_DOUBLE) {
						types_i[i] = TYPE_DOUBLE;
						size_t vs = strlen((*values)[i]);
						size_t ds = strlen(decimal);
						size_t s = vs + ds + 1; 
						char cpy[s];
						memset(cpy,0,s);
						strncpy(cpy,(*values)[i],vs);
						strncat(cpy,decimal,ds);
						free((*values)[i]);
						(*values)[i] = duplicate_str(cpy);
						if(!(*values)[i]){
							fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F,L-2);
							return -1;
						}
						continue;
					}
					types_i[i] = sch->types[i];
					continue;
				}else if(is_number_array(sch->types[i])){
					switch(sch->types[i]){
					case TYPE_SET_BYTE:
					case TYPE_SET_INT:
					case TYPE_SET_LONG:
					case TYPE_ARRAY_BYTE:
					case TYPE_ARRAY_INT:
					case TYPE_ARRAY_LONG:
					{
						if((sch->types[i] == TYPE_ARRAY_LONG || sch->types[i] == TYPE_SET_LONG)
								&& types_i[i] == TYPE_LONG 
								&& (option == AAR || option == FRC)){
							types_i[i] = sch->types[i];
							break;
						}
						if((sch->types[i] == TYPE_ARRAY_INT || sch->types[i] == TYPE_SET_INT)
								&& types_i[i] == TYPE_INT 
								&& (option == AAR || option == FRC)){
							types_i[i] = sch->types[i];
							break;
						}
						if((sch->types[i] == TYPE_ARRAY_BYTE || sch->types[i] == TYPE_SET_BYTE)
								&& types_i[i] == TYPE_BYTE 
								&& (option == AAR || option == FRC)){
							types_i[i] = sch->types[i];
							break;
						}
						char *p = NULL;
						int index = 0;
						while((p = strstr((*values)[i],","))){
							*p ='@';
							if(index == 0){
								index = p - (*values)[i];
								size_t sz = index == 1 ? (index + 1) : index;	
								char num[sz];
								memset(num,0,sz);
								strncpy(num,(*values)[i],sz-1);
								if(!is_integer(num)) return -1;
								int result = 0;
								if((result = is_number_in_limits(num)) == 0)
									return -1;
								if(sch->types[i] == TYPE_ARRAY_INT 
										|| sch->types[i] == TYPE_SET_INT){
									if(result == IN_INT) continue;
									return -1;
								}else if(sch->types[i] == TYPE_ARRAY_LONG 
										|| sch->types[i] == TYPE_SET_LONG){
									if(result == IN_INT || result == IN_LONG)
										continue;
									return -1;
								}
								continue;
							}
							int start = index;
							index = p - (*values)[i];
							size_t sz = index-start;
							if(sz == 1) 
								sz++;

							char num[sz];
							memset(num,0,sz);
							strncpy(num,&(*values)[i][start+1],sz-1);
							if(!is_integer(num))
								return -1;

							int result = 0;
							if((result = is_number_in_limits(num)) == 0)
								return -1;
							if(sch->types[i] == TYPE_ARRAY_INT
										|| sch->types[i] == TYPE_SET_INT){
								if(result == IN_INT) continue;
								return -1;
							}else if(sch->types[i] == TYPE_ARRAY_LONG
										|| sch->types[i] == TYPE_SET_LONG){
								if(result == IN_INT || result == IN_LONG)
									continue;

								return -1;
							}else if(sch->types[i] == TYPE_ARRAY_BYTE 
									|| sch->types[i] == TYPE_SET_BYTE){
								/*
								 * type byte is a number stored in 
								 * exactly 1 byte(unsigned) so it can't be bigger 
								 * than 2^7 * 2  
								 * and it can't be negative
								 *  
								 * */
								errno = 0;
								long l = string_to_long(num);
								if(errno == 0){ 
									if(l > UCHAR_MAX)return -1;
									if(l < 0)return -1;
								}
							}
						}
						types_i[i] = sch->types[i];
						replace('@',',',(*values)[i]);
						break;
					}
					case TYPE_SET_DOUBLE:
					case TYPE_ARRAY_DOUBLE:
					{
						if(types_i[i] == TYPE_DOUBLE && (option == AAR || option == FRC)){
							types_i[i] = sch->types[i];
							break;
						}
						return -1;
					}
					case TYPE_SET_FLOAT:
					case TYPE_ARRAY_FLOAT:
					{
						if(types_i[i] == TYPE_FLOAT && (option == AAR || option == FRC)){
							types_i[i] = sch->types[i];
							break;
						}
						return -1;
					}
					default:
						return -1;
					}
					continue;
				}

				types_i[i] = sch->types[i];
				continue;
			}
		}
		if(name_check != sch->fields_num) return -1;
		break;
	}
	case SCHEMA_CT:
	{
		int j;
		for(j = 0; j <sch->fields_num;j++){
			if(strlen(sch->fields_name[j]) != strlen(names[j])) continue;

			if(strncmp(sch->fields_name[j],names[j],strlen(sch->fields_name[j])) == 0){
				name_check++;
				if(sch->types[j] != types_i[j] && sch->types[j] != -1){
					if(is_number_type(sch->types[j])){
						if(sch->types[j] == TYPE_DOUBLE) {
							types_i[j] = TYPE_DOUBLE;
							size_t vs = strlen((*values)[j]);
							size_t ds = strlen(decimal);
							size_t s = vs + ds + 1; 
							char cpy[s];
							memset(cpy,0,s);
							strncpy(cpy,(*values)[j],vs);
							strncat(cpy,decimal,ds);
							free((*values)[j]);
							(*values)[j] = duplicate_str(cpy);
							if(!(*values)[j]){
								fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F,L-2);
								return -1;
							}
							continue;
						}else if(sch->types[j] == TYPE_BYTE){
							errno = 0;
							long l = string_to_long((*values)[j]);
							if(errno == EINVAL){
								fprintf(stderr,"value: '%s' is not valid for TYPE_BYTE\n",(*values)[j]);
								return -1;
							}

							if(l <= UCHAR_MAX){
								types_i[j] = sch->types[j];
								continue;
							} 
							
							fprintf(stderr,"value: '%s' is not valid for TYPE_BYTE\n",(*values)[j]);
							return -1;

						}
					}else if(is_number_array(sch->types[j])){
						switch(sch->types[j]){
						case TYPE_SET_BYTE:
						case TYPE_SET_INT:
						case TYPE_SET_LONG:
						case TYPE_ARRAY_INT:
						case TYPE_ARRAY_BYTE:
						case TYPE_ARRAY_LONG:
						{
							if((sch->types[j] == TYPE_ARRAY_LONG || sch->types[j] == TYPE_SET_LONG)
									&& types_i[j] == TYPE_LONG 
									&& (option == AAR || option == FRC)){
								types_i[j] = sch->types[j];
								break;
							}
							if((sch->types[j] == TYPE_ARRAY_INT || sch->types[j] == TYPE_SET_INT)
									&& types_i[j] == TYPE_INT 
									&& (option == AAR || option == FRC)){
								types_i[j] = sch->types[j];
								break;
							}
							if((sch->types[j] == TYPE_ARRAY_BYTE || sch->types[j] == TYPE_SET_BYTE)
									&& types_i[j] == TYPE_BYTE
									&& (option == AAR || option == FRC)){
								types_i[j] = sch->types[j];
								break;
							}

							char *p = NULL;
							int index = 0;
							ui8 is_array = 0;
							while((p = strstr((*values)[j],","))){
								is_array = 1;
								*p ='@';
								if(index == 0){
									index = p - (*values)[j];
									size_t sz = index == 1 ? (index + 1) : index;	
									char num[sz];
									memset(num,0,sz);
									strncpy(num,(*values)[j],sz-1);
									if(!is_integer(num)) return -1;
									int result = 0;
									if((result = is_number_in_limits(num)) == 0)
										return -1;
									if(sch->types[j] == TYPE_ARRAY_INT || sch->types[j] == TYPE_SET_INT){
										if(result == IN_INT) continue;
										return -1;
									}else if(sch->types[j] == TYPE_ARRAY_LONG || sch->types[j] == TYPE_SET_LONG){
										if(result == IN_INT || result == IN_LONG)
											continue;

										return -1;
									}
									continue;
								}
								int start = index;
								index = p - (*values)[j];
								size_t sz = index-start;
								if(sz == 1) sz++;
								char num[sz];
								memset(num,0,sz);
								strncpy(num,&(*values)[j][start+1],sz-1);
								if(!is_integer(num)) return -1;
								int result = 0;
								if((result = is_number_in_limits(num)) == 0)
									return -1;
								if(sch->types[j] == TYPE_ARRAY_INT || sch->types[j] == TYPE_SET_INT){
									if(result == IN_INT) continue;
									return -1;
								}else if(sch->types[j] == TYPE_ARRAY_LONG || sch->types[j] == TYPE_SET_LONG){
									if(result == IN_INT || result == IN_LONG)
										continue;

									return -1;
								}else if(sch->types[j] == TYPE_ARRAY_BYTE || sch->types[j] == TYPE_SET_BYTE){
									errno = 0;
									long l = string_to_long(num);
									if(errno == 0){ 
										if(l > UCHAR_MAX)return -1;
										if(l < 0)return -1;
									}
									continue;
								}
							}

							if(!is_array){ 
								if(!p){
									types_i[j] = sch->types[j];
									break;
								}
								return -1;
							}
							types_i[j] = sch->types[j];
							replace('@',',',(*values)[j]);
							break;
						}

						case TYPE_SET_DOUBLE:
						case TYPE_ARRAY_DOUBLE:
						{
							if(types_i[j] == TYPE_DOUBLE && 
									(option == AAR || option == FRC)){
								types_i[j] = sch->types[j];
								break;
							}
							return -1;
						}
						case TYPE_SET_FLOAT:
						case TYPE_ARRAY_FLOAT:
						{
							if(types_i[j] == TYPE_FLOAT && 
									(option == AAR || option == FRC)){
								types_i[j] = sch->types[j];
								break;
							}
							return -1;
						}
						default:
						break;
						}
						continue;
					}
					types_i[j] = sch->types[j];
					continue;
				}
			}
		}
		if(name_check != count) return -1;
		break;
	}
	default:
	fprintf(stderr,"schema not supported %s:%d.\n",F,L);
	return -1;
	}
	return 0;
}

int schema_eq_assign_type(struct Schema *sch, char names[][MAX_FIELD_LT],int *types_i)
{
	int count = 0;
	int i,j;
	for(i = 0; i < sch->fields_num; i++){
		for(j = 0; j < sch->fields_num; j++){
			if(strncmp(names[j],sch->fields_name[i],strlen(names[j])) != 0) continue;

			if(sch->types[i] == -1){	
				sch->types[i] = types_i[j];
				count++;
			}
			if(sch->types[i] != types_i[i]){
				types_i[i] = sch->types[i];	
			}
		}
	}

	return count >0;
}
static int check_double_compatibility(struct Schema *sch, char ***values)
{
	char *decimal = ".00";
	int i;
	for(i = 0; i < sch->fields_num; i++){ 
		if(sch->types[i] == TYPE_DOUBLE){
			if(!(*values)[i]) continue;
			if(is_floaintg_point((*values)[i])) continue;		

			size_t vs = strlen((*values)[i]);
			size_t ds = strlen(decimal);
			size_t s = vs + ds + 1; 
			char cpy[s];
			memset(cpy,0,s);
			strncpy(cpy,(*values)[i],vs);
			strncat(cpy,decimal,ds);
			free((*values)[i]);
			(*values)[i] = duplicate_str(cpy);
			if(!(*values)[i]){
				fprintf(stderr,"duplicate_str() failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}
	}	
	return 0;
}
int schema_ct_assign_type(struct Schema *sch, char names[][MAX_FIELD_LT],int *types_i,int count)
{
	int counter = 0;
	int name_check = 0;
	int i,j;
	for(i = 0; i < count; i++){
		for(j = 0; j < sch->fields_num; j++){
			size_t l = strlen(sch->fields_name[j]);
			if(strlen(names[i]) != l) continue;
			if(strncmp(names[i],sch->fields_name[j],l) != 0) continue;

			name_check++;
			if(sch->types[j] == -1){ 
				counter = SCHEMA_CT_NT;
				sch->types[j] = types_i[i];	
			}else{
				if(sch->types[j] != types_i[i]) types_i[i] = sch->types[j];

				if(counter != SCHEMA_CT_NT)counter++;
			}
		}

	}

	if(name_check != count ) return 0; 
	if(counter != SCHEMA_CT_NT) return SCHEMA_CT;
	return counter;
}

int schema_nw_assyign_type(struct Schema *sch_d, char names[][MAX_FIELD_LT], int *types_i, int count)
{
	int found = 0;
	int new_count = sch_d->fields_num;

	int i,j;
	for(i = 0; i < count; i++){
		for(j = 0; j < sch_d->fields_num ;j++){
			if(strncmp(names[i],sch_d->fields_name[j],strlen(names[i])) == 0){
				found++;
				break;
			}
		}

		if(found == 0){
			strncpy(sch_d->fields_name[new_count],names[i],strlen(names[i]));
			sch_d->types[new_count] = types_i[i];
			new_count++;
			continue;
		}

		found = 0;
	}

	sch_d->fields_num = new_count;
	return sch_d->fields_num < new_count;
}

unsigned char perform_checks_on_schema(
		int mode,
		char *buffer, 
		int fields_count,
		char *file_path, 
		struct Record_f *rec, 
		struct Header_d *hd,
		int *pos,
		int option,
		int update)
{

	/* check if the schema on the file is equal to the input Schema.*/

	char names[MAX_FIELD_NR][MAX_FIELD_LT];
	memset(names,0,MAX_FIELD_LT*MAX_FIELD_NR*sizeof(char));

	int types_i[MAX_FIELD_NR];
	memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);
	char **values = NULL;
	int count = 0;
	int err = 0;
	ui8 sorted = 0;

	if(mode == NO_TYPE_WR){
		values = extract_fields_value_types_from_input(buffer,names,types_i, &count);
		if(!values){
			fprintf(stderr,"extracting values, names and types failed %s:%d\n",__FILE__,__LINE__ -2);
			goto clean_on_error;
		}
		
		/*check if the names are valid names*/
		if(!check_fields_integrity(names,count)){
			printf("invalid input, one or more fields have the same name, or the value is missing\n");
			printf("input syntax: fieldname:type:value\n");
			goto clean_on_error;
		}

		if(!schema_has_type(hd)){
			/*
			 * one or more fields in the schema has no type
			 * we need to assign the types to the schema
			 * */

			if(count == hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(0, count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				int verify = 0;
				int i;
				for(i = 0; i < count; i++)
					if(names[i][0] == '\0')verify++;

				if(verify == count){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(!schema_eq_assign_type(hd->sch_d,names,types_i)){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}


				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for(i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, count, names, types_i, values,
								hd->sch_d, SCHEMA_EQ,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d.\n",
							__FILE__,__LINE__-2);
					goto clean_on_error;
				}
				free_strs(count,1,values);
				return SCHEMA_EQ_NT;

			}else if(count < hd->sch_d->fields_num){
				/* 
				 * check if the input is part of the schema 
				 * then assign type accordingly
				 * */
				int result = 0;
				if((result = schema_ct_assign_type(hd->sch_d, names,types_i,count)) == 0){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for( i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}
				if(parse_input_with_no_type(file_path, count, names, types_i, values,
								hd->sch_d, 
								SCHEMA_CT_NT,rec,update) == -1){	
					fprintf(stderr,"can't parse input to record,%s:%d\n",
							__FILE__,__LINE__-2);
					goto clean_on_error;
				}
				
				free_strs(count,1,values);
				return result;
			}else if(count > hd->sch_d->fields_num){
				/*schema new */
				if(!schema_nw_assyign_type(hd->sch_d, names,types_i,count)){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}


				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for( i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, count, names, types_i, values,
								hd->sch_d, SCHEMA_EQ,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}
				free_strs(count,1,values);
				return SCHEMA_NW_NT;
			}
		}else{
			/*
			 * schema has types already, 
			 * so we need to check
			 * if the input is correct 
			 * */

			if(count == hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(0, count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(schema_check_type(count, SCHEMA_EQ,hd->sch_d,names,types_i,&values,option) == -1 ){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						ui16 i;
						for(i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, count, names, types_i, values,
							hd->sch_d, SCHEMA_EQ,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d.\n",__FILE__,__LINE__-2);
					goto clean_on_error;
				}

				free_strs(count,1,values);
				return SCHEMA_EQ;

			}else if(count < hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(SCHEMA_CT, count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d.\n",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				sorted  = 1;
				int result = 0;
				if((result = schema_check_type(count, SCHEMA_CT,hd->sch_d,names,types_i,&values,option)) == -1 ){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for(i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, count, names, 
							result == SCHEMA_CT_NT ? types_i : hd->sch_d->types, 
							values,
							hd->sch_d, 
							result == SCHEMA_CT_NT ? result : SCHEMA_CT,
							rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					goto clean_on_error;
				}

				free_strs(hd->sch_d->fields_num,1,values);
				if(result == SCHEMA_CT_NT)
					return result;
				else 
					return SCHEMA_CT;

			}else if(count > hd->sch_d->fields_num){
				if(!schema_nw_assyign_type(hd->sch_d, names,types_i,count)){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						ui16 i;
						for(i = 0; i < count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, count, names, types_i, values,
							hd->sch_d, SCHEMA_NW,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					err = SCHEMA_ERR; goto clean_on_error;
				}
				free_strs(count,1,values);
				return SCHEMA_NW;
			}
		}
	} else if(mode == HYB_WR){
		

		/*check if schema has all types */
		if(schema_has_type(hd)){
			/*
			 * check input with 
			 * missing types if it is correct 
			 * against the schema types 
			 * */
			int check = 0;
			if((fields_count = get_name_types_hybrid(mode,buffer,names,types_i)) == -1) goto clean_on_error;
			if(get_values_hyb(buffer,&values,fields_count) == -1) goto clean_on_error;

			int i;	
			for(i = 0; i < fields_count; i++) {
				if(types_i[i] == -1){
					if((types_i[i] = assign_type(values[i])) == -1) goto clean_on_error;	
				}
			}

			if(fields_count == hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(0, fields_count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						ui16 i;
						for(i = 0; i < fields_count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}
				
				if(schema_check_type(fields_count, SCHEMA_EQ,hd->sch_d,names,types_i,&values,option) == -1 ){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}


				check = SCHEMA_EQ;
			}else if(fields_count < hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(SCHEMA_CT, fields_count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}
					
				sorted  = 1;
				if(schema_check_type(fields_count, SCHEMA_CT,hd->sch_d,names,types_i,&values,option) == -1 ){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}
				check = SCHEMA_CT;

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						ui16 i;
						for(i = 0; i < hd->sch_d->fields_num; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}
			}

			if(fields_count == 1 && is_number_type(types_i[0])){
				if(check_double_compatibility(hd->sch_d,&values) == -1){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}
			}else{

				int i,j;
				for(i = 0; i < fields_count; i++){
					for(j = 0; j < hd->sch_d->fields_num; j++){
						if(strncmp(names[i],hd->sch_d->fields_name[j],
									strlen(hd->sch_d->fields_name[j])) != 0)
							continue;

						if(!is_number_type(types_i[i])) continue;
								
						if(hd->sch_d->types[j] != TYPE_DOUBLE) continue;	


						if(check_double_compatibility(hd->sch_d,&values) == -1){
							err = SCHEMA_ERR;
							goto clean_on_error;
						}
					}
				}
			}

			if(parse_input_with_no_type(file_path, fields_count, names, types_i, values,
						hd->sch_d, check,rec,update) == -1){
				fprintf(stderr,"can't parse input to record,%s:%d",
					__FILE__,__LINE__-2);
				goto clean_on_error;
			}
			
			if(check == SCHEMA_CT)
				free_strs(hd->sch_d->fields_num,1,values);
			else 
				free_strs(fields_count,1,values);

			if(fields_count == hd->sch_d->fields_num) return SCHEMA_EQ;
			if(fields_count < hd->sch_d->fields_num)return SCHEMA_CT;
			if(fields_count > hd->sch_d->fields_num)return SCHEMA_NW;
		}else{

			if((fields_count = get_name_types_hybrid(mode,buffer,names,types_i)) == -1) goto clean_on_error;
			if(get_values_hyb(buffer,&values,fields_count) == -1) goto clean_on_error;
				
			int i;
			for(i = 0; i < fields_count; i++) {
				if(types_i[i] == -1){
					if((types_i[i] = assign_type(values[i])) == -1) goto clean_on_error;	
				}
			}


			if(fields_count == hd->sch_d->fields_num){
				if(!sort_input_like_header_schema(0, fields_count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(!schema_eq_assign_type(hd->sch_d,names,types_i)){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(check_double_compatibility(hd->sch_d,&values) == -1){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for(i = 0; i < fields_count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, fields_count, names, types_i, values,
							hd->sch_d, SCHEMA_EQ,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					goto clean_on_error;
				}
				free_strs(fields_count,1,values);
				return SCHEMA_EQ_NT;

			}else if(fields_count < hd->sch_d->fields_num){
				int result = 0;
				if((result = schema_ct_assign_type(hd->sch_d, names,types_i,fields_count)) == 0){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(!sort_input_like_header_schema(SCHEMA_CT_NT, fields_count, hd->sch_d, names, &values, types_i)){
					fprintf(stderr,"can't sort input like schema %s:%d",__FILE__,__LINE__-1);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				sorted = 1;
				if(check_double_compatibility(hd->sch_d,&values) == -1){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						ui16 i;
						for(i = 0; i < hd->sch_d->fields_num; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, fields_count, names, 
							result == SCHEMA_CT_NT ? types_i : hd->sch_d->types, 
							values,
							hd->sch_d, 
							result == SCHEMA_CT_NT ? result : SCHEMA_CT,
							rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					goto clean_on_error;
				}

				free_strs(hd->sch_d->fields_num,1,values);
				return SCHEMA_CT_NT;
				if(result == SCHEMA_CT_NT)
					return result;
				else 
					return SCHEMA_CT;

			}else if( fields_count > hd->sch_d->fields_num){
				if(!schema_nw_assyign_type(hd->sch_d, names,types_i,count)){
					err = SCHEMA_ERR;
					goto clean_on_error;
				}

				if(pos){
					if(pos[0] != -1){
						char *field_n =	find_field_to_reset_delim(pos,buffer);
						size_t fd_nl = strlen(field_n);
						int i;
						for(i = 0; i < fields_count; i++){
							if(fd_nl != strlen(names[i])) continue;
							if(strncmp(field_n,names[i], fd_nl) == 0){
								replace('{',':',values[i]);	
								break;
							}
						}
					}
				}

				if(parse_input_with_no_type(file_path, fields_count, names, types_i, values,
							hd->sch_d, SCHEMA_NW,rec,update) == -1){
					fprintf(stderr,"can't parse input to record,%s:%d",
							__FILE__,__LINE__-2);
					err = SCHEMA_ERR;
					goto clean_on_error;
				}
				free_strs(fields_count,1,values);
				return SCHEMA_NW;
			}


		}
	}

	if (hd->sch_d->fields_num != 0) {
		
		if (fields_count == -1) {
			fields_count = count;
		}
		unsigned char check = check_schema(fields_count, buffer, hd);
		switch (check){
		case SCHEMA_EQ:
			if(parse_d_flag_input(file_path, fields_count, buffer,hd->sch_d, SCHEMA_EQ,rec, pos,update) == -1) return SCHEMA_ERR;

			return SCHEMA_EQ;
		case SCHEMA_ERR:
			return SCHEMA_ERR;
		case SCHEMA_NW:
			if(parse_d_flag_input(file_path, fields_count, buffer,hd->sch_d, SCHEMA_NW,rec,pos,update) == -1) return SCHEMA_ERR;

			return SCHEMA_NW;
		case SCHEMA_CT:
			if(parse_d_flag_input(file_path, fields_count, buffer, hd->sch_d, SCHEMA_CT,rec,pos,update) == -1) return SCHEMA_ERR;

			return SCHEMA_CT;
		case SCHEMA_CT_NT:
			if(parse_d_flag_input(file_path, fields_count, buffer, hd->sch_d, SCHEMA_CT_NT,rec,pos,update) == -1) return SCHEMA_ERR;

			return SCHEMA_CT_NT;
		default:
			printf("check is %d -> no processable option for the SCHEMA. parse.c:%d.\n", check, __LINE__ - 17);
			return 0;
		}
	} else { /* in this case the SCHEMA IS ALWAYS NEW*/
		if(parse_d_flag_input(file_path, fields_count, buffer,hd->sch_d, 0,rec,pos,update) == -1) return SCHEMA_ERR;

		return SCHEMA_NW;
	}

	return 1;

	clean_on_error:
	fprintf(stderr,"schema different then file definition.\n");
	if(count == 0){

		if(fields_count < hd->sch_d->fields_num && sorted)
			free_strs(hd->sch_d->fields_num,1,values);
		else
			free_strs(fields_count,1,values);
	}else{
		if(count < hd->sch_d->fields_num && sorted)
			free_strs(hd->sch_d->fields_num,1,values);
		else
			free_strs(count,1,values);
	}

	return err;	
}

unsigned char compare_old_rec_update_rec(struct Record_f **rec_old,
		struct Record_f *rec, 
		unsigned char check,
		int option,
		struct Header_d hd,
		file_t *fds,
		char *file_path)
{
	int changed = 0;
	int update_new = 0;
	int types_i[MAX_FIELD_NR]; 
	memset(types_i,-1,sizeof(int)*MAX_FIELD_NR);

	if (check == SCHEMA_CT || check == SCHEMA_CT_NT ) {

		int j;
		for (j = 0; j < rec_old[0]->fields_num; j++) {

			if(!rec->field_set[j])
				continue;

			int f[rec->fields_num];
			memset(f,-1,sizeof(int)*rec->fields_num);
			if(hd.sch_d->has_unique(hd.sch_d->constraints,rec->fields_num,f)){
				int i;
				for(i = 0; i < rec->fields_num; i++){
					if(!rec->field_set[i] || f[i] == -1) continue;

					if(unique_constraint_update(fds,rec,rec_old[0],f[i],file_path) == -1){
						return 0;
					}
				}
			}

			switch (rec->fields[j].type) {
			case -1:
				break;
			case TYPE_INT:
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.i != rec->fields[j].data.i){
						rec_old[0]->fields[j].data.i = rec->fields[j].data.i;
						changed = 1;
						rec->field_set[j] = 0;
						break;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_DATE:
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.date != rec->fields[j].data.date){
						rec_old[0]->fields[j].data.date = rec->fields[j].data.date;
						changed = 1;
						rec->field_set[j] = 0;
						break;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_LONG:
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.l != rec->fields[j].data.l){
						rec_old[0]->fields[j].data.l = rec->fields[j].data.l;
						changed = 1;
						break;
						rec->field_set[j] = 0;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_PACK:
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.p != rec->fields[j].data.p){
						rec_old[0]->fields[j].data.p = rec->fields[j].data.p;
						changed = 1;
						rec->field_set[j] = 0;
						break;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_FLOAT:
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.f != rec->fields[j].data.f){
						rec_old[0]->fields[j].data.f = rec->fields[j].data.f;
						changed = 1;
						rec->field_set[j] = 0;
						break;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_STRING:
			{	
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if(rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if(rec_old[0]->fields[j].data.s && rec->fields[j].data.s){
						size_t size_old = strlen(rec_old[0]->fields[j].data.s);
						size_t size_new = strlen(rec->fields[j].data.s);
						if(size_new == size_old){
							if (strcmp(rec_old[0]->fields[j].data.s, rec->fields[j].data.s) != 0) {
								free(rec_old[0]->fields[j].data.s);
								rec_old[0]->fields[j].data.s = duplicate_str(rec->fields[j].data.s);
								if (!rec_old[0]->fields[j].data.s){
									fprintf(stderr, "duplicate_str failed, %s:%d.\n", F, L - 2);
									return 0;
								}
								/*free the memory in the new record*/
								free(rec->fields[j].data.s);
								rec->fields[j].data.s = NULL;
								changed = 1;
								break;
							}

							rec->field_set[j] = 0;
							break;

						}else{
							free(rec_old[0]->fields[j].data.s);
							rec_old[0]->fields[j].data.s = NULL;
							rec_old[0]->fields[j].data.s = duplicate_str(rec->fields[j].data.s);
							if(!rec_old[0]->fields[j].data.s){
								fprintf(stderr, "duplicate_str failed, %s:%d.\n", F, L - 2);
								return 0;
							}

							/*free the memory in the new record*/
							free(rec->fields[j].data.s);
							rec->fields[j].data.s = NULL;

							changed = 1;
							rec->field_set[j] = 0;
						}
						break;
					}
					break;
				}
				break;
			}
			case TYPE_BYTE:	
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
					update_new = 1; 
					break;
				}
				if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
					if (rec_old[0]->fields[j].data.b != rec->fields[j].data.b){
						rec_old[0]->fields[j].data.b = rec->fields[j].data.b;
						changed = 1;
						rec->field_set[j] = 0;
						break;
					}
					rec->field_set[j] = 0;
				}
				break;
			case TYPE_DOUBLE:
			if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 0){
				update_new = 1; 
				break;
			}
			if (rec->field_set[j] == 1  && rec_old[0]->field_set[j] == 1){
				if (rec_old[0]->fields[j].data.d != rec->fields[j].data.d){
					rec_old[0]->fields[j].data.d = rec->fields[j].data.d;
					changed = 1;
					rec->field_set[j] = 0;
					break;
				}
				rec->field_set[j] = 0;
			}
			break;
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
			{
				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.i[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}
						break;
					}
					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++) {
							if (rec_old[0]->fields[j].data.v.elements.i[a] == rec->fields[j].data.v.elements.i[a]) continue;

							rec_old[0]->fields[j].data.v.elements.i[a] = rec->fields[j].data.v.elements.i[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						rec_old[0]->field_set[j] = 1;
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.i[a],
										&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}
				break;
			}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
			{
				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.l[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}
						break;
					}
					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++) {
							if (rec_old[0]->fields[j].data.v.elements.l[a] == rec->fields[j].data.v.elements.l[a])continue;

							rec_old[0]->fields[j].data.v.elements.l[a] = rec->fields[j].data.v.elements.l[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						rec_old[0]->field_set[j] = 1;
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.insert((void *)&rec->fields[j].data.v.elements.l[a],
									&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}

				break;
			}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
			{

				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.f[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}
						break;
					}

					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++) {
							if (rec_old[0]->fields[j].data.v.elements.f[a] == rec->fields[j].data.v.elements.f[a])continue;

							rec_old[0]->fields[j].data.v.elements.f[a] = rec->fields[j].data.v.elements.f[a];
						}
						changed = 1;
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						rec_old[0]->field_set[j] = 1;
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.insert((void *)&rec->fields[j].data.v.elements.f[a],
									&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}
				break;
			}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
			{

				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.b[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}
						break;
					}
					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size) {
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++) {
							if (rec_old[0]->fields[j].data.v.elements.b[a] == rec->fields[j].data.v.elements.b[a])continue;
							
							rec_old[0]->fields[j].data.v.elements.b[a] = rec->fields[j].data.v.elements.b[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						rec_old[0]->field_set[j] = 1;
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.insert((void *)&rec->fields[j].data.v.elements.b[a],
									&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}
				break;
			}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
			{
				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)(*rec).fields[j].data.v.elements.s[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}

						changed = 1;
						break;
					}
					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++){
							if (strcmp(rec_old[0]->fields[j].data.v.elements.s[a], rec->fields[j].data.v.elements.s[a]) != 0){
								/*free memory before allocating other memory*/
								if (rec_old[0]->fields[j].data.v.elements.s[a] != NULL)	{
									free(rec_old[0]->fields[j].data.v.elements.s[a]);
									rec_old[0]->fields[j].data.v.elements.s[a] = NULL;
								}

								rec_old[0]->fields[j].data.v.elements.s[a] = duplicate_str(rec->fields[j].data.v.elements.s[a]);
								if (!rec_old[0]->fields[j].data.v.elements.s[a]){
									fprintf(stderr, "duplicate_str failed, %s:%d.\n", F, L - 2);
									return 0;
								}
								changed = 1;
							}
						}
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						rec_old[0]->field_set[j] = 1;
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.insert((void *)rec->fields[j].data.v.elements.s[a],
									&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}
				break;
				}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
			{
				if(rec_old[0]->field_set[j] && rec->field_set[j]){
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.
								insert((void *)&rec->fields[j].data.v.elements.d[a],
										&rec_old[0]->fields[j].data.v, 
										rec->fields[j].type);
						}
						changed = 1;
						break;
					}
					if (rec_old[0]->fields[j].data.v.size == rec->fields[j].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[j].data.v.size; a++) {
							if (rec_old[0]->fields[j].data.v.elements.d[a] == rec->fields[j].data.v.elements.d[a])continue;

							rec_old[0]->fields[j].data.v.elements.d[a] = rec->fields[j].data.v.elements.d[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[j].data.v.destroy(&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						int a;
						for (a = 0; a < rec->fields[j].data.v.size; a++) {
							rec_old[0]->fields[j].data.v.insert((void *)&rec->fields[j].data.v.elements.d[a],
									&rec_old[0]->fields[j].data.v, rec->fields[j].type);
						}
						changed = 1;
					}
					break;
				}else if(rec->field_set[j]){
					update_new = 1; 
					break;
				}
				break;
			}
			case TYPE_FILE: /*TODO is this necessary????*/
			{	
				if(!rec_old[0]->field_set[j]){
					update_new = 1;
					break;
				}

				

				if(!compare_old_rec_update_rec(&rec_old[0]->fields[j].data.file.recs,
							rec->fields[j].data.file.recs,check,option,hd,fds,file_path))
					return 0;
				else
					break;

				/*TODO: this is a record all together, i do not belive that
				 * we sholud treat it as an array
				 **/
				if(option == AAR){
					size_t n_size = rec_old[0]->fields[j].data.file.count + rec->fields[j].data.file.count;

					struct Record_f *n_recs = (struct Record_f*)realloc(rec_old[0]->fields[j].data.file.recs,
							n_size * sizeof(struct Record_f));
					if(!n_recs){
						fprintf(stderr,"realloc failed, %s:%d.\n",__FILE__,__LINE__ - 4);
						return -1;
					}

					rec_old[0]->fields[j].data.file.recs = n_recs;
					rec_old[0]->fields[j].data.file.count = n_size;
					rec_old[0]->field_set[j] = 1;
					ui32 k,t;
					for(t = 0,k = rec->fields[0].data.file.count;
							k < rec_old[0]->fields[j].data.file.count; 
							k++,t++){
						memcpy(&rec_old[0]->fields[j].data.file.recs[k],
								&rec->fields[j].data.file.recs[t],
								sizeof(struct Record_f));
					}
					changed = 1;
					break;
				}


				if(rec_old[0]->fields[j].data.file.count < rec->fields[j].data.file.count){

					/*zero out the old record*/
					memset(rec_old[0]->fields[j].data.file.recs,0,
							rec_old[0]->fields[j].data.file.count*sizeof(struct Record_f));

					size_t n_size = rec->fields[j].data.file.count;
					struct Record_f* new_recs = (struct Record_f*)realloc(rec_old[0]->fields[j].data.file.recs,
							n_size * sizeof *new_recs);
					if(!new_recs){
						fprintf(stderr,"realloc() failed %s:%d.\n",__FILE__,__LINE__-4);
						return 0;
					}

					rec_old[0]->fields[j].data.file.recs = new_recs;
					rec_old[0]->fields[j].data.file.count = n_size;

					memcpy(rec_old[0]->fields[j].data.file.recs,
							rec->fields[j].data.file.recs,
							rec->fields[j].data.file.count * sizeof *new_recs);

					changed = 1;
				}
				/*you need to test the other cases*/
				break;
			}
			default:
			printf("invalid type! type -> %d.\n", rec->fields[j].type);
			return 0;
			}
		}


		if(changed && update_new){
			return UPDATE_OLDN;
		}else if(changed && !update_new){
			return UPDATE_OLD;
		}else if (!changed && update_new){
			return UPDATE_OLDN;
		}else{
			return UPDATE_NOT;
		}
	}


	/*this get execute if schema is equal*/
	changed = 0;
	int i;
	for (i = 0; i < rec_old[0]->fields_num; i++){
		switch (rec_old[0]->fields[i].type){
			case -1:
				break;
			case TYPE_INT:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.i != rec->fields[i].data.i){
						changed = 1;
						rec_old[0]->fields[i].data.i = rec->fields[i].data.i;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
				}
				break;
			case TYPE_DATE:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.date != rec->fields[i].data.date){
						changed = 1;
						rec_old[0]->fields[i].data.date = rec->fields[i].data.date;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
				}
				break;
			case TYPE_LONG:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.l != rec->fields[i].data.l){
						changed = 1;
						rec_old[0]->fields[i].data.l = rec->fields[i].data.l;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
				}
				break;
			case TYPE_FLOAT:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.f != rec->fields[i].data.f){
						changed = 1;
						rec_old[0]->fields[i].data.f = rec->fields[i].data.f;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
				}
				break;
			case TYPE_STRING:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				}

				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (strcmp(rec_old[0]->fields[i].data.s, rec->fields[i].data.s) != 0){
						/*free memory before allocating other memory*/
						if (rec_old[0]->fields[i].data.s != NULL) {
							char *p =rec_old[0]->fields[i].data.s; 
							free(p);
							rec_old[0]->fields[i].data.s = NULL;
						}
						size_t l = strlen(rec->fields[i].data.s);
						rec_old[0]->fields[i].data.s = (char *) malloc(l+1);
						if(!rec_old[0]->fields[i].data.s){
							fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__-2);
							return 0;
						}
						memset(rec_old[0]->fields[i].data.s,0,l+1);
						strncpy(rec_old[0]->fields[i].data.s,rec->fields[i].data.s,l);
						changed = 1;
						rec->field_set[i] = 0;
						break;
					}
					rec->field_set[i] = 0;
					break;
				}
				break;
			case TYPE_BYTE:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.b != rec->fields[i].data.b){
						changed = 1;
						rec_old[0]->fields[i].data.b = rec->fields[i].data.b;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
					break;
				}
				break;
			case TYPE_DOUBLE:
				if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
					update_new = 1;
					break;
				} else if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 1){ 
					if (rec_old[0]->fields[i].data.d != rec->fields[i].data.d){
						changed = 1;
						rec_old[0]->fields[i].data.d = rec->fields[i].data.d;
						rec->field_set[i] = 0;
					}
					rec->field_set[i] = 0;
					break;
				}
				break;
			case TYPE_ARRAY_INT:
			case TYPE_SET_INT:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)&rec->fields[i].data.v.elements.i[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}

					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}
					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size)
					{
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (rec_old[0]->fields[i].data.v.elements.i[a] == rec->fields[i].data.v.elements.i[a])
								continue;

							rec_old[0]->fields[i].data.v.elements.i[a] = rec->fields[i].data.v.elements.i[a];
							changed = 1;
						}
					}else{

						changed = 1;
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)&rec->fields[i].data.v.elements.i[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
					}
					break;
				}
			case TYPE_ARRAY_LONG:
			case TYPE_SET_LONG:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)&rec->fields[i].data.v.elements.l[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}
					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}
					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (rec_old[0]->fields[i].data.v.elements.l[a] == rec->fields[i].data.v.elements.l[a])continue;

							rec_old[0]->fields[i].data.v.elements.l[a] = rec->fields[i].data.v.elements.l[a];
						}
					}
					else{
						changed = 1;
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)&rec->fields[i].data.v.elements.l[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
					}
					break;
				}
			case TYPE_ARRAY_FLOAT:
			case TYPE_SET_FLOAT:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)&rec->fields[i].data.v.elements.f[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}

					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}
					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size)
					{
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (rec_old[0]->fields[i].data.v.elements.f[a] == rec->fields[i].data.v.elements.f[a])continue;

							rec_old[0]->fields[i].data.v.elements.f[a] = rec->fields[i].data.v.elements.f[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)&rec->fields[i].data.v.elements.f[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
					}
					break;
				}
			case TYPE_ARRAY_BYTE:
			case TYPE_SET_BYTE:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)&rec->fields[i].data.v.elements.b[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}

					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}

					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size)
					{
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (rec_old[0]->fields[i].data.v.elements.b[a] == rec->fields[i].data.v.elements.b[a])
								continue;
							rec_old[0]->fields[i].data.v.elements.b[a] = rec->fields[i].data.v.elements.b[a];
							changed = 1;
						}
					}else{
						changed = 1;
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)&rec->fields[i].data.v.elements.b[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
					}
					break;
				}
			case TYPE_ARRAY_STRING:
			case TYPE_SET_STRING:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)rec->fields[i].data.v.elements.s[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}

					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}

					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (strcmp(rec_old[0]->fields[i].data.v.elements.s[a], rec->fields[i].data.v.elements.s[a]) != 0){
								/* free memory before allocating other memory */
								if (rec_old[0]->fields[i].data.v.elements.s[a] != NULL){
									free(rec_old[0]->fields[i].data.v.elements.s[a]);
									rec_old[0]->fields[i].data.v.elements.s[a] = NULL;
								}

								rec_old[0]->fields[i].data.v.elements.s[a] = duplicate_str(rec->fields[i].data.v.elements.s[a]);
								if (!rec_old[0]->fields[i].data.v.elements.s[a]){
									fprintf(stderr, "duplicate_str failed, %s:%d.\n", F, L - 2);
									return 0;
								}
								changed = 1;
							}
						}
					}else{
						changed = 1;
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)rec->fields[i].data.v.elements.s[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
					}
					break;
				}
			case TYPE_ARRAY_DOUBLE:
			case TYPE_SET_DOUBLE:
				{
					if(option == AAR){
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++) {
							rec_old[0]->fields[i].data.v.
								insert((void *)&rec->fields[i].data.v.elements.d[a],
										&rec_old[0]->fields[i].data.v, 
										rec->fields[i].type);
						}
						changed = 1;
						break;
					}
					if(rec->field_set[i] == 1 && rec_old[0]->field_set[i] == 0){ 
						update_new = 1;
						break;
					}
					if (rec_old[0]->fields[i].data.v.size == rec->fields[i].data.v.size){
						/*check values*/
						int a;
						for (a = 0; a < rec_old[0]->fields[i].data.v.size; a++){
							if (rec_old[0]->fields[i].data.v.elements.d[a] == rec->fields[i].data.v.elements.d[a])continue;

							rec_old[0]->fields[i].data.v.elements.d[a] = rec->fields[i].data.v.elements.d[a];
							changed = 1;
						}
					}else{
						rec_old[0]->fields[i].data.v.destroy(&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						int a;
						for (a = 0; a < rec->fields[i].data.v.size; a++){
							rec_old[0]->fields[i].data.v.insert((void *)&rec->fields[i].data.v.elements.d[a],
									&rec_old[0]->fields[i].data.v, rec->fields[i].type);
						}
						changed = 1;
					}
					break;
				}
			case TYPE_FILE:
				{	
					if (!rec_old[0]->fields[i].data.file.recs || rec_old[0]->fields[i].data.file.count == 0){ 
						if (!changed) update_new = 1;
					}

					break;
				}
			default:
				printf("invalid type! type -> %d.\n", rec->fields[i].type);
				return 0;
		}
	}

	if(changed && update_new){
		return UPDATE_OLDN;
	}else if(changed && !update_new){
		return UPDATE_OLD;
	}else if (!changed && update_new){
		return UPDATE_OLDN;
	}else if(!changed && !update_new){
		return UPDATE_OLD;
	}

	return  UPDATE_OLDN;
}


void find_fields_to_update(
		struct Record_f **rec_old,
		char *positions,
		struct Record_f *rec,
		int option,
		struct Header_d hd,
		file_t *fds,
		char *file_path)
{
	int dif = 0;

	ui32 j;
	for (j = 0; j < (ui32)rec_old[0]->count; j++) {
			if (positions[j] != 'y' 
					|| positions[j] != 'e')
				positions[j] = 'n';

			int index = compare_rec(rec_old[j],rec,option);
			if(index == E_RCMP){
				positions[j] = '0';
				return;
			}
			if(index == -1){
				positions[j] = 'e';
				continue;
			}

			if(index == DIF){
				dif++; 
				continue;
			}

			int f[rec->fields_num];
			memset(f,-1,sizeof(int)*rec->fields_num);
			if(hd.sch_d->has_unique(hd.sch_d->constraints,rec->fields_num,f)){
				int i;
				for(i = 0; i < rec->fields_num; i++){
					if(f[i] != index) continue;
					if(unique_constraint_update(fds,rec,rec_old[j],f[i],file_path) == -1){
						positions[0] = '0';
						return;
					}
				}
			}
			switch (rec->fields[index].type) {
				case TYPE_INT:
					rec_old[j]->fields[index].data.i = rec->fields[index].data.i;
					positions[j] = 'y';
					break;
				case TYPE_LONG:
					rec_old[j]->fields[index].data.l = rec->fields[index].data.l;
					positions[j] = 'y';
					break;
				case TYPE_PACK:
					rec_old[j]->fields[index].data.p = rec->fields[index].data.p;
					positions[j] = 'y';
					break;
				case TYPE_FLOAT:
					rec_old[j]->fields[index].data.f = rec->fields[index].data.f;
					positions[j] = 'y';
					break;
				case TYPE_STRING:
					if (rec_old[j]->fields[index].data.s != NULL) {
						free(rec_old[j]->fields[index].data.s);
						rec_old[j]->fields[index].data.s = NULL;
					}

					rec_old[j]->fields[index].data.s = duplicate_str(rec->fields[index].data.s);
					positions[j] = 'y';
					break;
				case TYPE_BYTE:
					rec_old[j]->fields[index].data.b = rec->fields[index].data.b;
					positions[j] = 'y';
					break;
				case TYPE_DOUBLE:
					rec_old[j]->fields[index].data.d = rec->fields[index].data.d;
					positions[j] = 'y';
					break;
				case TYPE_ARRAY_INT:
				case TYPE_SET_INT:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.i[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.elements.i[a] =
								rec->fields[index].data.v.elements.i[a];
						}
						positions[j] = 'y';
						break;
					}else{

						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.i[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						rec_old[j]->fields[index].data.v.
							destroy(&rec_old[j]->fields[index].data.v, 
									rec->fields[index].type);

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.
								insert((void *)&rec->fields[index].data.v.elements.i[a],
										&rec_old[j]->fields[index].data.v, 
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_ARRAY_LONG:
				case TYPE_SET_LONG:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.l[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.elements.l[a] =
								rec->fields[index].data.v.elements.l[a];
						}
						positions[j] = 'y';
						break;
					}else{
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.l[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						rec_old[j]->fields[index].data.v.destroy(&rec_old[j]->fields[index].data.v, 
								rec->fields[index].type);

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.
								insert((void *)&rec->fields[index].data.v.elements.l[a],
										&rec_old[j]->fields[index].data.v, 
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_ARRAY_FLOAT:
				case TYPE_SET_FLOAT:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.f[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.elements.f[a] =
								rec->fields[index].data.v.elements.f[a];
						}
						positions[j] = 'y';
						break;
					}else{
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.f[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						rec_old[j]->fields[index].data.v.
							destroy(&rec_old[j]->fields[index].data.v, 
									rec->fields[index].type);

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.
								insert((void *)&rec->fields[index].data.v.elements.f[a],
										&rec_old[j]->fields[index].data.v, 
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_ARRAY_DOUBLE:
				case TYPE_SET_DOUBLE:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.d[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.elements.d[a] =
								rec->fields[index].data.v.elements.d[a];
						}
						positions[j] = 'y';
						break;
					}else{
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.d[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						rec_old[j]->fields[index].data.v.destroy(&rec_old[j]->fields[index].data.v, 
								rec->fields[index].type);

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.
								insert((void *)&rec->fields[index].data.v.elements.d[a],
										&rec_old[j]->fields[index].data.v, 
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_ARRAY_BYTE:
				case TYPE_SET_BYTE:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.b[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v.elements.b[a] =
								rec->fields[index].data.v.elements.b[a];
						}
						positions[j] = 'y';
						break;
					}else{
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.b[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						rec_old[j]->fields[index].data.v.
							destroy(&rec_old[j]->fields[index].data.v, 
									rec->fields[index].type);

						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++) {
							rec_old[j]->fields[index].data.v.
								insert((void *)&rec->fields[index].data.v.elements.b[a],
										&rec_old[j]->fields[index].data.v, 
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_ARRAY_STRING:
				case TYPE_SET_STRING:
					if (rec->fields[index].data.v.size == rec_old[j]->fields[index].data.v.size){
						if(option == AAR){
							int a,b;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								if(rec_old[j]->fields[index].data.v.
										insert((void *)rec->fields[index].data.v.elements.s[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type) == -1){
									b = 1;
									break;
								}

							}
							if(!b)
								positions[j] = 'y';
							break;
						}
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							free(rec_old[j]->fields[index].data.v.elements.s[a]);

							rec_old[j]->fields[index].data.v.elements.s[a] =
								duplicate_str(rec->fields[index].data.v.elements.s[a]);
							if (!rec_old[j]->fields[index].data.v.elements.s[a]){
								fprintf(stderr, "duplicate_str() failed %s:%d.\n", F, L - 2);
								positions[0] = '0';
								return;
							}
						}
						positions[j] = 'y';
						break;
					}else{
						if(option == AAR){
							int a;
							for (a = 0; a < rec->fields[index].data.v.size; a++) {
								rec_old[j]->fields[index].data.v.
									insert((void *)&rec->fields[index].data.v.elements.s[a],
											&rec_old[j]->fields[index].data.v, 
											rec->fields[index].type);
							}
							positions[j] = 'y';
							break;
						}
						/*
						 * if the sizes of the two arrays are different,
						 * simply we destroy the old one,
						 * and in the old record we create a new one we the data
						 * of the new record
						 * */
						rec_old[j]->fields[index].data.v.destroy(&rec_old[j]->fields[index].data.v, rec->fields[index].type);
						int a;
						for (a = 0; a < rec->fields[index].data.v.size; a++){
							rec_old[j]->fields[index].data.v
								.insert((void *)rec->fields[index].data.v.elements.s[a],
										&rec_old[j]->fields[index].data.v,
										rec->fields[index].type);
						}
						positions[j] = 'y';
						break;
					}
				case TYPE_FILE:
					{
#if 0
						int rlen =(int)strlen(rec->fields[index].field_name);
						int sfxl =strlen(".sch");
						int len = sfxl + rlen + 1;
						char fn[len];
						memset(fn,0,len);
						strncpy(fn,rec->fields[index].field_name,rlen);
						strncat(fn,".sch",sfxl);
						file_t fd_sch = open_file(fn,0);
						if(file_error_handler(1,fd_sch) != 0){
							positions[0] = '0';
							return;
						}
						struct Schema sch;
						memset(&sch,0,sizeof(struct Schema));

						struct Header_d hd = {0,0,&sch};

						if(!read_header(fd_sch,&hd)){
							fprintf(stderr,"(%s): read_header() failed, %s:%d.\n","db",F,L-1);
							close_file(1,fd_sch);
							positions[j] = 'y';
							return;
						}
						close_file(1,fd_sch);


						/*resize the memory accordingly*/
						size_t n_size = 0;
						if(option == AAR){
							n_size = rec_old[j]->fields[index].data.file.count + rec->fields[index].data.file.count;

							struct Record_f *n_recs = (struct Record_f*)realloc(
									rec_old[j]->fields[index].data.file.recs,
									n_size * sizeof(struct Record_f));
							if(!n_recs){
								fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__ - 4);
								free_schema(hd.sch_d);
								positions[0] = '0';
								return;
							}

							rec_old[j]->fields[index].data.file.recs = n_recs;
							rec_old[j]->fields[index].data.file.count = n_size;
							rec_old[j]->field_set[index] = 1;
							ui32 k,t;
							for(t = 0,k = rec->fields[index].data.file.count;
									k < rec_old[j]->fields[index].data.file.count; 
									k++,t++){
								if(!copy_rec(&rec->fields[index].data.file.recs[t],
											&rec_old[j]->fields[index].data.file.recs[k],
											hd.sch_d)){
									fprintf(stderr,"(%s): copy_rec() failed, %s:%d.\n","db",F,L-1);
									positions[0] = '0';
									free_schema(hd.sch_d);
									return;
								}
							}
							positions[j] = 'y';
							break;
						}else{
							/*
							 * ensure that previously allcoated memory
							 * in the rec is freed.
							 * this function also zeros the memory out
							 * when the second parameter is on (1)
							 * */
							/*free_type_file(rec_old[j],1);*/

							if(rec_old[j]->fields[index].data.file.count != rec->fields[index].data.file.count){
								struct Record_f *n_recs = (struct Record_f*)realloc(
										rec_old[j]->fields[index].data.file.recs,
										rec->fields[index].data.file.count * sizeof(struct Record_f));

								if(!n_recs){
									fprintf(stderr,"realloc() failed, %s:%d.\n",__FILE__,__LINE__ - 4);
									close_file(1,fd_sch);
									positions[0] = '0';
									free_schema(hd.sch_d);
									return;
								}

								rec_old[j]->fields[index].data.file.recs = n_recs;
								rec_old[j]->fields[index].data.file.count = rec->fields[index].data.file.count;
							}

							rec_old[j]->field_set[index] = 1;
							ui32 k,x;
							for(k = 0; k < rec_old[j]->fields[index].data.file.count; k++){
								for(x = 0; x < (ui32)rec->fields[index].data.file.recs->fields_num; x++){
									if(!rec->fields[index].data.file.recs->field_set[x] )
										continue;

									struct Record_f *or = rec_old[j]->fields[index].data.file.recs;
									struct Record_f *nr = rec->fields[index].data.file.recs;
									switch(nr->fields[x].type){
										case TYPE_INT:
											or->fields[x].data.i = nr->fields[x].data.i;
											break;
										case TYPE_LONG:
											or->fields[x].data.l = nr->fields[x].data.l;
											break;
										case TYPE_BYTE:
											or->fields[x].data.b = nr->fields[x].data.b;
											break;
										case TYPE_KEY:
											or->fields[x].data.k = nr->fields[x].data.k;
											break;
										case TYPE_DATE:
											or->fields[x].data.date = nr->fields[x].data.date;
											break;
										case TYPE_FLOAT:
											or->fields[x].data.f = nr->fields[x].data.f;
											break;
										case TYPE_DOUBLE:
											or->fields[x].data.d= nr->fields[x].data.d;
											break;
										case TYPE_STRING:
											{
												int sz = (int)strlen(nr->fields[x].data.s);

												if(sz != (int)strlen(or->fields[x].data.s) 
														|| strncmp(nr->fields[x].data.s,or->fields[x].data.s,sz) != 0){

													free(or->fields[x].data.s);
													or->fields[x].data.s = (char *)malloc(sz+1);
													if(!or->fields[x].data.s){
														fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__ - 2);
														free_schema(hd.sch_d);
														positions[0] = '0';
														return;
													}

													memset(or->fields[x].data.s,0,sz+1);
													if(!or->fields[x].data.s){
														fprintf(stderr,"malloc() failed, %s:%d.\n",__FILE__,__LINE__ - 2);
														free_schema(hd.sch_d);
														positions[0] = '0';
													}	
													strncpy(or->fields[x].data.s,nr->fields[x].data.s,sz);
												}
												break;
											}
										case TYPE_ARRAY_INT:
										case TYPE_SET_INT:
											{
												if(!or->fields[x].data.v.elements.i){
													or->fields[x].data.v.insert = insert_element;
													or->fields[x].data.v.destroy = free_dynamic_array; 
												}

												if (nr->fields[x].data.v.size == or->fields[x].data.v.size){
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															if(or->fields[x].data.v.
																	insert((void *)&nr->fields[x].data.v.elements.i[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type) == -1){
																break;
															}

														}
														break;
													}
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++)
														or->fields[x].data.v.elements.i[a] =  nr->fields[x].data.v.elements.i[a];

													break;
												}else{
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															or->fields[x].data.v.
																insert((void *)&nr->fields[x].data.v.elements.i[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type);
														}
														break;
													}
													/*
													 * if the sizes of the two arrays are different,
													 * simply we destroy the old one,
													 * and in the old record we create a new one with the data
													 * of the new record
													 * */
													or->fields[x].data.v.destroy(&or->fields[x].data.v, or->fields[x].type);
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++){
														or->fields[x].data.v
															.insert((void *)&nr->fields[x].data.v.elements.i[a],
																	&or->fields[x].data.v,
																	nr->fields[x].type);
													}
													break;
												}
												break;
											}
										case TYPE_ARRAY_LONG:
										case TYPE_SET_LONG:
											{
												if(!or->fields[x].data.v.elements.l){
													or->fields[x].data.v.insert = insert_element;
													or->fields[x].data.v.destroy = free_dynamic_array; 
												}

												if (nr->fields[x].data.v.size == or->fields[x].data.v.size){
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															if(or->fields[x].data.v.
																	insert((void *)&nr->fields[x].data.v.elements.l[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type) == -1){
																break;
															}
														}
														break;
													}
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++)
														or->fields[x].data.v.elements.l[a] =  nr->fields[x].data.v.elements.l[a];

													break;
												}else{
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															or->fields[x].data.v.
																insert((void *)&nr->fields[x].data.v.elements.l[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type);
														}
														break;
													}
													/*
													 * if the sizes of the two arrays are different,
													 * simply we destroy the old one,
													 * and in the old nrord we create a new one with the data
													 * of the new nrord
													 * */
													or->fields[x].data.v.destroy(&or->fields[x].data.v, nr->fields[x].type);
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++){
														or->fields[x].data.v
															.insert((void *)&nr->fields[x].data.v.elements.l[a],
																	&or->fields[x].data.v,
																	nr->fields[x].type);
													}
													break;
												}
												break;
											}
										case TYPE_ARRAY_BYTE:
										case TYPE_SET_BYTE:
											break;
										case TYPE_ARRAY_FLOAT:
										case TYPE_SET_FLOAT:
											break;
										case TYPE_ARRAY_DOUBLE:
										case TYPE_SET_DOUBLE:
											break;
										case TYPE_ARRAY_STRING:
										case TYPE_SET_STRING:
											{
												if(!or->fields[x].data.v.elements.s){
													or->fields[x].data.v.insert = insert_element;
													or->fields[x].data.v.destroy = free_dynamic_array; 
												}

												if (nr->fields[x].data.v.size == or->fields[x].data.v.size){
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															if(or->fields[x].data.v.
																	insert((void *)nr->fields[x].data.v.elements.s[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type) == -1){
																break;
															}
														}
														break;
													}
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++){
														free(or->fields[x].data.v.elements.s[a]);

														or->fields[x].data.v.elements.s[a] =
															duplicate_str(nr->fields[x].data.v.elements.s[a]);
														if (!or->fields[x].data.v.elements.s[a]){
															fprintf(stderr, "duplicate_str() failed %s:%d.\n", F, L - 2);
															return;
														}
													}
													break;
												}else{
													if(option == AAR){
														int a;
														for (a = 0; a < nr->fields[x].data.v.size; a++) {
															or->fields[x].data.v.
																insert((void *)nr->fields[x].data.v.elements.s[a],
																		&or->fields[x].data.v, 
																		nr->fields[x].type);
														}
														break;
													}
													/*
													 * if the sizes of the two arrays are different,
													 * simply we destroy the old one,
													 * and in the old nrord we create a new one we the data
													 * of the new nrord
													 * */
													or->fields[x].data.v.destroy(&or->fields[x].data.v, nr->fields[x].type);
													int a;
													for (a = 0; a < nr->fields[x].data.v.size; a++){
														or->fields[x].data.v
															.insert((void *)nr->fields[x].data.v.elements.s[a],
																	&or->fields[x].data.v,
																	nr->fields[x].type);
													}
													break;
												}
												break;
											}
										case TYPE_FILE:
											break;
										default:
											fprintf(stderr,"(%s): type not supported.%s:%d.\n",prog,__FILE__,__LINE__);
											free_schema(hd.sch_d);
											positions[0] = '0';
											return;
									}
								}
								free_schema(hd.sch_d);
							}
							positions[j] = 'y';
							break;
						}
#endif
					}
				default:
					printf("no matching type\n");
					positions[0] = '0';
					return;
			}
	}
}	

static char *print_constraints(struct Schema sch, int i)
{
	static char cn[512];
	memset(cn,0,512);

	if(sch.constraints[i] == CONST_NO){
		strncpy(cn,"NO COSTRAINTS",strlen("NO COSTRAINTS")+1);
		return &cn[0];
	}

	ui8 a[]= {CONST_DEFAULT,CONST_NOT_NULL,CONST_UNIQUE,CONST_FOREIGN_KEY};

	int j;
	long bwritten = 0;
	int CONST_COUNT = 4;
	for(j = 0; j < CONST_COUNT; j++){
		int r = 0, assigned = 0;
		if((r = sch.constraints[i] & a[j])){
			switch(r){
				case CONST_FOREIGN_KEY:
					strncpy(&cn[bwritten],"FOREING KEY",strlen("FOREING KEY")+1);
					bwritten += strlen("FOREING KEY");
					if(sch.constraints[i] > CONST_DEFAULT && !assigned){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}else if (sch.constraints[i] > CONST_DEFAULT && ((sch.constraints[i] - assigned) > 0)){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}
					break;
				case CONST_UNIQUE:
					strncpy(&cn[bwritten],"UNIQUE",strlen("UNIQUE")+1);
					bwritten += strlen("UNIQUE");
					if(sch.constraints[i] > CONST_DEFAULT && !assigned){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}else if (sch.constraints[i] > CONST_DEFAULT && ((sch.constraints[i] - assigned) > 0)){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}
					break;
				case CONST_NOT_NULL:
					strncpy(&cn[bwritten],"NOT NULL",strlen("NOT NULL")+1);
					bwritten += strlen("NOT NULL");
					if(sch.constraints[i] > CONST_DEFAULT && !assigned){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}else if (sch.constraints[i] > CONST_DEFAULT && ((sch.constraints[i] - assigned) > 0)){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}
					break;
				case CONST_DEFAULT:
					strncpy(&cn[bwritten],"DEFAULT -> ",strlen("DEFAULT -> ")+1);
					bwritten += strlen("DEFAULT -> ");
					char *df = get_def_value(sch,i);
					strncpy(&cn[bwritten],df,strlen(df));
					bwritten += strlen(df);
					if(sch.constraints[i] > CONST_DEFAULT && !assigned){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}else if (sch.constraints[i] > CONST_DEFAULT && ((sch.constraints[i] - assigned) > 0)){
						cn[bwritten++] = ',';
						cn[bwritten++] = ' ';
						assigned += r;
					}
					break;
				default:
					return NULL;
			}
		}
	}
	return &cn[0];
}

static char *get_def_value(struct Schema sch,int i)
{
	static char b[512] = {0};
	switch(sch.types[i]){
		case TYPE_INT: 
			if(copy_to_string(b,512,"%d",*(int*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}
			return &b[0];
		case TYPE_LONG: 
			if(copy_to_string(b,512,"%d",*(long*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}

			return &b[0];
		case TYPE_BYTE: 
			memset(b,0,512);
			if(copy_to_string(b,512,"%d",*(unsigned char*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}

			return &b[0];
		case TYPE_FLOAT: 
			memset(b,0,512);
			if(copy_to_string(b,512,"%.2f",*(float*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}

			return &b[0];
		case TYPE_DOUBLE: 
			memset(b,0,512);
			if(copy_to_string(b,512,"%.2f",*(double*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}

			return &b[0];
		case TYPE_STRING: 
			if(copy_to_string(b,512,"%s",(char*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}
			return &b[0];
		case TYPE_DATE: 
			{
				ui32 n = *(ui32*)sch.defaults[i];
				char date_buff[11] = {0};
				convert_number_to_date(date_buff,n);

				if(copy_to_string(b,512,"%s",date_buff) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return NULL;
				}

				return &b[0];
			}
		case TYPE_KEY: 
			if(copy_to_string(b,512,"%d",*(ui32*)sch.defaults[i]) < 0){
				fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
				return NULL;
			}

			return &b[0];
		default:
			return NULL;
	}
}

void print_schema(struct Schema sch)
{
	int i, max_fl_lt = 0;
	for(i = 0; i < sch.fields_num; i++){
		if(sch.is_dropped[i])
			continue;

		if(max_fl_lt > (int)strlen(sch.fields_name[i]))
			continue;

		max_fl_lt = (int) strlen(sch.fields_name[i]);
	}

	char c = ' ';
	char keyb = '0';
	int padding = max_fl_lt > (11 + (int) strlen("Fields Name")) ? max_fl_lt : 21;
	printf("Field Name%-*cType%-*cConstraints\n", 11, c, 11,c);
	int line_lenght = 25 + 33;
	char sep[line_lenght+1];
	memset(sep,0,line_lenght+1);
	memset(sep,'_',line_lenght);
	printf("%s\n",sep);
	for (i = 0; i < sch.fields_num; i++) {
		if(sch.is_dropped[i])
			continue;

		printf("%s%-*c", sch.fields_name[i], padding - (int)strlen(sch.fields_name[i]), c);
		switch (sch.types[i])
		{
			case TYPE_INT:
				printf("int%-*c%s\n",11+4 - (int)strlen("int"),c,print_constraints(sch,i));
				break;
			case TYPE_KEY:
				printf("key%-*c%s\n",11+4 - (int)strlen("key"),c,print_constraints(sch,i));
				break;
			case TYPE_FLOAT:
				printf("float%-*c%s\n",11+4 - (int)strlen("float"),c,print_constraints(sch,i));
				break;
			case TYPE_LONG:
				printf("long%-*c%s\n",11+4 - (int)strlen("long"),c,print_constraints(sch,i));
				break;
			case TYPE_BYTE:
				printf("byte%-*c%s\n",11+4 - (int)strlen("byte"),c,print_constraints(sch,i));
				break;
			case TYPE_PACK:
				printf("pack%-*c%s\n",11+4 - (int)strlen("pack"),c,print_constraints(sch,i));
				break;
			case TYPE_DATE:
				printf("date%-*c%s\n",11+4 - (int)strlen("date"),c,print_constraints(sch,i));
				break;
			case TYPE_DOUBLE:
				printf("double%-*c%s\n",11+4 - (int)strlen("double"),c,print_constraints(sch,i));
				break;
			case TYPE_STRING:
				printf("string.%-*c%s\n",11 + 4 - (int)strlen("string."),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_INT:
				printf("int[]%-*c%s\n",11 + 4 - (int)strlen("int[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_INT:
				printf("int[]-set%-*c%s\n",11 + 4 - (int)strlen("int[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_LONG:
				printf("long[]%-*c%s\n",11 + 4 - (int)strlen("long[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_LONG:
				printf("long[]-set%-*c%s\n",11 + 4 - (int)strlen("long[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_FLOAT:
				printf("float[]%-*c%s\n",11 + 4 - (int)strlen("float[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_FLOAT:
				printf("float[]-set%-*c%s\n",11 + 4 - (int)strlen("float[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_STRING:
				printf("string[]%-*c%s\n",11 + 4 - (int)strlen("string[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_STRING:
				printf("string[]-set%-*c%s\n",11 + 4 - (int)strlen("string[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_BYTE:
				printf("byte[]%-*c%s\n",11 + 4 - (int)strlen("byte[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_BYTE:
				printf("byte[]-set%-*c%s\n",11 + 4 - (int)strlen("byte[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_ARRAY_DOUBLE:
				printf("double[]%-*c%s\n",11 + 4 - (int)strlen("double[]"),c,print_constraints(sch,i));
				break;
			case TYPE_SET_DOUBLE:
				printf("double[]-set%-*c%s\n",11 + 4 - (int)strlen("double[]-set"),c,print_constraints(sch,i));
				break;
			case TYPE_FILE:
				printf("File.\n");
				break;
			default:
				printf("%-*c%s\n",11+4,c,print_constraints(sch,i));
				break;
		}
		if (i > 0 && (i % 20 == 0)){
			printf("press return key. . .\nenter q to quit . . .\n");
			keyb = (char)getc(stdin);
		}
		if (keyb == 'q')
			break;
	}

	printf("\n");
}

void print_header(struct Header_d hd)
{
	printf("Header: \n");
	printf("id: %x,\nversion: %d", hd.id_n, hd.version);
	printf("\n");
	print_schema(*hd.sch_d);
}

size_t compute_size_header(void *header)
{
	size_t sum = 0;

	struct Header_d *hd = (struct Header_d *)header;

	sum += sizeof(hd->id_n) + sizeof(hd->version) + sizeof(hd->sch_d->fields_num) + sizeof(hd->sch_d);
	int i = 0;

	for (i = 0; i < hd->sch_d->fields_num; i++) {
		sum += strlen(hd->sch_d->fields_name[i]);

		sum += sizeof(hd->sch_d->types[i]);
	}

	sum += hd->sch_d->fields_num; /* acounting for n '\0' */
	return sum;
}

static int is_input_correct(char names[][MAX_FIELD_LT],int fields_num)
{
	int i;
	for (i = 0; i < fields_num; i++){
		if (names[i][0] == '\0') {
			fprintf(stderr,"(%s): invalid input.\n",prog);
			fprintf(stderr,"(%s): input syntax: fieldName:TYPE:value\n",prog);
			return 0;
		}else if(strstr(names[i], "TYPE STRING")||
				strstr(names[i], "TYPE LONG") ||
				strstr(names[i], "TYPE INT") ||
				strstr(names[i], "TYPE BYTE") ||
				strstr(names[i], "TYPE FLOAT") ||
				strstr(names[i], "TYPE PACK") ||
				strstr(names[i], "TYPE DOUBLE") ||
				strstr(names[i], "TYPE DATE") ||
				strstr(names[i], "TYPE KEY") ||
				strstr(names[i], "TYPE FILE") ||
				strstr(names[i], "TYPE ARRAY INT") ||
				strstr(names[i], "TYPE ARRAY FLOAT") ||
				strstr(names[i], "TYPE ARRAY LONG") ||
				strstr(names[i], "TYPE ARRAY STRING") ||
				strstr(names[i], "TYPE ARRAY BYTE") ||
				strstr(names[i], "TYPE ARRAY DOUBLE") ||
				strstr(names[i], "TYPE SET BYTE") ||
				strstr(names[i], "TYPE SET INT") ||
				strstr(names[i], "TYPE SET LONG") ||
				strstr(names[i], "TYPE SET DOUBLE") ||
				strstr(names[i], "TYPE SET FLOAT") ||
				strstr(names[i], "TYPE SET STRING") ||
				strstr(names[i], "TYPE_STRING")||
				strstr(names[i], "TYPE_LONG") ||
				strstr(names[i], "TYPE_INT") ||
				strstr(names[i], "TYPE_BYTE") ||
				strstr(names[i], "TYPE_FLOAT") ||
				strstr(names[i], "TYPE_PACK") ||
				strstr(names[i], "TYPE_DOUBLE") ||
				strstr(names[i], "TYPE_DATE") ||
				strstr(names[i], "TYPE_KEY") ||
				strstr(names[i], "TYPE_FILE") ||
				strstr(names[i], "TYPE_ARRAY_INT") ||
				strstr(names[i], "TYPE_ARRAY_FLOAT") ||
				strstr(names[i], "TYPE_ARRAY_LONG") ||
				strstr(names[i], "TYPE_ARRAY_STRING") ||
				strstr(names[i], "TYPE_ARRAY_BYTE") ||
				strstr(names[i], "TYPE_ARRAY_DOUBLE") ||
				strstr(names[i], "TYPE_SET_BYTE") ||
				strstr(names[i], "TYPE_SET_INT") ||
				strstr(names[i], "TYPE_SET_LONG") ||
				strstr(names[i], "TYPE_SET_DOUBLE") ||
				strstr(names[i], "TYPE_SET_FLOAT") ||
				strstr(names[i], "TYPE_SET_STRING") ||
				strstr(names[i], ":t_s") ||
				strstr(names[i], ":t_l") ||
				strstr(names[i], ":t_i") ||
				strstr(names[i], ":t_b") ||
				strstr(names[i], ":t_pk") ||
				strstr(names[i], ":t_f") ||
				strstr(names[i], ":t_d") ||
				strstr(names[i], ":t_ad") ||
				strstr(names[i], ":t_ai") ||
				strstr(names[i], ":t_al") ||
				strstr(names[i], ":t_af") ||
				strstr(names[i], ":t_ab") ||
				strstr(names[i], ":t_as") || 
				strstr(names[i], ":t_sd") ||
				strstr(names[i], ":t_si") ||
				strstr(names[i], ":t_sl") ||
				strstr(names[i], ":t_sf") ||
				strstr(names[i], ":t_sb") ||
				strstr(names[i], ":t_ss") || 
				strstr(names[i], ":t_fl")) { 
					fprintf(stderr,"(%s): invalid input.\n",prog);
					fprintf(stderr,"(%s): input syntax: fieldName:TYPE:value\n",prog);
					return 0;
				}
		if (strlen(names[i]) > MAX_FIELD_LT) {
			printf("invalid input.\n");
			printf("one or more filed names are too long.\n");
			return 0;
		}
	}

	return 1;
}

static int enforce_constraints(struct Schema *sch, int i, int found,struct Record_f *rec)
{
	if(sch->constraints[i] & CONST_NOT_NULL 
			&& !(sch->constraints[i] & CONST_DEFAULT) 
			&& !found){
		fprintf(stderr,"(%s): field -> '%s' must not be NULL,please provide a value.\n",prog,sch->fields_name[i]);
		return -1;
	}else if(!found && sch->constraints[i] & CONST_DEFAULT){
		char b[30] = {0};
		switch(sch->types[i]){
			case TYPE_INT: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%d",*(int*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] , b,1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_LONG: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%d",*(long*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] , b,1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_BYTE: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%d",*(unsigned char*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] ,b, 1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_FLOAT: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%.2f",*(float*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] ,b,1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_DOUBLE: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%.2f",*(double*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] ,b,1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_STRING: 
				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] , (char*)sch->defaults[i],1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			case TYPE_DATE: 
				{
					ui32 n = *(ui32*)sch->defaults[i];
					char date_buff[11] = {0};
					convert_number_to_date(date_buff,n);

					if (!set_field(rec, i, sch->fields_name[i],sch->types[i] ,date_buff,1)) {
						printf("set_field failed %s:%d.\n", F, L - 2);
						return -1;
					}
					break;
				}
			case TYPE_KEY: 
				memset(b,0,30);
				if(copy_to_string(b,30,"%d",*(ui32*)sch->defaults[i]) < 0){
					fprintf(stderr,"(%s): copy_to_string() failed, %s:%d.\n",prog,__FILE__,__LINE__-1);
					return -1;
				}

				if (!set_field(rec, i, sch->fields_name[i],sch->types[i] , b,1)) {
					printf("set_field failed %s:%d.\n", F, L - 2);
					return -1;
				}
				break;
			default:
				return -1;
		}
	}
	return 0;
}

/* 
 **
 ** if the field update has a unique costrain, 
 ** we have to delete the key at index one and replace it with 
 ** the new field value. 
 ** 
 */
static int unique_constraint_update(file_t *fds, struct Record_f *rec, struct Record_f *old_rec, int index, char *file_path)
{
	/*LOAD ALL INDEX*/
	HashTable *ht = NULL;
	int inx = 0;
	if (!read_all_index_file(fds[0], &ht, &inx)) {
		fprintf(stderr,"cannot read index file. %s:%d\n",__FILE__,__LINE__);
		free_ht_array(ht, inx);
		return -1;
	}
	
	switch(old_rec->fields[index].type){
	case TYPE_LONG:
	{
#if defined(__linux__) || defined(__APPLE__)
		Node *n = ht_delete((void*)&old_rec->fields[index].data.l,&ht[1],UINT);
#elif defined(_WIN32) || defined(_WIN64)
		Node *n = ht_delete((void*)&old_rec->fields[index].data.l,&ht[1],UINT_KEY);
#endif
		assert(n != NULL);
		free_ht_node(n);
		break;
	}
	case TYPE_DOUBLE:
	{

		ui64 d = htonf(rec->fields[index].data.d);
		ui32 f = (ui32)d;
#if defined(__linux__) || defined(__APPLE__)
		Node *n = ht_delete((void*)&f,&ht[1],UINT);
#elif defined(_WIN32) || defined(_WIN64)
		Node *n = ht_delete((void*)&f,&ht[1],UINT_KEY);
#endif
		assert(n != NULL);
		free_ht_node(n);
		break;
	}
	case TYPE_FLOAT:
	{
		ui32 f = htonf(rec->fields[index].data.f);
#if defined(__linux__) || defined(__APPLE__)
		Node *n = ht_delete((void*)&f,&ht[1],UINT);
#elif defined(_WIN32) || defined(_WIN64)
		Node *n = ht_delete((void*)&f,&ht[1],UINT_KEY);
#endif
		assert(n != NULL);
		free_ht_node(n);
		break;
	}
	case TYPE_INT:
	{
#if defined(__linux__) || defined(__APPLE__)
		Node *n = ht_delete((void*)&old_rec->fields[index].data.i,&ht[1],UINT);
#elif defined(_WIN32) || defined(_WIN64)
		Node *n = ht_delete((void*)&old_rec->fields[index].data.i,&ht[1],UINT_KEY);
#endif
		assert(n != NULL);
		free_ht_node(n);
		break;
	}
	case TYPE_STRING:
	{
#if defined(__linux__) || defined(__APPLE__)
		Node *n = ht_delete((void*)old_rec->fields[index].data.s,&ht[1],STR);
#elif defined(_WIN32) || defined(_WIN64)
		Node *n = ht_delete((void*)old_rec->fields[index].data.s,&ht[1],STR_KEY);
#endif
		assert(n != NULL);
		free_ht_node(n);
		break;
	}
	default:

	}
	switch(rec->fields[index].type){
		case TYPE_INT:
#if defined(__linux__) || defined(__APPLE__)
			if(set_tbl(ht,(void*)&rec->fields[index].data.i,old_rec->offset,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
			if(set_tbl(ht,(void*)&rec->fields[index].data.i,old_rec->offset,UINT_KEY,1) == -1)
#endif
			{
				fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[index].field_name);
				free_ht_array(ht, inx);
				return -1;
			}
			break;
		case TYPE_LONG:
			{
				if(rec->fields[index].data.l > (long)UINT_MAX){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					free_ht_array(ht, inx);
					return -1;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(ht,(void*)&rec->fields[index].data.l,old_rec->offset,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(ht,(void*)&rec->fields[index].data.l,old_rec->offset,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[index].field_name);
					free_ht_array(ht, inx);
					return -1;
				}
				break;
			}
		case TYPE_FLOAT:
			{
				ui32 f = htonf(rec->fields[index].data.f);
				if(f > (ui32)MAX_KEY){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					free_ht_array(ht, inx);
					return -1;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(ht,(void*)&f,old_rec->offset,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(ht,(void*)&f,old_rec->offset,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[index].field_name);
					free_ht_array(ht, inx);
					return -1;
				}
				break;
			}
		case TYPE_DOUBLE:
			{
				ui64 d = htonf(rec->fields[index].data.d);
				if(d > (ui64)MAX_KEY){
					fprintf(stderr,"(%s): key out of range. %s:%d.\n",prog,__FILE__,__LINE__-2);
					free_ht_array(ht, inx);
					return -1;
				}

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(ht,(void*)d,old_rec->offset,UINT,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(ht,(void*)d,old_rec->offset,UINT_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[index].field_name);
					free_ht_array(ht, inx);
					return -1;
				}
				break;
			}
		case TYPE_STRING:
			{

#if defined(__linux__) || defined(__APPLE__)
				if(set_tbl(ht,rec->fields[index].data.s,old_rec->offset,STR,1) == -1)
#elif defined(_WIN32) || defined(_WIN64)
				if(set_tbl(ht,rec->fields[index].data.s,old_rec->offset,STR_KEY,1) == -1)
#endif
				{
					fprintf(stderr,"(%s): field '%s' must be unique!\n",prog,rec->fields[index].field_name);
					free_ht_array(ht, inx);
					return -1;
				}
				break;
			}
		default:
			fprintf(stderr,"(%s): type of field '%s' is %s, not yet implemented or supported!\n",
					prog,
					rec->fields[index].field_name,
					type_to_str(rec->fields[index].type));
			free_ht_array(ht, inx);
			return -1;
	}


	char files[3][MAX_FILE_PATH_LENGTH] = {0};
	three_file_path(file_path,files);
	if(write_index(fds,inx,ht,files[0]) == -1){
		fprintf(stderr,"(%s): write_index() failed. %s:%d.\n",prog,__FILE__,__LINE__-1);
		return -1;
	}

	free_ht_array(ht, inx);
	return 0 ;	
}
