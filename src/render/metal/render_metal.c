////////////////////////////////
//~ Generated Code

#include "generated/render_metal.meta.c"

////////////////////////////////
//~ Helpers

internal R_Metal_Window *
r_metal_window_from_handle(R_Handle handle)
{
  R_Metal_Window *window = (R_Metal_Window *)handle.u64[0];
  if(window == 0)
  {
    window = &r_metal_window_nil;
  }
  return window;
}

internal R_Handle
r_metal_handle_from_window(R_Metal_Window *window)
{
  R_Handle handle = {(U64)window};
  return handle;
}

internal R_Metal_Tex2D *
r_metal_tex2d_from_handle(R_Handle handle)
{
  R_Metal_Tex2D *texture = (R_Metal_Tex2D *)handle.u64[0];
  if(texture == 0)
  {
    texture = &r_metal_tex2d_nil;
  }
  return texture;
}

internal R_Handle
r_metal_handle_from_tex2d(R_Metal_Tex2D *texture)
{
  R_Handle handle = {(U64)texture};
  return handle;
}

internal R_Metal_Buffer *
r_metal_buffer_from_handle(R_Handle handle)
{
  R_Metal_Buffer *buffer = (R_Metal_Buffer *)handle.u64[0];
  if(buffer == 0)
  {
    buffer = &r_metal_buffer_nil;
  }
  return buffer;
}

internal R_Handle
r_metal_handle_from_buffer(R_Metal_Buffer *buffer)
{
  R_Handle handle = {(U64)buffer};
  return handle;
}

internal MTLPixelFormat
r_metal_pixel_format_from_tex2dformat(R_Tex2DFormat format)
{
  MTLPixelFormat result = MTLPixelFormatRGBA8Unorm;
  switch(format)
  {
    default:{}break;
    case R_Tex2DFormat_R8:    {result = MTLPixelFormatR8Unorm;}break;
    case R_Tex2DFormat_RG8:   {result = MTLPixelFormatRG8Unorm;}break;
    case R_Tex2DFormat_RGBA8: {result = MTLPixelFormatRGBA8Unorm;}break;
    case R_Tex2DFormat_BGRA8: {result = MTLPixelFormatBGRA8Unorm;}break;
    case R_Tex2DFormat_R16:   {result = MTLPixelFormatR16Unorm;}break;
    case R_Tex2DFormat_RGBA16:{result = MTLPixelFormatRGBA16Unorm;}break;
    case R_Tex2DFormat_R32:   {result = MTLPixelFormatR32Float;}break;
    case R_Tex2DFormat_RG32:  {result = MTLPixelFormatRG32Float;}break;
    case R_Tex2DFormat_RGBA32:{result = MTLPixelFormatRGBA32Float;}break;
  }
  return result;
}

internal id<MTLTexture>
r_metal_render_target_alloc(MTLPixelFormat format, Vec2S32 size)
{
  MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:size.x height:size.y mipmapped:NO];
  [desc setUsage:MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead];
  [desc setStorageMode:MTLStorageModePrivate];
  id<MTLTexture> result = [r_metal_state->device newTextureWithDescriptor:desc];
  return result;
}

internal B32
r_metal_scissor_from_clip(Rng2F32 clip, Vec2S32 resolution, MTLScissorRect *scissor_out)
{
  Rng2F32 rect = r2f32p(0, 0, resolution.x, resolution.y);
  if(clip.x0 != 0 || clip.y0 != 0 || clip.x1 != 0 || clip.y1 != 0)
  {
    rect = intersect_2f32(rect, clip);
  }
  Vec2S32 dim = v2s32((S32)rect.x1 - (S32)rect.x0, (S32)rect.y1 - (S32)rect.y0);
  B32 result = (dim.x > 0 && dim.y > 0);
  if(result)
  {
    *scissor_out = (MTLScissorRect){(S32)rect.x0, (S32)rect.y0, dim.x, dim.y};
  }
  return result;
}

