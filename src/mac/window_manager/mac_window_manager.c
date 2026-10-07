////////////////////////////////
//~ Objective-C Classes

@interface MAC_WM_AppDelegate : NSObject <NSApplicationDelegate>
@end

@interface MAC_WM_WindowDelegate : NSObject <NSWindowDelegate>
{
  @public
    MAC_WM_Window *wm_window;
}
@end

@interface MAC_WM_View : NSView <NSTextInputClient>
{
  @public
    MAC_WM_Window *wm_window;
}
@end

@implementation MAC_WM_AppDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
  mac_wm_push_event(WM_EventKind_WindowClose, 0);
  return NSTerminateCancel;
}

@end

@implementation MAC_WM_WindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender
{
  mac_wm_push_event(WM_EventKind_WindowClose, wm_window);
  return NO;
}

- (void)windowDidResignKey:(NSNotification *)notification
{
  mac_wm_push_event(WM_EventKind_WindowLoseFocus, wm_window);
  MemoryZeroArray(mac_wm_state->keycode_is_down);
  MemoryZeroArray(mac_wm_state->mouse_button_is_down);
  [[wm_window->view inputContext] discardMarkedText];
  wm_window->has_marked_text = 0;
}

// a resize that arrives while the application is parked in the pump is drawn
// at once, by running a frame from here. the one that matters is a drag of
// the window's edge: cocoa runs that as a loop of its own inside sendEvent:,
// and the pump does not get control back until the mouse is let go. a resize
// that arrives any other time is already inside a frame, so it asks for the
// next one.
- (void)windowDidResize:(NSNotification *)notification
{
  mac_wm_window_buttons_place(wm_window);
  if(wm_window->first_paint_done && mac_wm_state->is_pumping && !mac_wm_state->is_repainting)
  {
    mac_wm_state->is_repainting = 1;
    update();
    mac_wm_state->is_repainting = 0;
  }
  else
  {
    wm_send_wakeup_event();
  }
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
  [self windowDidResize:notification];
}

// gaining focus and coming out from behind another window change what the
// window should show, and neither is an event the application hears about
- (void)windowDidBecomeKey:(NSNotification *)notification
{
  wm_send_wakeup_event();
}

- (void)windowDidChangeOcclusionState:(NSNotification *)notification
{
  wm_send_wakeup_event();
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
  mac_wm_window_buttons_place(wm_window);
}

@end

@implementation MAC_WM_View

- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

//- file drops

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
  return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
  NSArray *urls = [[sender draggingPasteboard] readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]] options:0];
  if([urls count] != 0)
  {
    WM_Event *e = mac_wm_push_event(WM_EventKind_FileDrop, wm_window);
    e->pos = mac_wm_pos_from_window_point(wm_window, [sender draggingLocation]);
    for(NSURL *url in urls)
    {
      str8_list_push(mac_wm_state->event_arena, &e->strings, mac_str8_from_nsstring(mac_wm_state->event_arena, [url path]));
    }
  }
  return [urls count] != 0;
}

//- text input. interpretKeyEvents: turns key presses into text through the
// system's input machinery (dead keys, option characters, input methods) and
// calls back here with the result.

- (void)insertText:(id)string replacementRange:(NSRange)replacement_range
{
  NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
  String8 text8 = str8_cstring((char *)[text UTF8String]);
  for(U64 off = 0; off < text8.size;)
  {
    UnicodeDecode decode = utf8_decode(text8.str + off, text8.size - off);
    U32 codepoint = decode.codepoint;
    if(codepoint >= 32 && codepoint != 127 && !(0xF700 <= codepoint && codepoint <= 0xF8FF))
    {
      WM_Event *e = mac_wm_push_event(WM_EventKind_Text, wm_window);
      e->character = codepoint;
    }
    off += decode.inc;
  }
  wm_window->has_marked_text = 0;
}

