#include <stdio.h>
#include "debug.h"

void loop_str_arr(char **str, int len)
{

        int i = 0;
        for (i = 0; i < len; i++)
                printf("%s, ", str[i]);

        printf("\n");
}

void __er_file_pointer(char *file, int line)
{
        perror("file pointer: ");
        printf(" %s:%d.\n", file, line);
}

void __er_write_to_file(char *file, int line)
{
        perror("write to file: ");
        printf(" %s:%d.\n", file, line);
}

void __er_calloc(char *file, int line)
{
        printf("calloc failed, %s:%d.\n", file, line);
}

void __er_malloc(char *file, int line)
{
        printf("malloc failed, %s:%d.\n", file, line);
}

void __er_realloc(char *file, int line)
{
        printf("realloc failed, %s:%d.\n", file, line);
}

void __er_munmap(char *file, int line)
{
        printf("munmap failed, %s:%d.\n", file, line);
}
void __er_release_lock_smo(char *file, int line)
{
        printf("release_lock_smo() failed, %s:%d.\n", file, line);
}
void __er_acquire_lock_smo(char *file, int line)
{
        printf("acquire_lock_smo() failed, %s:%d.\n", file, line);
}
