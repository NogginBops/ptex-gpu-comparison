
#include "Methods.hh"

namespace Methods {
	ReducedTraverseMethod reducedTraverse;

	void ReducedTraverseMethod::init(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy) {
		// setup color output buffer for reduced traverse method
		{
			color_attachment_desc color_desc = {
				"Attachment: reduced_traverse.color (RGB32F)",
				GL_RGB8,
				GL_RGB,
				GL_FLOAT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc* color_descriptions = new color_attachment_desc[1];
			color_descriptions[0] = color_desc;

			depth_attachment_desc depth_desc = {
				"Attachment: reduced_traverse.depth (DEPTH32F)",
				GL_DEPTH_COMPONENT32F,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
			depth_descriptions[0] = depth_desc;

			framebuffer_desc = {
				"FBO: reduced_traverse",
				1,
				color_descriptions,
				depth_descriptions,
				1 // number of samples
			};

			framebuffer = create_framebuffer(framebuffer_desc, width, height);
		}

		// setup resolve output buffer only used for screenshots
		{
			color_attachment_desc color_desc = {
				"Attachment: reduced_traverse_resolve.color (RGB32F)",
				GL_RGB8,
				GL_RGB,
				GL_FLOAT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc* color_descriptions = new color_attachment_desc[1];
			color_descriptions[0] = color_desc;

			depth_attachment_desc depth_desc = {
				"Attachment: reduced_traverse_resolve.depth (DEPTH32F)",
				GL_DEPTH_COMPONENT32F,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
			depth_descriptions[0] = depth_desc;

			resolve_framebuffer_desc = {
				"FBO: reduced_traverse_resolve",
				1,
				color_descriptions,
				depth_descriptions,
				1 // number of samples
			};

			resolve_framebuffer = create_framebuffer(resolve_framebuffer_desc, width, height);
		}

		ptex_program = compile_shader("program: reduced_traverse", "shaders/ptex.vert", "shaders/ptex_reduced_traverse.frag");

		sampler_desc border_desc = {
			GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER,
			mag_filter, min_filter,
			max_anisotropy,
			{ 0, 0, 0, 0 }
		};

		border_sampler = create_sampler("sampler: reduced_traverse.border", border_desc);

		int blockIndex = glGetUniformBlockIndex(ptex_program, "FaceDataUniform");
		glUniformBlockBinding(ptex_program, blockIndex, 0);

		{
			char name[32];
			for (int i = 0; i < 32; i++)
			{
				sprintf(name, "aTex[%d]", i);
				uniform_1i(ptex_program, name, i);
			}
		}
	}

	void ReducedTraverseMethod::render(GLuint vao, int vertex_count, gl_ptex_data ptex_data, mat4_t mvp, vec3_t bg_color)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);

		glBindVertexArray(vao);

		uniform_mat4(ptex_program, "mvp", &mvp);
		uniform_1i(ptex_program, "visualize", visualize);

		glUseProgram(ptex_program);

		// Bind adjacency data
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, ptex_data.face_data_buffer);

		glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		assert(ptex_data.array_textures->size <= 32);
		for (int i = 0; i < ptex_data.array_textures->size; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D_ARRAY, (*ptex_data.array_textures).arr[i].texture);
			glBindSampler(i, border_sampler.sampler);
		}

		glDrawArrays(GL_TRIANGLES, 0, vertex_count);

		for (int i = 0; i < ptex_data.array_textures->size; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
			glBindSampler(i, 0);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ReducedTraverseMethod::resize_buffers(int width, int height)
	{
		recreate_framebuffer(&framebuffer, framebuffer_desc, width, height);
		recreate_framebuffer(&resolve_framebuffer, resolve_framebuffer_desc, width, height);
	}
}