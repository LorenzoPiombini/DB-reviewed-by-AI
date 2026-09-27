#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "input.h"
#include "str_op.h"


void print_usage(char *argv[])
{
        printf("Usage: %s -f <database file>\n", argv[0]);
        printf("\t -a - add record to a file.\n");
        printf("\t -n - create a new database file\n");
        printf("\t -f - [required] path to file (file name)\n");
        printf("\t -F - create a file to be use as a field\n");
        printf("\t -c - creates the files specified in the txt file.\n");
        printf("\t -D - specify the index where you want to delete the record.\n");
        printf("\t -R - define a file definition witout values.\n");
        printf("\t -k - specify the record id, the program will perform CRUD ops based on this id.\n");
        printf("\t -t - list of available types. this flag will exit the program.\n");
        printf("\t -l - list the file definition specified with -f.\n");
        printf("\t -u - update the file specified by -f .\n");
        printf("\t -e - delete the file specified by -f .\n");
        printf("\t -x - list the keys value for the file specified by -f .\n");
        printf("\t -b - specify the file name (txt,csv,tab delimited file) to build from .\n");
        printf("\t -B - specify the file name (txt,csv,tab delimited file) to import the data from .\n");
        printf("\t -o - add options to a CRUD operation .\n");
        printf("\t -s - specify how many buckets the HashTable (index) will have.\n");
        printf("\t -i - specify how many indexes the file will have.\n");
        printf("\t -A - add indexes -i to the file specified by -f .\n");
        printf("\t -I - create only the .dat file specified by -f .\n");
        printf("\t -j - display the journal of operations.\n");
        printf("\t -N - display the nr of record in the file.\n");
        printf("\t -d - drop one or more fields in the file.\n");
}

void print_types(void)
{
        printf("Avaiable types:\n");
        printf("\tTYPE_INT, integer number, %ld bytes (%ld bits).\n", sizeof(int), 8 * sizeof(int));
        printf("\tTYPE_FLOAT, floating point number, %ld bytes (%ld bits).\n",
               sizeof(float), 8 * sizeof(float));
        printf("\tTYPE_LONG, large integer number, %ld bytes (%ld bits).\n",
               sizeof(long), 8 * sizeof(long));
        printf("\tTYPE_STRING, text rappresentation, variable length. \"Hello\" is %ld bytes.\n",
               strlen("Hello"));
        printf("\tTYPE_BYTE, small unsigned integer number, %ld bytes (%ld bits).\n",
               sizeof(unsigned char), 8 * sizeof(unsigned char));
        printf("\tTYPE_DOUBLE, floating point number, %ld bytes (%ld bits).\n",
               sizeof(double), 8 * sizeof(double));
		printf("\tTYPE_DATE, unsigned integer, %ld bytes, (%ld bits).\n",sizeof(ui32),8*sizeof(ui32));
}

