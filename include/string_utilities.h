#ifndef _STRING_UTILS_H
#define _STRING_UTILS_H


#include "db_types.h"

extern i64 error_value;
#define INVALID_VALUE 22 /*EINVAL*/

#define STR_BASE 1024
struct String{
	char base[STR_BASE];
	char *str;	
	size_t size;
	int (*append)			(struct String*,char *);
	void (*close)			(struct String*);
	i8 (*is_empty)			(struct String*);
};


int init(struct String *str,char *val);
size_t string_length(const char *str);
char *find_needle(const char *haystack,const char *needle);				/*strstr*/
int extract_numbers_from_string(char *buff,size_t size,char *format,...);	/*sscanf*/

/*mode for string_compare function*/
#define COMPARE_STANDARD 0
#define COMPARE_CONTAIN 1 

int string_compare(char *src, char *dest, size_t size,int mode); /*strncmp*/
int complementary_span(const char *s, const char *reject);


/*conversions */
int long_to_string(long n, char *buff);
int copy_to_string(char *buff,size_t size,char *format,...); /*snprintf*/
int double_to_string(double d, char *buff);
long string_to_long(char *str);
double string_to_double(char *str);


/*helpers*/
size_t number_of_digit(long n);
size_t digits_with_decimal(float n);

int copy_to_string(char *buff,size_t size,char *format,...);
long power(long n, int pow);

#endif
