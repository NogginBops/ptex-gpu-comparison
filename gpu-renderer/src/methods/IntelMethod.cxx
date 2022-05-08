
#include "Methods.hh"

namespace Methods {
	
    IntelMethod intel;

	void IntelMethod::init(int width, int height, gl_ptex_data data, GLenum mag_filter, GLenum min_filter, int max_anisotropy)
	{
        ptex_data = data;

        // setup multi-sampler color output buffer for intel method
        {
            color_attachment_desc color_desc = {
                "Attachment: intel.color (RGB32F_MS)",
                GL_RGB32F,
                GL_RGB,
                GL_FLOAT,
                GL_REPEAT, GL_REPEAT,
                GL_LINEAR, GL_LINEAR
            };

            color_attachment_desc* color_descriptions = new color_attachment_desc[1];
            color_descriptions[0] = color_desc;

            depth_attachment_desc depth_desc = {
                "Attachment: intel.depth (DEPTH32F_MS)",
                GL_DEPTH_COMPONENT32F,
                GL_REPEAT, GL_REPEAT,
                GL_LINEAR, GL_LINEAR
            };

            depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
            depth_descriptions[0] = depth_desc;

            ms_color_framebuffer_desc = {
                "FBO: intel",
                1,
                color_descriptions,
                depth_descriptions,
                8 // number of samples
            };

            ms_color_framebuffer = create_framebuffer(ms_color_framebuffer_desc, width, height);
        }

        ptex_program = compile_shader("program: intel_ptex", "shaders/ptex.vert", "shaders/ptex_intel.frag");

        {
            char name[32];
            for (int i = 0; i < 24; i++)
            {
                sprintf(name, "aTexBorder[%d]", i);
                uniform_1i(ptex_program, name, i);
            }

            for (int i = 0; i < 24; i++)
            {
                sprintf(name, "aTexClamp[%d]", i);
                uniform_1i(ptex_program, name, i + 24);
            }
        }

        sampler_desc border_desc = {
            GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER,
            mag_filter, min_filter,
            max_anisotropy,
            { 0, 0, 0, 0 }
        };

        sampler_desc clamp_desc = {
            GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            mag_filter, min_filter,
            max_anisotropy,
            { 0, 0, 0, 0 }
        };

        border_sampler = create_sampler("sampler: intel.border", border_desc);
        clamp_sampler = create_sampler("sampler: intel.clamp", border_desc);
	}

    void IntelMethod::render(GLuint vao, int vertex_count, mat4_t mvp, vec3_t bg_color) {
        glBindFramebuffer(GL_FRAMEBUFFER, ms_color_framebuffer.framebuffer);

        glBindVertexArray(vao);

        uniform_mat4(ptex_program, "mvp", &mvp);

        glUseProgram(ptex_program);

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ptex_data.face_data_buffer);

        glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
        assert(ptex_data.array_textures->size <= 24);
        for (int i = 0; i < ptex_data.array_textures->size; i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D_ARRAY, (*ptex_data.array_textures).arr[i].texture);
            glBindSampler(i, border_sampler.sampler);

            glActiveTexture(GL_TEXTURE0 + i + 24);
            glBindTexture(GL_TEXTURE_2D_ARRAY, (*ptex_data.array_textures).arr[i].texture);
            glBindSampler(i + 24, clamp_sampler.sampler);
        }

        glDrawArrays(GL_TRIANGLES, 0, vertex_count);

        for (size_t i = 0; i < 48; i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            glBindSampler(i, 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void IntelMethod::resize_buffers(int width, int height)
    {
        recreate_framebuffer(&ms_color_framebuffer, ms_color_framebuffer_desc, width, height);
    }
}