int check_input_and_values(struct String schema_def, struct String file_path, struct String data_to_add, struct String key, char *argv[],
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
							unsigned char swap_index)
{
	if((del_field && file_path.is_empty(&file_path)) 
			|| (del_field && ( del 					
						|| update 					
						|| del_file
						|| list_def 
						|| new_file 				
						|| !key.is_empty(&key)
						|| build
						|| create		 			
						|| index_add 
						|| modify_schema
						|| import_from_data
						|| nr_of_record_display
						|| swap_index))){
		printf("option -d must be used with -f.\n\n");
		printf("isam.db -f [file_name] -d [fields:that:you:want:to:drop].\n\n");
		print_usage(argv);
		return 0;
	}

	if(del_field)
		return 1;

	if(nr_of_record_display && file_path.is_empty(&file_path)){
		printf("option -N must be used with -f.\n\n");
		print_usage(argv);
		return 0;
	}

	if((modify_schema && file_path.is_empty(&file_path))
			|| ( modify_schema && (del_field 
				|| del 					
				|| update 					
				|| del_file
				|| list_def 
				|| new_file 				
				|| !key.is_empty(&key)
				|| build
				|| create		 			
				|| index_add 
				|| import_from_data
				|| nr_of_record_display
				|| swap_index))){
		printf("option -M must be used with -f.\n\n");
		printf("isam.db -Mf [file_name] -R [fields:that:you:want:to:change:the:name].\n\n");
		print_usage(argv);
		return 0;
	}
	if (journal_display && (!file_path.is_empty(&file_path)  
			|| del 					
			|| update 					
			|| del_file
			|| list_def 
			|| new_file 				
			|| !key.is_empty(&key)
			|| !data_to_add.is_empty(&data_to_add)	
			|| build
			|| create		 			
			|| index_add 
			|| modify_schema
			|| import_from_data
			|| nr_of_record_display
			|| swap_index)){
                printf("option -j must be used by itself.\n");
                return 0;
        }

	if (create && (!file_path.is_empty(&file_path) 
				|| del 					
				|| update 					
				|| del_file
				|| list_def 
				|| new_file 				
				|| !key.is_empty(&key)
				|| !data_to_add.is_empty(&data_to_add)	
				|| build
				|| index_add 
				|| modify_schema
				|| import_from_data
				|| nr_of_record_display
				|| swap_index)){
		printf("option -c must be used by itself.\n");
		printf(" -c <txt-with-files-definitions>.\n");
		return 0;
	}

	if(list_def && (del 
				|| !schema_def.is_empty(&schema_def)
				|| update
				|| del_file
				|| new_file 
				|| modify_schema
				|| import_from_data
				|| !key.is_empty(&key)
				|| !data_to_add.is_empty(&data_to_add) 
				|| build
				|| nr_of_record_display
				|| swap_index)){
		printf("option -l must be used with -f only.\n");
		printf("[example]: isam_db -lf a_file.\n");
		return 0;
	}
	if (index_add && (del 
				|| update
				|| del_file
				|| list_def 
				|| new_file 
				|| modify_schema
				|| import_from_data
				|| !key.is_empty(&key)
				|| !data_to_add.is_empty(&data_to_add) 
				|| build
				|| nr_of_record_display
				|| swap_index)){
		print_usage(argv);
		fprintf(stderr, "\nyou can use option -A only with options -f -i and -x\n");
		return 0;
	}

	if (!file_path.is_empty(&file_path) && (create || import_from_data)){
		print_usage(argv);
		return 0;
	}

	if (build && !file_path.is_empty(&file_path)    &&
			(del || update || del_file || list_def || new_file || !key.is_empty(&key) || !data_to_add.is_empty(&data_to_add)  ))
	{
		printf("you must use options -b only with option -f:\n\t-f <filename> -b <filename[txt,csv,tab delimited]> \n");
		print_usage(argv);
		return 0;
	}

	if(import_from_data && (!file_path.is_empty(&file_path) 
				|| del
				|| update 
				|| del_file
				|| list_def 
				|| new_file 
				|| !key.is_empty(&key)
				|| !data_to_add.is_empty(&data_to_add) 
				|| build
				|| index_add 
				|| create
				|| nr_of_record_display
				|| swap_index)){
		printf("option -B must be used by itself.\n");
		printf(" -B <txt with files data to import>.\n");
		fprintf(stderr,"!! the file system MUST exist already !!\n");
		return 0;
	}

	if (!file_path.is_empty(&file_path)  || file_field){
		if(file_path.str){
			if(!is_file_name_valid(file_path.str)){
				printf("file name or path not valid.\n");
				return 0;
			}
		}else{
			if(!is_file_name_valid(file_path.base)){
				printf("file name or path not valid.\n");
				return 0;
			}
		}
	}


	if (update && key.is_empty(&key) ){
		printf("option -k is required.\n\n");
		print_usage(argv);
		return 0;
	}

	if (new_file && list_def)
	{
		printf("option -l can`t be used on new file, or at file creation.\n\n");
		print_usage(argv);
		return 0;
	}

	if (del_file && 
			(del 					
			 || update 					
			 || list_def 
			 || new_file 				
			 || !key.is_empty(&key)
			 || !data_to_add.is_empty(&data_to_add)	
			 || build
			 || create		 			
			 || index_add 
			 || import_from_data
			 || nr_of_record_display
			 || swap_index)){

		printf("you cannot use option -e with other options! Only -ef <fileName>.\n");
		print_usage(argv);
		return 0;
	}

	if ((del && key.is_empty(&key) ) && !options)
	{
		printf("missing record key, option -k.\n");
		print_usage(argv);
		return 0;
	}


	if((swap_index && file_path.is_empty(&file_path)) 
			|| (swap_index && ( del 					
					|| update 					
					|| del_file
					|| list_def 
					|| new_file 				
					|| !key.is_empty(&key)
					|| !data_to_add.is_empty(&data_to_add) 
					|| build
					|| create		 			
					|| index_add 
					|| modify_schema
					|| import_from_data
					|| nr_of_record_display))){
		fprintf(stderr,"option -S is allowed only with options -s (source index) and -i (dest index)\n");
		fprintf(stderr,"Usage: -Sf [file_name] -s 3 -i 2\n");
		return 0;
	}
	return 1;
}

int convert_options(char *options)
{
	size_t l = strlen(options);
	size_t i;
	for (i = 0; i < l; i++)
		options[i] = tolower(options[i]);

	if (strncmp(options, ALL_OP,strlen(ALL_OP)) == 0) return ALL;
	if (strncmp(options, ADD_AR_OP,strlen(ADD_AR_OP)) == 0) return AAR;
	if (strncmp(options, FORCE,strlen(FORCE)) == 0) return FRC;


	return -1;
}
