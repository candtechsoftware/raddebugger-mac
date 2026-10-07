////////////////////////////////
//~ cand: Helpers

internal DateTime
mac_date_time_from_tm(tm in, U32 msec)
{
  DateTime dt = {0};
  dt.sec  = in.tm_sec;
  dt.min  = in.tm_min;
  dt.hour = in.tm_hour;
  dt.day  = in.tm_mday-1;
  dt.mon  = in.tm_mon;
  dt.year = in.tm_year+1900;
  dt.msec = msec;
  return dt;
}

internal tm
mac_tm_from_date_time(DateTime dt)
{
  tm result = {0};
  result.tm_sec = dt.sec;
  result.tm_min = dt.min;
  result.tm_hour= dt.hour;
  result.tm_mday= dt.day+1;
  result.tm_mon = dt.mon;
  result.tm_year= dt.year-1900;
  return result;
}

internal timespec
mac_timespec_from_date_time(DateTime dt)
{
  tm tm_val = mac_tm_from_date_time(dt);
  time_t seconds = timegm(&tm_val);
  timespec result = {0};
  result.tv_sec = seconds;
  return result;
}

internal DenseTime
mac_dense_time_from_timespec(timespec in)
{
  DenseTime result = 0;
  {
    struct tm tm_time = {0};
    gmtime_r(&in.tv_sec, &tm_time);
    DateTime date_time = mac_date_time_from_tm(tm_time, in.tv_nsec/Million(1));
    result = dense_time_from_date_time(date_time);
  }
  return result;
}

internal FileProperties
mac_file_properties_from_stat(struct stat *s)
{
  FileProperties props = {0};
  props.size     = s->st_size;
  props.created  = mac_dense_time_from_timespec(s->st_birthtimespec);
  props.modified = mac_dense_time_from_timespec(s->st_mtimespec);
  if(s->st_mode & S_IFDIR)
  {
    props.flags |= FilePropertyFlag_IsFolder;
  }
  return props;
}

internal timespec
mac_timespec_from_endt_us(U64 endt_us)
{
  U64 now_us = now_time_us();
  U64 wait_us = (endt_us > now_us) ? (endt_us - now_us) : 0;
  wait_us = Min(wait_us, (U64)Million(1)*Billion(1));
  timespec result = {0};
  result.tv_sec  = wait_us/Million(1);
  result.tv_nsec = (wait_us%Million(1))*Thousand(1);
  return result;
}

internal String8
mac_ipc_name_from_hash(Arena *arena, U64 hash)
{
  String8 result = str8f(arena, "/rd%016llx", (unsigned long long)hash);
  return result;
}

internal void
mac_ipc_name_unlink_at_exit(String8 ipc_name)
{
  pthread_mutex_lock(&mac_state.entity_mutex);
  MAC_IPCName *n = push_array(mac_state.entity_arena, MAC_IPCName, 1);
  n->name = push_str8_copy(mac_state.entity_arena, ipc_name);
  SLLStackPush(mac_state.first_ipc_name, n);
  pthread_mutex_unlock(&mac_state.entity_mutex);
}

internal void
mac_ipc_names_unlink(void)
{
  for EachNode(n, MAC_IPCName, mac_state.first_ipc_name)
  {
    sem_unlink((char *)n->name.str);
    shm_unlink((char *)n->name.str);
  }
}

internal void
mac_safe_call_sig_handler(int sig, siginfo_t *info, void *context)
{
  MAC_SafeCallChain *chain = mac_safe_call_chain;
  if(chain != 0 && chain->fail_handler != 0)
  {
    chain->fail_handler(chain->ptr);
  }
  abort();
}


////////////////////////////////
//~ cand: Entities

internal MAC_Entity *
mac_entity_alloc(MAC_EntityKind kind)
{
  MAC_Entity *entity = 0; 
  DeferLoop(pthread_mutex_lock(&mac_state.entity_mutex), 
            pthread_mutex_unlock(&mac_state.entity_mutex)) 
  {
    entity = mac_state.entity_free; 
    if(entity) 
    {
      SLLStackPop(mac_state.entity_free); 
    } 
    else 
    {
      entity = push_array_no_zero(mac_state.entity_arena, MAC_Entity, 1); 
    } 
  } 
  MemoryZeroStruct(entity); 
  entity->kind = kind; 
  return entity; 
} 

internal void 
mac_entity_release(MAC_Entity *entity)
{
  
  DeferLoop(pthread_mutex_lock(&mac_state.entity_mutex), 
            pthread_mutex_unlock(&mac_state.entity_mutex)) 
  {
    SLLStackPush(mac_state.entity_free, entity); 
  } 
} 

////////////////////////////////
//~ cand: Thread Entry Point

internal void *
mac_thread_entry_point(void *ptr)
{
  MAC_Entity *entity = (MAC_Entity *)ptr;
  ThreadEntryPointFunctionType *func = entity->thread.func;
  void *thread_ptr = entity->thread.ptr;
  supplement_thread_base_entry_point(func, thread_ptr);
  return 0;
}

////////////////////////////////
//~ cand: @per_os_impl Debugger Attachment Checking
internal B32 
debugger_is_attached(void)
{
  B32 result = 0; 
  // TODO(cand); 
  return result; 
} 

////////////////////////////////
//~ cand: @per_os_impl Platform Time Functions
internal U64 
now_time_us(void)
{
  struct timespec t; 
  clock_gettime(CLOCK_MONOTONIC, &t); 
  U64 result = t.tv_sec*Million(1)+(t.tv_nsec/Thousand(1));
  return result; 
} 

internal U32
now_time_unix(void)
{
  time_t t = time(0);
  return (U32)t;
}

internal DateTime
now_time_universal(void)
{
  time_t t = 0;
  time(&t);
  struct tm universal_tm = {0};
  gmtime_r(&t, &universal_tm);
  DateTime result = mac_date_time_from_tm(universal_tm, 0);
  return result;
}

internal DateTime
universal_from_local_time(DateTime *dt)
{
  // cand: local DateTime -> universal time_t
  tm local_tm = mac_tm_from_date_time(*dt);
  local_tm.tm_isdst = -1;
  time_t universal_t = mktime(&local_tm);
  
  // cand: universal time_t -> DateTime
  tm universal_tm = {0};
  gmtime_r(&universal_t, &universal_tm);
  DateTime result = mac_date_time_from_tm(universal_tm, 0);
  return result;
}

internal DateTime
local_from_universal_time(DateTime *dt)
{
  // cand: universal DateTime -> local time_t
  tm universal_tm = mac_tm_from_date_time(*dt);
  universal_tm.tm_isdst = -1;
  time_t universal_t = timegm(&universal_tm);
  tm local_tm = {0};
  localtime_r(&universal_t, &local_tm);
  
  // cand: local tm -> DateTime
  DateTime result = mac_date_time_from_tm(local_tm, 0);
  return result;
}

internal void
sleep_ms(U32 ms)
{
  usleep(ms*Thousand(1));
}

////////////////////////////////
//~ cand: @per_os_impl Platform GUID Functions

