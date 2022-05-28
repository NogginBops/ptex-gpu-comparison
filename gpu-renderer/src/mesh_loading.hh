
#include <stdint.h>
#include "maths.hh"

typedef struct {
	vec3_t position;
	vec3_t normal;
	vec2_t uv;
	int32_t face_id;
} ptex_vertex_t;

typedef struct {
	int num_vertices;
	ptex_vertex_t* vertices;
	vec3_t center;
} ptex_mesh_t;

ptex_mesh_t* load_ptex_mesh(const char* filename);

ptex_mesh_t* load_mesh(const char* filename);
