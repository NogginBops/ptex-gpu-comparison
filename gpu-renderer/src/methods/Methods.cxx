
#include "Methods.hh"

namespace Methods {
	const char* method_names[3] = {
		"cpu",
		"nvidia",
		"intel"
	};

	void init_methods(int width, int height, Ptex::PtexTexture* texture, Ptex::PtexFilter* filter, GLenum mag_filter, GLenum min_filter, int max_anisotropy)
	{
		gl_ptex_textures textures = extract_textures(texture);
		gl_ptex_data data = create_gl_texture_arrays(textures, mag_filter, min_filter);

		cpu.init(width, height, texture, filter);
		intel.init(width, height, data, mag_filter, min_filter, max_anisotropy);
		nvidia.init(width, height, data, mag_filter, min_filter, max_anisotropy);
	}
}
