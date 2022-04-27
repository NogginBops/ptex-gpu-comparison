
#include "ptex_utils.hh"

#include "gl_utils.hh"

#include <glad/glad.h>

#include <assert.h>
#include <stb_image_write.h>
#include <vector>

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

		// DataSize(dataType()) * numChannels() * getFaceInfo(faceid).res.size()
		int data_size = DataSize(tex->dataType())* tex->numChannels()* face_info.res.size();
		void* data = malloc(data_size);

		tex->getData(i, data, 0);

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

		res_tex->textures.push_back({ i, data });

		/*
		char name[1024];
		sprintf(name, "ptex_output/%d_%dx%d.png", i, width, height);
		stbi_write_png(name, width, height, 4, data, width * 4);
		*/
	}

	for (struct res_textures& tex : res_textures)
	{
		printf("Res %dx%d:\n", tex.res.ulog2, tex.res.vlog2);
		for (auto t : tex.textures)
		{
			printf("  %d\n", t.face_id);
		}
	}

	gl_ptex_textures result;

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

void create_gl_texture_arrays(gl_ptex_textures textures) {

	GLuint* gl_textures = (GLuint*)malloc(textures.num_resolutions * sizeof(GLuint));
	glGenTextures(textures.num_resolutions, gl_textures);

	glActiveTexture(GL_TEXTURE0);

	for (int i = 0; i < textures.num_resolutions; i++)
	{
		ptex_res_textures* res_textures = &textures.resolutions[i];

		Ptex::Res res = res_textures->res;

		glBindTexture(GL_TEXTURE_2D_ARRAY, gl_textures[i]);

		if (has_KHR_debug)
		{
			char name[256];
			sprintf(name, "ptex%d", i);
			glObjectLabel(GL_TEXTURE, gl_textures[i], -1, name);
		}

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, res.u(), res.v(), res_textures->num_textures, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		for (int j = 0; j < res_textures->num_textures; j++)
		{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, j, res.u(), res.v(), 1, GL_RGBA, GL_UNSIGNED_BYTE, res_textures->textures[j].data);
		}

		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	}
}
