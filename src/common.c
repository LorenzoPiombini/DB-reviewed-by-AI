#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common.h"

static int BST_node_init(struct BSTnode **node, struct BSTnode **v);

void *array_init(size_t size, int type)
{
	switch(type){

#if defined(_WIN32)
	case ARR_INT:
#else
	case INT:
#endif
	{
		struct Metadata *head = malloc(sizeof(int)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(int)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}
#if defined(_WIN32)
	case ARR_LONG:
#else
	case LONG:
#endif
	{
		struct Metadata *head = malloc(sizeof(long)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(long)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		
		return (void*)(head +1);
	}
#if defined(_WIN32)
	case ARR_BYTE:
#else
	case BYTE:
#endif
	{
		struct Metadata *head = malloc(sizeof(unsigned char)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(unsigned char)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}
	case STRING:
	{
		struct Metadata *head = malloc(sizeof(char*)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(char*)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}
#if defined(_WIN32)
	case ARR_DOUBLE:
#else
	case DOUBLE:
#endif
	{
		struct Metadata *head = malloc(sizeof(double)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(double)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}	
#if defined(_WIN32)
	case ARR_FLOAT:
#else
	case FLOAT:
#endif
	{
		struct Metadata *head = malloc(sizeof(float)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(float)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}	
	/* this was more of a challenge that i want to accomplish
	 * I hardly find the need to have an array of mixed type, 
	 * but was fun to implement*/
#if defined(_WIN32)
	case ARR_VOID:
#else
	case VOID:
#endif

	{
		struct Metadata *head = malloc(sizeof(struct Mix_t)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(struct Mix_t)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}
	case USER_DEF:
	{
		struct Metadata *head = malloc(sizeof(void *)*size +sizeof(struct Metadata));
		if(!head)
			return NULL;

		memset(head,0,sizeof(void)*size+sizeof(struct Metadata));
		head->capacity = (long long)size;
		head->elements = (long long) 0;
		head->type = type;
		return (void*)(head +1);
	}
	default:
		return NULL;
	}
}

int mix_type_init(int type,struct Mix_t **el, void* value)
{
	if(!(*el)){
		*el = malloc(sizeof(struct Mix_t));
		if(!(*el)){
			return -1;
		}
		memset(*el,0,sizeof(struct Mix_t));
	}

	switch(type){
#if defined(_WIN32)
	case ARR_INT:
#else
	case INT:
#endif
	{ 
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(int));
		if(!(*el)->v){
			return -1;
		}
		*(int*)(*el)->v = *(int *)value;	
		return 0;
	}
#if defined(_WIN32)
	case ARR_LONG:
#else
	case LONG:
#endif
	{
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(long));
		if(!(*el)->v){
			return -1;
		}
		*(long*)(*el)->v = *(long*)value;	
		return 0;
	}
#if defined(_WIN32)
	case ARR_BYTE:
#else
	case BYTE:
#endif
	{
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(unsigned char));
		if(!(*el)->v){
			return -1;
		}
		*(unsigned char*)(*el)->v = *(unsigned char*)value;	
		return 0;
	}
#if defined(_WIN32)
	case ARR_FLOAT:
#else
	case FLOAT:
#endif
	{
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(float));
		if(!(*el)->v){
			return -1;
		}
		*(float*)(*el)->v = *(float*)value;	
		return 0;
	}
#if defined(_WIN32)
	case ARR_DOUBLE:
#else
	case DOUBLE:
#endif
	{
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(double));
		if(!(*el)->v){
			return -1;
		}
		*(double*)(*el)->v = *(double*)value;	
		return 0;
	}
	case STRING:
	{
		size_t size = strlen((char*)value);
		(*el)->type = type;
		(*el)->v = (void*)malloc(sizeof(char*)*(size+1));
		if(!(*el)->v){
			return -1;
		}
		memcpy((char*)(*el)->v,(char*)value,size);
		return 0;
	}
	default:
		return -1;
	}

}
/*
 * insert at, should always expand the memory allocation at least one
 * 
 * */
int array_insert_at(int i, void **arr, void *el)
{
	struct Metadata *h = (struct Metadata*)*arr - 1;
	switch(h->type){
#if defined(_WIN32)
	case ARR_LONG:
#else
	case LONG:
#endif
	{
		if(h->capacity <= i){
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(long)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((long*)*arr)[h->capacity],0,sizeof(long) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(long)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((long*)*arr)[h->capacity],0,sizeof(long) * 1);
			h->capacity++;
		}

		/*move the data over to the right*/
		long *a = (long*)*arr;
		int j;
		for(j = h->capacity - 1; j != i; j--){
				a[j-1] = a[j] ^ a[j-1];
				a[j] = a[j - 1] ^ a[j];
				a[j-1] = a[j] ^ a[j-1];
		}

		a[i] = *(long*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_INT:
#else
	case INT:
#endif
	{
		if(h->capacity <= i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(int)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((int*)*arr)[h->capacity],0,sizeof(int) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(int)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((int*)*arr)[h->capacity],0,sizeof(int) * 1);
			h->capacity++;
		}

		/*move the data over to the right*/
		int *a = (int*)*arr;
		int j;
		for(j = h->capacity - 1; j != i; j--){
				a[j-1] = a[j] ^ a[j-1];
				a[j] = a[j - 1] ^ a[j];
				a[j-1] = a[j] ^ a[j-1];
		}

		a[i] = *(int*)el;
		*arr = (void*)a;
		h->elements++;
		return 0; 
	}
#if defined(_WIN32)
	case ARR_DOUBLE:
#else
	case DOUBLE:
#endif
	{
		if(h->capacity <= i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(double)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((double*)*arr)[h->capacity],0,sizeof(double) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(double)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((double*)*arr)[h->capacity],0,sizeof(double) * 1);
			h->capacity++;
		}

		/*move the data over to the right*/
		double *a = (double*)*arr;
		int j;
		for(j = h->capacity - 1; j != i; j--){
			double temp = a[j-1];
			a[j-1] = a[j];
			a[j] = temp;
		}
		a[i] = *(double*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_FLOAT:
#else
	case FLOAT:
#endif
	{
		if(h->capacity < i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(float)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((float*)*arr)[h->capacity],0,sizeof(float) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(float)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((float*)*arr)[h->capacity],0,sizeof(long) * 1);
			h->capacity++;
		}

		/*move the data over to the right*/
		float *a = (float*)*arr;
		int j;
		for(j = h->capacity - 1; j != i; j--){
			float temp = a[j-1];
			a[j-1] = a[j];
			a[j] = temp;
		}
		a[i] = *(float*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_BYTE:
#else
	case BYTE:
#endif
	{
		if(h->capacity < i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(unsigned char)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((unsigned char*)*arr)[h->capacity],0,sizeof(unsigned char) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(unsigned char)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((unsigned char*)*arr)[h->capacity],0,sizeof(unsigned char) * 1);
			h->capacity++;
		}

		/*move the data over to the right*/
		unsigned char *a = (unsigned char*)*arr;
		int j;
		for(j = h->capacity - 1; j != i; j--){
				a[j-1] = a[j] ^ a[j-1];
				a[j] = a[j - 1] ^ a[j];
				a[j-1] = a[j] ^ a[j-1];
		}
		a[i] = *(unsigned char*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
	case STRING:
	{
		if(h->capacity < i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(char*)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((char**)*arr)[h->capacity],0,sizeof(char*) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(char*)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((char**)*arr)[h->capacity],0,sizeof(char*) * 1);
			h->capacity++;
		}

		char **a = (char **)*arr;
		int j;
		for(j= h->capacity -1; j != i; j--){
			if(a[j-1] && !a[j]){
				int size = (int) strlen(a[j-1]);
				a[j] = (char*) malloc(size+1);
				if(!a[j])
					return -1;
				memset(a[j], 0,size+1);
				strncpy(a[j],a[j-1],size);
				free(a[j-1]);
				a[j-1] = NULL;
			}
		}
		a[i] = (char*)malloc(strlen((char*)el)+1);
		a[i][strlen((char*) el)] = '\0';
		memcpy(a[i],(char*) el,strlen((char*)el));
		h->elements++;
		*arr = (void*)a;
		return 0;
	}
#if defined(_WIN32)
	case ARR_VOID:
#else
	case VOID:
#endif
	{
		if(h->capacity < i){
			/*realloc*/
			int new_size = (i - h->capacity) + 1;
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(struct Mix_t)*(h->capacity + new_size)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((struct Mix_t*)*arr)[h->capacity],0,sizeof(struct Mix_t) * new_size);
			h->capacity += new_size;
		}else {
			void *n = realloc((struct Metadata*)*arr - 1, (sizeof(struct Mix_t)*(h->capacity + 1)) + sizeof(struct Metadata));
			if(!n){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__ -2);
				return -1;
			}
			h = (struct Metadata*)n;
			*arr = (struct Metadata*) n + 1;
			memset(&((struct Mix_t*)*arr)[h->capacity],0,sizeof(struct Mix_t) * 1);
			h->capacity++;
		}

		struct Mix_t *a = (struct Mix_t*)*arr;
		int j;
		for(j= h->capacity -1; j != i; j--){
			if(a[j-1].type != -1 && a[j].type == -1){
				struct Mix_t *temp = &a[j-1];
				a[j] = *temp;
				a[j-1].type = -1;
				a[j-1].v = NULL;
			}
		}
		a[i] = *(struct Mix_t*) el;
		h->elements++;
		*arr = (void*)a;
		return 0;
	}
	default:
		return -1;
	}
}

int array_push(void **arr, void *el)
{
	struct Metadata *h = (struct Metadata*)*arr - 1;
	switch(h->type){
#if defined(_WIN32)
	case ARR_LONG:
#else
	case LONG:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(long)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			/*zeroed out the new alloc*/
			*arr = (struct Metadata*) r + 1;
			memset(&((long*)*arr)[h->capacity],0,sizeof(long) * h->capacity);
			h->capacity = h->capacity * 2;
		}
			long *a = (long*)*arr;
			a[h->elements] = *(long*)el;
			*arr = (void*)a;
			h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_INT:
#else
	case INT:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(int)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			*arr = (struct Metadata*)r + 1;
			memset(&((int*)*arr)[h->capacity],0,sizeof(int) * h->capacity);
			h->capacity = h->capacity * 2;
		}
		int *a = (int*)arr;
		a[h->elements] = *(int*)el;
		*arr = (void*)a;
		h->elements++;
		return 0; 
	}
#if defined(_WIN32)
	case ARR_DOUBLE:
#else
	case DOUBLE:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(double)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			*arr = (struct Metadata*)r + 1;
			memset(&((double*)*arr)[h->capacity],0,sizeof(double) * h->capacity);
			h->capacity = h->capacity * 2;

		}
		double *a = (double*)*arr;
		a[h->elements] = *(double*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_FLOAT:
#else
	case FLOAT:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(float)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			*arr = (struct Metadata*)r + 1;
			memset(&((float*)*arr)[h->capacity],0,sizeof(float) * h->capacity);
			h->capacity = h->capacity * 2;
		}
		float *a = (float*)*arr;
		a[h->elements] = *(float*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
#if defined(_WIN32)
	case ARR_BYTE:
#else
	case BYTE:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(unsigned char)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			h->capacity = h->capacity * 2;
			*arr = (struct Metadata*)r + 1;
			memset(&((unsigned char*)*arr)[h->capacity],0,sizeof(unsigned char) * h->capacity);
		}
		unsigned char *a = (unsigned char*)*arr;
		a[h->elements] = *(unsigned char*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
	case STRING:
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(char*)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			h->capacity = h->capacity * 2;
			*arr = (struct Metadata*)r + 1;
			memset(&((char**)*arr)[h->capacity],0,sizeof(char*) * h->capacity);
		}
		char **a = (char **)*arr;
		a[h->elements] = (char*)malloc(strlen((char*)el)+1);
		a[h->elements][strlen((char*) el)] = '\0';
		memcpy(a[h->elements],(char*) el,strlen((char*)el));
		h->elements++;
		*arr = (void*)a;
		return 0;
	}