internal id<MTLRenderCommandEncoder>
r_metal_encoder_begin(id<MTLCommandBuffer> cmd_buffer, id<MTLTexture> color, id<MTLTexture> depth, MTLLoadAction load_action)
{
  MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
  MTLRenderPassColorAttachmentDescriptor *color_attachment = [[desc colorAttachments] objectAtIndexedSubscript:0];
  [color_attachment setTexture:color];
  [color_attachment setLoadAction:load_action];
  [color_attachment setClearColor:MTLClearColorMake(0, 0, 0, 0)];
  [[desc depthAttachment] setTexture:depth];
  id<MTLRenderCommandEncoder> result = [cmd_buffer renderCommandEncoderWithDescriptor:desc];
  [result setCullMode:MTLCullModeBack];
  return result;
}

internal void
r_metal_stage_finalize(id<MTLCommandBuffer> cmd_buffer, id<MTLTexture> stage, id<MTLTexture> dst)
{
  id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(cmd_buffer, dst, 0, MTLLoadActionClear);
  [encoder setRenderPipelineState:r_metal_state->pipelines[R_Metal_ShaderKind_Finalize]];
  [encoder setFragmentTexture:stage atIndex:0];
  [encoder setFragmentSamplerState:r_metal_state->samplers[R_Tex2DSampleKind_Nearest] atIndex:0];
  [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
  [encoder endEncoding];
}

////////////////////////////////
//~ Backend Hooks

//- top-level layer initialization

r_hook void
r_init(CmdLine *cmdln)
{
  @autoreleasepool
  {
    Arena *arena = arena_alloc();
    r_metal_state = push_array(arena, R_Metal_State, 1);
    r_metal_state->arena = arena;
    r_metal_state->device_mutex = mutex_alloc();
    
    //- device and command queue
    r_metal_state->device = MTLCreateSystemDefaultDevice();
    if(r_metal_state->device == 0)
    {
      sh_message(1, str8_lit("Fatal Error"), str8_lit("Could not create a Metal device. The process is terminating."));
      abort_self(1);
    }
    r_metal_state->command_queue = [r_metal_state->device newCommandQueue];
    
    //- shader library. every shader is in one source string, compiled here.
    id<MTLLibrary> library = 0;
    {
      NSError *error = 0;
      library = [r_metal_state->device newLibraryWithSource:mac_nsstring_from_str8(r_metal_shader_src) options:0 error:&error];
      if(library == 0)
      {
        sh_message(1, str8_lit("Metal Shader Compilation Failure"), str8_cstring((char *)[[error localizedDescription] UTF8String]));
        abort_self(1);
      }
    }
    
    //- one pipeline per row of the shader table
    for EachEnumVal(R_Metal_ShaderKind, kind)
    {
      NSError *error = 0;
      id<MTLFunction> vfunc = [library newFunctionWithName:mac_nsstring_from_str8(r_metal_shader_kind_vfunc_name_table[kind])];
      id<MTLFunction> ffunc = [library newFunctionWithName:mac_nsstring_from_str8(r_metal_shader_kind_ffunc_name_table[kind])];
      MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
      [desc setVertexFunction:vfunc];
      [desc setFragmentFunction:ffunc];
      [desc setDepthAttachmentPixelFormat:r_metal_shader_kind_depth_format_table[kind]];
      MTLRenderPipelineColorAttachmentDescriptor *color_attachment = [[desc colorAttachments] objectAtIndexedSubscript:0];
      [color_attachment setPixelFormat:r_metal_shader_kind_color_format_table[kind]];
      [color_attachment setBlendingEnabled:YES];
      [color_attachment setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
      [color_attachment setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
      r_metal_state->pipelines[kind] = [r_metal_state->device newRenderPipelineStateWithDescriptor:desc error:&error];
      [desc release];
      [vfunc release];
      [ffunc release];
      if(r_metal_state->pipelines[kind] == 0)
      {
        sh_message(1, str8_lit("Metal Pipeline Creation Failure"), str8_cstring((char *)[[error localizedDescription] UTF8String]));
        abort_self(1);
      }
    }
    [library release];
    
    //- samplers
    for EachEnumVal(R_Tex2DSampleKind, kind)
    {
      MTLSamplerMinMagFilter filter = (kind == R_Tex2DSampleKind_Linear) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
      MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
      [desc setMinFilter:filter];
      [desc setMagFilter:filter];
      [desc setSAddressMode:MTLSamplerAddressModeRepeat];
      [desc setTAddressMode:MTLSamplerAddressModeRepeat];
      r_metal_state->samplers[kind] = [r_metal_state->device newSamplerStateWithDescriptor:desc];
      [desc release];
    }
    
    //- a blur samples past the edges of the target, and must find the edge
    // there, not the opposite side of the window. clamping to the edge is
    // what a sampler does unless told to repeat.
    {
      MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
      [desc setMinFilter:MTLSamplerMinMagFilterLinear];
      [desc setMagFilter:MTLSamplerMinMagFilterLinear];
      r_metal_state->blur_sampler = [r_metal_state->device newSamplerStateWithDescriptor:desc];
      [desc release];
    }
    
    //- depth state for meshes
    {
      MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
      [desc setDepthCompareFunction:MTLCompareFunctionLess];
      [desc setDepthWriteEnabled:YES];
      r_metal_state->plain_depth_stencil = [r_metal_state->device newDepthStencilStateWithDescriptor:desc];
      [desc release];
    }
    
    //- backup texture, for a rect group that names none
    {
      U32 backup_texture_data[] =
      {
        0xff00ffff, 0x330033ff,
        0x330033ff, 0xff00ffff,
      };
      r_metal_state->backup_texture = r_tex2d_alloc(R_ResourceKind_Static, v2s32(2, 2), R_Tex2DFormat_RGBA8, backup_texture_data);
    }
  }
}

//- window setup/teardown

// the window manager owns the window and its view; the surface on that view
// is the renderer's, the way a d3d11 swapchain is made for somebody's HWND
r_hook R_Handle
r_window_equip(WM_Window window)
{
  R_Handle result = {0};
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Window *w = r_metal_state->first_free_window;
    if(w != 0)
    {
      SLLStackPop(r_metal_state->first_free_window);
    }
    else
    {
      w = push_array_no_zero(r_metal_state->arena, R_Metal_Window, 1);
    }
    MemoryZeroStruct(w);
    
    MAC_WM_Window *mac_window = mac_wm_window_from_handle(window);
    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    w->layer = [[CAMetalLayer alloc] init];
    [w->layer setDevice:r_metal_state->device];
    [w->layer setPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB];
    [w->layer setColorspace:color_space];
    [w->layer setOpaque:YES];
    [mac_window->view setLayer:w->layer];
    [mac_window->view setWantsLayer:YES];
    CGColorSpaceRelease(color_space);
    
    result = r_metal_handle_from_window(w);
  }
  return result;
}

r_hook void
r_window_unequip(WM_Window window, R_Handle window_equip)
{
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Window *w = r_metal_window_from_handle(window_equip);
    if(w != &r_metal_window_nil)
    {
      [w->stage_color release];
      [w->stage_scratch_color release];
      [w->geo3d_color release];
      [w->geo3d_depth release];
      [w->cmd_buffer release];
      [w->layer release];
      SLLStackPush(r_metal_state->first_free_window, w);
    }
  }
}

//- textures

r_hook R_Handle
r_tex2d_alloc(R_ResourceKind kind, Vec2S32 size, R_Tex2DFormat format, void *data)
{
  R_Metal_Tex2D *texture = 0;
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    //- allocate
    texture = r_metal_state->first_free_tex2d;
    if(texture != 0)
    {
      SLLStackPop(r_metal_state->first_free_tex2d);
    }
    else
    {
      texture = push_array_no_zero(r_metal_state->arena, R_Metal_Tex2D, 1);
    }
    MemoryZeroStruct(texture);
    
    //- create the texture, and fill it if there is data
    if(size.x > 0 && size.y > 0)
    {
      MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:r_metal_pixel_format_from_tex2dformat(format) width:size.x height:size.y mipmapped:NO];
      [desc setStorageMode:MTLStorageModeShared];
      texture->texture = [r_metal_state->device newTextureWithDescriptor:desc];
    }
    if(data != 0)
    {
      [texture->texture replaceRegion:MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel:0 withBytes:data bytesPerRow:r_tex2d_format_bytes_per_pixel_table[format]*size.x];
    }
    
    //- fill basics
    texture->kind = kind;
    texture->size = size;
    texture->format = format;
  }
  R_Handle result = r_metal_handle_from_tex2d(texture);
  return result;
}

