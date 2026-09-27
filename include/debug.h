#ifndef DEBUG_H
#define DEBUG_H

#define L __LINE__
#define F __FILE__

void loop_str_arr(char **str, int len);
void __er_file_pointer(char *file, int line);
void __er_write_to_file(char *file, int line);
void __er_calloc(char *file, int line);
void __er_malloc(char *file, int line);
void __er_realloc(char *file, int line);
void __er_munmap(char *file, int line);
void __er_release_lock_smo(char *file, int line);
void __er_acquire_lock_smo(char *file, int line);

#endif /* debug.h */
