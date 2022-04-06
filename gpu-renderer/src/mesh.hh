#ifndef MESH_H
#define MESH_H

#include "maths.hh"

typedef struct {
    vec3_t position;
    vec2_t texcoord;
    vec3_t normal;
} vertex_t;

typedef struct {
    int num_faces;
    vertex_t *vertices;
    vec3_t center;
} mesh_t;

mesh_t* load_obj(const char* filename);

#endif // MESH_H