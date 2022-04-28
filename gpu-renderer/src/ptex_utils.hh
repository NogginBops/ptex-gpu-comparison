#ifndef PTEX_UTILS_H
#define PTEX_UTILS_H

#include "array.hh"
#include "gl_utils.hh"

#include <stdint.h>
#include <Ptexture.h>

typedef struct {
	int face_id;
	int neighbors[4];
	void* data;
} ptex_face_texture;

typedef struct {
	Ptex::Res res;
	int num_textures;
	ptex_face_texture* textures;
} ptex_res_textures;

typedef struct {
	int num_faces;
	int num_resolutions;
	ptex_res_textures* resolutions;
} gl_ptex_textures;

typedef struct {
	uint16_t neighbor[4];
	uint16_t neighborTransforms[4];
	uint16_t texIndex;
	uint16_t texSlice;
} PTexParameters;

typedef struct {
	uint16_t texIndex, texSilce;
	uint16_t neighborIndexes[4];
	uint16_t neighborTransforms[4];
} TexIndex;

typedef struct {
	array_t<array_texture_t>* array_textures;
	array_t<TexIndex>* face_tex_indices;

	GLuint face_data_buffer;
} gl_ptex_data;

gl_ptex_textures extract_textures(Ptex::PtexTexture* tex);

gl_ptex_data create_gl_texture_arrays(gl_ptex_textures textures, GLenum mag_filter, GLenum min_filter);

#endif // !PTEX_UTILS_H