/* Legacy isolated unit suites: persistence is exercised by durable_crash_test. */
#include "crud.h"
int db_durable_open(const char *p) { (void)p; return 0; }
void db_durable_close(void) {}
int db_durable_active(void) { return 0; }
int db_durable_failed(void) { return 0; }
void db_durable_request(int a) { (void)a; }
int db_durable_in_request(void) { return 0; }
int db_durable_commit(struct Cache *c,int n) { (void)c; (void)n; return 0; }
