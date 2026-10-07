#ifndef RENDER_METAL_H
#define RENDER_METAL_H

#include "mac/mac_framework.h"

////////////////////////////////
//~ Generated Code

#include "generated/render_metal.meta.h"

StaticAssert(sizeof(R_Rect2DInst) == 128, r_metal_rect2dinst_size_check);

typedef struct R_Metal_Uniforms_Rect R_Metal_Uniforms_Rect; 
struct R_Metal_Uniforms_Rect
{
  Vec4F32 xform[3]; 
  Vec2F32 viewport_size; 
  Vec2F32 xform_scale; 
  F32 opacity; 
  F32 _padding0_; 
  Vec2F32 texture_t2d_size; 
  Mat4x4F32 texture_sample_channel_map; 
}; 

StaticAssert(sizeof(R_Metal_Uniforms_Rect) == 144, r_metal_uniforms_rect_size_check);

typedef struct R_Metal_Uniforms_Blur R_Metal_Uniforms_Blur;
struct R_Metal_Uniforms_Blur
{
  Rng2F32 rect;
  Vec4F32 corner_radii;
  Vec2F32 direction;
  Vec2F32 viewport_size;
  U32 blur_count;
  U32 _padding0_[3];
  Vec4F32 blur_kernel[32];
};
StaticAssert(sizeof(R_Metal_Uniforms_Blur) == 576, r_metal_uniforms_blur_size_check);

typedef struct R_Metal_Uniforms_Mesh R_Metal_Uniforms_Mesh;
struct R_Metal_Uniforms_Mesh
{
  Mat4x4F32 xform;
};

////////////////////////////////
//~ Main State Types

typedef struct R_Metal_Tex2D R_Metal_Tex2D;
struct R_Metal_Tex2D
{
  R_Metal_Tex2D *next;
  id<MTLTexture> texture;
  R_ResourceKind kind;
  Vec2S32 size;
  R_Tex2DFormat format;
};

typedef struct R_Metal_Buffer R_Metal_Buffer;
struct R_Metal_Buffer
{
  R_Metal_Buffer *next;
  id<MTLBuffer> buffer;
};

typedef struct R_Metal_Window R_Metal_Window;
struct R_Metal_Window
{
  R_Metal_Window *next;
  CAMetalLayer *layer;
  id<MTLTexture> stage_color;
  id<MTLTexture> stage_scratch_color;
  id<MTLTexture> geo3d_color;
  id<MTLTexture> geo3d_depth;
  id<MTLCommandBuffer> cmd_buffer;
  Vec2S32 last_resolution;
  B32 has_presented;
};

typedef struct R_Metal_State R_Metal_State;
struct R_Metal_State
{
  Arena *arena;
  R_Metal_Window *first_free_window;
  R_Metal_Tex2D *first_free_tex2d;
  R_Metal_Buffer *first_free_buffer;
  R_Metal_Tex2D *first_to_free_tex2d;
  R_Metal_Buffer *first_to_free_buffer;
  Mutex device_mutex;
  id<MTLDevice> device;
  id<MTLCommandQueue> command_queue;
  id<MTLRenderPipelineState> pipelines[R_Metal_ShaderKind_COUNT];
  id<MTLSamplerState> samplers[R_Tex2DSampleKind_COUNT];
  id<MTLSamplerState> blur_sampler;
  id<MTLDepthStencilState> plain_depth_stencil;
  R_Handle backup_texture;
  B32 frame_presented;
};

////////////////////////////////
//~ Globals

global R_Metal_State *r_metal_state = 0;
global R_Metal_Window r_metal_window_nil = {&r_metal_window_nil};
global R_Metal_Tex2D r_metal_tex2d_nil = {&r_metal_tex2d_nil};
global R_Metal_Buffer r_metal_buffer_nil = {&r_metal_buffer_nil};

////////////////////////////////
//~ Helpers

internal R_Metal_Window *r_metal_window_from_handle(R_Handle handle);
internal R_Handle r_metal_handle_from_window(R_Metal_Window *window);
internal R_Metal_Tex2D *r_metal_tex2d_from_handle(R_Handle handle);
internal R_Handle r_metal_handle_from_tex2d(R_Metal_Tex2D *texture);
internal R_Metal_Buffer *r_metal_buffer_from_handle(R_Handle handle);
internal R_Handle r_metal_handle_from_buffer(R_Metal_Buffer *buffer);
internal MTLPixelFormat r_metal_pixel_format_from_tex2dformat(R_Tex2DFormat format);
internal id<MTLTexture> r_metal_render_target_alloc(MTLPixelFormat format, Vec2S32 size);
internal B32 r_metal_scissor_from_clip(Rng2F32 clip, Vec2S32 resolution, MTLScissorRect *scissor_out);
internal id<MTLRenderCommandEncoder> r_metal_encoder_begin(id<MTLCommandBuffer> cmd_buffer, id<MTLTexture> color, id<MTLTexture> depth, MTLLoadAction load_action);
internal void r_metal_stage_finalize(id<MTLCommandBuffer> cmd_buffer, id<MTLTexture> stage, id<MTLTexture> dst);



#endif //RENDER_METAL_H
