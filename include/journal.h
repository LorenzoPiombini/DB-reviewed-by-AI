#ifndef _JOURNAL_H_
#define _JOURNAL_H_

#include "hash_tbl.h"
#include "time.h"
#include "file.h"

#define JINX "journal.inx"
#define JHST "jarchive.inx"


/* operations */
#define J_DEL 0
#define J_ADD 1

#define DEL_REG 1 	/*0000 0001*/
#define DEL_ORIG 0 	/*0000 0000*/

#define DEL_INX 0

#define PROC_PATH "/proc/self/fd/%d"
#define MAX_FILE_NAME 2048
#define MAX_KEY_STRING 1024


/*errors*/
#define EJCAP 2 /*index file has MAX_STACK_CAP elements*/



/*stack for journal index file */

struct Node_stack{
	time_t timestamp;
	char file_name[MAX_FILE_NAME];
	struct Key key;
	file_offset offset;
	int operation;
};

struct stack{
	int capacity;
	struct Node_stack *elements;
};


int push_journal(struct stack *index, struct Node_stack node);
int pop_journal(struct stack *index);
int peek_journal(struct stack *index, struct Node_stack *node);
int is_empty(struct stack *index);

int journal(file_t caller_fd, file_offset offset, void *key, int key_type, int operation);
int write_journal_index(file_t *fd,struct stack *index);
int read_journal_index(file_t fd,struct stack *index);
int show_journal();
void free_stack(struct stack *index);

#endif
