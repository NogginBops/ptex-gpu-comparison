
#include "cpu_renderer.hh"

#include <stdlib.h>
#include <assert.h>

Ptex::PtexTexture* g_ptex_texture;
Ptex::PtexFilter* g_ptex_filter;
Ptex::PtexFilter::FilterType g_current_filter_type;

bool use_cross_derivatives = false;

uint8_t convert_float_uint(float f)
{
    //return (uint8_t)((f * 255.0f) + 0.5f);
    //if (f == 0.5) return 127;
    return (uint8_t)roundf(f * 255.0f);
}

rgb8_t vec3_to_rgb8(vec3_t vec)
{
    return { convert_float_uint(vec.x), convert_float_uint(vec.y), convert_float_uint(vec.z) };
}

rgb8_t* vec3_buffer_to_rgb8(vec3_t* buffer, int width, int height)
{
    rgb8_t* rgb_buffer = (rgb8_t*)malloc(width * height * sizeof(rgb8_t));
    assert(rgb_buffer != NULL);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int index = y * width + x;

            rgb_buffer[index] = vec3_to_rgb8(buffer[index]);
        }
    }

    return rgb_buffer;
}


vec3_t sample_ptex_texture(Ptex::PtexTexture* tex, Ptex::PtexFilter* filter, int faceID, vec2_t uv, vec4_t uv_derivatives)
{
    //assert(faceID < tex->numFaces());

    if (faceID >= tex->numFaces())
        return { 1.0f, 0.0f, 1.0f };

    if (tex->numChannels() == 1)
    {
        float gray;
        filter->eval(&gray, 0, 1, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return { gray, gray, gray };
    }
    else if (tex->numChannels() == 3)
    {
        vec3_t color;
        filter->eval((float*)&color, 0, 3, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return color;
    }
    else if (tex->numChannels() == 4)
    {
        vec3_t color;
        filter->eval((float*)&color, 0, 3, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return color;
    }
    else assert(false);
}

vec3_t* calculate_image_cpu(int width, int height, uint16_t* faceID_buffer, vec3_t* uv_buffer, vec4_t* uv_deriv_buffer, vec3_t background_color)
{
    vec3_t* cpu_data = (vec3_t*)malloc(width * height * sizeof(vec3_t));
    assert(cpu_data != NULL);

    vec3_t bg = background_color;
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++) {

            int i = y * width + x;

            int16_t id = faceID_buffer[i] - 1;

            if (id == -1)
            {
                cpu_data[i] = bg;
                continue;
            }

            vec3_t uv = uv_buffer[i];

            vec4_t uv_deriv = uv_deriv_buffer[i];

            vec3_t color = { uv.x, uv.y, uv.z };
            
            if (use_cross_derivatives == false)
            {
                uv_deriv.y = 0;
                uv_deriv.w = 0;
            }

            vec3_t ptex = sample_ptex_texture(g_ptex_texture, g_ptex_filter, id, { uv.x, uv.y }, uv_deriv);

            //const rgb8_t colors[] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255} };

            cpu_data[i] = ptex;
            //cpu_data[i] = { uv.x, uv.y, 0 };
        }
    }

    return cpu_data;
}

