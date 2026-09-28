#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include "crud.h"
#include "parse.h"
#include "durable.h"

/* Compiled separately and linked into libcrud.
 * Whole-cache redo snapshots: publish once, replay until checkpoint completes.
 * Private directory + lifetime flock; no CLI/other writer may bypass this lock.
 */
#ifdef __linux__
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#define D_MAX 90
static int d_rootfd=-1, d_dirfd=-1, d_lockfd=-1, d_failed, d_request;
static char d_root[PATH_MAX];
struct d_entry { char path[PATH_MAX]; uint64_t size; uint32_t crc; };
#ifdef DB_DURABILITY_TEST_HOOK
extern int db_durability_test_hook(const char *, int);
#define D_HOOK(p,n) db_durability_test_hook(p,n)
#else
#define D_HOOK(p,n) 0
#endif
static int d_sync(int fd)
{
    int r;
    do { r=fsync(fd); } while(r<0 && errno==EINTR);
    return r;
}
static uint32_t d_crc(uint32_t crc, const unsigned char *p, size_t n)
{
    size_t i; int j;
    for(i=0;i<n;++i){
        crc^=p[i];
        for(j=0;j<8;++j) crc=(crc>>1)^((crc&1)?0xedb88320U:0);
    }
    return crc;
}
static int d_measure(int fd, uint64_t *size, uint32_t *crc)
{
    unsigned char buf[16384]; ssize_t n;
    *size=0; *crc=0xffffffffU;
    if(lseek(fd,0,SEEK_SET)<0) return -1;
    for(;;){
        n=read(fd,buf,sizeof(buf));
        if(n<0 && errno==EINTR) continue;
        if(n<0) return -1;
        if(!n) break;
        *size+=(uint64_t)n; *crc=d_crc(*crc,buf,(size_t)n);
    }
    *crc^=0xffffffffU;
    return 0;
}
static int d_number(int fd, uint64_t *value, int bytes, int writing)
{
    unsigned char buf[8]; int i;
    if(writing){
        for(i=0;i<bytes;++i) buf[i]=(unsigned char)(*value>>(8*i));
        return os_write(fd,buf,(size_t)bytes);
    }
    if(os_read(fd,buf,(size_t)bytes)<0) return -1;
    *value=0;
    for(i=0;i<bytes;++i) *value|=(uint64_t)buf[i]<<(8*i);
    return 0;
}
static int d_path_ok(const char *path)
{
    size_t n=strlen(d_root), length=strlen(path);
    char parent[PATH_MAX], canonical[PATH_MAX], *slash;
    struct stat st;
    if(length<=n+1 || strncmp(path,d_root,n) || path[n]!='/') return 0;
    if(length<4 || (strcmp(path+length-4,".inx") && strcmp(path+length-4,".dat") && strcmp(path+length-4,".sch"))) return 0;
    if(strstr(path,"/../") || strstr(path,"/./") || strstr(path,"//")) return 0;
    strcpy(parent,path); slash=strrchr(parent,'/'); *slash=0;
    if(!realpath(parent,canonical) || strcmp(parent,canonical)) return 0;
    return lstat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_nlink==1;
}
static int d_cleanup(const char *name)
{
    int fd=openat(d_dirfd,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    unsigned i; char file[32];
    if(fd<0) return errno==ENOENT?0:-1;
    for(i=0;i<D_MAX;++i){
        snprintf(file,sizeof(file),"%u",i);
        if(unlinkat(fd,file,0)<0 && errno!=ENOENT){ close(fd); return -1; }
    }
    if(unlinkat(fd,"seal",0)<0 && errno!=ENOENT){ close(fd); return -1; }
    if(unlinkat(fd,"manifest",0)<0 && errno!=ENOENT){ close(fd); return -1; }
    close(fd);
    if(unlinkat(d_dirfd,name,AT_REMOVEDIR)<0) return -1;
    return d_sync(d_dirfd);
}
static int d_install(int journal, const struct d_entry *entry, int index)
{
    char file[32], temporary[PATH_MAX], parent[PATH_MAX], *slash;
    unsigned char buf[16384]; ssize_t n;
    int src=-1, dst=-1, dir=-1, result=-1;
    struct stat st;
    if(!d_path_ok(entry->path) || stat(entry->path,&st)<0) return -1;
    if(snprintf(temporary,sizeof(temporary),"%s.wser-install",entry->path)>=(int)sizeof(temporary)) return -1;
    snprintf(file,sizeof(file),"%d",index);
    src=openat(journal,file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(src<0) goto end;
    /* This reserved temporary name is only used under the lifetime writer lock. */
    if(unlink(temporary)<0 && errno!=ENOENT) goto end;
    dst=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(dst<0) goto end;
    for(;;){
        n=read(src,buf,sizeof(buf));
        if(n<0 && errno==EINTR) continue;
        if(n<0) goto end;
        if(!n) break;
        if(os_write(dst,buf,(size_t)n)<0) goto end;
    }
    if(fchmod(dst,st.st_mode&0777)<0 || d_sync(dst)<0) goto end;
    if(close(dst)<0){ dst=-1; goto end; } dst=-1;
    if(D_HOOK("before_install",index)<0 || rename(temporary,entry->path)<0) goto end;
    if(D_HOOK("after_rename",index)<0) goto end;
    strcpy(parent,entry->path); slash=strrchr(parent,'/'); *slash=0;
    dir=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(dir<0 || d_sync(dir)<0 || D_HOOK("installed",index)<0) goto end;
    result=0;
end:
    if(src>=0) close(src);
    if(dst>=0) close(dst);
    if(dir>=0) close(dir);
    return result;
}
static int d_recover(void)
{
    int dir=-1, manifest=-1, fd=-1, result=-1;
    unsigned i,j,count; uint64_t value,size; uint32_t crc;
    char magic[8], file[32], extra;
    uint64_t sealed_size, sealed_crc;
    struct stat seal_stat;
    struct d_entry *entries=NULL;
    dir=openat(d_dirfd,"pending",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(dir<0) return errno==ENOENT?0:-1;
    manifest=openat(dir,"manifest",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    fd=openat(dir,"seal",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(manifest<0 || fd<0 || fstat(fd,&seal_stat)<0 || seal_stat.st_size!=12 ||
       d_number(fd,&sealed_size,8,0)<0 || d_number(fd,&sealed_crc,4,0)<0 ||
       d_measure(manifest,&size,&crc)<0 || size!=sealed_size || crc!=(uint32_t)sealed_crc) goto end;
    close(fd); fd=-1;
    if(lseek(manifest,0,SEEK_SET)<0 || os_read(manifest,magic,8)<0 || memcmp(magic,"WSERRED1",8)) goto end;
    if(d_number(manifest,&value,4,0)<0 || value>D_MAX || value==0) goto end;
    count=(unsigned)value;
    entries=calloc(count,sizeof(*entries)); if(!entries) goto end;
    /* Validate the entire redo set before replacing any live file. */
    for(i=0;i<count;++i){
        if(d_number(manifest,&value,4,0)<0 || !value || value>=PATH_MAX) goto end;
        if(os_read(manifest,entries[i].path,(size_t)value)<0 || memchr(entries[i].path,0,(size_t)value)) goto end;
        if(!d_path_ok(entries[i].path)) goto end;
        for(j=0;j<i;++j) if(!strcmp(entries[i].path,entries[j].path)) goto end;
        if(d_number(manifest,&entries[i].size,8,0)<0 || d_number(manifest,&value,4,0)<0) goto end;
        entries[i].crc=(uint32_t)value;
        snprintf(file,sizeof(file),"%u",i);
        fd=openat(dir,file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(fd<0 || d_measure(fd,&size,&crc)<0 || size!=entries[i].size || crc!=entries[i].crc) goto end;
        close(fd); fd=-1;
    }
    do { fd=(int)read(manifest,&extra,1); } while(fd<0 && errno==EINTR);
    if(fd!=0){ fd=-1; goto end; } fd=-1;
    for(i=0;i<count;++i) if(d_install(dir,&entries[i],(int)i)<0) goto end;
    if(D_HOOK("checkpoint",0)<0) goto end;
    if(renameat(d_dirfd,"pending",d_dirfd,"done")<0 || d_sync(d_dirfd)<0) goto end;
    if(D_HOOK("retired",0)<0 || d_cleanup("done")<0) goto end;
    result=0;
end:
    if(fd>=0) close(fd);
    if(manifest>=0) close(manifest);
    if(dir>=0) close(dir);
    free(entries);
    return result;
}
int durable_active(void) { return d_lockfd>=0; }
int durable_failed(void) { return d_failed; }
void durable_request(int active) { d_request=active; }
int durable_in_request(void) { return d_request; }
void durable_close(void)
{
    if(d_lockfd>=0) close(d_lockfd);
    if(d_dirfd>=0) close(d_dirfd);
    if(d_rootfd>=0) close(d_rootfd);
    d_lockfd=d_dirfd=d_rootfd=-1; d_request=0;
}
int durable_open(const char *directory)
{
    struct stat st;
    if(durable_active()){ errno=EBUSY; return -1; }
    d_failed=0;
    if(!directory || !realpath(directory,d_root) || !strcmp(d_root,"/")) return -1;
    d_rootfd=open(d_root,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(d_rootfd<0 || fstat(d_rootfd,&st)<0 || st.st_uid!=geteuid() || (st.st_mode&022)) goto failed;
    if(mkdirat(d_rootfd,".wser-durable",0700)<0 && errno!=EEXIST) goto failed;
    if(d_sync(d_rootfd)<0) goto failed;
    d_dirfd=openat(d_rootfd,".wser-durable",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(d_dirfd<0 || fstat(d_dirfd,&st)<0 || st.st_uid!=geteuid() || (st.st_mode&077)) goto failed;
    d_lockfd=openat(d_dirfd,"lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
    if(d_lockfd<0 || fstat(d_lockfd,&st)<0 || !S_ISREG(st.st_mode) || st.st_nlink!=1 ||
       flock(d_lockfd,LOCK_EX|LOCK_NB)<0) goto failed;
    if(d_cleanup("building")<0 || d_cleanup("done")<0 || d_recover()<0) goto failed;
    return 0;
failed:
    d_failed=1;
    durable_close();
    return -1;
}
int durable_commit(struct Cache *caches, int count)
{
    int dir=-1, manifest=-1, fd=-1, i,j,k,n=0,result=-1;
    char file[32], raw[PATH_MAX], resolved[PATH_MAX];
    const char *extensions[]={".inx",".dat",".sch"};
    uint64_t value,size; uint32_t crc;
    struct Header_d header;
    struct d_entry *seen=NULL;
    if(!durable_active() || d_failed || count<0 || count>D_MAX/3) return -1;
    for(i=0;i<count;++i) if(caches[i].index_file && caches[i].file_name) n+=3;
    if(!n) return 0;
    seen=calloc((size_t)n,sizeof(*seen));
    if(!seen) goto end;
    if(d_cleanup("building")<0 || mkdirat(d_dirfd,"building",0700)<0) goto end;
    dir=openat(d_dirfd,"building",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(dir<0) goto end;
    manifest=openat(dir,"manifest",O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    value=(uint64_t)n;
    if(manifest<0 || os_write(manifest,"WSERRED1",8)<0 || d_number(manifest,&value,4,1)<0) goto end;
    n=0;
    for(i=0;i<count;++i){
        struct Cache *c=&caches[i];
        if(!c->index_file || !c->file_name) continue;
        if(c->indexes<1 || c->data_file.size>c->data_file.capacity || (c->data_file.size && !c->data_file.mem)) goto end;
        for(j=0;j<3;++j){
            if(snprintf(raw,sizeof(raw),"%s%s",c->file_name,extensions[j])>=(int)sizeof(raw) ||
               !realpath(raw,resolved) || !d_path_ok(resolved)) goto end;
            for(k=0;k<n;++k) if(!strcmp(seen[k].path,resolved)) goto end;
            strcpy(seen[n].path,resolved);
            snprintf(file,sizeof(file),"%d",n);
            fd=openat(dir,file,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
            if(fd<0) goto end;
            if(j==0){
                if(!write_index_file_head(fd,c->indexes)) goto end;
                for(k=0;k<c->indexes;++k) if(!write_index_body(fd,k,&c->index_file[k])) goto end;
            } else if(j==1){
                if(os_write(fd,c->data_file.mem,c->data_file.size)<0) goto end;
            } else {
                memset(&header,0,sizeof(header)); header.sch_d=&c->sch;
                if(!create_header(&header) || !write_header(fd,&header)) goto end;
            }
            if(D_HOOK("snapshot",n)<0 || d_sync(fd)<0 || d_measure(fd,&size,&crc)<0) goto end;
            if(close(fd)<0){ fd=-1; goto end; } fd=-1;
            value=strlen(resolved);
            if(d_number(manifest,&value,4,1)<0 || os_write(manifest,resolved,(size_t)value)<0 ||
               d_number(manifest,&size,8,1)<0) goto end;
            value=crc;
            if(d_number(manifest,&value,4,1)<0) goto end;
            ++n;
        }
    }
    if(d_sync(manifest)<0 || d_measure(manifest,&size,&crc)<0) goto end;
    fd=openat(dir,"seal",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    value=crc;
    if(fd<0 || d_number(fd,&size,8,1)<0 || d_number(fd,&value,4,1)<0 || d_sync(fd)<0) goto end;
    if(close(fd)<0){ fd=-1; goto end; } fd=-1;
    if(d_sync(dir)<0 || D_HOOK("prepared",0)<0) goto end;
    if(renameat(d_dirfd,"building",d_dirfd,"pending")<0) goto end;
    if(D_HOOK("published",0)<0 || d_sync(d_dirfd)<0 || D_HOOK("committed",0)<0) goto end;
    if(d_recover()<0) goto end;
    result=0;
end:
    if(fd>=0) close(fd);
    if(manifest>=0) close(manifest);
    if(dir>=0) close(dir);
    free(seen);
    if(result<0) d_failed=1; /* Unknown outcome: stop serving; recover on restart. */
    return result;
}
#else
int durable_open(const char *d) { (void)d; return -1; }
void durable_close(void) {}
int durable_active(void) { return 0; }
int durable_failed(void) { return 1; }
void durable_request(int a) { (void)a; }
int durable_in_request(void) { return 0; }
int durable_commit(struct Cache *c,int n) { (void)c;(void)n;return -1; }
#endif