- (void)doCommandBySelector:(SEL)selector
{
  if(selector == @selector(insertNewline:))
  {
    WM_Event *e = mac_wm_push_event(WM_EventKind_Text, wm_window);
    e->character = '\n';
  }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selected_range replacementRange:(NSRange)replacement_range
{
  wm_window->has_marked_text = ([string length] != 0);
}

- (void)unmarkText
{
  wm_window->has_marked_text = 0;
}

- (BOOL)hasMarkedText
{
  return wm_window->has_marked_text;
}

- (NSRange)markedRange
{
  return wm_window->has_marked_text ? NSMakeRange(0, 1) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
  return NSMakeRange(NSNotFound, 0);
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual_range
{
  return 0;
}

- (NSArray *)validAttributesForMarkedText
{
  return [NSArray array];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual_range
{
  NSPoint mouse = [NSEvent mouseLocation];
  return NSMakeRect(mouse.x, mouse.y, 0, 0);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
  return NSNotFound;
}

@end

////////////////////////////////
//~ Helpers

//- handles

internal WM_Window
mac_wm_handle_from_window(MAC_WM_Window *window)
{
  WM_Window handle = {(U64)window};
  return handle;
}

internal MAC_WM_Window *
mac_wm_window_from_handle(WM_Window handle)
{
  MAC_WM_Window *window = (MAC_WM_Window *)handle.u64[0];
  return window;
}

internal MAC_WM_Window *
mac_wm_window_from_nswindow(NSWindow *nswindow)
{
  MAC_WM_Window *result = 0;
  for EachNode(w, MAC_WM_Window, mac_wm_state->first_window)
  {
    if(w->window == nswindow)
    {
      result = w;
      break;
    }
  }
  return result;
}

// a monitor handle is its display id, which outlives any one NSScreen object
internal WM_Monitor
mac_wm_monitor_from_screen(NSScreen *screen)
{
  WM_Monitor result = {[[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue]};
  return result;
}

internal NSScreen *
mac_wm_screen_from_monitor(WM_Monitor monitor)
{
  NSScreen *result = 0;
  for(NSScreen *screen in [NSScreen screens])
  {
    if(mac_wm_monitor_from_screen(screen).u64[0] == monitor.u64[0])
    {
      result = screen;
      break;
    }
  }
  return result;
}

//- events

internal WM_Event *
mac_wm_push_event(WM_EventKind kind, MAC_WM_Window *window)
{
  WM_Event *result = wm_event_list_push_new(mac_wm_state->event_arena, &mac_wm_state->events, kind);
  result->window = mac_wm_handle_from_window(window);
  result->modifiers = wm_get_modifiers();
  
  // an event queued while the pump is asleep has to wake it
  if(mac_wm_state->is_waiting)
  {
    wm_send_wakeup_event();
  }
  return result;
}

//- coordinates
//
// every rect and position in this api is in pixels, with the origin top-left
// and y down, as on the other backends. cocoa works in points, and its screen
// space has the origin bottom-left of the primary screen with y up. so screen
// space is flipped about the primary screen's height and scaled by the primary
// screen's factor, and window space is scaled by the window's own factor.

internal Rng2F32
mac_wm_rect_from_nsrect(NSRect rect)
{
  NSScreen *primary = [[NSScreen screens] firstObject];
  F32 scale = primary ? (F32)[primary backingScaleFactor] : 1.f;
  F32 height = primary ? (F32)NSMaxY([primary frame]) : 0.f;
  F32 x0 = (F32)NSMinX(rect);
  F32 y0 = height - (F32)NSMaxY(rect);
  Rng2F32 result = r2f32p(x0*scale, y0*scale, (x0 + (F32)rect.size.width)*scale, (y0 + (F32)rect.size.height)*scale);
  return result;
}

internal NSRect
mac_wm_nsrect_from_rect(Rng2F32 rect)
{
  NSScreen *primary = [[NSScreen screens] firstObject];
  F32 scale = primary ? (F32)[primary backingScaleFactor] : 1.f;
  F32 height = primary ? (F32)NSMaxY([primary frame]) : 0.f;
  Vec2F32 dim = dim_2f32(rect);
  NSRect result = NSMakeRect(rect.x0/scale, height - rect.y1/scale, dim.x/scale, dim.y/scale);
  return result;
}

// `point` is in the window's base space, as -locationInWindow reports it
internal Vec2F32
mac_wm_pos_from_window_point(MAC_WM_Window *window, NSPoint point)
{
  NSPoint view_point = [window->view convertPoint:point fromView:0];
  F32 scale = (F32)[window->window backingScaleFactor];
  Vec2F32 result = v2f32((F32)view_point.x*scale, (F32)view_point.y*scale);
  return result;
}

//- keys

// command is where a mac keeps the shortcuts that sit on ctrl elsewhere, and
// the application's bindings are written for ctrl, so both keys mean ctrl
internal WM_Modifiers
mac_wm_modifiers_from_flags(NSEventModifierFlags flags)
{
  WM_Modifiers result = 0;
  if(flags & (NSEventModifierFlagCommand|NSEventModifierFlagControl))
  {
    result |= WM_Modifier_Ctrl;
  }
  if(flags & NSEventModifierFlagShift)
  {
    result |= WM_Modifier_Shift;
  }
  if(flags & NSEventModifierFlagOption)
  {
    result |= WM_Modifier_Alt;
  }
  return result;
}

// indexed by the virtual key code, which names a position on the keyboard
read_only global WM_Key mac_wm_key_from_keycode_table[128] =
{
  [0x00] = WM_Key_A,
  [0x01] = WM_Key_S,
  [0x02] = WM_Key_D,
  [0x03] = WM_Key_F,
  [0x04] = WM_Key_H,
  [0x05] = WM_Key_G,
  [0x06] = WM_Key_Z,
  [0x07] = WM_Key_X,
  [0x08] = WM_Key_C,
  [0x09] = WM_Key_V,
  [0x0B] = WM_Key_B,
  [0x0C] = WM_Key_Q,
  [0x0D] = WM_Key_W,
  [0x0E] = WM_Key_E,
  [0x0F] = WM_Key_R,
  [0x10] = WM_Key_Y,
  [0x11] = WM_Key_T,
  [0x12] = WM_Key_1,
  [0x13] = WM_Key_2,
  [0x14] = WM_Key_3,
  [0x15] = WM_Key_4,
  [0x16] = WM_Key_6,
  [0x17] = WM_Key_5,
  [0x18] = WM_Key_Equal,
  [0x19] = WM_Key_9,
  [0x1A] = WM_Key_7,
  [0x1B] = WM_Key_Minus,
  [0x1C] = WM_Key_8,
  [0x1D] = WM_Key_0,
  [0x1E] = WM_Key_RightBracket,
  [0x1F] = WM_Key_O,
  [0x20] = WM_Key_U,
  [0x21] = WM_Key_LeftBracket,
  [0x22] = WM_Key_I,
  [0x23] = WM_Key_P,
  [0x24] = WM_Key_Return,
  [0x25] = WM_Key_L,
  [0x26] = WM_Key_J,
  [0x27] = WM_Key_Quote,
  [0x28] = WM_Key_K,
  [0x29] = WM_Key_Semicolon,
  [0x2A] = WM_Key_BackSlash,
  [0x2B] = WM_Key_Comma,
  [0x2C] = WM_Key_Slash,
  [0x2D] = WM_Key_N,
  [0x2E] = WM_Key_M,
  [0x2F] = WM_Key_Period,
  [0x30] = WM_Key_Tab,
  [0x31] = WM_Key_Space,
  [0x32] = WM_Key_Tick,
  [0x33] = WM_Key_Backspace,
  [0x35] = WM_Key_Esc,
  [0x36] = WM_Key_Ctrl,
  [0x37] = WM_Key_Ctrl,
  [0x38] = WM_Key_Shift,
  [0x39] = WM_Key_CapsLock,
  [0x3A] = WM_Key_Alt,
  [0x3B] = WM_Key_Ctrl,
  [0x3C] = WM_Key_Shift,
  [0x3D] = WM_Key_Alt,
  [0x3E] = WM_Key_Ctrl,
  [0x40] = WM_Key_F17,
  [0x41] = WM_Key_NumPeriod,
  [0x43] = WM_Key_NumStar,
  [0x45] = WM_Key_NumPlus,
  [0x47] = WM_Key_NumLock,
  [0x4B] = WM_Key_NumSlash,
  [0x4C] = WM_Key_Return,
  [0x4E] = WM_Key_NumMinus,
  [0x4F] = WM_Key_F18,
  [0x50] = WM_Key_F19,
  [0x52] = WM_Key_Num0,
  [0x53] = WM_Key_Num1,
  [0x54] = WM_Key_Num2,
  [0x55] = WM_Key_Num3,
  [0x56] = WM_Key_Num4,
  [0x57] = WM_Key_Num5,
  [0x58] = WM_Key_Num6,
  [0x59] = WM_Key_Num7,
  [0x5A] = WM_Key_F20,
  [0x5B] = WM_Key_Num8,
  [0x5C] = WM_Key_Num9,
  [0x60] = WM_Key_F5,
  [0x61] = WM_Key_F6,
  [0x62] = WM_Key_F7,
  [0x63] = WM_Key_F3,
  [0x64] = WM_Key_F8,
  [0x65] = WM_Key_F9,
  [0x67] = WM_Key_F11,
  [0x69] = WM_Key_F13,
  [0x6A] = WM_Key_F16,
  [0x6B] = WM_Key_F14,
  [0x6D] = WM_Key_F10,
  [0x6E] = WM_Key_Menu,
  [0x6F] = WM_Key_F12,
  [0x71] = WM_Key_F15,
  [0x72] = WM_Key_Insert,
  [0x73] = WM_Key_Home,
  [0x74] = WM_Key_PageUp,
  [0x75] = WM_Key_Delete,
  [0x76] = WM_Key_F4,
  [0x77] = WM_Key_End,
  [0x78] = WM_Key_F2,
  [0x79] = WM_Key_PageDown,
  [0x7A] = WM_Key_F1,
  [0x7B] = WM_Key_Left,
  [0x7C] = WM_Key_Right,
  [0x7D] = WM_Key_Down,
  [0x7E] = WM_Key_Up,
};

//- custom border

// the close, minimize and zoom buttons stay the system's own. they are moved
// to sit centered in the application's title bar, however tall it is.
internal void
mac_wm_window_buttons_place(MAC_WM_Window *window)
{
  if(window->custom_border)
  {
    F32 title_thickness = window->custom_border_title_thickness / (F32)[window->window backingScaleFactor];
    for(NSWindowButton kind = NSWindowCloseButton; kind <= NSWindowZoomButton; kind += 1)
    {
      NSButton *button = [window->window standardWindowButton:kind];
      NSRect frame = [button frame];
      NSPoint origin = frame.origin;
      origin.y = Max(0, NSHeight([[button superview] frame]) - (title_thickness + NSHeight(frame))*0.5f);
      if(title_thickness != 0 && origin.y != frame.origin.y)
      {
        [button setFrameOrigin:origin];
      }
    }
  }
}

internal F32
wm_custom_title_bar_left_pad_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  F32 result = 0;
  if(window != 0 && window->custom_border)
  {
    NSRect zoom_frame = [[window->window standardWindowButton:NSWindowZoomButton] frame];
    F32 pad_after_buttons = 8.f;
    result = ((F32)NSMaxX(zoom_frame) + pad_after_buttons) * (F32)[window->window backingScaleFactor];
  }
  return result;
}

// the same questions win32 answers in WM_NCHITTEST, in the same order
internal MAC_WM_HitKind
mac_wm_hit_kind_from_pos(MAC_WM_Window *window, Vec2F32 pos)
{
  WM_Window handle = mac_wm_handle_from_window(window);
  Rng2F32 client_rect = wm_client_rect_from_window(handle);
  F32 edge = window->custom_border_edge_thickness;
  MAC_WM_HitKind result = MAC_WM_HitKind_Client;
  if(!contains_2f32(client_rect, pos))
  {
    result = MAC_WM_HitKind_Outside;
  }
  else if(!window->custom_border || wm_window_is_fullscreen(handle))
  {
    result = MAC_WM_HitKind_Client;
  }
  else if(pos.x < edge || pos.x >= client_rect.x1 - edge || pos.y >= client_rect.y1 - edge)
  {
    result = MAC_WM_HitKind_Edge;
  }
  else if(pos.y < window->custom_border_title_thickness)
  {
    B32 is_over_client_area = 0;
    for EachNode(area, MAC_WM_TitleBarClientArea, window->first_title_bar_client_area)
    {
      if(contains_2f32(area->rect, pos))
      {
        is_over_client_area = 1;
        break;
      }
    }
    if(pos.x < wm_custom_title_bar_left_pad_from_window(handle))
    {
      result = MAC_WM_HitKind_WindowButtons;
    }
    else if(is_over_client_area)
    {
      result = MAC_WM_HitKind_Client;
    }
    else if(pos.y < edge)
    {
      result = MAC_WM_HitKind_Edge;
    }
    else
    {
      result = MAC_WM_HitKind_TitleBar;
    }
  }
  return result;
}

////////////////////////////////
//~ @per_os_impl Main Initialization API (Implemented Per-OS)

internal void
wm_init(void)
{
  Arena *arena = arena_alloc();
  mac_wm_state = push_array(arena, MAC_WM_State, 1);
  mac_wm_state->arena = arena;
  mac_wm_state->event_arena = arena_alloc();
  mac_wm_state->pump_pool = [[NSAutoreleasePool alloc] init];
  
  // the application object exists from here, but the process does not become
  // a foreground application until a window is first painted. the main loop
  // stays ours: nothing here ever calls -run.
  [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO] forKey:@"ApplePressAndHoldEnabled"]];
  [NSWindow setAllowsAutomaticWindowTabbing:NO];
  [NSApplication sharedApplication];
  [NSApp setDelegate:[[MAC_WM_AppDelegate alloc] init]];
  [NSApp finishLaunching];
  
  //- gfx info
  {
    NSScreen *primary = [[NSScreen screens] firstObject];
    F32 caret_blink_period_ms = [[NSUserDefaults standardUserDefaults] floatForKey:@"NSTextInsertionPointBlinkPeriod"];
    mac_wm_state->gfx_info.double_click_time = (F32)[NSEvent doubleClickInterval];
    mac_wm_state->gfx_info.caret_blink_time = (caret_blink_period_ms > 0) ? caret_blink_period_ms/1000.f : 0.5f;
    mac_wm_state->gfx_info.default_refresh_rate = primary ? (F32)[primary maximumFramesPerSecond] : 60.f;
  }
  
  //- cursors
  {
    mac_wm_state->cursors[WM_Cursor_Pointer]         = [NSCursor arrowCursor];
    mac_wm_state->cursors[WM_Cursor_IBar]            = [NSCursor IBeamCursor];
    mac_wm_state->cursors[WM_Cursor_LeftRight]       = [NSCursor resizeLeftRightCursor];
    mac_wm_state->cursors[WM_Cursor_UpDown]          = [NSCursor resizeUpDownCursor];
    mac_wm_state->cursors[WM_Cursor_DownRight]       = [NSCursor frameResizeCursorFromPosition:NSCursorFrameResizePositionBottomRight inDirections:NSCursorFrameResizeDirectionsAll];
    mac_wm_state->cursors[WM_Cursor_UpRight]         = [NSCursor frameResizeCursorFromPosition:NSCursorFrameResizePositionTopRight inDirections:NSCursorFrameResizeDirectionsAll];
    mac_wm_state->cursors[WM_Cursor_UpDownLeftRight] = [NSCursor closedHandCursor];
    mac_wm_state->cursors[WM_Cursor_HandPoint]       = [NSCursor pointingHandCursor];
    mac_wm_state->cursors[WM_Cursor_Disabled]        = [NSCursor operationNotAllowedCursor];
    for EachEnumVal(WM_Cursor, cursor)
    {
      [mac_wm_state->cursors[cursor] retain];
    }
  }
}

////////////////////////////////
//~ @per_os_impl Graphics System Info (Implemented Per-OS)

internal WM_SystemInfo *
wm_get_system_info(void)
{
  return &mac_wm_state->gfx_info;
}

////////////////////////////////
//~ @per_os_impl Clipboards (Implemented Per-OS)

internal void
wm_set_clipboard_text(String8 string)
{
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard clearContents];
  [pasteboard setString:mac_nsstring_from_str8(string) forType:NSPasteboardTypeString];
}

