
#include <stdio.h>

#include <Ptexture.h>

#include "../mesh_loading.hh"

#include <stdint.h>

#include "stb_image.h"

#include "../maths.hh"

#include "../platform.hh"

#include <math.h>

struct rgb8_t {
	uint8_t r, g, b;
};

struct rgba8_t {
	uint8_t r, g, b, a;
};

struct texture_t {
	rgb8_t* data;
	int width, height;
};

rgba8_t rgb_to_rgba(rgb8_t rgb, uint8_t alpha = 255)
{
	return { rgb.r, rgb.g, rgb.b, alpha };
}


vec3_t sample_texel(const texture_t* tex, int x, int y)
{
	int index = y * tex->width + x;

	vec3_t color;
	color.x = (tex->data[index].r / 255.0f);
	color.y = (tex->data[index].g / 255.0f);
	color.z = (tex->data[index].b / 255.0f);
	return color;
}

float tex_coord_wrap(int coord, int size)
{
	if (coord < 0) return 0;
	else if (coord >= size) return size - 1;
	else return coord;
}

rgb8_t sample_texture(const texture_t* tex, vec2_t uv)
{
	float u = uv.x * tex->width;
	float v = (1-uv.y) * tex->height;

	vec3_t color = { 0 };
	// See 8.14 in: https://www.khronos.org/registry/OpenGL/specs/gl/glspec45.core.pdf

		/*
		float dudx = uv_derivatives.x;
		float dvdx = uv_derivatives.y;
		float dudy = uv_derivatives.z;
		float dvdy = uv_derivatives.w;
		float rho1 = sqrtf(dudx * dudx + dvdx * dvdx);
		float rho2 = sqrtf(dudy * dudy + dvdy * dvdy);
		float rho = rho1 > rho2 ? rho1 : rho2;
		float λ_base = log2f(rho);
		*/

	float uMHalf = u - 0.5f;
	float vMHalf = v - 0.5f;

	int i0 = tex_coord_wrap((int)floorf(uMHalf), tex->width);
	int j0 = tex_coord_wrap((int)floorf(vMHalf), tex->height);

	int i1 = tex_coord_wrap((int)floorf(uMHalf) + 1, tex->width);
	int j1 = tex_coord_wrap((int)floorf(vMHalf) + 1, tex->height);

	assert(i0 >= 0 && i0 < tex->width);
	assert(i1 >= 0 && i0 < tex->width);
	assert(j0 >= 0 && i0 < tex->height);
	assert(j1 >= 0 && i0 < tex->height);

	//float int_part;
	//float α = modff(uMHalf, &int_part);
	//float β = modff(vMHalf, &int_part);
	float α = uMHalf - floorf(uMHalf);
	float β = vMHalf - floorf(vMHalf);

	assert(α >= 0 && α <= 1.0f);
	assert(β >= 0 && β <= 1.0f);

	vec3_t τ_i0j0 = sample_texel(tex, i0, j0);
	vec3_t τ_i0j1 = sample_texel(tex, i0, j1);
	vec3_t τ_i1j0 = sample_texel(tex, i1, j0);
	vec3_t τ_i1j1 = sample_texel(tex, i1, j1);

	vec3_t τ_i01j0 = vec3_lerp(τ_i0j0, τ_i1j0, α);
	vec3_t τ_i01j1 = vec3_lerp(τ_i0j1, τ_i1j1, α);

	vec3_t τ = vec3_lerp(τ_i01j0, τ_i01j1, β);

	/*
	vec3_t τ_i01j0 = vec3_lerp(τ_i0j0, τ_i0j1, β);
	vec3_t τ_i01j1 = vec3_lerp(τ_i1j0, τ_i1j1, β);
	vec3_t τ = vec3_lerp(τ_i01j0, τ_i01j1, α);
	*/

	color = τ;

	return { (uint8_t)(color.x * 255), (uint8_t)(color.y * 255) , (uint8_t)(color.z * 255) };
}

rgb8_t sample_texture(rgb8_t* tex_data, int width, int height, vec2_t uv) {

	int x = uv.x * (width - 1);
	int y = (1-uv.y) * (height - 1);

	return tex_data[y * width + x];
}

float wedge(vec2_t v, vec2_t w)
{
	return v.x * w.y - v.y * w.x;
}

