#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "file.h"

/* Inject faults around real file I/O, rather than substituting storage. */
static int target = -1, calls, mode;
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
static int fault(size_t *count)
{
    ++calls;
    if(calls == 1){ errno = EINTR; return -1; }
    if(calls == 3 && mode == 1){ errno = ENOSPC; return -1; }
    if(calls == 3 && mode == 2) return 0;
    if(*count > 3) *count = 3;
    return 1;
}
ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
    if(fd == target){ int result = fault(&count); if(result <= 0) return result; }
    return __real_read(fd,buffer,count);
}
ssize_t __wrap_write(int fd, const void *buffer, size_t count)
{
    if(fd == target){ int result = fault(&count); if(result <= 0) return result; }
    return __real_write(fd,buffer,count);
}
int main(void)
{
    char path[] = "/tmp/db-file-io-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    const char expected[] = "a complete database field";
    char actual[sizeof(expected)];
    struct Ram_file cache = {0};
    assert(get_all_record(fd,&cache) == 0 && cache.size == 0 && cache.capacity > 0);
    close_ram_file(&cache);
    target = fd;
    assert(os_write(fd,(void *)expected,sizeof(expected)) == 0 && calls > 3);
    assert(lseek(fd,0,SEEK_SET) == 0);
    calls = 0;
    assert(os_read(fd,actual,sizeof(actual)) == 0 && calls > 3);
    assert(!memcmp(actual,expected,sizeof(actual)));
    /* Real EOF after a prefix must not masquerade as a complete field. */
    assert(lseek(fd,-2,SEEK_END) >= 0);
    calls = 0;
    assert(os_read(fd,actual,sizeof(actual)) == -1 && errno == EIO);
    for(mode = 1; mode <= 2; ++mode){
        calls = 0;
        assert(os_write(fd,(void *)expected,sizeof(expected)) == -1);
        assert(errno == (mode == 1 ? ENOSPC : EIO));
        assert(calls == 3);
        assert(lseek(fd,0,SEEK_SET) == 0);
        calls = 0;
        assert(os_read(fd,actual,sizeof(actual)) == -1);
        assert(errno == (mode == 1 ? ENOSPC : EIO));
        assert(calls == 3);
    }
    calls = 0;
    assert(os_read(fd,NULL,0) == 0 && os_write(fd,NULL,0) == 0 && !calls);
    target = -1;
    assert(ftruncate(fd,0) == 0 && lseek(fd,0,SEEK_SET) == 0);
    assert(os_write(fd,(void *)expected,sizeof(expected)) == 0);
    assert(get_all_record(fd,&cache) == 0 && cache.size >= sizeof(expected));
    assert(!memcmp(cache.mem,expected,sizeof(expected)));
    close_ram_file(&cache);
    close(fd);
    assert(unlink(path) == 0);
    fd = 123;
    assert(open_file(path,0,&fd) == -1 && fd == -1 && errno == ENOENT);
    assert(open_file(path,1,&fd) == -1 && fd == -1 && errno == ENOENT);
    puts("PASS: real file I/O, partial transfers, EINTR, EOF, write failures, failed opens");
}
