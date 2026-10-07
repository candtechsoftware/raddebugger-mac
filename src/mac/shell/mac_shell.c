////////////////////////////////
//~ @per_os_impl Shell Operations

internal void
sh_message(B32 error, String8 title, String8 message)
{
  if(error)
  {
    fprintf(stderr, "[X] "); 
  }
  fprintf(stderr, "%.*s\n", str8_varg(title));
  fprintf(stderr, "%.*s\n\n", str8_varg(message));
  
  if(NSApp != 0 && 
     [NSApp activationPolicy] == NSApplicationActivationPolicyRegular &&
     [NSThread isMainThread])
  {
    @autoreleasepool 
    {
      NSAlert *alert = [[NSAlert alloc] init]; 
      [alert setAlertStyle:error ? NSAlertStyleCritical : NSAlertStyleInformational]; 
      [alert setMessageText:mac_nsstring_from_str8(title)]; 
      [alert setInformativeText:mac_nsstring_from_str8(message)];
      [alert runModal]; 
      [alert release]; 
    } 
  }
}

internal String8
sh_pick_file(Arena *arena, String8 title, String8 initial_path)
{
  String8 result = {0}; 
  if(NSApp != 0)
  {
    @autoreleasepool
    {
      NSOpenPanel *panel = [NSOpenPanel openPanel]; 
      [panel setMessage:mac_nsstring_from_str8(title)]; 
      if(initial_path.size != 0) 
      {
        [panel setDirectoryURL:[NSURL fileURLWithPath:mac_nsstring_from_str8(initial_path)]];
      } 
      if([panel runModal] ==  NSModalResponseOK)
      {
        result = mac_str8_from_nsstring(arena, [[panel URL] path]); 
      } 
    } 
  } 
  return result; 
} 

internal void 
sh_show_in_file_browser(String8 path)
{
  @autoreleasepool
  {
    [[NSWorkspace sharedWorkspace] selectFile:mac_nsstring_from_str8(path) inFileViewerRootedAtPath:@""];
  }
} 

internal void
sh_open_in_browser(String8 url)
{
  @autoreleasepool
  {
    NSURL *nsurl = [NSURL URLWithString:mac_nsstring_from_str8(url)];
    if(nsurl == 0 || ![[NSWorkspace sharedWorkspace] openURL:nsurl])
    {
      Temp scratch = scratch_begin(0, 0);
      sh_message(1, str8_lit("Error"), str8f(scratch.arena, "Could not open %S in a browser.", url));
      scratch_end(scratch);
    }
  }
}

internal B32
sh_install_or_uninstall_self(B32 write, B32 install)
{
  B32 install_state = 0;
  Temp scratch = scratch_begin(0, 0);
  
  // TODO(cand): really verify if this is the right place for mac? 
  String8 bin_path = str8f(scratch.arena, "%s/.local/bin", getenv("HOME"));
  String8 symlink_path = str8f(scratch.arena, "%S/raddbg", bin_path);
  
  if(write)
  {
    delete_file_at_path(symlink_path);
    if(install)
    {
      make_directory(str8_chop_last_slash(bin_path));
      make_directory(bin_path);
      symlink((char *)get_process_info()->binary_file_path.str, (char *)symlink_path.str);
    }
    install_state = install;
  }
  else
  {
    install_state = file_path_exists(symlink_path);
  }
  
  scratch_end(scratch);
  return install_state;
}