////////////////////////////////
//~ @dmn_os_hooks Main Layer Initialization (Implemented Per-OS)

internal void
dmn_init(void)
{
}

////////////////////////////////
//~ @dmn_os_hooks Blocking Control Thread Operations (Implemented Per-OS)

internal DMN_CtrlCtx *
dmn_ctrl_begin(void)
{
  DMN_CtrlCtx *ctx = (DMN_CtrlCtx *)1;
  return ctx;
}

internal void
dmn_ctrl_exclusive_access_begin(void)
{
}

internal void
dmn_ctrl_exclusive_access_end(void)
{
}

internal U32
dmn_ctrl_launch(DMN_CtrlCtx *ctx, ProcessLaunchParams *params)
{
  return 0;
}

internal B32
dmn_ctrl_attach(DMN_CtrlCtx *ctx, U32 pid)
{
  return 0;
}

internal B32
dmn_ctrl_kill(DMN_CtrlCtx *ctx, DMN_Handle process, U32 exit_code)
{
  return 0;
}

internal B32
dmn_ctrl_detach(DMN_CtrlCtx *ctx, DMN_Handle process)
{
  return 0;
}

internal DMN_EventList
dmn_ctrl_run(Arena *arena, DMN_CtrlCtx *ctx, DMN_RunCtrls *ctrls)
{
  // the control thread keeps calling until it gets an event; with nothing
  // attached the other backends answer with this one
  DMN_EventList events = {0};
  DMN_Event *e = dmn_event_list_push(arena, &events);
  e->kind       = DMN_EventKind_Error;
  e->error_kind = DMN_ErrorKind_NotAttached;
  return events;
}

////////////////////////////////
//~ @dmn_os_hooks Halting (Implemented Per-OS)

internal void
dmn_halt(U64 code, U64 user_data)
{
}

////////////////////////////////
//~ @dmn_os_hooks Introspection Functions (Implemented Per-OS)

//- non-blocking-control-thread access barriers

internal B32
dmn_access_open(void)
{
  return 1;
}

internal void
dmn_access_close(void)
{
}

//- processes

internal U64
dmn_process_memory_reserve(DMN_Handle process, U64 vaddr, U64 size)
{
  return 0;
}

internal void
dmn_process_memory_commit(DMN_Handle process, U64 vaddr, U64 size)
{
}

internal void
dmn_process_memory_decommit(DMN_Handle process, U64 vaddr, U64 size)
{
}

internal void
dmn_process_memory_release(DMN_Handle process, U64 vaddr, U64 size)
{
}

internal void
dmn_process_memory_protect(DMN_Handle process, U64 vaddr, U64 size, AccessFlags flags)
{
}

internal U64
dmn_process_read(DMN_Handle process, Rng1U64 range, void *dst)
{
  return 0;
}

internal B32
dmn_process_write(DMN_Handle process, Rng1U64 range, void *src)
{
  return 0;
}

//- threads

internal B32
dmn_thread_read_reg_block(DMN_Handle handle, void *reg_block)
{
  return 0;
}

internal B32
dmn_thread_write_reg_block(DMN_Handle handle, void *reg_block)
{
  return 0;
}

internal B32
dmn_thread_get_module_tls_vaddr(DMN_Handle thread, DMN_Handle module, U64 *vaddr_out)
{
  return 0;
}

//- system process listing

internal void
dmn_process_iter_begin(DMN_ProcessIter *iter)
{
  MemoryZeroStruct(iter);
}

internal B32
dmn_process_iter_next(Arena *arena, DMN_ProcessIter *iter, DMN_ProcessInfo *info_out)
{
  return 0;
}

internal void
dmn_process_iter_end(DMN_ProcessIter *iter)
{
}