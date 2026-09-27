#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "date.h"
#include "debug.h"
#include "str_op.h"
#include "string_utilities.h"


enum date_delim{
	DOT,
	SLASH,
	DASH
};

static int is_valid_date(char *date);
static int detect_date_format(char *date);

unsigned char is_date_this_week(char* str_d,int format)
{
	/* take current time to understand which week are we in*/
	struct tm* current_t = get_now(); 
 
	/*put the input date in the tm struct from time.h*/
	struct tm input_date;
	memset(&input_date,0,sizeof(struct tm));

	if(!convert_date_str(format,str_d, &input_date)){
		printf("error date convert. %s:%d.\n",F,L-2);
		return 0;
	}	

	if(mktime(&input_date) == -1)
	{
		printf("mktime failed. %s:%d.\n",F,L-2);
		return 0;
	}

	return get_week_number(current_t) == get_week_number(&input_date);
}

int get_week_number(struct tm* time_in)
{
	int day_y = time_in->tm_yday + 1;
	int day_w = time_in->tm_wday;
	int week_n;
	
	/*ISO week the first day is monday */
	/*this way the program knows that 0 is monday and 6 is sunday*/
	if(day_w == 0)
		day_w = 6;
	else
		day_w -= 1;


	week_n = (day_y - day_w + ISO_W_ADJ) / 7;
	return week_n;
}

static int detect_date_format(char *date){
	
	/*detect the dilimeter*/	
	int res = is_valid_date(date);
	switch(res){
	case DOT:
	{
		int i;
		char *p = date;
		for(i = 0; *p != '.' && *p; p++,i++);

		if(i == 4)
			return YYYY_MM_DD;
		break;
	}
	case SLASH:
	{
		int i;
		char *p = date;
		for(i = 0; *p != '\\' && *p; p++,i++);

		if(i == 4)
			return YYYY_MM_DD;
		break;
	}
	case DASH:
	{
		int i;
		char *p = date;
		for(i = 0; *p != '-' && *p; p++,i++);

		if(i == 4)
			return YYYY_MM_DD;
		break;
	}
	default:
		return -1;
	}
	return 2;
}

int convert_date_str(int format, char *str, struct tm* input_date)
{
	int year = 0, day = 0, month = 0;
	if(is_valid_date(str) == -1) return -1;

	if(format == YYYY_MM_DD){
		if(extract_numbers_from_string(str,strlen(str),"%d-%d-%d",&year,&month, &day) == -1){
			printf("date convert failed, %s:%d.\n", F,L-2);
			return -1;
		}
	}else if(format == DD_MM_YYYY){
		if(extract_numbers_from_string(str,strlen(str),"%d-%d-%d",&day,&month, &year) == -1){
			printf("date convert failed, %s:%d.\n", F,L-2);
			return -1;
		}
	} else{
		if(extract_numbers_from_string(str,strlen(str),"%d-%d-%d",&month, &day, &year) == -1) {
			printf("date convert failed, %s:%d.\n", F,L-2);
			return -1;
		}
	}

	input_date->tm_mday = day;
	input_date->tm_mon = month - 1;
	if (year < 99)
		year += 2000;
	
	input_date->tm_year = year - 1900;
	input_date->tm_isdst = -1;
#if PROD || defined(__APPLE__)
	input_date->tm_zone = "EST";
#elif defined(__linux__)
	input_date->__tm_zone = "EST";
#endif

	return 0;
}

unsigned char extract_date(char* key, char *date)
{
	
	size_t first_d = strcspn(key,"-");
	size_t end = strcspn(key,"e");
	
	if(first_d == strlen(key)) {
		printf("no date in the key. %s:%d.\n",F,L-2);
		return 0;
	}
	
	size_t start = first_d - 2;	

	size_t i = 0;
	int j = 0;
	size_t size = end - start + 1;
	char cpy_dt[size];
	memset(cpy_dt,0,size);

	for(i = start, j = 0; i < end; i++) {
		cpy_dt[j] = key[i];
		j++;
	}
		
	strncpy(date,cpy_dt,size);
	return 0;
}

