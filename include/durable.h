#ifndef DB_DURABLE_H
#define DB_DURABLE_H
struct Cache;
/* Internal storage API; external callers use crud.h. */
int durable_open(const char *database_directory);
void durable_close(void);
int durable_active(void);
int durable_failed(void);
void durable_request(int active);
int durable_in_request(void);
int durable_commit(struct Cache *caches, int count);
#endif