internal String8
wm_get_clipboard_text(Arena *arena)
{
  String8 result = mac_str8_from_nsstring(arena, [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString]);
  return result;
}

////////////////////////////////
//~ @per_os_impl Windows (Implemented Per-OS)

internal WM_Window
wm_window_open(Rng2F32 rect, WM_WindowFlags flags, String8 title)
{
  //- allocate
  MAC_WM_Window *window = mac_wm_state->free_window;
  if(window != 0)
  {
    SLLStackPop(mac_wm_state->free_window);
  }
  else
  {
    window = push_array_no_zero(mac_wm_state->arena, MAC_WM_Window, 1);
  }
  MemoryZeroStruct(window);
  DLLPushBack(mac_wm_state->first_window, mac_wm_state->last_window, window);
  
  //- make the window. with a custom border the content view covers the whole
  // frame, title bar included, and the system draws only its three buttons.
  NSRect frame = mac_wm_nsrect_from_rect(rect);
  NSWindowStyleMask style = (NSWindowStyleMaskTitled|
                             NSWindowStyleMaskClosable|
                             NSWindowStyleMaskMiniaturizable|
                             NSWindowStyleMaskResizable);
  if(flags & WM_WindowFlag_CustomBorder)
  {
    style |= NSWindowStyleMaskFullSizeContentView;
    window->custom_border = 1;
    window->paint_arena = arena_alloc();
  }
  window->window = [[NSWindow alloc] initWithContentRect:frame styleMask:style backing:NSBackingStoreBuffered defer:NO];
  [window->window setFrame:frame display:NO];
  [window->window setTitle:mac_nsstring_from_str8(title)];
  [window->window setReleasedWhenClosed:NO];
  [window->window setAcceptsMouseMovedEvents:YES];
  if(window->custom_border)
  {
    [window->window setTitlebarAppearsTransparent:YES];
    [window->window setTitleVisibility:NSWindowTitleHidden];
  }
  if(flags & WM_WindowFlag_UseDefaultPosition)
  {
    [window->window center];
  }
  
  //- make the content view. the renderer puts its own surface on it later.
  MAC_WM_View *view = [[MAC_WM_View alloc] initWithFrame:[[window->window contentView] frame]];
  view->wm_window = window;
  [view registerForDraggedTypes:[NSArray arrayWithObject:NSPasteboardTypeFileURL]];
  [window->window setContentView:view];
  [window->window makeFirstResponder:view];
  window->view = view;
  [view release];
  
  //- make the delegate
  MAC_WM_WindowDelegate *delegate = [[MAC_WM_WindowDelegate alloc] init];
  delegate->wm_window = window;
  [window->window setDelegate:delegate];
  window->delegate = delegate;
  
  return mac_wm_handle_from_window(window);
}

