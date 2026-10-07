#ifndef MAC_FRAMEWORKS_H
#define MAC_FRAMEWORKS_H

////////////////////////////////
//~ cand: Framework Headers
//
// Apple's headers and the base layer disagree about four names. `internal` and
// `global` are keywords here and struct fields there; `FileInfo` is a type on
// both sides; `nil` is a macro there and a variable name in the debug info layers.

#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global
#define FileInfo FileInfo__carbon
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#undef FileInfo
#undef nil
#pragma pop_macro("global")
#pragma pop_macro("internal")

////////////////////////////////
//~ String Conversions

internal NSString *
mac_nsstring_from_str8(String8 string)
{
  NSString *result = [[[NSString alloc] initWithBytes:string.str length:string.size encoding:NSUTF8StringEncoding] autorelease];
  if(result == 0)
  {
    result = @"";
  }
  return result;
}

internal String8
mac_str8_from_nsstring(Arena *arena, NSString *string)
{
  String8 result = {0};
  if(string != 0)
  {
    result = str8_copy(arena, str8_cstring((char *)[string UTF8String]));
  }
  return result;
}

#endif // MAC_FRAMEWORKS_H