// a released texture may still be named by draw commands built earlier this
// frame, so it is only queued here, and destroyed when the frame ends
r_hook void
r_tex2d_release(R_Handle handle)
{
  MutexScope(r_metal_state->device_mutex)
  {
    R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(handle);
    if(texture != &r_metal_tex2d_nil)
    {
      SLLStackPush(r_metal_state->first_to_free_tex2d, texture);
    }
  }
}

r_hook R_ResourceKind
r_kind_from_tex2d(R_Handle handle)
{
  R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(handle);
  return texture->kind;
}

r_hook Vec2S32
r_size_from_tex2d(R_Handle handle)
{
  R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(handle);
  return texture->size;
}

r_hook R_Tex2DFormat
r_format_from_tex2d(R_Handle handle)
{
  R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(handle);
  return texture->format;
}

r_hook void
r_fill_tex2d_region(R_Handle handle, Rng2S32 subrect, void *data)
{
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(handle);
    if(texture != &r_metal_tex2d_nil && texture->kind == R_ResourceKind_Dynamic)
    {
      Vec2S32 dim = dim_2s32(subrect);
      [texture->texture replaceRegion:MTLRegionMake2D(subrect.x0, subrect.y0, dim.x, dim.y) mipmapLevel:0 withBytes:data bytesPerRow:r_tex2d_format_bytes_per_pixel_table[texture->format]*dim.x];
    }
  }
}

