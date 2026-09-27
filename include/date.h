#ifndef _DATE_H
#define _DATE_H

#include <time.h>
#include "db_types.h"

#define THREE_HOURS 60*60*3
#define TWENTY_MINUTES 60*20
#define ISO_W_ADJ 10
#define DD_MM_YYYY 0
#define YYYY_MM_DD 1
#define SEC_IN_A_DAY 60*60*24
#define SEC_IN_A_YEAR SEC_IN_A_DAY * 365

unsigned char is_date_this_week(char* str_d,int format);
int get_week_number(struct tm* time_in);
int convert_date_str(int format, char *str, struct tm* input_date);
unsigned char extract_date(char* key, char *date);
unsigned char w_day(long seconds);
struct tm* get_now(void);
struct tm* mk_tm_from_seconds(long time);
unsigned char is_today(long time);
long now_seconds();
int create_string_date(long time, char* date_str, int format);
long convert_str_date_to_seconds(char* date,int format);
long third_friday_of_the_month(long *seconds);
int convert_number_to_date(char *date, int date_number);
ui32 convert_date_to_number(int format,char *date);
long next_friday(long seconds);
char *display_today();
int is_past(int h,int m);
int is_date_today(char *date, int format);
long rewind_to_first_of_this_month();
long third_friday_dicember_two_years_out();
long third_friday_january_two_years_out();
int convert_date_format(char *src, char *dest, int sform, int dform);
int current_year();
#endif /* date.h */
