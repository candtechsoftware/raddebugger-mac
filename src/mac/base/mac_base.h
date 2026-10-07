#ifndef MAC_BASE_H
#define MAC_BASE_H

#include <copyfile.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

typedef struct tm tm;
typedef struct timespec timespec;

#define MAC_RETRY_ON_EINTR(expr)             \
(__extension__({                           \
__typeof__(expr) __ret;                    \
do {                                       \
__ret = (expr);                          \
} while ((__ret == -1) && errno == EINTR); \
__ret;                                     \
}))


typedef struct MAC_FileIter MAC_FileIter; 
struct MAC_FileIter 
{
  DIR *dir; 
  struct dirent *dp;
  String8 path; 
}; 
StaticAssert(sizeof(Member(FileIter, memory)) >= sizeof(MAC_FileIter), mac_file_iter_size_check);


typedef struct MAC_SafeCallChain MAC_SafeCallChain;
struct MAC_SafeCallChain 
{
  MAC_SafeCallChain *next; 
  ThreadEntryPointFunctionType *fail_handler; 
  void *ptr; 
}; 

typedef enum MAC_EntityKind
{
  MAC_EntityKind_Thread, 
  MAC_EntityKind_Mutex, 
  MAC_EntityKind_RWMutex, 
  MAC_EntityKind_ConditionVariable, 
  MAC_EntityKind_Barrier, 
}
MAC_EntityKind; 

typedef struct MAC_Entity MAC_Entity; 
struct MAC_Entity
{
  MAC_Entity *next; 
  MAC_EntityKind kind; 
  union 
  {
    struct
    {
      pthread_t handle;
      ThreadEntryPointFunctionType *func; 
      void *ptr; 
    }thread; 
    pthread_mutex_t  mutex_handle; 
    pthread_rwlock_t rwmutex_handle; 
    struct
    {
      pthread_cond_t cond_handle;
      pthread_mutex_t rwlock_mutex_handle; 
    }cv; 
    struct
    {
      pthread_mutex_t mutex_handle; 
      pthread_cond_t cond_handle; 
      U64 count; 
      U64 waiting_count; 
      U64 generation;
    }barrier; 
  }; 
}; 

typedef struct MAC_IPCName MAC_IPCName;
struct MAC_IPCName
{
  MAC_IPCName *next; 
  String8 name; 
}; 

typedef struct MAC_State MAC_State;
struct MAC_State
{
  Arena *arena; 
  SystemInfo system_info;
  ProcessInfo process_info; 
  pthread_mutex_t entity_mutex; 
  Arena *entity_arena;
  MAC_Entity *entity_free;
  MAC_IPCName *first_ipc_name; 
  U64 default_env_count; 
  char **default_env; 
}; 

global MAC_State mac_state = {0}; 
thread_static MAC_SafeCallChain *mac_safe_call_chain = 0; 


////////////////////////////////
//~ cand: Helpers
internal DateTime mac_date_time_from_tm(tm in, U32 msec);
internal tm mac_tm_from_date_time(DateTime dt);
internal timespec mac_timespec_from_date_time(DateTime dt);
internal DenseTime mac_dense_time_from_timespec(timespec in);
internal FileProperties mac_file_properties_from_stat(struct stat *s);
internal timespec mac_timespec_from_endt_us(U64 endt_us);
internal String8 mac_ipc_name_from_hash(Arena *arena, U64 hash);
internal void mac_ipc_name_unlink_at_exit(String8 ipc_name);
internal void mac_ipc_names_unlink(void);
internal void mac_safe_call_sig_handler(int sig, siginfo_t *info, void *context);

////////////////////////////////
//~ cand: Entities

internal MAC_Entity *mac_entity_alloc(MAC_EntityKind kind);
internal void mac_entity_release(MAC_Entity *entity);

////////////////////////////////
//~ cand: Thread Entry Point

internal void *mac_thread_entry_point(void *ptr);


#endif //MAC_BASE_H