//- buffers

r_hook R_Handle
r_buffer_alloc(R_ResourceKind kind, U64 size, void *data)
{
  R_Metal_Buffer *buffer = 0;
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    //- allocate
    buffer = r_metal_state->first_free_buffer;
    if(buffer != 0)
    {
      SLLStackPop(r_metal_state->first_free_buffer);
    }
    else
    {
      buffer = push_array_no_zero(r_metal_state->arena, R_Metal_Buffer, 1);
    }
    MemoryZeroStruct(buffer);
    
    //- create the buffer. there is no hook to fill one later, so one made
    // with no data is no buffer.
    if(size != 0 && data != 0)
    {
      buffer->buffer = [r_metal_state->device newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
    }
  }
  R_Handle result = r_metal_handle_from_buffer(buffer);
  return result;
}

r_hook void
r_buffer_release(R_Handle handle)
{
  MutexScope(r_metal_state->device_mutex)
  {
    R_Metal_Buffer *buffer = r_metal_buffer_from_handle(handle);
    if(buffer != &r_metal_buffer_nil)
    {
      SLLStackPush(r_metal_state->first_to_free_buffer, buffer);
    }
  }
}

//- frame markers

r_hook void
r_begin_frame(void)
{
  r_metal_state->frame_presented = 0;
}

