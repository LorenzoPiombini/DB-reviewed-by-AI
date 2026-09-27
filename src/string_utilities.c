#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "string_utilities.h"

/*static functions to manage String struct*/

static i8 empty(struct String *str);
static int append(struct String *str,char *str_to_appen);
static void free_string(struct String *str);

i64 error_value = -1;

static void free_string(struct String *str){
	memset(str->base,0,STR_BASE);
	if(str->str)
		free(str->str);

}

int init(struct String *str,char *val)
{
	if(!str) return -1;

	memset(str,0,sizeof(struct String));
	
	if(!val){
		str->append = append;
		str->is_empty = empty;
		str->close = free_string;
		return 0;
	}

	/*compute string size*/
	size_t i;
	char *p = val;
	for(i = 0; *p != '\0'; i++,p++);

	if(i > STR_BASE){
		str->str = (char*)malloc(i+1);
		if(!str->str){
			fprintf(stderr,"malloc failed.%s:%d.\n",__FILE__,__LINE__-2);
			return -1;
		}
		str->str[i] = '\0';
	}

	str->size = i;
	if(str->str)
		strncpy(str->str,val,i);
	else
		strncpy(str->base,val,i);

	str->append = append;
	str->is_empty = empty;
	str->close = free_string;
	return 0;
}

static int append(struct String *str, char *str_to_appen)
{
	if(!str_to_appen) 
		return -1;

	size_t nl = strlen(str_to_appen);

	if(str->str){
		char *n = (char*)realloc(str->str,str->size + nl  +1);
		if(!n){
			fprintf(stderr,"realloc() failed, %s:%d\n",__FILE__,__LINE__-2);	
			return -1;
		}

		str->str = n;
		str->str[str->size + nl] = '\0';
		strncpy(&str->str[str->size],str_to_appen,nl);
		str->size += nl;
		return 0;
	} else if((str->size + nl) >= STR_BASE){
		str->str = (char *) malloc(str->size + nl + 1);
		if(!str->str){
			fprintf(stderr,"malloc failed.%s:%d.\n",__FILE__,__LINE__-2);
			return -1;
		}
		str->str[str->size+nl] = '\0';
		strncpy(str->str,str->base,str->size);
		strncpy(&str->str[str->size],str_to_appen,nl);
		memset(str->base,0,STR_BASE);
		str->size += nl;
	}else if(str->size+nl < STR_BASE){
		strncpy(&str->base[str->size],str_to_appen,nl);
		str->size += nl;
	}

	return 0;
}

static i8 empty(struct String *str)
{
	if(str->str) return *str->str == '\0';
	if(str->base[0] == '\0') return 1; 
	return 0;
}


size_t number_of_digit(long n)
{
	if (n < 10)
	{
		return 1;
	}
	else if (n >= 10 && n < 100)
	{
		return 2;
	}
	else if (n >= 100 && n < 1000)
	{
		return 3;
	}
	else if (n >= 1000 && n < 10000)
	{
		return 4;
	}
	else if (n >= 10000 && n < 100000)
	{
		return 5;
	}
	else if (n >= 100000 && n < 1000000)
	{
		return 6;
	}
	else if (n >= 1000000 && n < 1000000000)
	{
		return 7;
	}
	else if (n >= 1000000000)
	{
		return 10;
	}

	return -1;
}

