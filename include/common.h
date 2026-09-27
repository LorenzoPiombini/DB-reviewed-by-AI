#ifndef COMMON_H
#define COMMON_H 

/* ==== BINARY SEARCH TREE ==== */
#define ERR -2
#define LEFT -1
#define RIGHT 1
struct BSTnode{
	void *value;
	struct BSTnode *left;
	struct BSTnode *right;
};


int comparison(void *src, void *dest);
int BST_insert(struct BSTnode **root, struct BSTnode *node,int (*comparison)(void*,void*));
void BST_free(struct BSTnode **root);

/* =========================== */

/*used for arrays in the database*/
struct Metadata{
	long long elements;
	long long capacity;
	int type;
};

struct Mix_t{
	int type;
	void *v;
};
int mix_type_init(int type,struct Mix_t **el,void *value);
#define FREE_MIX_TYPE(n) \
						do{ \
						free(((struct Mix_t*)n)->v);\
						free((struct Mix_t*)n);\
						n = NULL;\
						}while(0)

enum arr_type{
#if defined(_WIN32)
	ARR_INT,
	ARR_LONG,
	ARR_BYTE,
	ARR_DOUBLE,
	ARR_FLOAT,
	ARR_VOID,
#else
	INT,
	LONG,
	BYTE,
	DOUBLE,
	FLOAT,
	VOID,
#endif
	STRING,
	USER_DEF
};

void *array_init(size_t size, int type);
int array_push(void **arr, void *el);
int array_insert_at(int i, void **arr, void *el);
void array_free(void* arr);

#define MAX_FILE_PATH_LENGTH 256
#define STATUS_ERROR -1
#define SCHEMA_ERR 2
#define SCHEMA_EQ 3
#define SCHEMA_NW 4
#define SCHEMA_CT 5
#define UPDATE_OLD 6 /*you can overwrite the old record with no worries!*/
#define UPDATE_OLDN 8
#define ALREADY_KEY 9
#define EFLENGTH 10 /*file path or name too long */
#define SCHEMA_EQ_NT 11 /*schema eq but NO type in the schema file*/
#define SCHEMA_CT_NT 12 /*intput is part of the schema but NO type in the schema file*/
#define SCHEMA_NW_NT 13 /*intput is part of the schema but also new part no part of the schema*/
#define NTG_WR 14 /* nothing to write to the file*/
#define NO_ELEMENT 15 /* ?? */	
#define KEY_NOT_FOUND 16 /*key not in the index file*/
#define UPDATE_NOT 17 /*nothing tio update*/
#define INDEX_OUT_OF_RANGE 18 /*indexing error */
#define CACHE_FAILED 19 /*cache operation failed */
#define E_RCMP 201 /* record comparison failed*/	
#define DIF 202 /* new record is different*/	

	

#endif
