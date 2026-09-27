#ifndef HASH_TBL_H
#define HASH_TBL_H


#include "db_types.h"

#define MAX_KEY 4294967296
#define MAX_HT_BUCKET 503 

#if defined(_WIN32)
	#define STR_KEY 1
	#define UINT_KEY 2
#else
	#define STR 1
	#define UINT 2
#endif

struct Key{
	int type;
	ui8 size;
	union {
		ui16 n16;
		ui32 n;
		char *s;
	}k;
	ui8 paked_k[5];
};/* 25 bytes*/

typedef struct Node {
	struct Key key;
	file_offset value;
	struct Node *next;
}Node;

typedef struct HashTable {
	int size;
	Node *data_map[MAX_HT_BUCKET];
	int (*write)(file_t fd, struct HashTable *ht);
} HashTable;

/*this will contains all the keys of the hash table*/
struct Keys_ht{
	struct Key *keys;
	int length;
};

/*
 * COPRIME is a prime number needed for the hash function
 * I simply pick a prime number between 1 and 3000
 * this will be used along with a very large prime number ( 2^t - 1 )
 * the decison of using such a large number is mainly because i want to be able
 * to handle a very large system and the integer key for the hash table
 * will be of type ui32 wich give us more than 4 billions of key for each hash table
 * this hash table will be used as index file in a file db,
 * so you will have the possibility to store billions of records.
 * */
#define COPRIME 2837


void print_hash_table(HashTable tbl);
int write_ht(file_t fd, HashTable *ht);
int hash(void *key, int size, int key_type);
Node *ht_delete(void *key, HashTable *tbl, int key_type);
int set(void *key, int key_type, file_offset value, HashTable *tbl);
void free_ht_array(HashTable *ht, int l);
void destroy_hasht(HashTable *tbl); /*free memory*/
file_offset get(void *key, HashTable *tbl, int key_type);
int keys(HashTable *ht, struct Keys_ht *all_keys);
int len(HashTable tbl);
void free_nodes(Node **dataMap, int size);
void free_ht_node(Node *node);
void free_keys_data(struct Keys_ht *data);
int swap_indexes(int src,int dest, HashTable *ht);
unsigned char copy_ht(HashTable *src, HashTable *dest, int mode);

#endif /* hash_tbl.h */
