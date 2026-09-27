#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "file.h"
#include "hash_tbl.h"
#include "str_op.h"
#include "debug.h"
#include "endian.h"
#include "db_types.h"
#include "common.h"
#include "string_utilities.h"


void print_hash_table(HashTable tbl)
{
	int i = 0;
	for (i = 0; i < tbl.size; i++)
	{
		printf("%d: \n", i);
		Node *node = tbl.data_map[i];

		while (node)
		{
			switch (node->key.type)
			{
#if defined(_WIN32)
			case STR_KEY:
#else
			case STR:
#endif
			{
				if (node->key.k.s)
				{
					printf("\t{ %s,%ld }\n",
						   node->key.k.s, node->value);
				}

				node = node->next;
				break;
			}
#if defined(_WIN32)
			case UINT_KEY:
#else
			case UINT:
#endif
			{
				printf("\t{ %u,%ld}\n", node->key.k.n, node->value);
				node = node->next;
				break;
			}
			default:
				break;
			}
		}

		if (i > 0 && (i % 30 == 0))
			printf("\npress any key . . ."), getchar();
	}
}

int write_ht(file_t fd, HashTable *ht)
{
	int i = 0;
	
	/*NOTE: you have to write the data about the size and len of HashTable
	 * even if it is empty*/

	const unsigned long EIGTH_Kib = 1024 * 8;
	long msize = EIGTH_Kib;
	long bwritten = 0;
	ui8 *buff = malloc(msize);
	if(!buff){
		fprintf(stderr,"malloc failed, %s:%d.\n",F, L - 2);
		return 0;
	}

	ui32 sz_n = swap32(ht->size);
	memcpy(&buff[bwritten],&sz_n,sizeof(sz_n));
	bwritten += sizeof(sz_n);

	ui32 ht_ln = swap32(len(*ht));
	memcpy(&buff[bwritten],&ht_ln,sizeof(ht_ln));
	bwritten += sizeof(ht_ln);

	if(len(*ht) == 0) {
		if(os_write(fd,buff,bwritten) == -1){
			free(buff);
			return 0;
		}
		free(buff);
		return 1;
	}

	for(i = 0; i < ht->size; i++) {
		if(ht->data_map[i] == NULL)
			continue;

		Node *current = ht->data_map[i];
		while (current != NULL) {
			switch (current->key.type){
#if defined(_WIN32)
			case STR_KEY:
#else
			case STR:
#endif
			{
				ui32 type = swap32(current->key.type);
				ui64 key_l = swap64(strlen(current->key.k.s));
				ui64 value = swap64(current->value);

				if(bwritten + (sizeof(ui32) + (sizeof(ui64) * 2)) > EIGTH_Kib){
					ui8 *new = realloc(buff,msize + EIGTH_Kib);
					if(!new){
						free(buff);
						return 0;
					}

					buff = new;
					msize += EIGTH_Kib;
				}

				memcpy(&buff[bwritten],&type,sizeof(type));
				bwritten += sizeof(type);
				memcpy(&buff[bwritten],&key_l,sizeof(key_l));
				bwritten += sizeof(key_l);
				memcpy(&buff[bwritten],current->key.k.s,strlen(current->key.k.s)+1);
				bwritten += strlen(current->key.k.s)+1;
				memcpy(&buff[bwritten],&value,sizeof(value));
				bwritten += sizeof(value);

				current = current->next;
				break;
			}
#if defined(_WIN32)
			case UINT_KEY:
#else
			case UINT:
#endif
			{
				ui32 type = swap32(current->key.type);
				ui8 size = (ui8)current->key.size;
				ui64 value = swap64(current->value);
				
				ui32 k = 0;
				ui16 k16 = 0;
				if(current->key.size == 32)
					k = swap32(current->key.k.n);
				else
					k16 = swap16(current->key.k.n16);

				if(k){
					if(bwritten + (sizeof(k) + sizeof(ui32) + (sizeof(ui8) + sizeof(ui64))) > EIGTH_Kib){
						ui8 *new = realloc(buff,msize + EIGTH_Kib);
						if(!new){
							free(buff);
							return 0;
						}

						buff = new;
						msize += EIGTH_Kib;
					}
				}else{
					if(bwritten + (sizeof(k16) + sizeof(ui32) + (sizeof(ui64) * 2)) > EIGTH_Kib){
						ui8 *new = realloc(buff,msize + EIGTH_Kib);
						if(!new){
							free(buff);
							return 0;
						}

						buff = new;
						msize += EIGTH_Kib;
					}
				}

				memcpy(&buff[bwritten],&type,sizeof(type));
				bwritten += sizeof(type);
				memcpy(&buff[bwritten],&size,sizeof(size));
				bwritten += sizeof(size);

				if(current->key.size == 16){
					memcpy(&buff[bwritten],&k16,sizeof(k16));
					bwritten += sizeof(k16);
				}else{
					memcpy(&buff[bwritten],&k,sizeof(k));
					bwritten += sizeof(k);
				}

				memcpy(&buff[bwritten],&value,sizeof(value));
				bwritten += sizeof(value);

				current = current->next;
				break;
			}
			default:
				fprintf(stderr, "key type not supported.\n");
				free(buff);
				return 0;
			}
		}
	}

	if(os_write(fd, buff, bwritten) == -1) {
		perror("writing index file");
		free(buff);
		return 0;
	}

	free(buff);
	return 1;
}