#if defined(_WIN32)
	case ARR_VOID:
#else
	case VOID:
#endif
	{
		if(h->elements == h->capacity){
			/*realloc*/
			void *r = realloc((struct Metadata*)*arr-1,(sizeof(struct Mix_t)*(h->capacity * 2))+ sizeof(struct Metadata));
			if(!r){
				fprintf(stderr,"realloc() failed. %s:%d\n",__FILE__,__LINE__);
				return -1;
			}
			h = (struct Metadata*)r;
			h->capacity = h->capacity * 2;
			*arr = (struct Metadata*)r + 1;
			memset(&((struct Mix_t*)*arr)[h->capacity],0,sizeof(struct Mix_t) * h->capacity);
		}
		struct Mix_t *a = (struct Mix_t*)*arr;
		a[h->elements] = *(struct Mix_t*)el;
		*arr = (void*)a;
		h->elements++;
		return 0;
	}
	default:
		return -1;
	}
}

void array_free(void*arr)
{
	if(!arr)
		return;

	struct Metadata *h = (struct Metadata *)arr - 1;
	switch(h->type){
#if defined(_WIN32)
	case ARR_INT:
	case ARR_LONG:
	case ARR_DOUBLE:
	case ARR_FLOAT:
	case ARR_BYTE:
#else
	case INT:
	case LONG:
	case DOUBLE:
	case FLOAT:
	case BYTE:
#endif
		free(h);
		return;
	case STRING:
	{
		char **c = (char**)arr;
		int i;
		for(i = 0; i < h->capacity; i++){
			if(c[i])
				free(c[i]);
		}
		free(h);
		return;
	}
#if defined(_WIN32)
	case ARR_VOID:
#else
	case VOID:
#endif
	{
		struct Mix_t *v = (struct Mix_t*)arr;
		int i;
		for(i = 0; i < h->capacity; i++){
			if(v[i].v)
				free(v[i].v);
		}
		free(h);
		return;
	}
	default:
		return;
	}
}

