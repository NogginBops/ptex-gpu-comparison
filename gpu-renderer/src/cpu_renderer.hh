
#pragma once

#include "maths.hh"
#include "util.hh"

#include <stdint.h>
#include <Ptexture.h>

extern Ptex::PtexTexture* g_ptex_texture;
extern Ptex::PtexFilter* g_ptex_filter;
extern Ptex::PtexFilter::FilterType g_current_filter_type;

extern bool use_cross_derivatives;

rgb8_t* vec3_buffer_to_rgb8(vec3_t* buffer, int width, int height);

vec3_t* calculate_image_cpu(int width, int height, uint16_t* faceID_buffer, vec3_t* uv_buffer, vec4_t* uv_deriv_buffer, vec3_t background_color);