internal void
wm_window_close(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    [window->window setDelegate:0];
    [window->delegate release];
    [window->window close];
    [window->window release];
    if(window->paint_arena != 0)
    {
      arena_release(window->paint_arena);
    }
    DLLRemove(mac_wm_state->first_window, mac_wm_state->last_window, window);
    SLLStackPush(mac_wm_state->free_window, window);
  }
}

internal void
wm_window_set_title(WM_Window handle, String8 title)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    [window->window setTitle:mac_nsstring_from_str8(title)];
  }
}

internal void
wm_window_first_paint(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    //- the first window on screen is what makes this a foreground application
    if([NSApp mainMenu] == 0)
    {
      NSString *name = [[NSProcessInfo processInfo] processName];
      NSMenu *main_menu = [[NSMenu alloc] initWithTitle:@""];
      NSMenu *app_menu = [[NSMenu alloc] initWithTitle:@""];
      NSMenu *window_menu = [[NSMenu alloc] initWithTitle:@"Window"];
      [app_menu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
      [app_menu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];
      [window_menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
      [window_menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
      [[main_menu addItemWithTitle:@"" action:0 keyEquivalent:@""] setSubmenu:app_menu];
      [[main_menu addItemWithTitle:@"Window" action:0 keyEquivalent:@""] setSubmenu:window_menu];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
      [NSApp setMainMenu:main_menu];
      [NSApp setWindowsMenu:window_menu];
      [main_menu release];
      [app_menu release];
      [window_menu release];
    }
    
    window->first_paint_done = 1;
    wm_window_focus(handle);
    if(window->fullscreen_at_first_paint)
    {
      [window->window toggleFullScreen:0];
    }
  }
}

internal void
wm_window_focus(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    [window->window makeKeyAndOrderFront:0];
    [NSApp activate];
  }
}

internal B32
wm_window_is_focused(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  B32 result = (window != 0 && [window->window isKeyWindow]);
  return result;
}

// a window cannot go fullscreen before it is on screen, so a request made
// before the first paint is kept until then
internal B32
wm_window_is_fullscreen(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  B32 result = 0;
  if(window != 0)
  {
    result = window->first_paint_done ? !!([window->window styleMask] & NSWindowStyleMaskFullScreen) : window->fullscreen_at_first_paint;
  }
  return result;
}

internal void
wm_window_set_fullscreen(WM_Window handle, B32 fullscreen)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0 && !window->first_paint_done)
  {
    window->fullscreen_at_first_paint = fullscreen;
  }
  else if(window != 0 && wm_window_is_fullscreen(handle) != !!fullscreen)
  {
    [window->window toggleFullScreen:0];
  }
}