internal Guid
make_guid(void)
{
  Guid guid = {0};
  arc4random_buf(guid.v, sizeof(guid.v));
  guid.data3 &= 0x0fff;
  guid.data3 |= (4 << 12);
  guid.data4[0] &= 0x3f;
  guid.data4[0] |= 0x80;
  return guid;
}

////////////////////////////////
//~ cand: @per_os_impl Platform Memory Allocation

internal void *
reserve_memory(U64 size)
{
  void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if(result == MAP_FAILED)
  {
    result = 0;
  }
  return result;
}

internal B32
commit_memory(void *ptr, U64 size)
{
  return mprotect(ptr, size, PROT_READ|PROT_WRITE) == 0;
}

internal void
decommit_memory(void *ptr, U64 size)
{
  mmap(ptr, size, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

internal void
release_memory(void *ptr, U64 size)
{
  munmap(ptr, size);
}

//- cand: large pages

internal void *
reserve_memory_large(U64 size)
{
  void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if(result == MAP_FAILED)
  {
    result = 0;
  }
  return result;
}

internal B32
commit_memory_large(void *ptr, U64 size)
{
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
  return 1;
}

////////////////////////////////
//~ cand: @per_os_impl Shared Memory

internal SharedMemory
shared_memory_alloc(U64 size, String8 name)
{
  SharedMemory result = { max_U64 };
  
  if(size == 0 || size > max_S64) { return result; }
  
  Temp scratch = scratch_begin(0, 0);
  U64 name_hash = u64_hash_from_str8(name);
  if(name.size == 0)
  {
    arc4random_buf(&name_hash, sizeof(name_hash));
  }
  String8 ipc_name = mac_ipc_name_from_hash(scratch.arena, name_hash);
  shm_unlink((char *)ipc_name.str);
  int id = shm_open((char *)ipc_name.str, O_RDWR|O_CREAT|O_EXCL, 0666);
  if(name.size == 0)
  {
    shm_unlink((char *)ipc_name.str);
  }
  else
  {
    mac_ipc_name_unlink_at_exit(ipc_name);
  }
  scratch_end(scratch);
  
  if(id >= 0)
  {
    if(ftruncate(id, size) == 0) { result.u64[0] = (U64)id; }
    else                         { close(id); }
  }
  
  return result;
}

internal SharedMemory
shared_memory_open(String8 name)
{
  Temp scratch = scratch_begin(0, 0);
  String8      ipc_name  = mac_ipc_name_from_hash(scratch.arena, u64_hash_from_str8(name));
  int          id        = shm_open((char *)ipc_name.str, O_RDWR, 0);
  SharedMemory result    = {id >= 0 ? (U64)id : max_U64};
  scratch_end(scratch);
  return result;
}

internal void
shared_memory_close(SharedMemory handle)
{
  close((int)handle.u64[0]);
}

internal void *
shared_memory_view_open(SharedMemory handle, Rng1U64 range)
{
  int id = (int)handle.u64[0];
  void *base = mmap(0, dim_1u64(range), PROT_READ|PROT_WRITE, MAP_SHARED, id, range.min);
  if(base == MAP_FAILED)
  {
    base = 0;
  }
  return base;
}

internal void
shared_memory_view_close(SharedMemory handle, void *ptr, Rng1U64 range)
{
  munmap(ptr, dim_1u64(range));
}

////////////////////////////////
//~ cand: @per_os_impl System Info

internal SystemInfo *
get_system_info(void)
{
  return &mac_state.system_info;
}

////////////////////////////////
//~ cand: @per_os_impl Current Thread Info

internal U32
tid(void)
{
  uint64_t thread_id = 0;
  pthread_threadid_np(0, &thread_id);
  U32 result = (U32)thread_id;
  return result;
}

internal void
set_platform_thread_name(String8 name)
{
  Temp scratch = scratch_begin(0, 0);
  String8 name_copy = str8_copy(scratch.arena, name);
  pthread_setname_np((char *)name_copy.str);
  scratch_end(scratch);
}

////////////////////////////////
//~ cand: @per_os_impl Thread Functions

internal Thread
thread_launch(ThreadEntryPointFunctionType *f, void *p)
{
  ProfBeginFunction();
  MAC_Entity *entity = mac_entity_alloc(MAC_EntityKind_Thread);
  entity->thread.func = f;
  entity->thread.ptr = p;
  {
    // a new thread gets 512KB of stack by default here, where Linux gives 8MB
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, MB(8));
    int pthread_result = pthread_create(&entity->thread.handle, &attr, mac_thread_entry_point, entity);
    pthread_attr_destroy(&attr);
    if(pthread_result != 0)
    {
      mac_entity_release(entity);
      entity = 0;
    }
  }
  Thread handle = {(U64)entity};
  ProfEnd();
  return handle;
}

internal B32
thread_join(Thread thread, U64 endt_us)
{
  if(MemoryIsZeroStruct(&thread)) { return 0; }
  MAC_Entity *entity = (MAC_Entity *)thread.u64[0];
  int join_result = pthread_join(entity->thread.handle, 0);
  B32 result = (join_result == 0);
  mac_entity_release(entity);
  return result;
}

internal void
thread_detach(Thread thread)
{
  if(MemoryIsZeroStruct(&thread)) { return; }
  MAC_Entity *entity = (MAC_Entity *)thread.u64[0];
  mac_entity_release(entity);
}

////////////////////////////////
//~ cand: @per_os_impl Synchronization Primitive Functions

//- cand: recursive mutexes

internal Mutex
mutex_alloc(void)
{
  // the thread that holds one may take it again, as the heading says and as
  // a windows critical section allows; a plain pthread mutex would deadlock
  MAC_Entity *entity = mac_entity_alloc(MAC_EntityKind_Mutex);
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int init_result = pthread_mutex_init(&entity->mutex_handle, &attr);
  pthread_mutexattr_destroy(&attr);
  if(init_result == -1)
  {
    mac_entity_release(entity);
    entity = 0;
  }
  Mutex handle = {(U64)entity};
  return handle;
}

internal void
mutex_release(Mutex mutex)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  pthread_mutex_destroy(&entity->mutex_handle);
  mac_entity_release(entity);
}

internal void
mutex_take(Mutex mutex)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  pthread_mutex_lock(&entity->mutex_handle);
}

internal void
mutex_drop(Mutex mutex)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  pthread_mutex_unlock(&entity->mutex_handle);
}

//- cand: reader/writer mutexes

internal RWMutex
rw_mutex_alloc(void)
{
  MAC_Entity *entity = mac_entity_alloc(MAC_EntityKind_RWMutex);
  int init_result = pthread_rwlock_init(&entity->rwmutex_handle, 0);
  if(init_result == -1)
  {
    mac_entity_release(entity);
    entity = 0;
  }
  RWMutex handle = {(U64)entity};
  return handle;
}

internal void
rw_mutex_release(RWMutex mutex)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  pthread_rwlock_destroy(&entity->rwmutex_handle);
  mac_entity_release(entity);
}

internal void
rw_mutex_take(RWMutex mutex, B32 write_mode)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  if(write_mode)
  {
    pthread_rwlock_wrlock(&entity->rwmutex_handle);
  }
  else
  {
    pthread_rwlock_rdlock(&entity->rwmutex_handle);
  }
}