int copy_to_string(char *buff,size_t size,char *format,...)
{
	if(strlen(format) > size){
		fprintf(stderr,"buffer is not big enough %s:%d.\n",__FILE__,__LINE__);
		return -1;
	}

	va_list list; 
	long l = 0;
	char *string = 0x0;
	int precision = 0;
	double d = 0;

	va_start(list,format);
	size_t j;
	for(j = 0; *format != '\0';format++){
		if(*format == '%'){
			format++;
			
			for(;;){
				switch(*format){
				case 's':
				{
					string = va_arg(list,char*);
					for(; *string != '\0'; string++){
						if(j < size)
							buff[j] = *string;
						j++;
					}
					break;
				}
				case 'l':
					format++;
					continue;
				case 'u':
				case 'd':
				{
					l = va_arg(list,int);
					size_t sz = number_of_digit(l);
					/*1 id for '\0' and 1 is for a possible sign */
					char num_str[sz+2];
					memset(num_str,0,sz+2);
					long_to_string(l,num_str);	
					int i;
					for(i = 0; num_str[i] != '\0'; i++){
						if(j < size)
							buff[j] = num_str[i];
						j++;
					}
					break;
				}
				case 'f':
				{
					d = va_arg(list,double);
					
					size_t sz = number_of_digit(d < 0 ? (int)d * -1 : (int)d);
					/* 1 for the sign 15 digits after the . ,the . and '\0'  at the end*/
					char num_str[sz+18];
					memset(num_str,0,sz+18);
					double_to_string(d,num_str);	
					int i,pc,com;
					for(i = 0, pc = 0, com = 0; num_str[i] != '\0'; i++){
						if(precision && precision == pc){
							/*this is rounding the floating or double*/
							if(((int)num_str[i] - '0') > 5){
								if(((int)buff[j-1] - '0') == 9){
									buff[j-1] = '0';
								}else{
									int n = buff[j-1] - '0';
									buff[j-1] = (char)++n + '0';
								}
							}
							break;
						}
						if(com) pc++;
						if(num_str[i] == '.') com = 1;

						if(j < size)
							buff[j] = num_str[i];
						j++;
					}
					break;
				}
				case '.':
				{
					char num[4];
					memset(num,0,4);
					int k = 0;
					while(isdigit(*(++format))){
						num[k] = *format;	
						k++;
					}	

					precision = string_to_long(num);
					if(error_value == INVALID_VALUE){
						va_end(list);
						return -1;
					}

					continue;
				}
				default:
					break;
				}
				format++;
				break;
			}
		}	
		if(*format == '\0') break;
		if(j < size)
			buff[j] = *format;
		j++;
	}
	va_end(list);
	return 0;
}
/*sscanf*/
int extract_numbers_from_string(char *buff,size_t size,char *format,...)
{
	if(strstr(format,"%s")){
		fprintf(stderr,"invalid format string, unexpected '%%s'\nformat specifier acceptted %%ld %%d %%u\n");	
		return -1;
	}

	va_list list;
	va_start(list,format);
	size_t i;
	i8 is_long = 0;
	for(i = 0; *format != '\0'; format++){
		if(*format == '%'){
			format++;
			if(*format == '\0')break;
			for(;;){
				switch(*format){
				case 's':
					va_end(list);
					return -1;
				case 'l':
					format++;
					is_long = 1;
					continue;
				case 'd':
				{
					char number[15];
					memset(number,0,15);
					int j = 0;
				
					if(isdigit(buff[i])){
						while(isdigit(buff[i])){
							if(j < 15){
								number[j] = buff[i];
								j++;
							}else{
								va_end(list);
								fprintf(stderr,
									"number is too large: %s, has more than 15 digit\n",
									number);
								return -1;
							}
							i++;
							
							if( i >= size){
								va_end(list);
								return -1;
							}
						}		
					}else{

						while(!isdigit(buff[i])){
							i++;
							if(i > size){
								va_end(list);
								return -1;
							}
						}

						while(isdigit(buff[i])){
							if(j < 15){
								number[j] = buff[i];
								j++;
							}else{
								va_end(list);
								fprintf(stderr,
									"number is too large: %s, has more than 15 digit\n",
									number);
								return -1;
							}
							i++;
							
							if(i > size){
								va_end(list);
								return -1;
							}
						}
					}
					long l = string_to_long(number);
					if(error_value == INVALID_VALUE){
						va_end(list);
						fprintf(stderr,"string to number conversion failed, %s,%d.\n",
								__FILE__,__LINE__-4);
						return -1;
					}

					if(is_long){
						long *n = va_arg(list,long*);
						*n = l;
					}else{
						int *n = va_arg(list,int*);
						*n = l;
					}
					break;
				}	
				default:
					break;
				}
				format++;
				break;
			}
		}

		if(*format == '\0') break;
	}
	va_end(list);
	return 0;
}


int double_to_string(double d, char *buff){

	long integer = (long)d;
	double fraction = d - (double)integer;

	long_to_string(integer,buff);

	buff[strlen(buff)] = '.';

	if (d < 0){
		fraction *= -1;
		integer *= -1;
	}

	int i;
	int j = strlen(buff);
	int zeros_after_point = 15;
	if(d < 0) zeros_after_point -= 1;

	for(i = 0; i < zeros_after_point; i++){
		fraction *= 10;
		buff[j] =  ((int)fraction + '0');
		fraction = (double)(fraction - (int)fraction);
		j++;
	}

	return 0;
}

int long_to_string(long n, char *buff){

	int pos = 0;
	if(n < 0){
		pos++;
		buff[0] = '-';
		n *= -1;
	}
	pos += number_of_digit(n)-1;
	if(n == 0){
		buff[pos] = (char)(n + (int)'0');
	}
	while(n > 0){
		int m = n % 10;
		if(pos >= 0)
			buff[pos] = (char)(m + (int)'0');

		n /= 10;	
		pos--;
	}
	return 0;
}

long string_to_long(char *str)
{
	size_t l = strlen(str);
	if(l == 0) return 0;

	if(isspace(str[l-1])){
		str[l-1] = '\0';
		l--;
	}

	int at_the_power_of = l - 1;	
	long converted = 0;
	int negative = 0;
	for(; *str != '\0';str++){
		if(*str == '-'){
			at_the_power_of--;
			negative = 1;
			continue;
		}

		if(isalpha(*str)) {
			error_value = INVALID_VALUE;
			return -1;
		}


		long n = (long)*str - '0';
		if(at_the_power_of >= 0)
			converted += power(10,at_the_power_of) * n;
		at_the_power_of--;
	}

	return negative == 0 ? converted : converted * (-1);
}
double string_to_double(char *str)
{
	int comma = 0;
	int position = 0;
	char num_buff[30];
	memset(num_buff,0,30);

	int i;
	for(i = 0; *str != '\0';str++){
		if (i == 30) break;
		if(*str == '.') {
			comma = 1;
			continue;
		}

		if(isalpha(*str)){
			error_value = INVALID_VALUE;
			return -1;
		}

		if(i < 30)
			num_buff[i] = *str;

		if(comma) position++;
		i++;
	}

	long l = string_to_long(num_buff);
	if(comma){
		return ((double)l / power(10,position));
	}else{
		return (double) l;
	}
}

long power(long n, int pow)
{
	long a = n;
	if(pow == 0) return 1;
	
	int i;
	for(i = 1; i < pow; i++){
		a *= n;
	}

	return a;
}

int complementary_span(const char *s, const char *reject)
{
	
	char *f = strstr(s,reject);
	if(!f) return -1;

	return (int)((unsigned char*)f -(unsigned char*)s);
}

