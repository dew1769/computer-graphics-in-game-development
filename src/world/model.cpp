#define TINYOBJLOADER_IMPLEMENTATION

#include "model.h"

#include "utils/error_handler.h"

#include <linalg.h>


using namespace linalg::aliases;
using namespace cg::world;

cg::world::model::model() {}

cg::world::model::~model() {}

void cg::world::model::load_obj(const std::filesystem::path& model_path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.string().c_str())) {
        throw std::runtime_error(warn + err);
    }

    allocate_buffers(shapes);
    fill_buffers(shapes, attrib, materials, model_path.parent_path());
}

void model::allocate_buffers(const std::vector<tinyobj::shape_t>& shapes)
{
    vertex_buffers.resize(shapes.size());
    index_buffers.resize(shapes.size());

    for (size_t i = 0; i < shapes.size(); i++) {
        vertex_buffers[i] = std::make_shared<cg::resource<cg::vertex>>(shapes[i].mesh.indices.size());
        index_buffers[i] = std::make_shared<cg::resource<unsigned int>>(shapes[i].mesh.indices.size());
    }
}

float3 cg::world::model::compute_normal(const tinyobj::attrib_t& attrib, const tinyobj::mesh_t& mesh, size_t index_offset)
{
    tinyobj::index_t idx0 = mesh.indices[index_offset + 0];
    tinyobj::index_t idx1 = mesh.indices[index_offset + 1];
    tinyobj::index_t idx2 = mesh.indices[index_offset + 2];

    float3 v0 = float3{
        attrib.vertices[3 * idx0.vertex_index + 0],
        attrib.vertices[3 * idx0.vertex_index + 1],
        attrib.vertices[3 * idx0.vertex_index + 2]
    };
    float3 v1 = float3{
        attrib.vertices[3 * idx1.vertex_index + 0],
        attrib.vertices[3 * idx1.vertex_index + 1],
        attrib.vertices[3 * idx1.vertex_index + 2]
    };
    float3 v2 = float3{
        attrib.vertices[3 * idx2.vertex_index + 0],
        attrib.vertices[3 * idx2.vertex_index + 1],
        attrib.vertices[3 * idx2.vertex_index + 2]
    };

    float3 normal = normalize(cross(v1 - v0, v2 - v0));
    return normal;
}


void model::fill_vertex_data(cg::vertex& vertex, const tinyobj::attrib_t& attrib, const tinyobj::index_t idx, const float3 computed_normal, const tinyobj::material_t material)
{
    vertex.position = float3{
        attrib.vertices[3 * idx.vertex_index + 0],
        attrib.vertices[3 * idx.vertex_index + 1],
        attrib.vertices[3 * idx.vertex_index + 2]
    };
    vertex.normal = computed_normal;
    vertex.color = float3{material.diffuse[0], material.diffuse[1], material.diffuse[2]};
}

void model::fill_buffers(const std::vector<tinyobj::shape_t>& shapes, const tinyobj::attrib_t& attrib, const std::vector<tinyobj::material_t>& materials, const std::filesystem::path& base_folder)
{
    for (size_t i = 0; i < shapes.size(); i++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[i].mesh.num_face_vertices[f];

            float3 normal = compute_normal(attrib, shapes[i].mesh, index_offset);

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
                tinyobj::material_t material = materials[shapes[i].mesh.material_ids[f]];

                cg::vertex vertex;
                fill_vertex_data(vertex, attrib, idx, normal, material);

                vertex_buffers[i]->item(index_offset + v) = vertex;
                index_buffers[i]->item(index_offset + v) = index_offset + v;
            }
            index_offset += fv;
        }
    }
}


const std::vector<std::shared_ptr<cg::resource<cg::vertex>>>&
cg::world::model::get_vertex_buffers() const
{
	return vertex_buffers;
}

const std::vector<std::shared_ptr<cg::resource<unsigned int>>>&
cg::world::model::get_index_buffers() const
{
	return index_buffers;
}

const std::vector<std::filesystem::path>& cg::world::model::get_per_shape_texture_files() const
{
	return textures;
}


const float4x4 cg::world::model::get_world_matrix() const
{
	return float4x4{
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{0, 0, 0, 1}};
}
