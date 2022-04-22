
#include "mesh_loading.hh"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include <tinyobj_loader_c.h>

#include <stdio.h>
#include <vector>

typedef struct {
	//FILE* file;
} file_reader_context_t;

void file_reader(void* ctx, const char* filename, int is_mtl, const char* obj_filename, char** buf, size_t* len) {
	FILE* file = fopen(filename, "rb");
	assert(file != NULL);

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	*buf = (char*)malloc((size + 1) * sizeof(char));
	assert(*buf != NULL);

	size_t read_size = fread(*buf, sizeof(char), size, file);
	(*buf)[size] = '\0';
	
	assert(size == read_size);
	
	*len = read_size;

	fclose(file);
}

ptex_mesh_t* load_ptex_mesh(const char* filename) {

	tinyobj_attrib_t attrib;
	tinyobj_attrib_init(&attrib);
	tinyobj_shape_t* shapes;
	size_t num_shapes;
	tinyobj_material_t* materials;
	size_t num_materials;

	{
		file_reader_context_t file_reader_context;
		//file_reader_context.file = fopen(filename, "rb");
		//assert(file_reader_context.file != NULL);

		int result = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials, filename, file_reader, &file_reader_context, 0);

		if (result != TINYOBJ_SUCCESS)
		{
			printf("Could not load file '%s'.\n", filename);
		}
	}

	vec3_t* positions = (vec3_t*)attrib.vertices;
	vec3_t* normals = (vec3_t*)attrib.normals;

	std::vector<ptex_vertex_t> vertices;

	size_t vertex_id = 0;
	for (size_t face_id = 0; face_id < attrib.num_face_num_verts; face_id++)
	{
		int num_verts = attrib.face_num_verts[face_id];
		printf("verts for face %zu: %d\n", face_id, num_verts);

		assert(num_verts == 4 && "We only handle quad meshes for now.");

		for (size_t i = 0; i < num_verts; i++)
		{
			auto face = attrib.faces[vertex_id + i];

			ptex_vertex_t vertex;

			vertex.position = positions[face.v_idx];
			vertex.normal = normals[face.vn_idx];
			vertex.face_id = face_id;
			switch (i)
			{
			case 0: vertex.uv = { 0, 0 }; break;
			case 1: vertex.uv = { 0, 1 }; break;
			case 2: vertex.uv = { 1, 0 }; break;
			case 3: vertex.uv = { 1, 1 }; break;
			}

			vertices.push_back(vertex);
		}
	}

	ptex_mesh_t* mesh = (ptex_mesh_t*)malloc(sizeof(ptex_mesh_t));
	mesh->num_vertices = vertices.size();
	mesh->vertices = (ptex_vertex_t*)malloc(sizeof(ptex_vertex_t) * mesh->num_vertices);
	memcpy(mesh->vertices, vertices.data(), sizeof(ptex_vertex_t) * mesh->num_vertices);

	tinyobj_attrib_free(&attrib);
	tinyobj_materials_free(materials, num_materials);
	tinyobj_shapes_free(shapes, num_shapes);

	return mesh;
}
