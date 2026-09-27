#ifndef __LOAD_DB_H_
#define __LOAD_DB_H_ 1


#ifndef _REQUEST_H_ 
	struct Request;
#else
 	#include "request.h"
#endif

int load_resource_db(struct Request *req, struct Content *cont,int data_sock);




#endif
