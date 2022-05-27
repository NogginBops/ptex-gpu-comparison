
#pragma once

#include "../ptex_utils.hh"
#include <Ptexture.h>

namespace Methods {
	
	enum class Methods {
		cpu = 0,
		nvidia = 1,
		intel = 2,
		reduced_traverse = 3,

		last,
	};
	extern const char* method_names[4];
	
	struct CpuMethod {
		framebuffer_desc to_cpu_framebuffer_desc;
		framebuffer_t to_cpu_framebuffer;

		framebuffer_desc cpu_result_framebuffer_desc;
		framebuffer_t cpu_result_framebuffer;
		
		GLuint to_cpu_program;
		GLuint cpu_stream_program;

		texture_t cpu_stream_texture;

		void init(int width, int height);
		void render(GLuint vao, int vertex_count, Ptex::PtexTexture* texture, Ptex::PtexFilter* filter, mat4_t mvp, vec3_t bg_color);
		void resize_buffers(int width, int height);
	};

	struct NvidiaMethod {
		framebuffer_desc framebuffer_desc;
		framebuffer_t framebuffer;

		GLuint ptex_program;

		sampler_t border_sampler;

		void init(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy);
		void render(GLuint vao, int vertex_count, gl_ptex_data ptex_data, mat4_t mvp, vec3_t bg_color);
		void resize_buffers(int width, int height);
	};

	struct IntelMethod {
		framebuffer_desc ms_color_framebuffer_desc;
		framebuffer_t ms_color_framebuffer;

		// Used when taking screenshots.
		framebuffer_desc resolve_color_framebuffer_desc;
		framebuffer_t resolve_color_framebuffer;

		GLuint ptex_program; 

		sampler_t border_sampler;
		sampler_t clamp_sampler;

		void init(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy);
		void render(GLuint vao, int vertex_count, gl_ptex_data ptex_data, mat4_t mvp, vec3_t bg_color);
		void resize_buffers(int width, int height);
	};
	
	struct ReducedTraverseMethod {
		framebuffer_desc framebuffer_desc;
		framebuffer_t framebuffer;

		GLuint ptex_program;

		sampler_t border_sampler;
		
		bool visualize;

		void init(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy);
		void render(GLuint vao, int vertex_count, gl_ptex_data ptex_data, mat4_t mvp, vec3_t bg_color);
		void resize_buffers(int width, int height);
	};

	extern CpuMethod cpu;
	extern NvidiaMethod nvidia;
	extern IntelMethod intel;
	extern ReducedTraverseMethod reducedTraverse;

	void init_methods(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy);
}