static int BST_node_init(struct BSTnode **node, struct BSTnode **v)
{
	*node = malloc(sizeof **node);
	if(!(*node)){
		return -1;
	}
	memset(*node,0,sizeof(**node));
	(*node)->value = malloc(sizeof(struct Mix_t));
	if(!(*node)->value){
		return -1;
	}
	memset((*node)->value,0,sizeof(struct Mix_t));

	struct Mix_t* node_value = (struct Mix_t*)(*node)->value;
	if(mix_type_init((int)((struct Mix_t*)(*v)->value)->type,
							&node_value,
							(void*)((struct Mix_t*)(*v)->value)->v) == -1){
		return -1;
	}

	return 0;
}

int comparison(void *src, void *dest)
{
    struct Mix_t *s = (struct Mix_t*)src;
    struct Mix_t *d = (struct Mix_t*)dest;
	if(s->type != d->type){
		return ERR;	
	}

	switch(s->type){
#if defined(_WIN32)
	case ARR_BYTE:
#else
	case BYTE:
#endif
		if(*(unsigned char*)s->v > *(unsigned char*)d->v) return LEFT;
		if(*(unsigned char*)s->v == *(unsigned char*)d->v) return 0;
		return RIGHT;
#if defined(_WIN32)
	case ARR_INT:
#else
	case INT:
#endif
		if(*(int*)s->v > *(int*)d->v) return LEFT;
		if(*(int*)s->v == *(int*)d->v) return 0;
		return RIGHT;
#if defined(_WIN32)
	case ARR_LONG:
#else
	case LONG:
#endif
		if(*(long*)s->v > *(long*)d->v) return LEFT;
		if(*(long*)s->v == *(long*)d->v) return 0;
		return RIGHT;
	default:
		return ERR;
	}
}

int BST_insert(struct BSTnode **root, struct BSTnode *node,int (*comparison)(void*,void*))
{
	if(!(*root)){
		if(BST_node_init(root,&node) == -1)
			return -1;
		else 
			return 0;
	}

	int result = comparison((*root)->value,node->value);
	switch(result){
	case LEFT:
		/*GO LEFT*/
		if((*root)->left)
			return BST_insert(&(*root)->left,node,comparison);
		if(BST_node_init(&(*root)->left, &node) == -1)
			return -1;
		return 0;
	case RIGHT:
		/*GO right*/
		if((*root)->right)
			return BST_insert(&(*root)->right,node,comparison);
		if(BST_node_init(&(*root)->right,&node) == -1)
			return -1;
		return 0;
	case ERR:
		/*CLEANUP*/
		fprintf(stderr,"comparison() failed, check the types\n");
		return -1;
	default:
		return 0;	
	}
	return 0;
}

void BST_free(struct BSTnode **root)
{
	if(!(*root))
		return;
	
	if((*root)->left)
		BST_free(&(*root)->left);

	if((*root)->right)
		BST_free(&(*root)->right);
		
	FREE_MIX_TYPE((*root)->value);
	free(*root);
	*root = NULL;
}
