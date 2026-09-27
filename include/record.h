#ifndef RECORD_H
#define RECORD_H

#include "db_types.h"
#define MAX_FILE_NAME_LEN 256

/*default dynamic array size*/
#define DEF_SIZE 1

#define HEADER_ID_SYS 0x657A3234
#define VS 1
#define MAX_HD_SIZE 7232
#define MAX_FIELD_NR 200 /*max field nr in a file */
#define MAX_FIELD_LT 32	 /*max char length for a field name*/

struct Schema {
	ui16 fields_num;
	char **fields_name;
	int *types;
	ui8 *is_dropped;
	ui8 *constraints;
	void **defaults; 
	ui8 (*has_unique)(ui8*,int,int*);
};/*48 b*/

struct Header_d{
	ui32 id_n;
	ui16 version;
	struct Schema *sch_d;
};/*16 b*/

enum Constraint{
	CONST_NO = 0,
	CONST_FOREIGN_KEY = 2,
	CONST_NOT_NULL = 4,
	CONST_UNIQUE = 8,
	CONST_DEFAULT = 16,
};

enum ValueType
{
	TYPE_INT,
	TYPE_LONG,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BYTE,
	TYPE_PACK,
	TYPE_DOUBLE,
	TYPE_ARRAY_INT,
	TYPE_ARRAY_LONG,
	TYPE_ARRAY_FLOAT,
	TYPE_ARRAY_STRING,
	TYPE_ARRAY_BYTE,
	TYPE_ARRAY_DOUBLE,
	TYPE_FILE,
	TYPE_DATE,
	TYPE_KEY,
	TYPE_SET_INT,
	TYPE_SET_LONG,
	TYPE_SET_FLOAT,
	TYPE_SET_STRING,
	TYPE_SET_BYTE,
	TYPE_SET_DOUBLE,
	/*THESE ARE NOT DATABASE TYPE -- used to make different OS calls compatible*/
	LOW_T_U64,
	LOW_T_U32,
	LOW_T_U8,
	LOW_T_STRING
};

struct Record_f;	

struct array{
	union {
		int *i;
		long *l;
		float *f;
		char **s;
		unsigned char *b;
		double *d;
	} elements;
	int size;
	int is_set;
	int (*insert)(void *, struct array *, enum ValueType);
	void (*destroy)(struct array *, enum ValueType);
};/*28 b*/


struct File {
	struct Record_f *recs;
	ui32 count;	
};/*?? bytes*/
	
/*TODO think if it make sense find a way to make the type flexable,
 * and that can change from each write to file */
struct Field {
	char field_name[MAX_FIELD_LT];
	int type;
	int is_dropped;
	union {
		int i;
		long l;
		float f;
		char *s;
		ui32 k;
		ui32 p;
		ui32 date;
		unsigned char b;
		double d;
		struct array v;
		struct File file;
	}data;
};/*72 b*/

struct Record_f {
	char file_name[MAX_FILE_NAME_LEN];
	file_offset offset; /*8 bytes*/
	int fields_num;
	ui8 *field_set; /*8 bytes*/
	struct Field *fields;
	ui32 count;
	struct Record_f *next;
};


char *type_to_str(int type);
int write_field_to_record(char *field_name,struct Record_f *rec,void *data, int type);
int init_array(struct array **v, enum ValueType type);
int insert_element(void *element, struct array *v, enum ValueType type);
void free_dynamic_array(struct array *v, enum ValueType type);
int create_record(char *file_name, struct Schema sch, struct Record_f *rec);
unsigned char set_field(struct Record_f *rec, int index, char *field_name, enum ValueType type, char *value,ui8 field_bit);
void free_record(struct Record_f *rec, int fields_num);
void print_record(int count, struct Record_f recs);
void free_record_array(int len, struct Record_f **recs);
void free_array_of_arrays(int len, struct Record_f ****array, int *len_ia, int size_ia);
unsigned char copy_rec(struct Record_f *src, struct Record_f *dest, struct Schema *sch);
unsigned char get_index_rec_field(char *field_name, struct Record_f *rec,int *field_i_r, int *rec_index);
int schema_has_type(struct Header_d *hd);
int compare_rec(struct Record_f *src, struct Record_f *dest,int option);
int copy_schema(struct Schema *src,struct Schema *dest);
int set_schema(char names[][MAX_FIELD_LT], int *types_i, struct Schema *sch, int fields_c, int *constraints, char **def_value);
int free_schema(struct Schema *sch);
void free_type_file(struct Record_f *rec,int optimized);
int parse_record_to_json(struct Record_f *rec,char **buffer);
int drop_field(struct Schema *s, char *fields);
int change_fields_name(char *buffer,struct Schema *sch);
ui8 has_constrain_unique(ui8 *constraints, int field_num,int *field_unique);
int copy_fields(struct Field *src,struct Field *dest);
int combine_old_and_new_rec(char *file_name,struct Record_f *old, struct Record_f *new,struct Schema sch);

#endif /*record.h*/