internal void
rw_mutex_drop(RWMutex mutex, B32 write_mode)
{
  if(MemoryIsZeroStruct(&mutex)) { return; }
  MAC_Entity *entity = (MAC_Entity *)mutex.u64[0];
  pthread_rwlock_unlock(&entity->rwmutex_handle);
}

//- cand: condition variables

internal CondVar
cond_var_alloc(void)
{
  MAC_Entity *entity = mac_entity_alloc(MAC_EntityKind_ConditionVariable);
  int init_result = pthread_cond_init(&entity->cv.cond_handle, 0);
  if(init_result == -1)
  {
    mac_entity_release(entity);
    entity = 0;
  }
  int init2_result = 0;
  if(entity)
  {
    init2_result = pthread_mutex_init(&entity->cv.rwlock_mutex_handle, 0);
  }
  if(init2_result == -1)
  {
    pthread_cond_destroy(&entity->cv.cond_handle);
    mac_entity_release(entity);
    entity = 0;
  }
  CondVar handle = {(U64)entity};
  return handle;
}

internal void
cond_var_release(CondVar cv)
{
  if(MemoryIsZeroStruct(&cv)) { return; }
  MAC_Entity *entity = (MAC_Entity *)cv.u64[0];
  pthread_cond_destroy(&entity->cv.cond_handle);
  pthread_mutex_destroy(&entity->cv.rwlock_mutex_handle);
  mac_entity_release(entity);
}

internal B32
cond_var_wait(CondVar cv, Mutex mutex, U64 endt_us)
{
  if(MemoryIsZeroStruct(&cv)) { return 0; }
  if(MemoryIsZeroStruct(&mutex)) { return 0; }
  MAC_Entity *cv_entity = (MAC_Entity *)cv.u64[0];
  MAC_Entity *mutex_entity = (MAC_Entity *)mutex.u64[0];
  timespec wait_timespec = mac_timespec_from_endt_us(endt_us);
  int wait_result = pthread_cond_timedwait_relative_np(&cv_entity->cv.cond_handle, &mutex_entity->mutex_handle, &wait_timespec);
  B32 result = (wait_result != ETIMEDOUT);
  return result;
}

internal B32
cond_var_wait_rw(CondVar cv, RWMutex mutex_rw, B32 write_mode, U64 endt_us)
{
  if(MemoryIsZeroStruct(&cv)) { return 0; }
  if(MemoryIsZeroStruct(&mutex_rw)) { return 0; }
  MAC_Entity *cv_entity = (MAC_Entity *)cv.u64[0];
  MAC_Entity *rw_mutex_entity = (MAC_Entity *)mutex_rw.u64[0];
  timespec wait_timespec = mac_timespec_from_endt_us(endt_us);
  
  pthread_mutex_lock(&cv_entity->cv.rwlock_mutex_handle);
  pthread_rwlock_unlock(&rw_mutex_entity->rwmutex_handle);
  int wait_result = pthread_cond_timedwait_relative_np(&cv_entity->cv.cond_handle, &cv_entity->cv.rwlock_mutex_handle, &wait_timespec);
  pthread_mutex_unlock(&cv_entity->cv.rwlock_mutex_handle);
  if(write_mode)
  {
    pthread_rwlock_wrlock(&rw_mutex_entity->rwmutex_handle);
  }
  else
  {
    pthread_rwlock_rdlock(&rw_mutex_entity->rwmutex_handle);
  }
  B32 result = (wait_result != ETIMEDOUT);
  return result;
}

internal void
cond_var_signal(CondVar cv)
{
  if(MemoryIsZeroStruct(&cv)) { return; }
  MAC_Entity *cv_entity = (MAC_Entity *)cv.u64[0];
  pthread_mutex_lock(&cv_entity->cv.rwlock_mutex_handle);
  pthread_cond_signal(&cv_entity->cv.cond_handle);
  pthread_mutex_unlock(&cv_entity->cv.rwlock_mutex_handle);
}

internal void
cond_var_broadcast(CondVar cv)
{
  if(MemoryIsZeroStruct(&cv)) { return; }
  MAC_Entity *cv_entity = (MAC_Entity *)cv.u64[0];
  pthread_mutex_lock(&cv_entity->cv.rwlock_mutex_handle);
  pthread_cond_broadcast(&cv_entity->cv.cond_handle);
  pthread_mutex_unlock(&cv_entity->cv.rwlock_mutex_handle);
}

//- cand: cross-process semaphores

internal Semaphore
semaphore_alloc(U32 initial_count, U32 max_count, String8 name)
{
  Temp scratch = scratch_begin(0, 0);
  U64 name_hash = u64_hash_from_str8(name);
  if(name.size == 0)
  {
    arc4random_buf(&name_hash, sizeof(name_hash));
  }
  String8 ipc_name = mac_ipc_name_from_hash(scratch.arena, name_hash);
  sem_unlink((char *)ipc_name.str);
  sem_t *s = sem_open((char *)ipc_name.str, O_CREAT | O_EXCL, 0666, initial_count);
  if(name.size == 0)
  {
    sem_unlink((char *)ipc_name.str);
  }
  else
  {
    mac_ipc_name_unlink_at_exit(ipc_name);
  }
  Semaphore result = {0};
  if(s != SEM_FAILED)
  {
    result.u64[0] = (U64)s;
  }
  scratch_end(scratch);
  return result;
}

internal void
semaphore_release(Semaphore semaphore)
{
  semaphore_close(semaphore);
}

internal Semaphore
semaphore_open(String8 name)
{
  Semaphore result = {0};
  {
    Temp scratch = scratch_begin(0, 0);
    String8 ipc_name = mac_ipc_name_from_hash(scratch.arena, u64_hash_from_str8(name));
    sem_t *s = sem_open((char *)ipc_name.str, 0);
    if(s != SEM_FAILED)
    {
      result.u64[0] = (U64)s;
    }
    scratch_end(scratch);
  }
  return result;
}

internal void
semaphore_close(Semaphore semaphore)
{
  if(semaphore.u64[0] != 0)
  {
    sem_t *s = (sem_t *)semaphore.u64[0];
    sem_close(s);
  }
}

internal B32
semaphore_take(Semaphore semaphore, U64 endt_us)
{
  B32 result = 0;
  if(semaphore.u64[0] != 0)
  {
    sem_t *s = (sem_t *)semaphore.u64[0];
    if(endt_us == max_U64)
    {
      result = (MAC_RETRY_ON_EINTR(sem_wait(s)) == 0);
    }
    else for(;;)
    {
      // there is no timed wait on a posix semaphore here, so a deadline polls
      if(MAC_RETRY_ON_EINTR(sem_trywait(s)) == 0)
      {
        result = 1;
        break;
      }
      U64 now_us = now_time_us();
      if(now_us >= endt_us)
      {
        break;
      }
      usleep((useconds_t)Min(endt_us - now_us, Thousand(1)));
    }
  }
  return result;
}

internal void
semaphore_drop_count(Semaphore semaphore, U64 count)
{
  for EachIndex(i, count) {
    int err = -1;
    if(semaphore.u64[0] != 0)
    {
      err = MAC_RETRY_ON_EINTR(sem_post((sem_t*)*semaphore.u64));
      Assert(err == 0);
    }
  }
}