internal B32
wm_window_is_maximized(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  B32 result = (window != 0 && [window->window isZoomed]);
  return result;
}

internal void
wm_window_set_maximized(WM_Window handle, B32 maximized)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0 && wm_window_is_maximized(handle) != !!maximized)
  {
    [window->window zoom:0];
  }
}

internal B32
wm_window_is_minimized(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  B32 result = (window != 0 && [window->window isMiniaturized]);
  return result;
}

internal void
wm_window_set_minimized(WM_Window handle, B32 minimized)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0 && minimized)
  {
    [window->window miniaturize:0];
  }
  else if(window != 0)
  {
    [window->window deminiaturize:0];
  }
}

internal void
wm_window_bring_to_front(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    [window->window orderFront:0];
  }
}

internal void
wm_window_set_monitor(WM_Window handle, WM_Monitor monitor)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  if(window != 0 && screen != 0)
  {
    NSRect work_rect = [screen visibleFrame];
    NSRect frame = [window->window frame];
    [window->window setFrameOrigin:NSMakePoint(NSMidX(work_rect) - NSWidth(frame)/2, NSMidY(work_rect) - NSHeight(frame)/2)];
  }
}

internal void
wm_window_clear_custom_border_data(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0 && window->custom_border)
  {
    arena_clear(window->paint_arena);
    window->first_title_bar_client_area = window->last_title_bar_client_area = 0;
    window->custom_border_title_thickness = 0;
    window->custom_border_edge_thickness = 0;
  }
}