r_hook void
r_end_frame(void)
{
  MutexScope(r_metal_state->device_mutex)
  {
    for(R_Metal_Tex2D *texture = r_metal_state->first_to_free_tex2d, *next = 0; texture != 0; texture = next)
    {
      next = texture->next;
      [texture->texture release];
      texture->texture = 0;
      SLLStackPush(r_metal_state->first_free_tex2d, texture);
    }
    for(R_Metal_Buffer *buffer = r_metal_state->first_to_free_buffer, *next = 0; buffer != 0; buffer = next)
    {
      next = buffer->next;
      [buffer->buffer release];
      buffer->buffer = 0;
      SLLStackPush(r_metal_state->first_free_buffer, buffer);
    }
    r_metal_state->first_to_free_tex2d = 0;
    r_metal_state->first_to_free_buffer = 0;
  }
  
  // presenting is what paces the application to the display. a frame that
  // presented to no window at all still takes one refresh interval.
  if(!r_metal_state->frame_presented)
  {
    sleep_ms((U32)(1000.f/wm_get_system_info()->default_refresh_rate));
  }
}

r_hook void
r_window_begin_frame(WM_Window window, R_Handle window_equip)
{
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Window *w = r_metal_window_from_handle(window_equip);
    if(w != &r_metal_window_nil)
    {
      //- get resolution
      MAC_WM_Window *mac_window = mac_wm_window_from_handle(window);
      Vec2F32 client_dim = dim_2f32(wm_client_rect_from_window(window));
      Vec2S32 resolution = v2s32(Max(1, (S32)client_dim.x), Max(1, (S32)client_dim.y));
      
      //- resolution change: the layer's drawables and every window-sized
      // target are remade at the new size
      if(w->last_resolution.x != resolution.x || w->last_resolution.y != resolution.y)
      {
        w->last_resolution = resolution;
        [w->layer setContentsScale:[mac_window->window backingScaleFactor]];
        [w->layer setDrawableSize:CGSizeMake(resolution.x, resolution.y)];
        [w->stage_color release];
        [w->stage_scratch_color release];
        [w->geo3d_color release];
        [w->geo3d_depth release];
        w->stage_color         = r_metal_render_target_alloc(MTLPixelFormatRGBA16Float, resolution);
        w->stage_scratch_color = r_metal_render_target_alloc(MTLPixelFormatRGBA16Float, resolution);
        w->geo3d_color         = r_metal_render_target_alloc(MTLPixelFormatRGBA8Unorm, resolution);
        w->geo3d_depth         = r_metal_render_target_alloc(MTLPixelFormatDepth32Float, resolution);
      }
      
      //- start this frame's command buffer, and clear the stage
      w->cmd_buffer = [[r_metal_state->command_queue commandBuffer] retain];
      id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(w->cmd_buffer, w->stage_color, 0, MTLLoadActionClear);
      [encoder endEncoding];
    }
  }
}

r_hook void
r_window_end_frame(WM_Window window, R_Handle window_equip)
{
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Window *w = r_metal_window_from_handle(window_equip);
    if(w->cmd_buffer != 0)
    {
      //- a window nobody can see takes no drawable: the layer has only a few,
      // gets none back while it is hidden, and makes the next request wait a
      // second before giving up. the exception is a window's very first
      // picture, which has to be there before the window is shown.
      MAC_WM_Window *mac_window = mac_wm_window_from_handle(window);
      B32 is_visible = !!([mac_window->window occlusionState] & NSWindowOcclusionStateVisible);
      B32 is_resizing = [mac_window->window inLiveResize];
      [w->layer setPresentsWithTransaction:is_resizing];
      id<CAMetalDrawable> drawable = (is_visible || !w->has_presented) ? [w->layer nextDrawable] : 0;
      
      //- finalize, by writing the stage out to the drawable
      if(drawable != 0)
      {
        r_metal_stage_finalize(w->cmd_buffer, w->stage_color, [drawable texture]);
        w->has_presented = 1;
        r_metal_state->frame_presented = 1;
      }
      
      //- commit, and present. normally the command buffer presents when the
      // gpu is done. during a live resize the picture has to land in the same
      // transaction as the window's new size, or it is stretched for a frame,
      // so there the present is made here, once the work is scheduled.
      if(drawable != 0 && !is_resizing)
      {
        [w->cmd_buffer presentDrawable:drawable];
      }
      [w->cmd_buffer commit];
      if(drawable != 0 && is_resizing)
      {
        [w->cmd_buffer waitUntilScheduled];
        [drawable present];
      }
      [w->cmd_buffer release];
      w->cmd_buffer = 0;
    }
  }
}