//- cand: barriers

internal Barrier
barrier_alloc(U64 count)
{
  MAC_Entity *entity = mac_entity_alloc(MAC_EntityKind_Barrier);
  if(entity != 0)
  {
    pthread_mutex_init(&entity->barrier.mutex_handle, 0);
    pthread_cond_init(&entity->barrier.cond_handle, 0);
    entity->barrier.count = count;
  }
  Barrier result = {IntFromPtr(entity)};
  return result;
}

internal void
barrier_release(Barrier barrier)
{
  MAC_Entity *entity = (MAC_Entity*)PtrFromInt(barrier.u64[0]);
  if(entity != 0)
  {
    pthread_cond_destroy(&entity->barrier.cond_handle);
    pthread_mutex_destroy(&entity->barrier.mutex_handle);
    mac_entity_release(entity);
  }
}

internal void
barrier_wait(Barrier barrier)
{
  MAC_Entity *entity = (MAC_Entity*)PtrFromInt(barrier.u64[0]);
  if(entity != 0)
  {
    pthread_mutex_lock(&entity->barrier.mutex_handle);
    U64 generation = entity->barrier.generation;
    entity->barrier.waiting_count += 1;
    if(entity->barrier.waiting_count == entity->barrier.count)
    {
      entity->barrier.waiting_count = 0;
      entity->barrier.generation += 1;
      pthread_cond_broadcast(&entity->barrier.cond_handle);
    }
    else for(;entity->barrier.generation == generation;)
    {
      pthread_cond_wait(&entity->barrier.cond_handle, &entity->barrier.mutex_handle);
    }
    pthread_mutex_unlock(&entity->barrier.mutex_handle);
  }
}

////////////////////////////////
//~ cand: @per_os_impl Safe Calls

internal void
safe_call(ThreadEntryPointFunctionType *func, ThreadEntryPointFunctionType *fail_handler, void *ptr)
{
  // cand: push handler to chain
  MAC_SafeCallChain chain = {0};
  SLLStackPush(mac_safe_call_chain, &chain);
  chain.fail_handler = fail_handler;
  chain.ptr = ptr;
  
  // cand: set up sig handler info
  struct sigaction new_act = {0};
  new_act.sa_sigaction = mac_safe_call_sig_handler;
  new_act.sa_flags = SA_SIGINFO;
  int signals_to_handle[] =
  {
    SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP,
  };
  struct sigaction og_act[ArrayCount(signals_to_handle)] = {0};
  
  // cand: attach handler info for all signals
  for(U32 i = 0; i < ArrayCount(signals_to_handle); i += 1)
  {
    sigaction(signals_to_handle[i], &new_act, &og_act[i]);
  }
  
  // cand: call function
  func(ptr);
  
  // cand: reset handler info for all signals
  for(U32 i = 0; i < ArrayCount(signals_to_handle); i += 1)
  {
    sigaction(signals_to_handle[i], &og_act[i], 0);
  }
  mac_safe_call_chain = chain.next;
}

////////////////////////////////
//~ cand: @per_os_impl File System (Implemented Per-OS)

//- cand: files

internal File
file_open(AccessFlags flags, String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  String8 path_copy = push_str8_copy(scratch.arena, path);
  int mac_flags = 0;
  if(flags & AccessFlag_Read && flags & AccessFlag_Write)
  {
    mac_flags = O_RDWR;
  }
  else if(flags & AccessFlag_Write)
  {
    mac_flags = O_WRONLY;
  }
  else if(flags & AccessFlag_Read)
  {
    mac_flags = O_RDONLY;
  }
  if(flags & AccessFlag_Append)
  {
    mac_flags |= O_APPEND;
  }
  if(flags & (AccessFlag_Write|AccessFlag_Append))
  {
    mac_flags |= O_CREAT;
  }
  
  // writing starts the file over, as CREATE_ALWAYS does on windows. the
  // linux backend leaves the old contents past what gets written.
  if((flags & AccessFlag_Write) && !(flags & AccessFlag_Append))
  {
    mac_flags |= O_TRUNC;
  }
  mac_flags |= O_CLOEXEC;
  if(flags & AccessFlag_CreateNew) { mac_flags |= O_CREAT|O_EXCL; }
  int fd = open((char *)path_copy.str, mac_flags, 0755);
  File handle = {0};
  if(fd != -1)
  {
    handle.u64[0] = fd;
  }
  scratch_end(scratch);
  return handle;
}

internal void
file_close(File file)
{
  if(file_match(file, file_zero())) { return; }
  int fd = (int)file.u64[0];
  close(fd);
}

internal FilePair
file_pipe_make(B32 read_inherited, B32 write_inherited)
{
  FilePair result = {0};
  int fds[2] = {0};
  if(pipe(fds) == 0)
  {
    if(!read_inherited)  { fcntl(fds[0], F_SETFD, FD_CLOEXEC); }
    if(!write_inherited) { fcntl(fds[1], F_SETFD, FD_CLOEXEC); }
    result.read.u64[0] = fds[0];
    result.write.u64[0] = fds[1];
  }
  return result;
}

internal U64
file_pipe_read(File file, void *out_data, U64 size)
{
  ssize_t read_size = MAC_RETRY_ON_EINTR(read((int)file.u64[0], out_data, size));
  return read_size > 0 ? (U64)read_size : 0;
}

internal U64
file_pipe_write(File file, void *data, U64 size)
{
  ssize_t write_size = MAC_RETRY_ON_EINTR(write((int)file.u64[0], data, size));
  return write_size > 0 ? (U64)write_size : 0;
}

internal U64
file_pipe_bytes_available(File file)
{
  int size = 0;
  B32 is_ok = !file_match(file, file_zero()) && ioctl((int)file.u64[0], FIONREAD, &size) == 0;
  return is_ok && size > 0 ? (U64)size : 0;
}

internal B32
file_pipe_is_end(File file)
{
  struct pollfd poll_fd = { .fd = (int)file.u64[0], .events = POLLIN|POLLHUP };
  poll(&poll_fd, 1, 0);
  return !!(poll_fd.revents & (POLLHUP|POLLERR|POLLNVAL));
}

internal U64
file_read(File file, Rng1U64 rng, void *out_data)
{
  if(file_match(file, file_zero())) { return 0; }
  int fd = (int)file.u64[0];
  U64 total_num_bytes_to_read = dim_1u64(rng);
  U64 total_num_bytes_read = 0;
  U64 total_num_bytes_left_to_read = total_num_bytes_to_read;
  for(;total_num_bytes_left_to_read > 0;)
  {
    int read_result = pread(fd, (U8 *)out_data + total_num_bytes_read, total_num_bytes_left_to_read, rng.min + total_num_bytes_read);
    if(read_result > 0)
    {
      total_num_bytes_read += read_result;
      total_num_bytes_left_to_read -= read_result;
    }
    else if(read_result == 0 || errno != EINTR)
    {
      break;
    }
  }
  return total_num_bytes_read;
}