internal void
wm_window_push_custom_title_bar(WM_Window handle, F32 thickness)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    window->custom_border_title_thickness = thickness;
    mac_wm_window_buttons_place(window);
  }
}

internal void
wm_window_push_custom_edges(WM_Window handle, F32 thickness)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0)
  {
    window->custom_border_edge_thickness = thickness;
  }
}

internal void
wm_window_push_custom_title_bar_client_area(WM_Window handle, Rng2F32 rect)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  if(window != 0 && window->custom_border)
  {
    MAC_WM_TitleBarClientArea *area = push_array(window->paint_arena, MAC_WM_TitleBarClientArea, 1);
    area->rect = rect;
    SLLQueuePush(window->first_title_bar_client_area, window->last_title_bar_client_area, area);
  }
}

internal Rng2F32
wm_rect_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  Rng2F32 result = {0};
  if(window != 0)
  {
    result = mac_wm_rect_from_nsrect([window->window frame]);
  }
  return result;
}

internal Rng2F32
wm_client_rect_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  Rng2F32 result = {0};
  if(window != 0)
  {
    NSRect rect_px = [window->view convertRectToBacking:[window->view bounds]];
    result = r2f32p(0, 0, (F32)rect_px.size.width, (F32)rect_px.size.height);
  }
  return result;
}

internal F32
wm_dpi_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  F32 result = 96.f;
  if(window != 0)
  {
    result = 96.f * (F32)[window->window backingScaleFactor];
  }
  return result;
}

////////////////////////////////
//~ @per_os_impl External Windows (Implemented Per-OS)

// what can be asked about another program's window here is which application
// is in front, so the handle is that application's process id
internal WM_ExtWindow
wm_focused_external_window(void)
{
  WM_ExtWindow result = {(U64)[[[NSWorkspace sharedWorkspace] frontmostApplication] processIdentifier]};
  return result;
}

internal void
wm_focus_external_window(WM_ExtWindow handle)
{
  if(handle.u64[0] != 0)
  {
    [[NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)handle.u64[0]] activateWithOptions:0];
  }
}

////////////////////////////////
//~ @per_os_impl Monitors (Implemented Per-OS)

internal WM_MonitorArray
wm_push_monitors_array(Arena *arena)
{
  NSArray *screens = [NSScreen screens];
  WM_MonitorArray result = {0};
  result.count = [screens count];
  result.v = push_array(arena, WM_Monitor, result.count);
  for EachIndex(idx, result.count)
  {
    result.v[idx] = mac_wm_monitor_from_screen([screens objectAtIndex:idx]);
  }
  return result;
}

internal WM_Monitor
wm_primary_monitor(void)
{
  WM_Monitor result = mac_wm_monitor_from_screen([[NSScreen screens] firstObject]);
  return result;
}

internal WM_Monitor
wm_monitor_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  WM_Monitor result = {0};
  if(window != 0)
  {
    result = mac_wm_monitor_from_screen([window->window screen]);
  }
  return result;
}

internal String8
wm_name_from_monitor(Arena *arena, WM_Monitor monitor)
{
  String8 result = mac_str8_from_nsstring(arena, [mac_wm_screen_from_monitor(monitor) localizedName]);
  return result;
}

// screen space, so in the primary screen's pixels like every other screen rect
internal Vec2F32
wm_dim_from_monitor(WM_Monitor monitor)
{
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  Vec2F32 result = {0};
  if(screen != 0)
  {
    result = dim_2f32(mac_wm_rect_from_nsrect([screen visibleFrame]));
  }
  return result;
}

internal F32
wm_dpi_from_monitor(WM_Monitor monitor)
{
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  F32 result = 96.f;
  if(screen != 0)
  {
    result = 96.f * (F32)[screen backingScaleFactor];
  }
  return result;
}

////////////////////////////////
//~ @per_os_impl Events (Implemented Per-OS)

// posting an event is the one thing here another thread may do; it is what
// makes a blocked -nextEventMatchingMask: return
internal void
wm_send_wakeup_event(void)
{
  @autoreleasepool
  {
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:0 subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:NO];
  }
}

