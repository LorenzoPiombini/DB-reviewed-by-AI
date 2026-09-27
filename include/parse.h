#ifndef PARSE_H
#define PARSE_H

/* for Record_f, ValueTypes */

#include "record.h"
#include "db_types.h"
#if defined(_WIN32) || defined(_WIN64) 
	#include <windows.h>
#endif

#define NO_TYPE 0

int parse_d_flag_input(
			char *file_path, 
			int fields_num, 
			char *buffer, 
			struct Schema *sch, 
			int check_sch,
			struct Record_f *rec, 
			int *pos,
			int update);

unsigned char perform_checks_on_schema(
					int mode,
					char *buffer, 
					int fields_count,
					char *file_path, 
					struct Record_f *rec, 
					struct Header_d *hd,
					int *pos,
					int option,
					int update
					);

int parse_input_with_no_type(
			char *file_path, 
			int fields_num, 
			char names[][MAX_FIELD_LT], 
			int *types_i, 
			char **values,
			struct Schema *sch, 
			int check_sch,
			struct Record_f *rec,
			int update);

int create_header(struct Header_d *hd);
int write_empty_header(file_t fd, struct Header_d *hd);
int write_header(file_t fd, struct Header_d *hd);
int read_header(file_t fd, struct Header_d *hd);
unsigned char ck_input_schema_fields(char names[][MAX_FIELD_LT], int *types_i, struct Header_d *hd);
unsigned char check_schema(int fields_n, char *buffer,struct Header_d *hd);
int check_schema_with_no_types(char names[][MAX_FIELD_LT], struct Header_d hd,char **sorted_names);
int sort_input_like_header_schema(int schema_tp, 
					int fields_num, 
					struct Schema *sch, 
					char names[][MAX_FIELD_LT], 
					char ***values, 
					int *types_i);

unsigned char ck_schema_contain_input(char names[][MAX_FIELD_LT], int *types_i, struct Header_d *hd, int fields_num);
unsigned char add_fields_to_schema(int mode, int fields_num, char *buffer, struct Schema *sch);
int create_file_definition_with_no_value(int mode, int fields_num, char *buffer, struct Schema *sch);

unsigned char compare_old_rec_update_rec(struct Record_f **rec_old,
		struct Record_f *rec, 
		unsigned char check,
		int option,
		struct Header_d hd,
		file_t *fds,
		char *file_path);

void find_fields_to_update(
		struct Record_f **rec_old,
		char *positions,
		struct Record_f *rec,
		int option,
		struct Header_d hd,
		file_t *fds,
		char *file_path);
void print_schema(struct Schema sch);
void print_header(struct Header_d hd);
size_t compute_size_header(void *header);
char **extract_fields_value_types_from_input(char *buffer, char names[][MAX_FIELD_LT], int *types_i, int *count);
#endif