internal U64
file_write(File file, Rng1U64 rng, void *data)
{
  if(file_match(file, file_zero())) { return 0; }
  int fd = (int)file.u64[0];
  U64 total_num_bytes_to_write = dim_1u64(rng);
  U64 total_num_bytes_written = 0;
  U64 total_num_bytes_left_to_write = total_num_bytes_to_write;
  for(;total_num_bytes_left_to_write > 0;)
  {
    int write_result = pwrite(fd, (U8 *)data + total_num_bytes_written, total_num_bytes_left_to_write, rng.min + total_num_bytes_written);
    if(write_result >= 0)
    {
      total_num_bytes_written += write_result;
      total_num_bytes_left_to_write -= write_result;
    }
    else if(errno != EINTR)
    {
      break;
    }
  }
  return total_num_bytes_written;
}

internal B32
file_set_times(File file, DateTime date_time)
{
  if(file_match(file, file_zero())) { return 0; }
  int fd = (int)file.u64[0];
  timespec time = mac_timespec_from_date_time(date_time);
  timespec times[2] = {time, time};
  int futimens_result = futimens(fd, times);
  B32 good = (futimens_result != -1);
  return good;
}

internal FileProperties
properties_from_file(File file)
{
  if(file_match(file, file_zero())) { return (FileProperties){0}; }
  int fd = (int)file.u64[0];
  struct stat fd_stat = {0};
  int fstat_result = fstat(fd, &fd_stat);
  FileProperties props = {0};
  if(fstat_result != -1)
  {
    props = mac_file_properties_from_stat(&fd_stat);
  }
  return props;
}

internal FileID
id_from_file(File file)
{
  if(file_match(file, file_zero())) { return (FileID){0}; }
  int fd = (int)file.u64[0];
  struct stat fd_stat = {0};
  int fstat_result = fstat(fd, &fd_stat);
  FileID id = {0};
  if(fstat_result != -1)
  {
    id.v[0] = fd_stat.st_dev;
    id.v[1] = fd_stat.st_ino;
  }
  return id;
}

internal B32
file_set_size(File file, U64 size)
{
  return size <= max_S64 && ftruncate((int)file.u64[0], (off_t)size) == 0;
}

internal B32
file_flush(File file)
{
  return fsync((int)file.u64[0]) == 0;
}

internal B32
replace_file_path(String8 dst, String8 src)
{
  Temp scratch = scratch_begin(0,0);
  char *dst_cstr = (char *)push_str8_copy(scratch.arena, dst).str;
  char *src_cstr = (char *)push_str8_copy(scratch.arena, src).str;
  B32   result   = rename(src_cstr, dst_cstr) == 0;
  scratch_end(scratch);
  return result;
}

internal B32
memory_placeholders_supported(void)
{
  return 0;
}

internal void *
reserve_memory_placeholders(U64 size, U64 block_size)
{
  Assert(size > 0);
  Assert(size == (size_t)size);
  Assert(block_size > 0);
  Assert((size % block_size) == 0);
  Assert((block_size % get_system_info()->page_size) == 0);
  return reserve_memory(size);
}

internal void
release_memory_placeholders(void *ptr, U64 size, U64 block_size)
{
  if(size && block_size) { release_memory(ptr, size); }
}

internal B32
unmap_memory_preserve_placeholder(void *ptr, U64 size)
{
  Assert(ptr != 0);
  Assert(size > 0);
  Assert(size == (size_t)size);
  Assert((IntFromPtr(ptr) % get_system_info()->page_size) == 0);
  Assert((size % get_system_info()->page_size) == 0);
  return mmap(ptr, size, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) == ptr;
}

internal void *
shared_memory_view_replace_placeholder(SharedMemory handle, void *ptr, Rng1U64 range, AccessFlags flags)
{
  Assert(range.max > range.min);
  Assert(range.max < max_S64);
  Assert(dim_1u64(range) == (size_t)dim_1u64(range));
  Assert((range.min % get_system_info()->page_size) == 0);
  Assert((range.max % get_system_info()->page_size) == 0);
  Assert((IntFromPtr(ptr) % get_system_info()->page_size) == 0);
  Assert((flags == AccessFlag_Read) || (flags == (AccessFlag_Read|AccessFlag_Write)));
  int   protection = flags == AccessFlag_Read ? PROT_READ : PROT_READ|PROT_WRITE;
  void *result     = mmap(ptr, dim_1u64(range), protection, MAP_FIXED|MAP_SHARED, (int)handle.u64[0], range.min);
  return result == MAP_FAILED ? 0 : result;
}

internal U64
get_available_commit_memory(void)
{
  return get_system_info()->physical_memory_size;
}

internal B32
split_memory_placeholder(void *ptr, U64 size, U64 first_size)
{
  return first_size > 0 && first_size <= size;
}

internal B32
coalesce_memory_placeholders(void *ptr, U64 size)
{
  return 1;
}

internal void *
file_map_view_replace_placeholder(FileMap map, void *ptr, Rng1U64 range)
{
  void *result = mmap(ptr, dim_1u64(range), PROT_READ, MAP_FIXED|MAP_PRIVATE, (int)map.u64[0], range.min);
  return result == MAP_FAILED ? 0 : result;
}

internal void
prefetch_memory_ranges(U64 count, Rng1U64 *ranges)
{
  U64 page = get_system_info()->page_size;
  for EachIndex(i, count) {
    U64 min = AlignDownPow2(ranges[i].min, page), max = AlignPow2(ranges[i].max, page);
    if (max > min) { madvise(PtrFromInt(min), max-min, MADV_WILLNEED); }
  }
}

internal B32
memory_read_fault_handler_set(MemoryReadFaultFunction *func, void *user_data)
{
  // the pager reads the fault's cause out of x64 registers in a Linux signal
  // frame; without it there is nothing to install, and nothing to remove
  return func == 0;
}

internal B32
file_reserve_size(File file, U64 size)
{
  fstore_t store = { .fst_flags = F_ALLOCATEALL, .fst_posmode = F_PEOFPOSMODE, .fst_length = (off_t)size };
  int status = fcntl((int)file.u64[0], F_PREALLOCATE, &store);
  return status != -1;
}

internal B32
delete_file_at_path(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  B32 result = 0;
  String8 path_copy = push_str8_copy(scratch.arena, path);
  if(remove((char *)path_copy.str) != -1)
  {
    result = 1;
  }
  scratch_end(scratch);
  return result;
}

internal B32
copy_file_path(String8 dst, String8 src)
{
  B32 result = 0;
  File src_h = file_open(AccessFlag_Read, src);
  File dst_h = file_open(AccessFlag_Write, dst);
  if(!file_match(src_h, file_zero()) &&
     !file_match(dst_h, file_zero()))
  {
    // sendfile only writes to sockets here
    int src_fd = (int)src_h.u64[0];
    int dst_fd = (int)dst_h.u64[0];
    result = (fcopyfile(src_fd, dst_fd, 0, COPYFILE_DATA) == 0);
  }
  file_close(src_h);
  file_close(dst_h);
  return result;
}

