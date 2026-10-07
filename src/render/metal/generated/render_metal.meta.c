// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

//- GENERATED CODE

C_LINKAGE_BEGIN
String8 r_metal_shader_kind_vfunc_name_table[5] =
{
str8_lit_comp("r_metal_rect_vertex"),
str8_lit_comp("r_metal_blur_vertex"),
str8_lit_comp("r_metal_mesh_vertex"),
str8_lit_comp("r_metal_geo3dcomposite_vertex"),
str8_lit_comp("r_metal_finalize_vertex"),
};

String8 r_metal_shader_kind_ffunc_name_table[5] =
{
str8_lit_comp("r_metal_rect_fragment"),
str8_lit_comp("r_metal_blur_fragment"),
str8_lit_comp("r_metal_mesh_fragment"),
str8_lit_comp("r_metal_geo3dcomposite_fragment"),
str8_lit_comp("r_metal_finalize_fragment"),
};

MTLPixelFormat r_metal_shader_kind_color_format_table[5] =
{
MTLPixelFormatRGBA16Float,
MTLPixelFormatRGBA16Float,
MTLPixelFormatRGBA8Unorm,
MTLPixelFormatRGBA16Float,
MTLPixelFormatBGRA8Unorm_sRGB,
};

MTLPixelFormat r_metal_shader_kind_depth_format_table[5] =
{
MTLPixelFormatInvalid,
MTLPixelFormatInvalid,
MTLPixelFormatDepth32Float,
MTLPixelFormatInvalid,
MTLPixelFormatInvalid,
};

C_LINKAGE_END

