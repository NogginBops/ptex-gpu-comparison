
#include "profiler.hh"

#include <imgui.h>

namespace profiler {

	time_info time_info::create()
	{
		time_info info;
		info.values = (double*)calloc(MOVING_AVERAGE_SAMPLES, sizeof(double));
		return info;
	}

	void time_info::destroy(time_info* info)
	{
		free(info->values);
	}

	double time_info::average() { return sum / MOVING_AVERAGE_SAMPLES; }

	void time_info::add(double time)
	{
		if (time < min) min = time;
		if (time > max) max = time;

		values[index] = time;
		index = (index + 1) % MOVING_AVERAGE_SAMPLES;

		sum -= values[index];
		sum += time;
	}

	render_pass_info* create_render_pass(const char* name, int parent_ID, buffered_query_t* query)
	{
		render_pass_info* info = new render_pass_info;
		info->name = name;
		info->parent_ID = parent_ID;
		info->query = query;

		info->cpu_time = time_info::create();
		info->gpu_time = time_info::create();

		return info;
	}

	tps start_of_frame_timestamp;
	custom_arrays::array_t<profile_entry> entries(100);
	custom_arrays::stack_t<int> parent_indices(100);

	custom_arrays::array_t<render_pass_info*> current_render_passes(100);
	custom_arrays::array_t<render_pass_info*> previous_render_passes(100);

	std::unordered_map<int, render_pass_info*> pass_infos = {};

	void push_span(const char* name, int pass_ID)
	{
		profile_entry entry;
		entry.name = name;
		entry.timestamp = chclock::now();
		entry.duration = -1;

		{
			int parent;
			if (parent_indices.try_peek(parent))
				entry.parent_index = parent;
			else entry.parent_index = -1;
		}
		
		entries.add(entry);

		int id = entries.size - 1;

		parent_indices.push(id);

		auto search = pass_infos.find(pass_ID);
		render_pass_info* pass_info;
		if (search == pass_infos.end())
		{
			buffered_query_t* query = new buffered_query_t;
			*query = create_query(name);
			pass_info = create_render_pass(name, entry.parent_index, query);

			pass_infos.emplace(pass_ID, pass_info);
		}
		else
		{
			pass_info = search->second;
		}

		pass_info->active_this_frame = true;

		current_render_passes.add(pass_info);

		begin_query(pass_info->query);
	}

	void pop_span(int pass_ID)
	{
		int stack_ID = parent_indices.pop();
		assert(stack_ID == pass_ID);

		profile_entry& entry = entries[stack_ID];
		entry.duration = std::chrono::duration_cast<dmilli>(chclock::now() - entry.timestamp).count();

		auto search = pass_infos.find(pass_ID);
		render_pass_info* info = search->second;

		info->cpu_time.add(entry.duration);

		end_query(info->query);
	}

	void new_frame() {
		start_of_frame_timestamp = chclock::now();

		entries.clear();
		parent_indices.clear();

		auto temp = current_render_passes;
		current_render_passes = previous_render_passes;
		previous_render_passes = temp;

		current_render_passes.clear();

		if (previous_render_passes.size != 0)
		{
			auto last = previous_render_passes[previous_render_passes.size - 1];

			// We only read the query if it's ready or if we have to read the query.
			if (is_query_ready(last->query) == true || (last->query->current_query + 1) % QUERY_BUFFER_SIZE == last->query->read_query)
			{
				tps start = chclock::now();
				while (is_query_ready(last->query) == false)
				{
					printf("Not ready! read: %d, write: %d\n", last->query->read_query, last->query->current_query);
				}
				tps end = chclock::now();

				double time = std::chrono::duration_cast<dmilli>(end - start).count();
				//std::cout << "Waiting for queries took: " << time << " ms" << std::endl;

				for (size_t i = 0; i < previous_render_passes.size; i++)
				{
					auto pass = previous_render_passes[i];
					long nano_seconds = get_time_elapsed_query_result(pass->query, false);
					pass->gpu_time.add((nano_seconds / 1000000000.0) * 1000.0);

					//printf("%s: cpu: %zg, gpu: %zg\n", pass->name, pass->cpu_time.average(), pass->gpu_time.average());
				}
			}
		}
	}


	custom_arrays::stack_t<int> ids(100);
	void show_profiler() {

		if (ImGui::Begin("Profiler"))
		{
			ids.clear();
			ids.push(-1);
			for (int i = 0; i < previous_render_passes.size; i++)
			{
				ImGuiTreeNodeFlags flags =
					ImGuiTreeNodeFlags_OpenOnDoubleClick |
					ImGuiTreeNodeFlags_OpenOnArrow |
					ImGuiTreeNodeFlags_SpanAvailWidth;

				bool open = false;

				if (i >= previous_render_passes.size - 1 || previous_render_passes[i + 1]->parent_ID != i)
				{
					flags |= ImGuiTreeNodeFlags_Leaf;
				}

				while (ids.size > 0 && ids.peek() > previous_render_passes[i]->parent_ID)
				{
					ids.pop();
					ImGui::TreePop();
				}

				int id = -1;
				if (ids.size > 0)
					id = ids.peek();

				bool show = id == previous_render_passes[i]->parent_ID;
				if (show)
				{
					auto pass_info = previous_render_passes[i];
					auto cpu_time = pass_info->cpu_time.average();
					auto gpu_time = pass_info->gpu_time.average();

					char text[1024];
					sprintf(text, "%s | cpu: %.3fms | gpu: %.3fms###%i", pass_info->name, cpu_time, gpu_time, i);
					open = ImGui::TreeNodeEx(text, flags);
				}

				if (open)
				{
					printf("");
				}

				if (open && (flags & ImGuiTreeNodeFlags_Leaf) == 0)
				{
					ids.push(i);
				}

				if (show && (flags & ImGuiTreeNodeFlags_Leaf) != 0)
				{
					ImGui::TreePop();
				}
			}

			for (int i = 0; i < ids.size - 1; i++)
			{
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
}