internal void
mac_wm_event_translate(NSEvent *event)
{
  NSEventType type = [event type];
  NSEventModifierFlags flags = [event modifierFlags];
  MAC_WM_Window *window = mac_wm_window_from_nswindow([event window]);
  B32 send_to_cocoa = 1;
  switch(type)
  {
    default:{}break;
    
    //- key presses and releases. cocoa never sees these: its responder chain
    // beeps at a key nobody claims. the one exception is a command shortcut
    // that the main menu owns.
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    {
      send_to_cocoa = 0;
      B32 is_down = (type == NSEventTypeKeyDown);
      U16 keycode = [event keyCode];
      B32 is_menu_shortcut = (is_down && (flags & NSEventModifierFlagCommand) && [[NSApp mainMenu] performKeyEquivalent:event]);
      B32 is_chord = !!(flags & (NSEventModifierFlagCommand|NSEventModifierFlagControl));
      B32 is_composing = (window != 0 && window->has_marked_text && !is_chord);
      if(!is_menu_shortcut && keycode < ArrayCount(mac_wm_key_from_keycode_table))
      {
        mac_wm_state->keycode_is_down[keycode] = is_down;
        
        // a key pressed in the middle of a composition (return to accept it,
        // arrows to choose) is the input method's, and not a key press
        if(!(is_down && is_composing))
        {
          WM_Event *e = mac_wm_push_event(is_down ? WM_EventKind_Press : WM_EventKind_Release, window);
          e->modifiers = mac_wm_modifiers_from_flags(flags);
          e->key = mac_wm_key_from_keycode_table[keycode];
          e->is_repeat = (is_down && [event isARepeat]);
          e->repeat_count = 1;
        }
        
        // text comes from the system's input machinery, by way of the view.
        // a shortcut chord is not text.
        if(is_down && window != 0 && !is_chord)
        {
          [window->view interpretKeyEvents:[NSArray arrayWithObject:event]];
        }
      }
    }break;
    
    //- modifier keys. the event names the key and carries the modifier state
    // after it, not whether the key went down or up. if the key's modifier is
    // now clear it went up; if it is set, the key changed, so it went the
    // opposite way from where it was (the other key of the pair may hold it).
    case NSEventTypeFlagsChanged:
    {
      U16 keycode = [event keyCode];
      WM_Key key = (keycode < ArrayCount(mac_wm_key_from_keycode_table)) ? mac_wm_key_from_keycode_table[keycode] : WM_Key_Null;
      B32 is_command = (keycode == 0x36 || keycode == 0x37);
      NSEventModifierFlags key_flag = 0;
      switch(key)
      {
        default:{}break;
        case WM_Key_Shift:    {key_flag = NSEventModifierFlagShift;}break;
        case WM_Key_Alt:      {key_flag = NSEventModifierFlagOption;}break;
        case WM_Key_Ctrl:     {key_flag = is_command ? NSEventModifierFlagCommand : NSEventModifierFlagControl;}break;
        case WM_Key_CapsLock: {key_flag = NSEventModifierFlagCapsLock;}break;
      }
      if(key_flag != 0)
      {
        B32 is_down = ((flags & key_flag) && !mac_wm_state->keycode_is_down[keycode]);
        mac_wm_state->keycode_is_down[keycode] = is_down;
        WM_Event *e = mac_wm_push_event(is_down ? WM_EventKind_Press : WM_EventKind_Release, window);
        e->modifiers = mac_wm_modifiers_from_flags(flags) & ~mac_wm_modifiers_from_flags(key_flag);
        e->key = key;
        e->right_sided = (keycode == 0x36 || keycode == 0x3C || keycode == 0x3D || keycode == 0x3E);
        e->repeat_count = 1;
      }
    }break;
    
    //- mouse presses and releases. each press has exactly one owner. over the
    // application's own area it is the application, and cocoa does not see
    // it. on the title bar it starts a window drag. anywhere else (the three
    // window buttons, the resize border, outside the content) it is cocoa's.
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
    {
      B32 is_down = (type == NSEventTypeLeftMouseDown || type == NSEventTypeRightMouseDown || type == NSEventTypeOtherMouseDown);
      NSInteger button = [event buttonNumber];
      if(window != 0 && button < ArrayCount(mac_wm_state->mouse_button_is_down))
      {
        WM_Key keys[] = {WM_Key_LeftMouseButton, WM_Key_RightMouseButton, WM_Key_MiddleMouseButton};
        Vec2F32 pos = mac_wm_pos_from_window_point(window, [event locationInWindow]);
        MAC_WM_HitKind hit_kind = mac_wm_hit_kind_from_pos(window, pos);
        B32 is_app_owned = is_down ? (hit_kind == MAC_WM_HitKind_Client) : mac_wm_state->mouse_button_is_down[button];
        B32 is_title_bar_press = (is_down && button == 0 && hit_kind == MAC_WM_HitKind_TitleBar);
        
        // cocoa makes a window key when it is sent a press, and it is not sent
        // these. this comes before the button is recorded as held: the window
        // that loses key forgets every held button.
        if(is_down && (is_app_owned || is_title_bar_press) && window->first_paint_done && ![window->window isKeyWindow])
        {
          [window->window makeKeyAndOrderFront:0];
        }
        if(is_app_owned)
        {
          send_to_cocoa = 0;
          mac_wm_state->mouse_button_is_down[button] = is_down;
          WM_Event *e = mac_wm_push_event(is_down ? WM_EventKind_Press : WM_EventKind_Release, window);
          e->modifiers = mac_wm_modifiers_from_flags(flags);
          e->key = keys[button];
          e->pos = pos;
        }
        else if(is_title_bar_press)
        {
          send_to_cocoa = 0;
          if([event clickCount] == 2)
          {
            [window->window zoom:0];
          }
          else
          {
            [window->window performWindowDragWithEvent:event];
          }
        }
      }
    }break;
    
    //- mouse motion
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    {
      if(window != 0)
      {
        Vec2F32 pos = mac_wm_pos_from_window_point(window, [event locationInWindow]);
        MAC_WM_HitKind hit_kind = mac_wm_hit_kind_from_pos(window, pos);
        B32 is_dragging = (mac_wm_state->mouse_button_is_down[0] || mac_wm_state->mouse_button_is_down[1] || mac_wm_state->mouse_button_is_down[2]);
        
        // the key window is sent every move, wherever on the screen the mouse
        // is. the application hears the ones over the window or during a
        // drag, and the first one after, which is how it sees the mouse leave.
        B32 is_over_window = (is_dragging || hit_kind != MAC_WM_HitKind_Outside);
        if(is_over_window || mac_wm_state->mouse_is_over_window)
        {
          WM_Event *e = mac_wm_push_event(WM_EventKind_MouseMove, window);
          e->modifiers = mac_wm_modifiers_from_flags(flags);
          e->pos = pos;
        }
        mac_wm_state->mouse_is_over_window = is_over_window;
        
        // the application's cursor applies over its own area; over the
        // resize border and outside the window the cursor is cocoa's
        mac_wm_state->mouse_is_over_client = (is_dragging || hit_kind == MAC_WM_HitKind_Client || hit_kind == MAC_WM_HitKind_TitleBar);
        if(mac_wm_state->mouse_is_over_client)
        {
          [mac_wm_state->cursors[mac_wm_state->cursor] set];
        }
      }
    }break;
    
    //- scrolling. the application's unit is the windows one: 120 for a wheel
    // notch, and 40 scrolls one row. a wheel here reports whole lines, a line
    // being a row. a trackpad reports points, a stream of small events that
    // only mean something summed, so they accumulate until they make a row.
    // the system has already turned a wheel with shift held into a sideways
    // scroll, which the application also does on seeing shift, so it is not
    // shown shift.
    case NSEventTypeScrollWheel:
    {
      if(window != 0)
      {
        send_to_cocoa = 0;
        Vec2F32 scroll = {0};
        if([event hasPreciseScrollingDeltas])
        {
          scroll = v2f32((F32)[event scrollingDeltaX]*2.f, (F32)[event scrollingDeltaY]*2.f);
        }
        else
        {
          scroll = v2f32((F32)CGEventGetIntegerValueField([event CGEvent], kCGScrollWheelEventDeltaAxis2)*40.f,
                         (F32)CGEventGetIntegerValueField([event CGEvent], kCGScrollWheelEventDeltaAxis1)*40.f);
        }
        Vec2F32 total = sub_2f32(mac_wm_state->scroll_remainder, scroll);
        Vec2F32 delta = v2f32((F32)((S32)(total.x/40.f))*40.f, (F32)((S32)(total.y/40.f))*40.f);
        mac_wm_state->scroll_remainder = sub_2f32(total, delta);
        if(delta.x != 0 || delta.y != 0)
        {
          WM_Event *e = mac_wm_push_event(WM_EventKind_Scroll, window);
          e->modifiers = mac_wm_modifiers_from_flags(flags) & ~WM_Modifier_Shift;
          e->pos = mac_wm_pos_from_window_point(window, [event locationInWindow]);
          e->delta = delta;
        }
      }
    }break;
    
    //- wakeup
    case NSEventTypeApplicationDefined:
    {
      send_to_cocoa = 0;
      mac_wm_push_event(WM_EventKind_Wakeup, 0);
    }break;
  }
  
  if(send_to_cocoa)
  {
    [NSApp sendEvent:event];
  }
}