internal B32
move_file_path(String8 dst, String8 src)
{
  B32 good = 0;
  Temp scratch = scratch_begin(0, 0);
  {
    char *src_cstr = (char *)str8_copy(scratch.arena, src).str;
    char *dst_cstr = (char *)str8_copy(scratch.arena, dst).str;
    int rename_result = rename(src_cstr, dst_cstr);
    good = (rename_result != -1);
  }
  scratch_end(scratch);
  return good;
}

internal String8
full_path_from_path(Arena *arena, String8 path)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8 path_copy = str8_copy(scratch.arena, path);
  char buffer[PATH_MAX] = {0};
  realpath((char *)path_copy.str, buffer);
  String8 result = str8_copy(arena, str8_cstring(buffer));
  scratch_end(scratch);
  return result;
}

internal B32
file_path_exists(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  String8 path_copy = push_str8_copy(scratch.arena, path);
  int access_result = access((char *)path_copy.str, F_OK);
  B32 result = 0;
  if(access_result == 0)
  {
    result = 1;
  }
  scratch_end(scratch);
  return result;
}

internal B32
folder_path_exists(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  B32 exists = 0;
  String8  path_copy = str8_copy(scratch.arena, path);
  DIR *handle = opendir((char *)path_copy.str);
  if(handle)
  {
    closedir(handle);
    exists = 1;
  }
  scratch_end(scratch);
  return exists;
}

internal FileProperties
properties_from_file_path(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  String8 path_copy = str8_copy(scratch.arena, path);
  struct stat f_stat = {0};
  int stat_result = stat((char *)path_copy.str, &f_stat);
  FileProperties props = {0};
  if(stat_result != -1)
  {
    props = mac_file_properties_from_stat(&f_stat);
  }
  scratch_end(scratch);
  return props;
}

//- cand: file maps

internal FileMap
file_map_open(AccessFlags flags, File file)
{
  FileMap map = {file.u64[0]};
  return map;
}

internal void
file_map_close(FileMap map)
{
  // NOTE(cand): nothing to do; `map` handles are the same as `file` handles in
  // the linux implementation (on Windows they require separate handles)
}

internal void *
file_map_view_open(FileMap map, AccessFlags flags, Rng1U64 range)
{
  if(MemoryIsZeroStruct(&map)) { return 0; }
  int fd = (int)map.u64[0];
  int prot_flags = 0;
  if(flags & AccessFlag_Write) { prot_flags |= PROT_WRITE; }
  if(flags & AccessFlag_Read)  { prot_flags |= PROT_READ; }
  int map_flags = MAP_PRIVATE;
  void *base = mmap(0, dim_1u64(range), prot_flags, map_flags, fd, range.min);
  if(base == MAP_FAILED)
  {
    base = 0;
  }
  return base;
}

internal void
file_map_view_close(FileMap map, void *ptr, Rng1U64 range)
{
  munmap(ptr, dim_1u64(range));
}

//- cand: directory iteration

internal FileIter *
file_iter_begin(Arena *arena, String8 path, FileIterFlags flags)
{
  FileIter *base_iter = push_array(arena, FileIter, 1);
  base_iter->flags = flags;
  MAC_FileIter *iter = (MAC_FileIter *)base_iter->memory;
  {
    String8 path_copy = push_str8_copy(arena, path);
    iter->dir = opendir((char *)path_copy.str);
    iter->path = path_copy;
  }
  return base_iter;
}

internal B32
file_iter_next(Arena *arena, FileIter *iter, FileInfo *info_out)
{
  B32 good = 0;
  MAC_FileIter *mac_iter = (MAC_FileIter *)iter->memory;
  for(;mac_iter->dir != 0;)
  {
    // cand: get next entry
    mac_iter->dp = readdir(mac_iter->dir);
    good = (mac_iter->dp != 0);
    
    // cand: unpack entry info
    struct stat st = {0};
    int stat_result = 0;
    if(good)
    {
      Temp scratch = scratch_begin(&arena, 1);
      String8 full_path = push_str8f(scratch.arena, "%S/%s", mac_iter->path, mac_iter->dp->d_name);
      stat_result = stat((char *)full_path.str, &st);
      scratch_end(scratch);
    }
    
    // cand: determine if filtered
    B32 filtered = 0;
    if(good)
    {
      filtered = ((S_ISDIR(st.st_mode) && iter->flags & FileIterFlag_SkipFolders) ||
                  (S_ISREG(st.st_mode) && iter->flags & FileIterFlag_SkipFiles) ||
                  (mac_iter->dp->d_name[0] == '.' && mac_iter->dp->d_name[1] == 0) ||
                  (mac_iter->dp->d_name[0] == '.' && mac_iter->dp->d_name[1] == '.' && mac_iter->dp->d_name[2] == 0));
    }
    
    // cand: output & exit, if good & unfiltered
    if(good && !filtered)
    {
      info_out->name = push_str8_copy(arena, str8_cstring(mac_iter->dp->d_name));
      if(stat_result != -1)
      {
        info_out->props = mac_file_properties_from_stat(&st);
      }
      break;
    }
    
    // cand: exit if not good
    if(!good)
    {
      break;
    }
  }
  return good;
}

internal void
file_iter_end(FileIter *iter)
{
  MAC_FileIter *mac_iter = (MAC_FileIter *)iter->memory;
  if(mac_iter->dir != 0)
  {
    closedir(mac_iter->dir);
  }
}

//- cand: directory creation

