#include <stdio.h>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#elif defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#endif

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include "lock.h"
#include "db_types.h"
#include "file.h"
#include "debug.h"
#include "str_op.h"
#include "parse.h"
#include "common.h"
#include "string_utilities.h"

static int lock(file_t fd, int flag);
int release_lock(file_t *fds,int mode){
	int r = 0, slept = 0, second_to_sleep = 0;
	if(IS_LOCK_FROM_LUA(mode))
		second_to_sleep = 1;

	mode = GET_TYPE_LOCK(mode);
	if(mode < 1 || mode > 3) mode = STD_LOCK;

	while((r = lock(fds[mode-1],UNLOCK)) == WTLK);
	if(r == -1){
		fprintf(stderr,"can't acquire or release proper lock.\n");
		return -1;
	}	
	if(mode == STD_LOCK) return 0;
	if(mode == LOCK_SCHEMA_FILE){
		if(IS_FILE_T_VALID(fds[0])){
			while((r = lock(fds[0],UNLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					return -1;
				}
			}
		}

		if(IS_FILE_T_VALID(fds[1])){
			while((r = lock(fds[1],UNLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					return -1;
				}
			}
		}
		
	}

	if(mode == LOCK_DATA_FILE){
		if(IS_FILE_T_VALID(fds[0])){
			while((r = lock(fds[0],UNLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					return -1;
				}
			}
		}

		if(IS_FILE_T_VALID(fds[2])){
			while((r = lock(fds[2],UNLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					return -1;
				}
			}
		}
	}

	return 0;
}


int acquire_lock(file_t *fds, int mode){
	int r = 0, slept = 0, second_to_sleep = 0;
	if(IS_LOCK_FROM_LUA(mode))
		second_to_sleep = 1;

	mode = GET_TYPE_LOCK(mode);
	if(mode < 1 || mode > 3) mode = STD_LOCK;

	while((r = lock(fds[mode-1],WLOCK)) == WTLK){
		if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
			slept = 1;
		}else{
			slept = 0;
			return -1;
		}
	}

	if(r == -1){
		fprintf(stderr,"can't acquire or release proper lock.\n");
		while((r = lock(fds[mode-1],UNLOCK)) == WTLK);
		return -1;
	}

	if(mode == STD_LOCK) return 0;

	/*IF THE LOCK is DIFFERENT than standard we need to check if the other 2 files are locked!*/
	if(mode == LOCK_SCHEMA_FILE){
		if(IS_FILE_T_VALID(fds[0])){
			while((r = lock(fds[0],WLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					slept = 0;
					return -1;
				}
			}
		}

		if(IS_FILE_T_VALID(fds[1])){
			while((r = lock(fds[1],WLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					slept = 0;
					return -1;
				}
			}
		}
	}

	if(mode == LOCK_DATA_FILE){
		if(IS_FILE_T_VALID(fds[0])){
			while((r = lock(fds[0],WLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					slept = 0;
					return -1;
				}
			}
		}

		if(IS_FILE_T_VALID(fds[0])){
			while((r = lock(fds[2],WLOCK)) == WTLK){
				if(!slept){
#if defined(__linux__) || defined(__APPLE__)
					sleep(second_to_sleep ? second_to_sleep : 10);
#elif defined(_WIN32) || defined(_WIN64)
					Sleep(second_to_sleep ? second_to_sleep*1000 : 10*1000);
#endif
					slept = 1;
				}else{
					slept = 0;
					return -1;
				}
			}
		}
	}

	return 0;
}

static int lock(file_t fd, int flag){

#if defined(__linux__) || defined(__APPLE__)
	struct stat st;
	errno = 0;
	if(fstat(fd,&st) == -1){
		fprintf(stderr,"can't aquire lock on file %s:%d.\n",__FILE__,__LINE__-1);
		fprintf(stderr,"%s\n",strerror(errno));
		return -1;
	}

	ui64 file_id = st.st_ino;
#elif defined(_WIN32) || defined(_WIN64)
	BY_HANDLE_FILE_INFORMATION file_info;
    
    if (!GetFileInformationByHandle(fd, &file_info)) {
        fprintf(stderr, "can't acquire lock on file %s:%d. Win32 Error: %lu\n", __FILE__, __LINE__ - 1, GetLastError());
        return -1;
    }

	ui64 file_id = ((ui64)file_info.nFileIndexHigh << 32) | file_info.nFileIndexLow;
#endif
	size_t l = number_of_digit(file_id) + strlen(".lock")+1;
	char file_name[l];
	memset(file_name,0,l);
	if(copy_to_string(file_name,l,"%u.lock",file_id) == -1){
		fprintf(stderr,"can't aquire lock on file %s:%d.\n",__FILE__,__LINE__-1);
		return -1;
	}


	FILE *fp = fopen(file_name,"r");
	if(fp && (flag == WLOCK || flag == RLOCK)) {
		char line[50];
		memset(line,0,50);
		while(fgets(line,50,fp)){
			errno = 0;
#if defined(__linux__) || defined(__APPLE__)
			pid_t p_on_file = (pid_t) string_to_long(line);
#elif defined(_WIN32) || defined(_WIN64)
			DWORD p_on_file = (DWORD) string_to_long(line);
#endif
			if(errno == EINVAL){ 
				fprintf(stderr,"string_to_long failed, %s:%d.\n",__FILE__,__LINE__-2);
				fclose(fp);
				return -1;
			}

#if defined(__linux__) || defined(__APPLE__)	
			fprintf(stderr,"process number %d, try to lock file locked by %d\n",getpid(),p_on_file);
			if(p_on_file == getpid())
#elif defined(_WIN32) || defined(_WIN64)
			fprintf(stderr,"process number %d, try to lock file locked by %d\n",GetCurrentProcessId(),p_on_file);
			if(p_on_file == (DWORD)GetCurrentProcessId())
#endif
			{
				fclose(fp);
#if defined(__linux__) || defined(__APPLE__)	
				fprintf(stderr,"process number %d already has lock on the file\n",getpid());
#elif defined(_WIN32) || defined(_WIN64)
				fprintf(stderr,"process number %d already has lock on the file\n",GetCurrentProcessId());
#endif
				return 0;
			}
#if defined(__linux__) || defined(__APPLE__)	
			fprintf(stderr,"process number %d did not lock the file because locked by %d\n",getpid(),p_on_file);
#elif defined(_WIN32) || defined(_WIN64)
			fprintf(stderr,"process number %d did not lock the file because locked by %d\n",GetCurrentProcessId(),p_on_file);
#endif
		}	
		fclose(fp);
		return WTLK; 
	} else if(fp && UNLOCK){
		char line[50];
		memset(line,0,50);
		while(fgets(line,50,fp));

		errno = 0;
#if defined(__linux__) || defined(__APPLE__)
			pid_t p_on_file = (pid_t) string_to_long(line);
#elif defined(_WIN32) || defined(_WIN64)
			DWORD p_on_file = (DWORD) string_to_long(line);
#endif
		if(errno == EINVAL){ 
			fclose(fp);
			return -1;
		}

#if defined(__linux__) || defined(__APPLE__)	
		if(p_on_file == getpid())
#elif defined(_WIN32) || defined(_WIN64)
		if(p_on_file == (DWORD)GetCurrentProcessId())
#endif
		{
			fclose(fp);
			unlink(file_name);
			return 0;
		}

		fprintf(stderr,"this pid does not own the lock\n");
		fclose(fp);
		return -1;
	}else if (!fp && flag == WLOCK){
		fp = fopen(file_name,"w");
		if(!fp){
			fprintf(stderr,"can't aquire lock on file %s:%d.\n",__FILE__,__LINE__-1);
			return -1;
		}

#if defined(__linux__) || defined(__APPLE__)	
		pid_t pid = getpid();
#elif defined(_WIN32) || defined(_WIN64)
		DWORD pid = (DWORD)GetCurrentProcessId();
#endif
		size_t pid_str_l = number_of_digit(pid)+2;
		char strpid[pid_str_l];
		memset(strpid,0,pid_str_l);

		if(copy_to_string(strpid,pid_str_l,"%ld\n",pid) < 0){
			fprintf(stderr,"can't aquire lock on file %s:%d.\n",__FILE__,__LINE__-1);
			return -1;
		}

		fputs(strpid,fp);
		fclose(fp);
		return 0;
	}else if(!fp && flag == RLOCK){ 
		return 0;
	}else if(!fp) return 0;

	return -1;
}

#if 0
int is_locked(int files, ...)
{
	va_list args;
	va_start(args, files);
	
	int i;
	for(i = 0; i < files; i++){
		file_t fd = va_arg(args,file_t);
		struct stat st;
		if(fstat(fd,&st) == -1) continue;
		
		size_t l = number_of_digit(st.st_ino)+ strlen(".lock")+1;
		char file_name[l];
		memset(file_name,0,l);
			
		if(copy_to_string(file_name,l,"%ld.lock",st.st_ino) == -1){
			fprintf(stderr,"can't verify lock status\n");
			return -1;
		}

		FILE *fp = fopen(file_name,"r");
		if (fp) {
			char line[50];
			memset(line,0,50);
			while(fgets(line,50,fp)){
				
				errno = 0;
				pid_t p_on_file = (pid_t) string_to_long(line);
				if(errno == EINVAL){
					fprintf(stderr,"string_to_long failed %s:%d.\n",__FILE__,__LINE__-2);
					return -1;
				}

				if(p_on_file == getpid()){
					fclose(fp);
					return 0;
				}
			}

			fclose(fp);	
			return LOCKED;
		} 
			
	}
	return 0;
}
#endif
