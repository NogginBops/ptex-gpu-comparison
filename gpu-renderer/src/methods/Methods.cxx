
#include "Methods.hh"

namespace Methods {
	const char* method_names[4] = {
		"cpu",
		"nvidia",
		"intel",
		"hybrid"
	};

	void init_methods(int width, int height, GLenum mag_filter, GLenum min_filter, int max_anisotropy)
	{
		cpu.init(width, height);
		intel.init(width, height, mag_filter, min_filter, max_anisotropy);
		nvidia.init(width, height, mag_filter, min_filter, max_anisotropy);
		hybrid.init(width, height, mag_filter, min_filter, max_anisotropy);
	}
}