internal WM_EventList
wm_get_events(Arena *arena, B32 wait)
{
  // a frame run from inside a resize must not take events out from under the
  // loop cocoa is running; it only collects what delegates have already queued
  if(!mac_wm_state->is_repainting)
  {
    // objects autoreleased by any hook since the last pump die here. nothing
    // else would ever release them, since this program never returns to
    // cocoa's own loop.
    [mac_wm_state->pump_pool drain];
    mac_wm_state->pump_pool = [[NSAutoreleasePool alloc] init];
    mac_wm_state->is_pumping = 1;
    for(;;)
    {
      mac_wm_state->is_waiting = (wait && mac_wm_state->events.count == 0);
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:(mac_wm_state->is_waiting ? [NSDate distantFuture] : [NSDate distantPast]) inMode:NSDefaultRunLoopMode dequeue:YES];
      mac_wm_state->is_waiting = 0;
      if(event == 0)
      {
        break;
      }
      mac_wm_event_translate(event);
    }
    mac_wm_state->is_pumping = 0;
  }
  WM_EventList result = wm_event_list_copy(arena, &mac_wm_state->events);
  arena_clear(mac_wm_state->event_arena);
  MemoryZeroStruct(&mac_wm_state->events);
  return result;
}

internal WM_Modifiers
wm_get_modifiers(void)
{
  WM_Modifiers result = mac_wm_modifiers_from_flags([NSEvent modifierFlags]);
  return result;
}

internal B32
wm_key_is_down(WM_Key key)
{
  B32 result = 0;
  switch(key)
  {
    case WM_Key_LeftMouseButton:   {result = mac_wm_state->mouse_button_is_down[0];}break;
    case WM_Key_RightMouseButton:  {result = mac_wm_state->mouse_button_is_down[1];}break;
    case WM_Key_MiddleMouseButton: {result = mac_wm_state->mouse_button_is_down[2];}break;
    default:
    {
      for EachElement(keycode, mac_wm_key_from_keycode_table)
      {
        if(mac_wm_key_from_keycode_table[keycode] == key && mac_wm_state->keycode_is_down[keycode])
        {
          result = 1;
          break;
        }
      }
    }break;
  }
  return result;
}

internal Vec2F32
wm_mouse_from_window(WM_Window handle)
{
  MAC_WM_Window *window = mac_wm_window_from_handle(handle);
  Vec2F32 result = {0};
  if(window != 0)
  {
    result = mac_wm_pos_from_window_point(window, [window->window mouseLocationOutsideOfEventStream]);
  }
  return result;
}

////////////////////////////////
//~ @per_os_impl Cursors (Implemented Per-OS)

internal void
wm_set_cursor(WM_Cursor cursor)
{
  mac_wm_state->cursor = cursor;
  if(mac_wm_state->mouse_is_over_client)
  {
    [mac_wm_state->cursors[cursor] set];
  }
}