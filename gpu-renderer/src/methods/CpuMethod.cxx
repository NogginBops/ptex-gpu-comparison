
#include "Methods.hh"

#include "../cpu_renderer.hh"

namespace Methods {
	CpuMethod cpu;

	void CpuMethod::init(int width, int height, Ptex::PtexTexture* texture, Ptex::PtexFilter* filter) {
		this->texture = texture;
		this->filter = filter;

		to_cpu_program = compile_shader("to_cpu_program", "shaders/ptex.vert", "shaders/ptex_output.frag");
		cpu_stream_program = compile_shader("cpu_stream_program", "shaders/fullscreen.vert", "shaders/fullscreen.frag");

		texture_desc cpu_stream_tex_desc = {
			GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
			GL_NEAREST, GL_NEAREST,
			true
		};
		cpu_stream_texture = create_empty_texture(cpu_stream_tex_desc, width, height, GL_RGB32F, "cpu stream texture");

		// Setup to_cpu framebuffer
		{
			color_attachment_desc faceID_desc = {
				"Attachment: to_cpu.faceID (R16UI)",
				GL_R16UI,
				GL_RED_INTEGER,
				GL_UNSIGNED_INT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc UV_desc = {
				"Attachment: to_cpu.UV (RGB32F)",
				GL_RGB32F,
				GL_RGB,
				GL_FLOAT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc UV_deriv_desc = {
				"Attachment: to_cpu.UV_deriv (RGBA32F)",
				GL_RGBA32F,
				GL_RGBA,
				GL_FLOAT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc* color_descriptions = new color_attachment_desc[3];
			color_descriptions[0] = faceID_desc;
			color_descriptions[1] = UV_desc;
			color_descriptions[2] = UV_deriv_desc;

			depth_attachment_desc depth_desc = {
				"Attachment: to_cpu.depth (DEPTH32F)",
				GL_DEPTH_COMPONENT32F,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
			depth_descriptions[0] = depth_desc;

			to_cpu_framebuffer_desc = {
				"FBO: to_cpu",
				3,
				color_descriptions,
				depth_descriptions,
				1 // number of samples
			};

			to_cpu_framebuffer = create_framebuffer(to_cpu_framebuffer_desc, width, height);
		}

		// Setup result framebuffer
		{
			color_attachment_desc color_desc = {
				"Attachemnt: cpu_result.color (RGB32F)",
				GL_RGB32F,
				GL_RGB,
				GL_FLOAT,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			color_attachment_desc* color_descriptions = new color_attachment_desc[1];
			color_descriptions[0] = color_desc;

			depth_attachment_desc depth_desc = {
				"Attachment: cpu_result.depth (DEPTH32F)",
				GL_DEPTH_COMPONENT32F,
				GL_REPEAT, GL_REPEAT,
				GL_LINEAR, GL_LINEAR
			};

			depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
			depth_descriptions[0] = depth_desc;

			cpu_result_framebuffer_desc = {
				"FBO: cpu_result",
				1,
				color_descriptions,
				depth_descriptions,
				1 // number of samples
			};

			cpu_result_framebuffer = create_framebuffer(cpu_result_framebuffer_desc, width, height);
		}
	}

	void CpuMethod::render(GLuint vao, int vertex_count, mat4_t mvp, vec3_t bg_color) {

		glBindFramebuffer(GL_FRAMEBUFFER, to_cpu_framebuffer.framebuffer);

		GLenum drawBuffers[] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
		};
		glDrawBuffers(3, drawBuffers);

		glBindVertexArray(vao);

		uint32_t faceClearValue[] = { 0, 0, 0, 0 };
		float uvClearValue[] = { 0, 0, 0, 0 };
		float depthClearValue = 1.0f;
		glClearBufferuiv(GL_COLOR, 0, faceClearValue);
		glClearBufferfv(GL_COLOR, 1, uvClearValue);
		glClearBufferfv(GL_COLOR, 2, uvClearValue);
		glClearBufferfv(GL_DEPTH, 0, &depthClearValue);

		uniform_mat4(to_cpu_program, "mvp", &mvp);

		glUseProgram(to_cpu_program);

		glDrawArrays(GL_TRIANGLES, 0, vertex_count);

		// Download data to cpu
		// FIXME: Do not re-allocate buffers every frame!
		{
			int width = to_cpu_framebuffer.width;
			int height = to_cpu_framebuffer.height;

			int pixels = width * height;

			glReadBuffer(GL_COLOR_ATTACHMENT0);
			void* faceID_buffer = malloc(pixels * sizeof(uint16_t));
			glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_SHORT, faceID_buffer);

			glReadBuffer(GL_COLOR_ATTACHMENT1);
			void* uv_buffer = malloc(pixels * sizeof(vec3_t));
			glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, uv_buffer);

			glReadBuffer(GL_COLOR_ATTACHMENT2);
			void* uv_deriv_buffer = malloc(pixels * sizeof(vec4_t));
			glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, uv_deriv_buffer);

			vec3_t* cpu_buffer = calculate_image_cpu(width, height, (uint16_t*)faceID_buffer, (vec3_t*)uv_buffer, (vec4_t*)uv_deriv_buffer, bg_color);

			rgb8_t* cpu_rgb8_buffer = vec3_buffer_to_rgb8(cpu_buffer, width, height);

			update_texture(&cpu_stream_texture, GL_RGB32F, GL_RGB, GL_FLOAT, width, height, cpu_buffer);

			free(faceID_buffer);
			free(uv_buffer);
			free(uv_deriv_buffer);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, cpu_result_framebuffer.framebuffer);

		glUseProgram(cpu_stream_program);

		uniform_1i(cpu_stream_program, "tex", 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, cpu_stream_texture.texture);

		glDisable(GL_DEPTH_TEST);

		glDrawArrays(GL_TRIANGLES, 0, 3);

		glEnable(GL_DEPTH_TEST);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void CpuMethod::resize_buffers(int width, int height)
	{
		recreate_framebuffer(&to_cpu_framebuffer, to_cpu_framebuffer_desc, width, height);
		recreate_framebuffer(&cpu_result_framebuffer, cpu_result_framebuffer_desc, width, height);
	}
}