internal B32
make_directory(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  B32 result = 0;
  String8 path_copy = push_str8_copy(scratch.arena, path);
  if(mkdir((char *)path_copy.str, 0755) != -1)
  {
    result = 1;
  }
  else
  {
    // match windows behavior
    result = file_path_exists(path);
  }
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ cand: @per_os_impl Aborting

internal void
abort_self(U64 exit_code)
{
  exit((int)exit_code);
}

////////////////////////////////
//~ cand: @per_os_impl Process Info

internal ProcessInfo *
get_process_info(void)
{
  return &mac_state.process_info;
}

internal String8
get_current_path(Arena *arena)
{
  char *cwdir = getcwd(0, 0);
  String8 string = str8_copy(arena, str8_cstring(cwdir));
  free(cwdir);
  return string;
}

internal U32
get_process_start_time_unix(void)
{
  // there is no /proc here; the kernel's process table is read with sysctl
  U64 start_time = 0;
  struct kinfo_proc info = {0};
  size_t info_size = sizeof(info);
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
  int err = sysctl(mib, ArrayCount(mib), &info, &info_size, 0, 0);
  if(err == 0)
  {
    start_time = info.kp_proc.p_starttime.tv_sec;
  }
  return (U32)start_time;
}

////////////////////////////////
//~ cand: @per_os_impl Child Processes

internal Process
process_launch(ProcessLaunchParams *params)
{
  Process handle = {0};
  posix_spawn_file_actions_t file_actions = {0};
  int file_actions_init_code = posix_spawn_file_actions_init(&file_actions);
  if(file_actions_init_code == 0)
  {
    Temp scratch = scratch_begin(0, 0);
    if(params->path.size != 0)
    {
      int chdir_code = posix_spawn_file_actions_addchdir_np(&file_actions, (char *)push_cstr(scratch.arena, params->path).str);
      Assert(chdir_code == 0);
    }
    if(!file_match(params->stdout_file, file_zero()))
    {
      int stdout_code = posix_spawn_file_actions_adddup2(&file_actions, (int)params->stdout_file.u64[0], STDOUT_FILENO);
      Assert(stdout_code == 0);
    }
    if(!file_match(params->stderr_file, file_zero()))
    {
      int stderr_code = posix_spawn_file_actions_adddup2(&file_actions, (int)params->stderr_file.u64[0], STDERR_FILENO);
      Assert(stderr_code == 0);
    }
    if(!file_match(params->stdin_file, file_zero()))
    {
      int stdin_code = posix_spawn_file_actions_adddup2(&file_actions, (int)params->stdin_file.u64[0], STDIN_FILENO);
      Assert(stdin_code == 0);
    }
    File std_files[] = { params->stdout_file, params->stderr_file, params->stdin_file };
    for EachIndex(i, ArrayCount(std_files))
    {
      int fd = (int)std_files[i].u64[0];
      B32 is_unique = !file_match(std_files[i], file_zero());
      for EachIndex(j, i) { is_unique &= !file_match(std_files[i], std_files[j]); }
      if(is_unique && fd > STDERR_FILENO)
      {
        int close_code = posix_spawn_file_actions_addclose(&file_actions, fd);
        Assert(close_code == 0);
      }
    }
    posix_spawnattr_t attr = {0};
    int attr_init_code = posix_spawnattr_init(&attr);
    if(attr_init_code == 0)
    {
      if(params->new_console || params->process_group.u64[0] != 0)
      {
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attr, 0);
      }
      
      // package argv
      char **argv = push_array(scratch.arena, char *, params->cmd_line.node_count + 1);
      {
        argv[0] = (char *)push_cstr(scratch.arena, params->cmd_line.first->string).str;
        U64 arg_idx = 1;
        for EachNode(n, String8Node, params->cmd_line.first->next)
        {
          argv[arg_idx] = (char *)push_cstr(scratch.arena, n->string).str;
          arg_idx += 1;
        }
      }
      
      // package envp
      char **envp = 0;
      if(params->inherit_env)
      {
        envp = mac_state.default_env;
      }
      else
      {
        envp = push_array(scratch.arena, char *, params->env.node_count + 2);
        U64 env_idx = 0;
        for EachNode(n, String8Node, params->cmd_line.first)
        {
          envp[env_idx] = (char *)n->string.str;
          env_idx += 1;
        }
      }
      
      // spawn process
      pid_t pid = 0;
      int spawn_code = posix_spawnp(&pid, argv[0], &file_actions, &attr, argv, envp);
      if(spawn_code == 0)
      {
        handle.u64[0] = (U64)pid;
        if(params->process_group.u64[0] != 0 && !process_group_add(params->process_group, handle))
        {
          kill(pid, SIGKILL);
          waitpid(pid, 0, 0);
          MemoryZeroStruct(&handle);
        }
      }
      
      // clean up attributes
      int attr_destroy_code = posix_spawnattr_destroy(&attr);
    }
    scratch_end(scratch);
    
    // clean up file actions
    int file_actions_destroy_code = posix_spawn_file_actions_destroy(&file_actions);
  }
  return handle;
}

internal U64
pid_from_process(Process process)
{
  U64 result = process.u64[0];
  return result;
}

internal B32
process_poll(Process process, U64 *exit_code_out)
{
  if(process_match(process, process_zero())) { return 0; }
  siginfo_t info = {0};
  B32 result = waitid(P_PID, (pid_t)process.u64[0], &info, WEXITED|WNOHANG|WNOWAIT) == 0 && info.si_pid != 0;
  if(result && exit_code_out != 0)
  {
    *exit_code_out = info.si_code == CLD_EXITED ? (U64)info.si_status : (U64)info.si_status + 128;
  }
  return result;
}

internal B32
process_is_active(Process process)
{
  B32 result = !process_match(process, process_zero()) &&
    !process_poll(process, 0) && kill((pid_t)process.u64[0], 0) == 0;
  return result;
}

internal B32
process_join(Process process, U64 endt_us, U64 *exit_code_out)
{
  B32 result = 0;
  
  pid_t pid = (pid_t)process.u64[0];
  for(;;)
  {
    int status = 0;
    pid_t wait_result = MAC_RETRY_ON_EINTR(waitpid(pid, &status, (endt_us == max_U64) ? 0 : WNOHANG));
    
    if((wait_result == pid) && (WIFEXITED(status) || WIFSIGNALED(status)))
    {
      result = 1;
      if(exit_code_out != 0)
      {
        if     (WIFEXITED(status))   { *exit_code_out = WEXITSTATUS(status); }
        else if(WIFSIGNALED(status)) { *exit_code_out = WTERMSIG(status) + 128; }
      }
      break;
    }
    
    if(wait_result == -1) { break; }
    if(endt_us == 0)      { break; }
    
    U64 now_us = now_time_us();
    if(now_us >= endt_us) { break; }
    
    U64 left_us  = endt_us - now_us;
    U64 sleep_us = Min(left_us, Thousand(1));
    usleep((useconds_t)sleep_us);
  }
  return result;
}

internal void
process_detach(Process process)
{
  // no need to close pid
}

internal B32
process_kill(Process process)
{
  int error_code = kill((pid_t)process.u64[0], SIGKILL);
  B32 is_killed = error_code == 0;
  return is_killed;
}

internal B32
process_send_ctrl_c(Process process)
{
  B32 result = !process_match(process, process_zero()) && kill((pid_t)process.u64[0], SIGINT) == 0;
  return result;
}

typedef struct MAC_ProcessGroup MAC_ProcessGroup;
struct MAC_ProcessGroup
{
  pid_t pid;
  B32 kill_on_close;
};

internal ProcessGroup
process_group_make(B32 kill_on_close)
{
  MAC_ProcessGroup *group = malloc(sizeof(*group));
  if(group != 0) { *group = (MAC_ProcessGroup){ .kill_on_close = kill_on_close }; }
  return (ProcessGroup){ .u64[0] = (U64)group };
}

internal B32
process_group_add(ProcessGroup group, Process process)
{
  MAC_ProcessGroup *mac_group = (MAC_ProcessGroup *)group.u64[0];
  if(mac_group == 0 || mac_group->pid != 0 || process_match(process, process_zero())) { return 0; }
  mac_group->pid = (pid_t)process.u64[0];
  return 1;
}

internal void
process_group_close(ProcessGroup group)
{
  MAC_ProcessGroup *mac_group = (MAC_ProcessGroup *)group.u64[0];
  if(mac_group != 0)
  {
    if(mac_group->kill_on_close && mac_group->pid != 0)
    {
      kill(-mac_group->pid, SIGKILL);
    }
    free(mac_group);
  }
}

////////////////////////////////
//~ cand: @per_os_impl Dynamically-Loaded Libraries

internal Library
library_open(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  char *path_cstr = (char *)str8_copy(scratch.arena, path).str;
  void *so = dlopen(path_cstr, RTLD_LAZY|RTLD_LOCAL);
  Library lib = { (U64)so };
  scratch_end(scratch);
  return lib;
}

