#ifndef INPUT_H
#define INPUT_H 1

#include "record.h"
#include "string_utilities.h"

#define ALL_OP "all" /*use to delete the all file content*/
#define ADD_AR_OP "aar" /* specificly append to the field array*/
#define FORCE "force" /* force the operation on the database */

void print_usage(char *argv[]);

int check_input_and_values( struct String schema_def, 
							struct String file_path, 
							struct String data_to_add,
							struct String key,
							char *argv[],
                          	unsigned char del, 
							unsigned char list_def, 
							unsigned char new_file,
                           	unsigned char update,
							unsigned char del_file,
							unsigned char build,
                           	unsigned char create,
							unsigned char options,
							unsigned char index_add,
			   				unsigned char file_field,
							unsigned char import_from_data,
							unsigned char journal_display,
							unsigned char nr_of_record_display,
							unsigned char del_field,
							unsigned char modify_schema,
							unsigned char swap_index);

void print_types(void);
int convert_options(char *options);
typedef enum
{
    ALL, /*delete the all file*/
    AAR, /*append to array*/
    FRC, /*Force operation */
} Option_t;

#endif