int hash(void *key, int size, int key_type)
{
	if(size == 0){
		fprintf(stderr,"something bad happend call security! %s:%d\n",__FILE__,__LINE__);
		return -1;
	}

	ui32 integer_key = 0;
	char *string_key = NULL;
	int hash = 0;
	int number = 78;

	/*compute the prime number */
	ui32 prime = (ui32)(power(2, 31) - 1);
	switch (key_type) {
#if defined(_WIN32)
	case UINT_KEY:
#else
	case UINT:
#endif
	{
		integer_key = *(ui32 *)key;
		/*printf("size is %d, integer_key %u, coprime is %d, prime is %d\n",size,integer_key,COPRIME,prime);*/
		hash = ((COPRIME * integer_key + number) % prime) % size;
		break;
	}
#if defined(_WIN32)
	case STR_KEY:
#else
	case STR:
#endif
		string_key = (char *)key;
		for (; *string_key != '\0'; string_key++, number = COPRIME * number % (size - 1)){
			hash = (number * hash + *string_key) % size;
		}
		break;
	default:
		fprintf(stderr, "key type not supported");
		return -1;
	}

	return hash;
}

file_offset get(void *key, HashTable *tbl, int key_type)
{
	int index = hash(key, tbl->size, key_type);
	Node *temp = tbl->data_map[index];
	while (temp != NULL)
	{
		switch (key_type) {
#if defined(_WIN32)
		case STR_KEY:
#else
		case STR:
#endif
			if(temp->key.type != key_type){
				temp = temp->next;
				break;
			}
			if(!temp->key.k.s)
				return -1;
			if (strcmp(temp->key.k.s, (char *)key) == 0)
				return temp->value;

			temp = temp->next;
			break;
#if defined(_WIN32)
		case UINT_KEY:
#else
		case UINT:
#endif
		{
			if(temp->key.size == 16 && *(ui16*)key < USHRT_MAX){
				if(temp->key.k.n16 == *(ui16 *)key)
					return temp->value;

			}else if (temp->key.size == 32 && *(ui32*)key < UINT_MAX){
				if(temp->key.k.n == *(ui32*)key)		
					return temp->value;
			}

			temp = temp->next;
			break;
		}
		default:
			fprintf(stderr, "key type not supported.\n");
			return -1;
		}
	}
	return -1;
}

