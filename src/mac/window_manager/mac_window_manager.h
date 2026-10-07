#ifndef MAC_WINDOW_MANAGER_H
#define MAC_WINDOW_MANAGER_H

#include "mac/mac_framework.h"

typedef struct MAC_WM_TitleBarClientArea MAC_WM_TitleBarClientArea;
struct MAC_WM_TitleBarClientArea
{
  MAC_WM_TitleBarClientArea *next; 
  Rng2F32 rect; 
}; 

typedef struct MAC_WM_Window MAC_WM_Window; 
struct MAC_WM_Window
{
  MAC_WM_Window *next; 
  MAC_WM_Window *prev; 
  NSWindow *window; 
  NSView   *view; 
  id        delegate; 
  B32 first_paint_done; 
  B32 fullscreen_at_first_paint; 
  B32 has_marked_text; 
  B32 custom_border; 
  B32 custom_border_title_thickness; 
  B32 custom_border_edge_thickness; 
  Arena *paint_arena; 
  MAC_WM_TitleBarClientArea *first_title_bar_client_area; 
  MAC_WM_TitleBarClientArea *last_title_bar_client_area; 
}; 

typedef enum MAC_WM_HitKind
{
  MAC_WM_HitKind_Outside,
  MAC_WM_HitKind_Client,
  MAC_WM_HitKind_TitleBar,
  MAC_WM_HitKind_WindowButtons,
  MAC_WM_HitKind_Edge,
}
MAC_WM_HitKind;

typedef struct MAC_WM_State MAC_WM_State; 
struct MAC_WM_State
{
  Arena *arena; 
  WM_SystemInfo gfx_info; 
  MAC_WM_Window *first_window; 
  MAC_WM_Window *last_window; 
  MAC_WM_Window *free_window; 
  NSAutoreleasePool *pump_pool; 
  
  Arena *event_arena; 
  WM_EventList events; 
  B32 is_pumping; 
  B32 is_waiting; 
  B32 is_repainting;
  
  B8 keycode_is_down[128]; 
  B8 mouse_button_is_down[3];
  Vec2F32 scroll_remainder; 
  
  NSCursor *cursors[WM_Cursor_COUNT]; 
  WM_Cursor cursor; 
  B32 mouse_is_over_client; 
  B32 mouse_is_over_window; 
};

////////////////////////////////
//~ Globals

global MAC_WM_State *mac_wm_state = 0;

////////////////////////////////
//~ Helpers

internal WM_Window mac_wm_handle_from_window(MAC_WM_Window *window);
internal MAC_WM_Window *mac_wm_window_from_handle(WM_Window handle);
internal MAC_WM_Window *mac_wm_window_from_nswindow(NSWindow *nswindow);
internal WM_Event *mac_wm_push_event(WM_EventKind kind, MAC_WM_Window *window);
internal Rng2F32 mac_wm_rect_from_nsrect(NSRect rect);
internal NSRect mac_wm_nsrect_from_rect(Rng2F32 rect);
internal Vec2F32 mac_wm_pos_from_window_point(MAC_WM_Window *window, NSPoint point);
internal WM_Modifiers mac_wm_modifiers_from_flags(NSEventModifierFlags flags);
internal MAC_WM_HitKind mac_wm_hit_kind_from_pos(MAC_WM_Window *window, Vec2F32 pos);
internal void mac_wm_window_buttons_place(MAC_WM_Window *window);
internal void mac_wm_event_translate(NSEvent *event);

#endif //MAC_WINDOW_MANAGER_H