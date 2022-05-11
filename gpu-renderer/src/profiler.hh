
#include "gl_utils.hh"

#include "array.hh"

#include <stdlib.h>
#include <float.h>

#include <chrono>

#include <iostream>

#include <unordered_map>

namespace profiler {
	using chclock = std::chrono::high_resolution_clock;
	using dsec = std::chrono::duration<double>;
	using dmilli = std::chrono::duration<double, std::milli>;
	using tps = std::chrono::time_point<chclock, dsec>;

	typedef struct {
		const char* name;
		tps timestamp;
		double duration;
		int parent_index;
	} profile_entry;

#define MOVING_AVERAGE_SAMPLES 200

	struct time_info {
		double min = DBL_MAX, max = DBL_MIN;
		double sum = 0;
		double* values;
		int index = 0;

		static time_info create();

		static void destroy(time_info* info);

		double average();

		void add(double time);
	};

	struct render_pass_info {
		const char* name;
		int parent_ID;
		bool active_this_frame;
		buffered_query_t* query;
		time_info cpu_time;
		time_info gpu_time;
	};

	render_pass_info* create_render_pass(const char* name, int parent_ID, buffered_query_t* query);

	extern tps start_of_frame_timestamp;
	extern custom_arrays::array_t<profile_entry> entries;
	extern custom_arrays::stack_t<int> parent_indices;

	extern custom_arrays::array_t<render_pass_info*> current_render_passes;
	extern custom_arrays::array_t<render_pass_info*> previous_render_passes;

	void push_span(const char* name, int passID);

	void pop_span(int passID);

	void new_frame();


	void show_profiler();
}