int set(void *key, int key_type, file_offset value, HashTable *tbl)
{

	int index = hash(key, tbl->size, key_type);
	Node *new_node = (Node*) malloc(sizeof *new_node);
	if (!new_node){
		fprintf(stderr,"malloc failed, %s:%d.\n",F, L - 2);
		return 0;
	}

	switch (key_type){
#if defined(_WIN32)
	case UINT_KEY:
#else
	case UINT:
#endif
	{
		if(*(ui32*)key < USHRT_MAX){ 
			new_node->key.k.n16 = *(ui16 *)key;
			new_node->key.size = 16;
		}else{ 
			new_node->key.k.n = *(ui32 *)key;
			new_node->key.size = 32;
			if(new_node->key.k.n > (ui32)UINT_MAX){
				fprintf(stderr,"key out of range, %s:%d.\n",F, L - 2);
				return 0;
			}
		}
		break;
	}
#if defined(_WIN32)
	case STR_KEY:
#else 
	case STR:
#endif
	{
		new_node->key.k.s = duplicate_str((char *)key);
		if (!new_node->key.k.s) {
			fprintf(stderr, "duplicate_str failed, %s:%d",F,L-2);
			return 0;
		}
		break;
	}
	default:
		fprintf(stderr, "key type not supported.\n");
		return 0;
	}

	new_node->key.type = key_type;
	new_node->value = value;
	new_node->next = NULL;

	if (tbl->data_map[index] == NULL) {
		tbl->data_map[index] = new_node;
#if defined(_WIN32)
	}else if (key_type == STR_KEY){
#else
	}else if (key_type == STR){
#endif
		/*for key strings*/
		/*
		 * check if the key already exists at
		 * the base  element of the index
		 * */
		size_t key_len = 0;
#if defined(_WIN32)
		if(tbl->data_map[index]->key.type == STR_KEY){
#else
		if(tbl->data_map[index]->key.type == STR){
#endif
			if ((key_len = strlen(tbl->data_map[index]->key.k.s)) == strlen(new_node->key.k.s))
			{
				if (strncmp(tbl->data_map[index]->key.k.s, new_node->key.k.s, ++key_len) == 0)
				{
					printf("key %s, already exist.\n", new_node->key.k.s);
					free(new_node->key.k.s);
					free(new_node);
					return 0;
				}
			}

		}
		/*
		 * check all the nodes in the index
		 * for duplicates keys
		 * */
		Node *temp = tbl->data_map[index];
		while (temp->next != NULL) {
#if defined(_WIN32)
			if (temp->next->key.type != STR_KEY){
#else
			if (temp->next->key.type != STR){
#endif
				temp = temp->next;
				continue;
			}
			if ((key_len = strlen(temp->next->key.k.s)) == strlen(new_node->key.k.s)) {
				if (strncmp(temp->next->key.k.s, new_node->key.k.s, ++key_len) == 0) {
					printf("could not insert new node \"%s\"\n", new_node->key.k.s);
					printf("key already exist. Choose another key value.\n");
					free(new_node->key.k.s);
					free(new_node);
					return 0;
				}
			}
			temp = temp->next;
		}
#if defined(_WIN32)
		if(temp->key.type == STR_KEY){
#else
		if(temp->key.type == STR){
#endif

			if ((key_len = strlen(temp->key.k.s)) == strlen(new_node->key.k.s)) {
				if (strncmp(temp->key.k.s, new_node->key.k.s, ++key_len) == 0) {
					printf("could not insert new node \"%s\"\n", new_node->key.k.s);
					printf("key already exist. Choose another key value.\n");
					free(new_node->key.k.s);
					free(new_node);
					return 0;
				}
			}
		}

		/*
		 * the key is unique
		 * we add the node to the list
		 * */
		temp->next = new_node;
#if defined(_WIN32)
	} else if (key_type == UINT_KEY) {
#else
	} else if (key_type == UINT) {
#endif
		if (tbl->data_map[index]->key.size == new_node->key.size){
			if(new_node->key.size == 16){
				if(tbl->data_map[index]->key.k.n16 == new_node->key.k.n16) {
					printf("could not insert new node '%u'\n", new_node->key.k.n16);
					printf("key already exist. Choose another key value.\n");
					free(new_node);
					return 0;
				}
			}else{
				if (tbl->data_map[index]->key.k.n == new_node->key.k.n) {
					printf("could not insert new node '%u'\n", new_node->key.k.n);
					printf("key already exist. Choose another key value.\n");
					free(new_node);
					return 0;
				}
			}
		}

		Node *temp = tbl->data_map[index];
		while (temp->next) {
			if(temp->key.size == new_node->key.size){
				if(new_node->key.size == 16){
					if(temp->key.k.n16 == new_node->key.k.n16) {
						printf("could not insert new node '%u'\n", new_node->key.k.n16);
						printf("key already exist. Choose another key value.\n");
						free(new_node);
						return 0;
					}
				}else{
					if (temp->key.k.n == new_node->key.k.n) {
						printf("could not insert new node '%u'\n", new_node->key.k.n);
						printf("key already exist. Choose another key value.\n");
						free(new_node);
						return 0;
					}
				}

			}
			temp = temp->next;
		}

		if(temp->key.size == new_node->key.size){
			if(new_node->key.size == 16){
				if(temp->key.k.n16 == new_node->key.k.n16) {
					printf("could not insert new node '%u'\n", new_node->key.k.n16);
					printf("key already exist. Choose another key value.\n");
					free(new_node);
					return 0;
				}
			}else{
				if (temp->key.k.n == new_node->key.k.n) {
					printf("could not insert new node '%u'\n", new_node->key.k.n);
					printf("key already exist. Choose another key value.\n");
					free(new_node);
					return 0;
				}
			}
		}

		temp->next = new_node;
	}

	return 1; /* succseed!*/
}

Node *ht_delete(void *key, HashTable *tbl, int key_type)
{
	int index = hash(key, tbl->size, key_type);
	Node *current = tbl->data_map[index];
	Node *previous = NULL;

	while (current != NULL)
	{
		switch (key_type)
		{
#if defined(_WIN32)
		case STR_KEY:
#else
		case STR:
#endif
			if (strcmp(current->key.k.s, (char *)key) == 0)
			{
				if (previous == NULL){
					tbl->data_map[index] = current->next;
				}else {
					previous->next = current->next;
				}

				return current;
			}
			previous = current;
			current = current->next;
			break;
#if defined(_WIN32)
		case UINT_KEY:
#else
		case UINT:
#endif
		{

			if(current->key.size == 16){
				if (current->key.k.n16 == *(ui16 *)key)
				{
					if (previous == NULL)
						tbl->data_map[index] = current->next;
					else
						previous->next = current->next;

					return current;
				}
			}else{
				if (current->key.k.n == *(ui32 *)key)
				{
					if (previous == NULL)
						tbl->data_map[index] = current->next;
					else
						previous->next = current->next;

					return current;
				}


			}
			previous = current;
			current = current->next;
			break;
		}
		default:
			fprintf(stderr, "key type not supported.\n");
			return NULL;
		}
	}

	return NULL;
}

void free_ht_array(HashTable *ht, int l)
{
	if (!ht)
		return;

	int i = 0;
	for (i = 0; i < l; i++)
		destroy_hasht(&ht[i]);

	free(ht);
}
void destroy_hasht(HashTable *tbl)
{

	int i = 0;
	for (i = 0; i < tbl->size; i++) {
		Node *current = tbl->data_map[i];
		while (current != NULL) {
			switch (current->key.type) {
#if defined(_WIN32)
			case STR_KEY:
#else
			case STR:
#endif
			{
				Node *next = current->next;
				free(current->key.k.s);
				free(current);
				current = next;
				break;
			}
#if defined(_WIN32)
		case UINT_KEY:
#else
		case UINT:
#endif
			{
				Node *next = current->next;
				free(current);
				current = next;
				break;
			}
			default:
				fprintf(stderr, "key type not supported.\n");
				current = NULL;
				return;
			}
		}
	}
}

int keys(HashTable *ht, struct Keys_ht *all_keys)
{
	int elements = len(*ht);

	if(elements == 0) return NO_ELEMENT;

	struct Key *keys = (struct Key*)malloc(elements * sizeof *keys);
	if (!keys){
		fprintf(stderr,"malloc failed, %s:%d.\n",F, L - 2);
		return -1;
	}

	int i = 0;
	int index = 0;
	for (i = 0; i < ht->size; i++)
	{
		Node *temp = ht->data_map[i];
		while (temp != NULL)
		{
#if defined(_WIN32)
			if (temp->key.type == STR_KEY)
#else
			if (temp->key.type == STR)
#endif
			{
				keys[index].k.s = duplicate_str(temp->key.k.s);
				if (!keys[index].k.s){
					fprintf(stderr, "duplicate_str failed, %s:%d",F,L-2);
					return -1;
				}
#if defined(_WIN32)
				keys[index].type = STR_KEY;
#else
				keys[index].type = STR;
#endif

			}
			else
			{
				if(temp->key.size == 16){
					keys[index].k.n16 = temp->key.k.n16;
#if defined(_WIN32)
					keys[index].type = UINT_KEY;
#else
					keys[index].type = UINT;
#endif
					keys[index].size = 16;
					pack(keys[index].k.n16,keys[index].paked_k);
				}else{
					keys[index].k.n = temp->key.k.n;
#if defined(_WIN32)
					keys[index].type = UINT_KEY;
#else
					keys[index].type = UINT;
#endif
					keys[index].size = 32;
					pack(keys[index].k.n,keys[index].paked_k);
					
				}
			}
			temp = temp->next;
			++index;
			
		}
	}

	all_keys->keys = keys;
	all_keys->length = elements;
	return 0;
}

int len(HashTable tbl)
{
	int counter = 0;

	if (tbl.size == 0) return 0;

	int i;
	for (i = 0; i < tbl.size; i++){
		Node *temp = tbl.data_map[i];
		while (temp != NULL)
		{
			counter++;
			temp = temp->next;
		}
	}

	return counter;
}

void free_nodes(Node **data_map, int size)
{
	int i = 0;
	for (i = 0; i < size; i++) {
		Node *current = (*data_map);
		while (current) {
			switch (current->key.type)
			{

#if defined(_WIN32)
			case STR_KEY:
#else
			case STR:
#endif
			{
				Node *next = current->next;
				free(current->key.k.s);
				free(current);
				current = next;
				break;
			}
#if defined(_WIN32)
			case UINT_KEY:
#else
			case UINT:
#endif
			{
				Node *next = current->next;
				free(current);
				current = next;
				break;
			}
			default:
				fprintf(stderr, "key type not supported.\n");
				return;/*safer than break (avoid infinete loop)*/
			}
		}
	}
}

void free_ht_node(Node *node)
{
	if (!node)
		return;

	switch (node->key.type)
	{
#if defined(_WIN32)
	case STR_KEY:
#else
	case STR:
#endif
		free(node->key.k.s);
		break;
	
#if defined(_WIN32)
	case UINT_KEY:
#else
	case UINT:
#endif
		break;
	default:
		fprintf(stderr, "key type not supported");
		return;
	}

	free(node);
}

void free_keys_data(struct Keys_ht *data)
{
	int i;
	for (i = 0; i < data->length; i++){
#if defined(_WIN32)
		if (data->keys[i].type == STR_KEY)
#else
		if (data->keys[i].type == STR)
#endif
			free(data->keys[i].k.s);
	}

	free(data->keys);
}

/*if mode is set to 1 it will overwrite the content of dest
	if mode is 0 then the src HashTable will be added to the dest, maintaing whatever is in dest*/
unsigned char copy_ht(HashTable *src, HashTable *dest, int mode)
{
	if (!src)
	{
		printf("no data to copy.\n");
		return 0;
	}

	if (mode == 1)
	{
		destroy_hasht(dest);

		(*dest).size = src->size;
		memset((*dest).data_map,0,sizeof(Node*)*MAX_HT_BUCKET);
	}

	int i;
	for (i = 0; i < src->size; i++)
	{
		if (src->data_map[i])
		{
			switch (src->data_map[i]->key.type){
#if defined(_WIN32)
			case STR_KEY:
#else
			case STR:
#endif
			{
				set((void *)src->data_map[i]->key.k.s,
					src->data_map[i]->key.type,
					src->data_map[i]->value, dest);
				Node *next = src->data_map[i]->next;
				while (next){
					set(next->key.k.s,
						next->key.type,
						next->value, dest);
					next = next->next;
				}
				break;
			}
#if defined(_WIN32)
			case UINT_KEY:
#else
			case UINT:
#endif
			{
				if(src->data_map[i]->key.size == 16){
					set((void *)&src->data_map[i]->key.k.n16,
							src->data_map[i]->key.type,
							src->data_map[i]->value, dest);
				}else{
					set((void *)&src->data_map[i]->key.k.n,
							src->data_map[i]->key.type,
							src->data_map[i]->value, dest);
				}

				Node *next = src->data_map[i]->next;
				while (next)
				{
					if(src->data_map[i]->key.size == 16){
						set((void *)&src->data_map[i]->key.k.n16,
							src->data_map[i]->key.type,
							src->data_map[i]->value, dest);
					}else{
						set((void *)&src->data_map[i]->key.k.n,
								src->data_map[i]->key.type,
								src->data_map[i]->value, dest);
					}
					next = next->next;
				}
				break;
			}
			default:
				fprintf(stderr, "key type not supported");
				return 0;
			}
		}
	}

	return 1;
}

int swap_indexes(int src,int dest, HashTable *ht)
{
	if(0 == len(ht[src]) && 0 == len(ht[dest])) 
		return -1;
	

	if(0 == len(ht[dest])){
		/*in this case the dest hash tasble is empty*/
		/*copy src hash table into dest*/
		int i;
		for(i = 0; i < ht[src].size; i ++){
			Node *c = ht[src].data_map[i];
			Node **n = &ht[dest].data_map[i];
			while(c){
				*n = malloc(sizeof **n);
				if(!(*n)){
					fprintf(stderr,"malloc failed, %s:%d.\n",__FILE__,__LINE__- 2);
					return -1;
				}
				memset(*n,0,sizeof **n);

				switch(c->key.type){
#if defined(_WIN32)
				case STR_KEY:
#else
				case STR:
#endif
				{
					(*n)->key.type = c->key.type;
					(*n)->key.size = c->key.size;
					(*n)->key.k.s = duplicate_str(c->key.k.s);
					if(!(*n)->key.k.s){
						fprintf(stderr, "duplicate_str failed, %s:%d",F,L-2);
						return -1;
					}
					(*n)->value = c->value;
					break;
				}
#if defined(_WIN32)
				case UINT_KEY:
#else
				case UINT:
#endif
				{
					(*n)->key.type = c->key.type;
					(*n)->key.size = c->key.size;
					if(c->key.size == 16)
						(*n)->key.k.n16 = c->key.k.n16;
					else
						(*n)->key.k.n = c->key.k.n;
					(*n)->value = c->value;
					break;
				}
				default:
					fprintf(stderr, "key type not supported.\n");
					return -1;
				}

				n = &(*n)->next;
				c = c->next;
			}
		}
		/*free memory for the source hast table*/
		destroy_hasht(&ht[src]);
		memset(&ht[src],0,sizeof(HashTable));
		ht[src].size = 7;
		ht[src].write = write_ht;
		return 0;
	}
	

	if(0 < len(ht[dest]) && 0 < len(ht[src])){
		/*in this case is a real swap*/
		HashTable dest_cpy = {0};
		dest_cpy.size = ht[dest].size;
		dest_cpy.write = write_ht;
		if(!copy_ht(&ht[dest],&dest_cpy,0)){
			fprintf(stderr,"copy_ht() faield, %s:%d\n",__FILE__,__LINE__-1);
			destroy_hasht(&dest_cpy);
			return -1;
		}
		
		if(copy_ht(&ht[src],&ht[dest],1)){
			fprintf(stderr,"copy_ht() faield, %s:%d\n",__FILE__,__LINE__-1);
			destroy_hasht(&dest_cpy);
			return -1;
		}

		if(copy_ht(&dest_cpy,&ht[src],1)){
			fprintf(stderr,"copy_ht() faield, %s:%d\n",__FILE__,__LINE__-1);
			destroy_hasht(&dest_cpy);
			return -1;
		}
		destroy_hasht(&dest_cpy);
		return 0;
	}

	if(0 < len(ht[dest]) && 0 == len(ht[src])){
		/*TODO*/


	}
	return 0;
}