unsigned char w_day(long seconds)
{
	if(!seconds){
		time_t now = time(NULL);
		struct tm* curr_t = localtime(&now); 
		unsigned char day = 0;
		if(curr_t->tm_wday == 0) {
			day = 6;
		}else {
			day = (unsigned char)curr_t->tm_wday - 1;
		}

		return day;
	}else{
#if defined(_WIN32) || defined(_WIN64)
		time_t s = (time_t)seconds;
		struct tm* curr_t = localtime(&s); 
#else
		struct tm* curr_t = localtime(&seconds); 
#endif
		unsigned char day = 0;
		if(curr_t->tm_wday == 0) {
			day = 6;
		}else {
			day = (unsigned char)curr_t->tm_wday - 1;
		}
		return day;
	}
}	

struct tm* get_now(void)
{
	time_t now = time(NULL);
	return localtime(&now);
}

struct tm* mk_tm_from_seconds(long time)
{
	time_t t = (time_t)time;
	return localtime(&t);
}

unsigned char is_today(long time)
{
	struct tm*  time_on_file = mk_tm_from_seconds(time);

	/*-- this lines are needed because the way localtime() function works	 --*/
	/*-- every time you use localtime() the static allocated struct tm *  	 --*/
	/*-- will be overwritten, if we do not store the data of time_on_file    --*/
	/*-- now and time_on_file will be pointing at the same memory location   --*/
	/*-- returning always 1 (TRUE), which will not be ideal for timecard ops --*/
	
	int tof_m = time_on_file->tm_mon;
	int tof_md = time_on_file->tm_mday;

	/*------------------------------------------------------------------------*/

	struct tm* now = get_now();
	return (now->tm_mon == tof_m) && (now->tm_mday == tof_md);
}

long now_seconds()
{
	return(long)time(NULL);
}


/*
 * if format with 4 digits year the date_str must be  11 char
 * 	pass 11 char array all the time!
 * */
