#ifndef DB_DURABLE_H
#define DB_DURABLE_H
struct Cache;
/* Linux worker API. All functions are called by the single worker thread. */
int db_durable_open(const char *database_directory);
void db_durable_close(void);
int db_durable_active(void);
int db_durable_failed(void);
void db_durable_request(int active);
int db_durable_in_request(void);
int db_durable_commit(struct Cache *caches, int count);
#endif
