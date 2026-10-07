////////////////////////////////
//~ Helpers

internal CGFontRef
fp_coretext_font_from_handle(FP_Handle handle)
{
  CGFontRef font = (CGFontRef)handle.u64[0];
  return font;
}

internal FP_Handle
fp_coretext_handle_from_font(CGFontRef font)
{
  FP_Handle handle = {(U64)font};
  return handle;
}

////////////////////////////////
//~ Backend Implementations

fp_hook void
fp_init(void)
{
}

fp_hook FP_Handle
fp_font_open(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  String8 folders[] = 
  {
    str8_lit(""),
    str8f(scratch.arena, "%s/Library/Fonts/", getenv("HOME")),
    str8_lit("/Library/Fonts/"),
    str8_lit("/System/Library/Fonts/"),
  }; 
  
  CGFontRef font = 0; 
  for EachElement(idx, folders)
  {
    if(font == 0 && path.size != 0) 
    {
      String8 full_path = str8f(scratch.arena, "%S%S", folders[idx], path);
      CGDataProviderRef provider = CGDataProviderCreateWithFilename((char *)full_path.str);
      font = CGFontCreateWithDataProvider(provider);
      CGDataProviderRelease(provider);
    } 
  } 
  FP_Handle handle = fp_coretext_handle_from_font(font); 
  scratch_end(scratch); 
  return handle; 
}

fp_hook FP_Handle
fp_font_open_from_static_data_string(String8 *data_ptr)
{
  CGDataProviderRef provider = CGDataProviderCreateWithData(0, data_ptr->str, data_ptr->size, 0);
  CGFontRef font = CGFontCreateWithDataProvider(provider);
  CGDataProviderRelease(provider);
  FP_Handle handle = fp_coretext_handle_from_font(font);
  return handle;
}

fp_hook void
fp_font_close(FP_Handle handle)
{
  CGFontRelease(fp_coretext_font_from_handle(handle));
}

fp_hook FP_Metrics
fp_metrics_from_font(FP_Handle handle)
{
  CGFontRef font = fp_coretext_font_from_handle(handle);
  FP_Metrics result = {0};
  if(font != 0)
  {
    result.design_units_per_em = (F32)CGFontGetUnitsPerEm(font);
    result.ascent              = (F32)CGFontGetAscent(font);
    result.descent             = -(F32)CGFontGetDescent(font);
    result.line_gap            = (F32)CGFontGetLeading(font);
    result.capital_height      = (F32)CGFontGetCapHeight(font);
  }
  return result;
}

fp_hook ASAN_NO_ADDR FP_RasterResult
fp_raster(Arena *arena, FP_Handle handle, F32 size, FP_RasterFlags flags, String8 string)
{
  ProfBeginFunction(); 
  CGFontRef font = fp_coretext_font_from_handle(handle); 
  FP_RasterResult result = {0};
  
  if(font != 0 && size > 0)
  {
    Temp scratch = scratch_begin(&arena, 1); 
    
    F32 size_px = (96.f/72.f) * size;
    FP_Metrics metrics = fp_metrics_from_font(handle);
    F32 px_per_design_unit = size_px / metrics.design_units_per_em;
    CTFontRef sized_font = CTFontCreateWithGraphicsFont(font, size_px, 0, 0); 
    
    String32 string32 = str32_from_8(scratch.arena, string); 
    U64 glyphs_count = string32.size; 
    CGGlyph *glyphs = push_array(scratch.arena, CGGlyph, glyphs_count); 
    for EachIndex(idx, glyphs_count)
    {
      U16 utf16[2] = {0};
      U32 utf16_count = utf16_encode(utf16, string32.str[idx]);
      CGGlyph glyph_per_utf16[2] = {0};
      CTFontGetGlyphsForCharacters(sized_font, utf16, glyph_per_utf16, utf16_count);
      glyphs[idx] = glyph_per_utf16[0];
    } 
    
    CGPoint *positions = push_array(scratch.arena, CGPoint, glyphs_count);
    F32 baseline = round_f32(px_per_design_unit*metrics.descent) + round_f32(px_per_design_unit*metrics.line_gap);
    F32 advance = 0;
    F32 ink_x1 = 0;
    for EachIndex(idx, glyphs_count)
    {
      CGRect ink = CTFontGetBoundingRectsForGlyphs(sized_font, kCTFontOrientationHorizontal, glyphs + idx, 0, 1);
      positions[idx] = CGPointMake(advance, baseline);
      ink_x1 = Max(ink_x1, advance + (F32)CGRectGetMaxX(ink));
      advance += round_f32((F32)CTFontGetAdvancesForGlyphs(sized_font, kCTFontOrientationHorizontal, glyphs + idx, 0, 1));
    }
    
    Vec2S16 dim = v2s16((S16)ceil_f32(Max(advance, ink_x1)) + 1,
                        (S16)round_f32(px_per_design_unit*(metrics.ascent + metrics.descent + metrics.line_gap)) + 1);
    
    U64 pixels_count = (U64)dim.x*(U64)dim.y;
    U8 *coverage = push_array(scratch.arena, U8, pixels_count);
    CGContextRef ctx = CGBitmapContextCreate(coverage, dim.x, dim.y, 8, dim.x, 0, (CGBitmapInfo)kCGImageAlphaOnly);
    CGContextSetShouldSmoothFonts(ctx, !!(flags & FP_RasterFlag_Smooth));
    CGContextSetShouldSubpixelPositionFonts(ctx, 0);
    CGContextSetShouldSubpixelQuantizeFonts(ctx, 0);
    CTFontDrawGlyphs(sized_font, glyphs, positions, glyphs_count, ctx);
    CGContextRelease(ctx);
    CFRelease(sized_font);
    
    U8 *atlas = push_array_no_zero(arena, U8, pixels_count*4);
    U64 coverage_sum = 0;
    for EachIndex(idx, pixels_count)
    {
      atlas[idx*4 + 0] = 255;
      atlas[idx*4 + 1] = 255;
      atlas[idx*4 + 2] = 255;
      atlas[idx*4 + 3] = coverage[idx];
      coverage_sum += coverage[idx];
    }
    result.atlas     = atlas;
    result.atlas_dim = (coverage_sum != 0) ? dim : v2s16(0, 0);
    result.advance   = advance;
    scratch_end(scratch);
  } 
  ProfEnd();
  return result;
}