void transfer_data(ptex_mesh_t* mesh, texture_t* texture, Ptex::PtexWriter* writer)
{
	// FIXME: Create adjecency data!

	bool b = false;

	for (size_t i = 0; i < mesh->num_vertices; i+=4)
	{
		int face_id = i / 4;

		ptex_vertex_t* v0 = &mesh->vertices[i + 0];
		ptex_vertex_t* v1 = &mesh->vertices[i + 1];
		ptex_vertex_t* v2 = &mesh->vertices[i + 2];
		ptex_vertex_t* v3 = &mesh->vertices[i + 3];

		if (v2->uv.x == 0.611208975f)
		{
			printf("maybe");
		}

		vec2_t resolution = vec2_new(texture->width, texture->height);

		float l01 = vec2_length(vec2_mul_vec2(vec2_abs(vec2_sub(v0->uv, v1->uv)), resolution));
		float l02 = vec2_length(vec2_mul_vec2(vec2_abs(vec2_sub(v0->uv, v2->uv)), resolution));
		float l31 = vec2_length(vec2_mul_vec2(vec2_abs(vec2_sub(v3->uv, v1->uv)), resolution));
		float l32 = vec2_length(vec2_mul_vec2(vec2_abs(vec2_sub(v3->uv, v2->uv)), resolution));

		float S = (l01 + l02 + l31 + l32) / 2;
		

		float res_log2 = log2f(sqrtf(S));

		Ptex::FaceInfo info;
		if (b)
		{
			info = Ptex::FaceInfo(Ptex::Res(8, 8));
		}
		else
		{
			info = Ptex::FaceInfo(Ptex::Res(7, 7));
		}

		b = !b;

		rgba8_t* face_data = (rgba8_t*)malloc(info.res.size() * sizeof(rgba8_t));

		for (size_t y = 0; y < info.res.u(); y++)
		{
			float v = y / (float)(info.res.u() - 1);

			for (size_t x = 0; x < info.res.v(); x++)
			{
				float u = x / (float)(info.res.v() - 1);

				vec2_t uv_vec = { u, v };

				vec2_t tex_uv0 = vec2_lerp(v0->uv, v1->uv, u);
				vec2_t tex_uv1 = vec2_lerp(v3->uv, v2->uv, u);
				vec2_t tex_uv = vec2_lerp(tex_uv0, tex_uv1, v);

				// sample the texture with this UV.
				rgb8_t color = sample_texture(texture, tex_uv);

				//rgba8_t uv = { (uint8_t)(tex_uv.x * 255), (uint8_t)(tex_uv.y * 255), 0, 255 };

				face_data[y * info.res.v() + x] = rgb_to_rgba(color);
				//face_data[y * info.res.v() + x] = uv;
			}
		}

		bool success = writer->writeFace(face_id, info, face_data, 0);
		assert(success);

		free(face_data);
	}
}

#ifdef __APPLE__
#define ASSETS_PATH "../assets"
#else
#define ASSETS_PATH "../../../assets"
#endif

int main(int argv, char** argc)
{
	change_directory(ASSETS_PATH);

	const char* ptex_path = "models/robot/Quandtum_BA-2_v1_1.ptex";
	const char* obj_path = "models/robot/robot_2.obj";
	const char* tex_path = "models/robot/textures/Turret-Diffuse.jpg";

	ptex_mesh_t* mesh = load_mesh(obj_path);

	int width, height, channels;
	rgb8_t* tex_data = (rgb8_t*)stbi_load(tex_path, &width, &height, &channels, 3);

	Ptex::String error_str;
	Ptex::PtexWriter* writer = Ptex::PtexWriter::open(ptex_path,
		Ptex::MeshType::mt_quad, 
		Ptex::DataType::dt_uint8, 
		4, 3, mesh->num_vertices / 4, 
		error_str, false);
	if (error_str.empty() == false)
	{
		printf("Ptex Writer Open Error: %s\n", error_str.c_str());
	}

	texture_t tex = { tex_data, width, height };

	transfer_data(mesh, &tex, writer);

	
	writer->close(error_str);
	if (error_str.empty() == false)
	{
		printf("Ptex Writer Close Error: %s\n", error_str.c_str());
	}

	writer->applyEdits(ptex_path, error_str);
	if (error_str.empty() == false)
	{
		printf("Ptex Writer Apply Changes Error: %s\n", error_str.c_str());
	}

	printf("Hello World!\n");
	return 0;
}