internal void
library_close(Library lib)
{
  void *so = (void *)lib.u64[0];
  dlclose(so);
}

internal VoidProc *
library_load_proc(Library lib, String8 name)
{
  Temp scratch = scratch_begin(0, 0);
  void *so = (void *)lib.u64[0];
  char *name_cstr = (char *)str8_copy(scratch.arena, name).str;
  VoidProc *proc = (VoidProc *)dlsym(so, name_cstr);
  scratch_end(scratch);
  return proc;
}

////////////////////////////////
//~ cand: Entry Point

internal void
mac_signal_handler(int sig, siginfo_t *info, void *arg)
{
  // crash handler
  {
    local_persist volatile U32 first = 0;
    if (ins_atomic_u32_eval_cond_assign(&first, 1, 0) != 0)
    {
      for(;;)
      {
        sleep(UINT32_MAX);
      }
    }
    
    local_persist void *ips[4096];
    int ips_count = backtrace(ips, ArrayCount(ips));
    
    fprintf(stderr, "A fatal signal was received: %s (%d). The process is terminating.\n", strsignal(sig), sig);
    fprintf(stderr, "Create a new issue with this report at %s.\n\n", BUILD_ISSUES_LINK_STRING_LITERAL);
    fprintf(stderr, "Callstack:\n");
    for EachIndex(i, ips_count)
    {
      Dl_info info = {0};
      dladdr(ips[i], &info);
      
      // atos is the symbolizer every mac with developer tools has; it prints
      // "function (in module) (file:line)" on one line
      char cmd[2048];
      snprintf(cmd, sizeof(cmd), "atos -o '%s' -l 0x%lx 0x%lx 2>/dev/null", info.dli_fname, (unsigned long)info.dli_fbase, (unsigned long)ips[i]);
      FILE *f = popen(cmd, "r");
      char symbol[512] = {0};
      if(f != 0 && fgets(symbol, sizeof(symbol), f))
      {
        fprintf(stderr, "%ld. [0x%016lx] %s", (long)(i+1), (unsigned long)ips[i], symbol);
      }
      else
      {
        fprintf(stderr, "%ld. [0x%016lx] %s\n", (long)(i+1), (unsigned long)ips[i], info.dli_fname);
      }
      if(f != 0)
      {
        pclose(f);
      }
    }
    fprintf(stderr, "\nVersion: %s%s\n\n", BUILD_VERSION_STRING_LITERAL, BUILD_GIT_HASH_STRING_LITERAL_APPEND);
    
    _exit(1);
  }
}

int
main(int argc, char **argv)
{
  atexit(mac_ipc_names_unlink);
  
  // install signal handler for the crash call stacks
  {
    struct sigaction handler = { .sa_sigaction = mac_signal_handler, .sa_flags = SA_SIGINFO, };
    sigfillset(&handler.sa_mask);
    sigaction(SIGILL, &handler, NULL);
    sigaction(SIGTRAP, &handler, NULL);
    sigaction(SIGABRT, &handler, NULL);
    sigaction(SIGFPE, &handler, NULL);
    sigaction(SIGBUS, &handler, NULL);
    sigaction(SIGSEGV, &handler, NULL);
    sigaction(SIGQUIT, &handler, NULL);
  }
  
  //- cand: set up OS layer
  {
    //- cand: get statically-allocated system/process info
    {
      U64 pages       = (U64)sysconf(_SC_PHYS_PAGES);
      U64 page_size   = (U64)sysconf(_SC_PAGESIZE);
      
      SystemInfo *info = &mac_state.system_info;
      info->logical_processor_count = (U32)sysconf(_SC_NPROCESSORS_ONLN);
      info->page_size               = (U64)getpagesize();
      info->large_page_size         = MB(2);
      info->allocation_granularity  = info->page_size;
      info->physical_memory_size    = pages * page_size;
    }
    {
      ProcessInfo *info = &mac_state.process_info;
      info->pid = (U32)getpid();
    }
    
    //- cand: set up thread context
    TCTX *tctx = tctx_alloc();
    tctx_select(tctx);
    
    //- cand: set up dynamically allocated state
    mac_state.arena = arena_alloc();
    mac_state.entity_arena = arena_alloc();
    pthread_mutex_init(&mac_state.entity_mutex, 0);
    
    // cache default environment
    {
      U64 env_count = 0;
      for(; environ[env_count] != 0; env_count += 1) {}
      char **default_env = push_array(mac_state.arena, char *, env_count+1);
      for EachIndex(idx, env_count)
      {
        default_env[idx] = (char *)str8_copy(mac_state.arena, str8_cstring(environ[idx])).str;
      }
      default_env[env_count] = 0;
      mac_state.default_env_count = env_count;
      mac_state.default_env       = default_env;
    }
    
    //- cand: grab dynamically allocated system info
    {
      Temp scratch = scratch_begin(0, 0);
      SystemInfo *info = &mac_state.system_info;
      
      // cand: get machine name
      B32 got_final_result = 0;
      U8 *buffer = 0;
      int size = 0;
      for(S64 cap = 4096, r = 0; r < 4; cap *= 2, r += 1)
      {
        scratch_end(scratch);
        buffer = push_array(scratch.arena, U8, cap);
        int gethostname_result = gethostname((char*)buffer, cap);
        size = cstring8_length(buffer);
        if(gethostname_result == 0 && size < cap)
        {
          got_final_result = 1;
          break;
        }
      }
      
      // cand: save name to info
      if(got_final_result && size > 0)
      {
        info->machine_name.size = size;
        info->machine_name.str = push_array_no_zero(mac_state.arena, U8, info->machine_name.size + 1);
        MemoryCopy(info->machine_name.str, buffer, info->machine_name.size);
        info->machine_name.str[info->machine_name.size] = 0;
      }
      
      scratch_end(scratch);
    }
    
    //- cand: grab dynamically allocated process info
    {
      Temp scratch = scratch_begin(0, 0);
      ProcessInfo *info = &mac_state.process_info;
      
      // cand: grab binary path
      {
        // cand: get self string. there is no /proc/self/exe here; dyld knows
        // the path it loaded, and says how big a buffer it needs when given none
        uint32_t size = 0;
        _NSGetExecutablePath(0, &size);
        U8 *buffer = push_array(scratch.arena, U8, size);
        _NSGetExecutablePath((char *)buffer, &size);
        
        // cand: save
        info->binary_file_path = full_path_from_path(mac_state.arena, str8_cstring((char *)buffer));
        info->binary_path = str8_chop_last_slash(info->binary_file_path);
      }
      
      // cand: grab initial directory
      {
        info->initial_path = get_current_path(mac_state.arena);
      }
      
      // cand: grab program/user data paths
      {
        char *home = getenv("HOME");
        info->user_program_config_data_path = str8f(mac_state.arena, "%s/Library/Application Support", home);
        info->user_program_cache_data_path  = str8f(mac_state.arena, "%s/Library/Caches", home);
        info->user_program_logs_data_path   = str8f(mac_state.arena, "%s/Library/Logs", home);
      }
      
      scratch_end(scratch);
    }
  }
  
  //- cand: call into "real" entry point
  main_thread_base_entry_point(argc, argv);
}