int create_string_date(long time, char* date_str, int format)
{
	time_t time_tm = (time_t) time;
	struct tm *date_t = localtime(&time_tm);
	
	
	memset(date_str,0,11); 

	char day[3];
	char month[3];
	char year[3];

	memset(day,0,3);
	memset(month,0,3);
	memset(year,0,3);
	if(date_t->tm_mday < 10 && date_t->tm_mon < 9){
		/*create string day*/
		day[0] = '0';	
		day[1] = date_t->tm_mday + '0';
		month[0] = '0';
		month[1] = (date_t->tm_mon + 1) + '0';
		if(format == YYYY_MM_DD){
			if(copy_to_string(date_str,10,"%d-%s-%s",
						date_t->tm_year + 1900, 
						month,
						day) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else if(format == DD_MM_YYYY){
			if(copy_to_string(date_str,10,"%s-%s-%d",
						day,
						month,
						date_t->tm_year + 1900) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else{
			if(copy_to_string(date_str,9,"%s-%s-%d",
						month,
						day,
						date_t->tm_year - 100) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}
		return 0;
	}else if(date_t->tm_mday < 10){
		day[0] = '0';	
		day[1] = date_t->tm_mday + '0';

		if(format == YYYY_MM_DD){
			if(copy_to_string(date_str,10,"%d-%d-%s",
						date_t->tm_year + 1900,
						date_t->tm_mon + 1,
						day) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else if(format == DD_MM_YYYY){
			if(copy_to_string(date_str,10,"%s-%d-%d",
						day,
						date_t->tm_mon + 1,
						date_t->tm_year + 1900) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else{
			if(copy_to_string(date_str,9,"%d-%s-%d",date_t->tm_mon + 1,
						day,
						date_t->tm_year - 100) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}
		return 0;
	}else if(date_t->tm_mon < 9){
		month[0] = '0';
		month[1] = (date_t->tm_mon + 1) + '0';
		if(format == YYYY_MM_DD){
			if(copy_to_string(date_str,10,"%d-%s-%d",
						date_t->tm_year +1900,
						month,
						date_t->tm_mday) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else if(format == DD_MM_YYYY){
			if(copy_to_string(date_str,10,"%s-%d-%d",month,
						date_t->tm_mday,
						date_t->tm_year +1900) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}else{
			if(copy_to_string(date_str,10,"%s-%d-%d",month,
						date_t->tm_mday,
						date_t->tm_year -100) == -1){
				printf("copy_to_string failed, %s:%d.\n",F,L-2);
				return -1;
			}
		}
		return 0;
	}


	if(format == YYYY_MM_DD){
		if(copy_to_string(date_str,10,"%d-%d-%d",
					date_t->tm_year+1900,
					date_t->tm_mon+1,
					date_t->tm_mday) == -1){
			printf("copy_to_string failed, %s:%d.\n",F,L-2);
			return -1;
		}
	}else if(format == DD_MM_YYYY){
		if(copy_to_string(date_str,10,"%d-%d-%d",
					date_t->tm_mday,
					date_t->tm_mon+1,
					date_t->tm_year+1900) == -1){
			printf("copy_to_string failed, %s:%d.\n",F,L-2);
			return -1;
		}
	}else{
		if(copy_to_string(date_str,9,"%d-%d-%d",date_t->tm_mon+1,
					date_t->tm_mday,
					date_t->tm_year-100) == -1){
			printf("copy_to_string failed, %s:%d.\n",F,L-2);
			return -1;
		}
	}
	return 0;
}

long convert_str_date_to_seconds(char* date,int format)
{
	struct tm input_date;
	memset(&input_date,0,sizeof(struct tm));
	if(convert_date_str(format,date, &input_date) == -1) 
		return -1;

	return (long) mktime(&input_date);
}

static int is_valid_date(char *date)
{
	char *p = date;
	ui8 dash = 0;
	ui8 slash = 0;
	ui8 dot = 0;
	for(; *p;p++){
		if(isalpha(*p)) return -1;
		if(*p == ' ' || *p == '\t') return -1;
		if(*p == '\\' || *p == '/') slash++;
		if(*p == '.') dot++;
		if(*p == '-') dash++;
		if(isdigit(*p)) continue;
	}

	if (slash == 2) return SLASH;
	if (dot == 2) return DOT;
	if (dash == 2) return DASH;
	return -1;
}


ui32 convert_date_to_number(int format, char *date)
{
	
	long seconds = 0;
	if(format == -1){
		format = detect_date_format(date);
		if(format == -1) return 0;
	}
	if((seconds = convert_str_date_to_seconds(date,format)) == -1){
		fprintf(stderr,"convert_str_date_to_seconds failed, %s:%d\n",__FILE__,__LINE__-1);
		return 0;
	}
	return seconds / (SEC_IN_A_DAY);
}

int convert_number_to_date(char *date, int date_number)
{
	long second = date_number * SEC_IN_A_DAY;		
	/*date MUST be 11 char long (MM-DD-YYYY\0)*/
	if(create_string_date(second,date,-1) == -1 ) return -1;
	return 0;
}

long next_friday(long seconds)
{
#if defined(_WIN32) || defined(_WIN64)
	time_t s = (time_t) seconds;
	struct tm *date = localtime(&s);	
#else
	struct tm *date = localtime(&seconds);	
#endif
	switch(date->tm_wday){
	case 0: return (long) (seconds + (SEC_IN_A_DAY * 5));
	case 1: return (long) (seconds + (SEC_IN_A_DAY * 4));
	case 2: return (long) (seconds + (SEC_IN_A_DAY * 3));
	case 3: return (long) (seconds + (SEC_IN_A_DAY * 2));
	case 4: return (long) (seconds + (SEC_IN_A_DAY * 1));
	case 5: return (long) (seconds + (SEC_IN_A_DAY * 7));
	case 6: return (long) (seconds + (SEC_IN_A_DAY * 6));
	}
	return 0;
}

long rewind_to_first_of_this_month()
{
#if defined(_WIN32) || defined(_WIN64)
		time_t now = (time_t)now_seconds();
#else
		long now = now_seconds();
#endif
	struct tm *date = localtime(&now);
	/*rewind to first day of the month*/
	time_t first_day_of_this_month = now - (SEC_IN_A_DAY * (date->tm_mday - 1));
	return (long) first_day_of_this_month;
}


long third_friday_of_the_month(long *seconds)
{
	time_t first_day_of_this_month = 0;
	struct tm today = {0};
	struct tm *date = NULL;
	long now = 0;
	int day_to_3th_friday = 19;
	if(!seconds){
#if defined(_WIN32) || defined(_WIN64)
		time_t now = (time_t)now_seconds();
#else
		long now = now_seconds();
#endif
		date = localtime(&now);
		memcpy(&today,date,sizeof(struct tm));

		/*rewind to first day of the month*/
		first_day_of_this_month = now - (SEC_IN_A_DAY * (date->tm_mday - 1));

		date = localtime(&first_day_of_this_month);
		assert(date->tm_mday == 1);

	}else{
		first_day_of_this_month = (time_t) *seconds;
		date = localtime(&first_day_of_this_month);
		assert(date->tm_mday == 1);
	}

	time_t third_friday = 0;
	if(date->tm_wday == 0)
		third_friday = first_day_of_this_month + (day_to_3th_friday * SEC_IN_A_DAY);
	else if(date->tm_wday == 6)
		third_friday = first_day_of_this_month + ((day_to_3th_friday + 1) * SEC_IN_A_DAY);
	else if(date->tm_wday == 5)
		third_friday = first_day_of_this_month + ((day_to_3th_friday - date->tm_wday) * SEC_IN_A_DAY);
	else
		third_friday = first_day_of_this_month + ((day_to_3th_friday - (date->tm_wday + 1)) * SEC_IN_A_DAY);


	if(!seconds){
		if(now >= third_friday){
			/*compute next month 3rd friday*/
			time_t next_month_1st_day = now;
			switch(today.tm_mon){
				case 0: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 1:
						{
							int february_days = (today.tm_year + 1900) % 4 == 0 ? 29 : 28;
							next_month_1st_day += ((february_days - today.tm_mday) +1) * SEC_IN_A_DAY; 
							break;
						}
				case 2: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 3: next_month_1st_day += ((30 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 4: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 5: next_month_1st_day += ((30 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 6: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 7: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 8: next_month_1st_day += ((30 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 9: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 10: next_month_1st_day += ((30 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
				case 11: next_month_1st_day += ((31 - today.tm_mday) +1) * SEC_IN_A_DAY; break;
			}
			date = localtime(&next_month_1st_day);
			assert(date->tm_mday == 1);

			if(date->tm_wday == 0)
				third_friday = next_month_1st_day + (day_to_3th_friday * SEC_IN_A_DAY);
			else
				third_friday = next_month_1st_day + ((day_to_3th_friday - (date->tm_wday + 1)) * SEC_IN_A_DAY);
		}
	}

	return (long)third_friday; 
}

char *display_today(){
	time_t t = time(NULL);
	char *d = ctime(&t);
	d[strlen(d)-1] = '\0';
	return &d[0];
}

int is_past(int h,int m)
{
	time_t t = time(NULL);
	struct tm *date = localtime(&t);	
	if(date->tm_hour > h)
		return 1;
	else if(date->tm_hour == h && date->tm_min >= m)
		return 1;

	return 0;
}

int is_date_today(char *date, int format)
{

	time_t t = (time_t)convert_str_date_to_seconds(date,format);
	if(t == -1)
		return 0;

	struct tm *d = localtime(&t);
	struct tm verify = {0};
	memcpy(&verify,d,sizeof(struct tm));
	time_t now = time(NULL);
	d = localtime(&now);

	return (verify.tm_mday == d->tm_mday) && (d->tm_mon == verify.tm_mon) && (d->tm_year == verify.tm_year);
}

long third_friday_january_two_years_out()
{

#if defined(_WIN32) || defined(_WIN64)
	time_t sec = (time_t)rewind_to_first_of_this_month();
	sec += (2 * SEC_IN_A_YEAR);
#else
	long sec = rewind_to_first_of_this_month();
	sec += (2 * SEC_IN_A_YEAR);
#endif
	struct tm *dt = localtime(&sec);
	sec -= dt->tm_yday * SEC_IN_A_DAY;
	return third_friday_of_the_month((long*)&sec);
}
long third_friday_dicember_two_years_out()
{
#if defined(_WIN32) || defined(_WIN64)
	time_t sec = (time_t)rewind_to_first_of_this_month();
	sec += (2 * SEC_IN_A_YEAR);
#else
	long sec = rewind_to_first_of_this_month();
	sec += (2 * SEC_IN_A_YEAR);
#endif
	/*this goes to dicember two years out*/
	struct tm *dt = localtime(&sec);
	if(((dt->tm_year + 2000) % 4) == 0)
		sec += ((366-31 - dt->tm_yday ) * SEC_IN_A_DAY);
	else
		sec += ((365-31 - dt->tm_yday ) * SEC_IN_A_DAY);

	return third_friday_of_the_month((long*)&sec);
}

int convert_date_format(char *src, char *dest, int sform, int dform)
{
	time_t s = convert_str_date_to_seconds(src,sform);
	if(s == -1)
		return -1;

	if(create_string_date(s,dest,dform) == -1) 
		return -1;

	return 0;
}

int current_year()
{
	time_t t = time(NULL);	
	struct tm *dt = localtime(&t);
	return dt->tm_year + 1900;
}
