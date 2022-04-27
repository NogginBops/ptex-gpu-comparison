#ifndef PTEX_UTILS_H
#define PTEX_UTILS_H

#include <stdint.h>
#include <Ptexture.h>

typedef struct {
	int face_id;
	void* data;
} ptex_face_texture;

typedef struct {
	Ptex::Res res;
	int num_textures;
	ptex_face_texture* textures;
} ptex_res_textures;

typedef struct {
	int num_resolutions;
	ptex_res_textures* resolutions;
} gl_ptex_textures;

typedef struct {
	uint16_t neighbor[4];
	uint16_t neighborTransforms[4];
	uint16_t texIndex;
	uint16_t texSlice;
} PTexParameters;

gl_ptex_textures extract_textures(Ptex::PtexTexture* tex);

void create_gl_texture_arrays(gl_ptex_textures textures);

#endif // !PTEX_UTILS_H