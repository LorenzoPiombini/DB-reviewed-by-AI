#ifndef STR_OP_H
#define STR_OP_H
#include "record.h"
#include "common.h"

#define TYPE_ ":TYPE_"
#define T_ ":t_"
#define CON_ ":CONST_"
#define C_ ":ct_"

#define MAX_KEY 4294967296

#define MAX_FILED_LT 32

/*operations on files */
#define FCRT 0 /* when create a file*/
#define FWRT 1 /* when writing to a file*/

/* input modes */
#define WR 128 
#define DF 64
#if defined(_WIN32)
	#define CONSTR 32
#else
	#define CONST 32
#endif

#define NO_TYPE 0
#define TYPE 1
#define HYB 3
#define HYB_WR (HYB | WR)	
#define TYPE_WR (TYPE | WR)	
#define NO_TYPE_WR (NO_TYPE | WR)	
#define HYB_DF (HYB | DF) 
#define TYPE_DF (TYPE | DF) 
#define NO_TYPE_DF (NO_TYPE | DF)		




#if defined(_WIN32)
	#define NO_TYPE_CONST (NO_TYPE | CONSTR)
	#define TYPE_CONST (TYPE | CONSTR)
	#define HYB_CONST (HYB | CONSTR)
#else
	#define NO_TYPE_CONST (NO_TYPE | CONST)
	#define TYPE_CONST (TYPE | CONST)
	#define HYB_CONST (HYB | CONST)
#endif

#define ASCII_INT_MAX 			526     /*the sum of ASCII values for every char in INT_MAX*/
#define ASCII_INT_MIN 			572     /*the sum of ASCII values for every char in INT_MIN*/
#define ASCII_LONG_MAX 			1000    /*the sum of ASCII values for every char in LONG_MAX*/
#define ASCII_LONG_MIN 			1573	/*the sum of ASCII values for every char in LONG_MIN*/
#define ASCII_FLT_MAX 			2377    /*the sum of ASCII values for every char in FLT_MAX*/
#define ASCII_FLT_MIN 			382     /*the sum of ASCII values for every char in FLT_MIN*/
#define ASCII_FLT_MAX_NEGATIVE 	2422  	/*the sum of  ASCII values for every char in FLT_MAX butwith the sign -*/
#define ASCII_DBL_MAX 			16599   /*the sum of the ASCII values for every char in DBL_MAX*/
#define ASCII_DBL_MAX_NEGATIVE 	16644 	/*the sum of  ASCII values for every char in DBL_MAX butwith the sign -*/
#define ASCII_DBL_MIN 			382     /*the sum of the ASCII values for every char in DBL_MIN*/

/*these value are used as return values to identify which range the software should store
the number in e.i (int or long)*/
#define IN_LONG 	100
#define IN_INT 		101
#define IN_FLOAT 	102
#define IN_DOUBLE 	103


#define is_number_type(type) (type == TYPE_LONG || type == TYPE_FLOAT \
				|| type == TYPE_DOUBLE || type == TYPE_INT || type == TYPE_DATE || type == TYPE_KEY \
				|| type == TYPE_BYTE) 

#define is_number_array(type) (type == TYPE_ARRAY_DOUBLE || type == TYPE_ARRAY_LONG \
				|| type == TYPE_ARRAY_BYTE || type == TYPE_ARRAY_FLOAT \
				|| type == TYPE_ARRAY_INT)

#define return_first_char(str) str[0]


char *exclude_sub_str(char *start_delim,char *end_delim, char *src,int loop);
char *get_sub_str(char *start_delim, char *end_delim, char *str,int loop);
int get_array_values(char *src, char ***values);
int is_num(char *key);
void *key_converter(char *key, int *key_type);
int three_file_path(char *file_path, char files[][MAX_FILE_PATH_LENGTH]);
int count_fields(char *fields, const char *user_target);
int get_type(char *s);
int get_fileds_name(char *fields_name, int fields_count, int steps, char names[][MAX_FILED_LT]);
int get_fields_name_with_no_type(char *fields_name, char names[][MAX_FILED_LT]);
unsigned char check_fields_integrity(char names[][MAX_FILED_LT], int fields_count);
int get_value_types(char *fields_input, int fields_count, int steps, int *types);
void free_strs(int fields_num, int count, ...);
int get_values_hyb(char *buff,char ***values,  int fields_count);
char **get_values_with_no_types(char *buff,int fields_count);
char **get_values(char *fields_input, int fields_count);
int is_file_name_valid(char *str);
void strip(const char c, char *str);
void replace(const char c, const char with, char *str);
char return_last_char(char *str);
size_t digits_with_decimal(float n);
unsigned char is_floaintg_point(char *str);
unsigned char is_integer(char *str);
float __round_alt(float n);
unsigned char is_number_in_limits(char *value);
int get_constrains(char *buff, int field_count,int **cnstr,char ***value);
int map_constraints(char *c);
int find_last_char(const char c, char *src);
int assign_type(char *value);
int get_names_with_no_type_skip_value(char *buffer, char names[][MAX_FIELD_LT]);
int check_handle_input_mode(char *buffer, int op);
int get_name_types_hybrid(int mode,char *buffer, char names[][MAX_FILED_LT],int *types_i);
int check_array_input(char *value);
int count_delim(char *delim, char *str);
int find_double_delim(char *delim, char *str, int *pos, struct Schema sch);
int find_delim_in_fields(char *delim, char *str, int *pos, struct Schema sch);
char *find_field_to_reset_delim(int *pos, char *buffer);
void pack(ui32 n, ui8 *digits_indexes);
long long unpack(ui8 *digits_index);
void print_pack_str(ui8 *digits_index);
void clear_tok();
char *tok(char *str, char *delim);
char *duplicate_str(char *str);
int extract_numbers_from_string(char *buff,size_t size,char *format,...);

#endif /* STR_OP_H */