//- render pass submission

r_hook void
r_window_submit(WM_Window window, R_Handle window_equip, R_PassList *passes)
{
  MutexScope(r_metal_state->device_mutex) @autoreleasepool
  {
    R_Metal_Window *w = r_metal_window_from_handle(window_equip);
    Vec2S32 resolution = w->last_resolution;
    
    //- every rect this submission draws goes into one buffer, sized here.
    // the command buffer holds on to it until the gpu has read it.
    U64 instance_buffer_size = 0;
    for EachNode(pass_n, R_PassNode, passes->first)
    {
      if(pass_n->v.kind == R_PassKind_UI)
      {
        for EachNode(group_n, R_BatchGroup2DNode, pass_n->v.params_ui->rects.first)
        {
          instance_buffer_size += group_n->batches.byte_count;
        }
      }
    }
    id<MTLBuffer> instance_buffer = 0;
    U64 instance_buffer_pos = 0;
    if(instance_buffer_size != 0)
    {
      instance_buffer = [r_metal_state->device newBufferWithLength:instance_buffer_size options:MTLResourceStorageModeShared];
    }
    
    //- do passes
    for EachNode(pass_n, R_PassNode, passes->first)
    {
      R_Pass *pass = &pass_n->v;
      switch(pass->kind)
      {
        default:{}break;
        
        ////////////////////////
        //- ui rendering pass
        //
        case R_PassKind_UI:
        {
          R_BatchGroup2DList *rect_batch_groups = &pass->params_ui->rects;
          id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(w->cmd_buffer, w->stage_color, 0, MTLLoadActionLoad);
          [encoder setRenderPipelineState:r_metal_state->pipelines[R_Metal_ShaderKind_Rect]];
          for EachNode(group_n, R_BatchGroup2DNode, rect_batch_groups->first)
          {
            R_BatchList *batches = &group_n->batches;
            R_BatchGroup2DParams *group_params = &group_n->params;
            
            // fill this group's part of the instance buffer
            U64 instance_buffer_off = instance_buffer_pos;
            for EachNode(batch_n, R_BatchNode, batches->first)
            {
              MemoryCopy((U8 *)[instance_buffer contents] + instance_buffer_pos, batch_n->v.v, batch_n->v.byte_count);
              instance_buffer_pos += batch_n->v.byte_count;
            }
            
            // get texture
            R_Handle texture_handle = group_params->tex;
            if(r_handle_match(texture_handle, r_handle_zero()))
            {
              texture_handle = r_metal_state->backup_texture;
            }
            R_Metal_Tex2D *texture = r_metal_tex2d_from_handle(texture_handle);
            
            // fill uniforms. metal's matrices are column-major, as the
            // base layer's are, so the transform goes over column by column.
            R_Metal_Uniforms_Rect uniforms = {0};
            {
              uniforms.xform[0] = v4f32(group_params->xform.v[0][0], group_params->xform.v[0][1], group_params->xform.v[0][2], 0);
              uniforms.xform[1] = v4f32(group_params->xform.v[1][0], group_params->xform.v[1][1], group_params->xform.v[1][2], 0);
              uniforms.xform[2] = v4f32(group_params->xform.v[2][0], group_params->xform.v[2][1], group_params->xform.v[2][2], 0);
              uniforms.viewport_size              = v2f32(resolution.x, resolution.y);
              uniforms.xform_scale.x              = length_2f32(v2f32(uniforms.xform[0].x, uniforms.xform[0].y));
              uniforms.xform_scale.y              = length_2f32(v2f32(uniforms.xform[1].x, uniforms.xform[1].y));
              uniforms.opacity                    = 1-group_params->transparency;
              uniforms.texture_t2d_size           = v2f32(texture->size.x, texture->size.y);
              uniforms.texture_sample_channel_map = r_sample_channel_map_from_tex2dformat(texture->format);
            }
            
            // draw
            MTLScissorRect scissor = {0};
            U64 instance_count = batches->byte_count / batches->bytes_per_inst;
            if(instance_count != 0 && texture->texture != 0 && r_metal_scissor_from_clip(group_params->clip, resolution, &scissor))
            {
              [encoder setVertexBuffer:instance_buffer offset:instance_buffer_off atIndex:0];
              [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
              [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
              [encoder setFragmentTexture:texture->texture atIndex:0];
              [encoder setFragmentSamplerState:r_metal_state->samplers[group_params->tex_sample_kind] atIndex:0];
              [encoder setScissorRect:scissor];
              [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:instance_count];
            }
          }
          [encoder endEncoding];
        }break;
        
        ////////////////////////
        //- blur rendering pass
        //
        case R_PassKind_Blur:
        {
          R_PassParams_Blur *params = pass->params_blur;
          
          // set up uniforms. the kernel has 32 entries, each one a pair
          // of taps fetched as a single bilinear sample, so a blur covers at
          // most 62 pixels to a side.
          R_Metal_Uniforms_Blur uniforms = {0};
          F32 blur_radius_px = 0;
          {
            F32 weights[ArrayCount(uniforms.blur_kernel)*2] = {0};
            F32 blur_size = Min(params->blur_size, ArrayCount(weights) - 2);
            U64 blur_count = (U64)round_f32(blur_size);
            F32 stdev = (blur_size-1.f)/2.f;
            F32 one_over_root_2pi_stdev2 = 1/sqrt_f32(2*pi32*stdev*stdev);
            F32 euler32 = 2.718281828459045f;
            weights[0] = 1.f;
            if(stdev > 0.f)
            {
              for(U64 idx = 0; idx < blur_count; idx += 1)
              {
                F32 kernel_x = (F32)idx;
                weights[idx] = one_over_root_2pi_stdev2*pow_f32(euler32, -kernel_x*kernel_x/(2.f*stdev*stdev));
              }
            }
            if(weights[0] > 1.f)
            {
              MemoryZeroArray(weights);
              weights[0] = 1.f;
            }
            else
            {
              for(U64 idx = 1; idx < blur_count; idx += 2)
              {
                F32 w0 = weights[idx + 0];
                F32 w1 = weights[idx + 1];
                F32 weight = w0 + w1;
                F32 t = w1 / weight;
                uniforms.blur_kernel[(idx+1)/2] = v4f32(weight, (F32)idx + t, 0, 0);
              }
            }
            uniforms.blur_kernel[0].x = weights[0];
            uniforms.viewport_size    = v2f32(resolution.x, resolution.y);
            uniforms.blur_count       = 1 + blur_count / 2;
            blur_radius_px            = (F32)blur_count;
          }
          
          // horizontal pass, stage -> scratch; vertical pass, scratch -> stage.
          // the vertical pass reads rows above and below the rect, which only
          // hold this frame's picture if the horizontal pass wrote them. so
          // the horizontal pass covers a taller, square-cornered, unclipped
          // rect, and the vertical pass draws the rect that was asked for.
          MTLScissorRect scissor = {0};
          if(r_metal_scissor_from_clip(params->clip, resolution, &scissor))
          {
            for EachEnumVal(Axis2, axis)
            {
              id<MTLTexture> src = (axis == Axis2_X) ? w->stage_color : w->stage_scratch_color;
              id<MTLTexture> dst = (axis == Axis2_X) ? w->stage_scratch_color : w->stage_color;
              id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(w->cmd_buffer, dst, 0, MTLLoadActionLoad);
              if(axis == Axis2_X)
              {
                uniforms.direction = v2f32(1.f/resolution.x, 0);
                uniforms.rect      = r2f32p(params->rect.x0, params->rect.y0 - blur_radius_px, params->rect.x1, params->rect.y1 + blur_radius_px);
                MemoryZeroStruct(&uniforms.corner_radii);
              }
              else
              {
                uniforms.direction = v2f32(0, 1.f/resolution.y);
                uniforms.rect      = params->rect;
                MemoryCopyArray(uniforms.corner_radii.v, params->corner_radii);
                [encoder setScissorRect:scissor];
              }
              [encoder setRenderPipelineState:r_metal_state->pipelines[R_Metal_ShaderKind_Blur]];
              [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
              [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
              [encoder setFragmentTexture:src atIndex:0];
              [encoder setFragmentSamplerState:r_metal_state->blur_sampler atIndex:0];
              [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
              [encoder endEncoding];
            }
          }
        }break;
        
        ////////////////////////
        //- 3d geometry rendering pass
        //
        case R_PassKind_Geo3D:
        {
          R_PassParams_Geo3D *params = pass->params_geo3d;
          R_BatchGroup3DMap *mesh_group_map = &params->mesh_batches;
          
          //- draw mesh batches, into the geo3d targets
          {
            Vec2F32 viewport_dim = dim_2f32(params->viewport);
            R_Metal_Uniforms_Mesh uniforms = {0};
            uniforms.xform = mul_4x4f32(params->projection, params->view);
            id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(w->cmd_buffer, w->geo3d_color, w->geo3d_depth, MTLLoadActionClear);
            [encoder setViewport:(MTLViewport){params->viewport.x0, params->viewport.y0, viewport_dim.x, viewport_dim.y, 0, 1}];
            [encoder setDepthClipMode:MTLDepthClipModeClamp];
            [encoder setRenderPipelineState:r_metal_state->pipelines[R_Metal_ShaderKind_Mesh]];
            [encoder setDepthStencilState:r_metal_state->plain_depth_stencil];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            for EachIndex(slot_idx, mesh_group_map->slots_count)
            {
              for EachNode(n, R_BatchGroup3DMapNode, mesh_group_map->slots[slot_idx])
              {
                R_Metal_Buffer *mesh_vertices = r_metal_buffer_from_handle(n->params.mesh_vertices);
                R_Metal_Buffer *mesh_indices = r_metal_buffer_from_handle(n->params.mesh_indices);
                if(mesh_vertices->buffer != 0 && mesh_indices->buffer != 0)
                {
                  [encoder setVertexBuffer:mesh_vertices->buffer offset:0 atIndex:0];
                  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:[mesh_indices->buffer length]/sizeof(U32) indexType:MTLIndexTypeUInt32 indexBuffer:mesh_indices->buffer indexBufferOffset:0];
                }
              }
            }
            [encoder endEncoding];
          }
          
          //- composite to main staging buffer
          MTLScissorRect scissor = {0};
          if(r_metal_scissor_from_clip(params->clip, resolution, &scissor))
          {
            id<MTLRenderCommandEncoder> encoder = r_metal_encoder_begin(w->cmd_buffer, w->stage_color, 0, MTLLoadActionLoad);
            [encoder setRenderPipelineState:r_metal_state->pipelines[R_Metal_ShaderKind_Geo3DComposite]];
            [encoder setFragmentTexture:w->geo3d_color atIndex:0];
            [encoder setFragmentSamplerState:r_metal_state->samplers[R_Tex2DSampleKind_Nearest] atIndex:0];
            [encoder setScissorRect:scissor];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
            [encoder endEncoding];
          }
        }break;
      }
    }
    
    [instance_buffer release];
  }
}