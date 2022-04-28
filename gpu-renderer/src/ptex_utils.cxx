
#include "ptex_utils.hh"

#include "util.hh"

#include <assert.h>
#include <stb_image_write.h>
#include <vector>

void* data_to_rgba(void* _data, int width, int height, int num_channels)
{
	rgba8_t* result = (rgba8_t*)malloc(width * height * 4 * sizeof(uint8_t));

	if (num_channels == 1) {
		uint8_t* data = (uint8_t*)_data;
		for (size_t y = 0; y < height; y++)
		{
			for (size_t x = 0; x < width; x++)
			{
				int index = y * width + x;
				uint8_t gray = data[index];
				result[index] = { gray, gray, gray, 255 };
			}
		}
	}
	else if (num_channels == 3) {
		rgb8_t* data = (rgb8_t*)_data;
		for (size_t y = 0; y < height; y++)
		{
			for (size_t x = 0; x < width; x++)
			{
				int index = y * width + x;
				rgb8_t color = data[index];
				result[index] = { color.r, color.g, color.b, 255 };
			}
		}
	}
	else if (num_channels == 4) {
		memcpy(result, _data, width * height * 4 * sizeof(uint8_t));
	}
	else assert(false);

	return result;
}

gl_ptex_textures extract_textures(Ptex::PtexTexture* tex) {

	int num_faces = tex->numFaces();

	struct res_textures {
		Ptex::Res res;
		std::vector<ptex_face_texture> textures;
	};

	std::vector<res_textures> res_textures;

	for (int i = 0; i < num_faces; i++)
	{
		auto face_info = tex->getFaceInfo(i);
		
		assert(tex->dataType() == Ptex::DataType::dt_uint8);
		assert(tex->numChannels() == 4 || tex->numChannels() == 1);

		// DataSize(dataType()) * numChannels() * getFaceInfo(faceid).res.size()
		int data_size = DataSize(tex->dataType())* tex->numChannels()* face_info.res.size();
		void* data = malloc(data_size);

		tex->getData(i, data, 0);

		void* rgba_data = data_to_rgba(data, face_info.res.u(), face_info.res.v(), tex->numChannels());
		free(data);

		int width = face_info.res.u();
		int height = face_info.res.v();

		struct res_textures* res_tex = NULL;
		for (struct res_textures& tex : res_textures)
		{
			if (tex.res == face_info.res)
			{
				res_tex = &tex;
				break;
			}
		}

		if (res_tex == NULL)
		{
			struct res_textures tex = {
				face_info.res,
				std::vector<ptex_face_texture>(),
			};

			res_textures.push_back(tex);

			res_tex = &res_textures.back();
		}

		int neighbor0 = face_info.adjface(0);
		int neighbor1 = face_info.adjface(1);
		int neighbor2 = face_info.adjface(2);
		int neighbor3 = face_info.adjface(3);

		res_tex->textures.push_back({ 
			i, // face_id
			neighbor0,neighbor1,neighbor2,neighbor3, // neightbors
			rgba_data // data
			});

		/*
		char name[1024];
		sprintf(name, "ptex_output/%d_%dx%d.png", i, width, height);
		stbi_write_png(name, width, height, 4, data, width * 4);
		*/
	}

	/*
	for (struct res_textures& tex : res_textures)
	{
		printf("Res %dx%d:\n", tex.res.ulog2, tex.res.vlog2);
		for (auto t : tex.textures)
		{
			printf("  %d\n", t.face_id);
		}
	}
	*/

	gl_ptex_textures result;

	result.num_faces = tex->numFaces();

	result.num_resolutions = res_textures.size();

	result.resolutions = (ptex_res_textures*)malloc(result.num_resolutions * sizeof(ptex_res_textures));

	/*
	std::qsort(res_textures.data(), res_textures.size(), sizeof(ptex_res_textures),
		[](const void* a, const void* b) {
			const ptex_res_textures* a1 = static_cast<const ptex_res_textures*>(a);
			const ptex_res_textures* b1 = static_cast<const ptex_res_textures*>(b);
			return 0;
		});
	*/

	for (int i = 0; i < res_textures.size(); i++)
	{
		auto res_texture = res_textures[i];
		auto textures = res_texture.textures;

		result.resolutions[i].res = res_texture.res;
		result.resolutions[i].num_textures = textures.size();
		result.resolutions[i].textures = (ptex_face_texture*)malloc(textures.size() * sizeof(ptex_face_texture));

		for (int j = 0; j < textures.size(); j++)
		{
			result.resolutions[i].textures[j] = textures[j];
		}
	}

	return result;
}

gl_ptex_data create_gl_texture_arrays(gl_ptex_textures textures, GLenum mag_filter, GLenum min_filter) {
	GLuint* gl_textures = alloc_array(GLuint, textures.num_resolutions);
	glGenTextures(textures.num_resolutions, gl_textures);

	glActiveTexture(GL_TEXTURE0);

	TexIndex* face_indices = new TexIndex[textures.num_faces];

	for (int i = 0; i < textures.num_resolutions; i++)
	{
		ptex_res_textures* res_textures = &textures.resolutions[i];

		Ptex::Res res = res_textures->res;

		glBindTexture(GL_TEXTURE_2D_ARRAY, gl_textures[i]);

		if (has_KHR_debug)
		{
			char name[256];
			sprintf(name, "ARRTEX: ptex%d %dx%d", i, res.u(), res.v());
			glObjectLabel(GL_TEXTURE, gl_textures[i], -1, name);
		}

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, res.u(), res.v(), res_textures->num_textures, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		for (int j = 0; j < res_textures->num_textures; j++)
		{
			assert(i < UINT16_MAX);
			assert(j < UINT16_MAX);

			face_indices[res_textures->textures[j].face_id] = { 
				(uint16_t)i, (uint16_t)j,
				0,0,0,0, // neihgbor indices
				0,0,0,0, // neighbor transforms
			};
			//face_indices->add({ (uint16_t)i, (uint16_t)j });

			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, j, res.u(), res.v(), 1, GL_RGBA, GL_UNSIGNED_BYTE, res_textures->textures[j].data);
		}

		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		vec4_t border = { 0, 0, 0, 0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (float*)&border);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	}

	array_t<array_texture_t>* array_textures = new array_t<array_texture_t>(textures.num_resolutions);

	for (int i = 0; i < textures.num_resolutions; i++)
	{
		array_texture_t tex;

		tex.width = textures.resolutions[i].res.u();
		tex.height = textures.resolutions[i].res.v();
		tex.slices = textures.resolutions[i].num_textures;
		
		tex.texture = gl_textures[i];

		tex.wrap_s = GL_CLAMP_TO_BORDER;
		tex.wrap_t = GL_CLAMP_TO_BORDER;
		
		tex.mag_filter = mag_filter;
		tex.min_filter = min_filter;
		
		tex.is_sRGB = false;

		array_textures->add(tex);
	}

	free(gl_textures);

	GLuint face_data_buffer;
	glGenBuffers(1, &face_data_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, face_data_buffer);

	if (has_KHR_debug)
	{
		glObjectLabel(GL_BUFFER, face_data_buffer, -1, "UBO: PTex");
	}

	glBufferData(GL_UNIFORM_BUFFER, textures.num_faces * sizeof(face_indices[0]), face_indices, GL_STATIC_DRAW);

	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	gl_ptex_data data;
	data.array_textures = array_textures;
	data.face_tex_indices = new array_t<TexIndex>(face_indices, textures.num_faces);

	data.face_data_buffer = face_data_buffer;

	return data;
}
