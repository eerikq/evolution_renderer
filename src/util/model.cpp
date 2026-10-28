#include "model.h"

#include "util/log.h"

#include <tiny_obj_loader.h>
#include <unordered_map>
#include <vector>

namespace model {
    void Load(const char* filename, std::vector<Vertex>* vertex_buffer, std::vector<uint16_t>* index_buffer) {
        tinyobj::attrib_t attributes;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warnings, errors;

        bool succeeded = tinyobj::LoadObj(&attributes, &shapes, &materials, &warnings, &errors, filename);
        if (!succeeded) {
            evoLog(PrintSeverity::Warn, "{}", warnings);
            evoLog(PrintSeverity::Error, "{}", errors);
        }

        evoLog(PrintSeverity::Debug, "Shape Count: {}", shapes.size());
        evoLog(PrintSeverity::Debug, "Index Count: {}", shapes[0].mesh.indices.size());

        std::unordered_map<Vertex, uint32_t> unique_vertices {};
        index_buffer->clear();
        vertex_buffer->clear();

        for (int i = 0; i < shapes.size(); i++) {
            tinyobj::shape_t* shape = &shapes[i];
            tinyobj::mesh_t* mesh = &shape->mesh;

            for (int j = 0; j < mesh->indices.size(); j++) {
                glm::vec3 position = {
                    attributes.vertices[mesh->indices[j].vertex_index * 3],
                    attributes.vertices[mesh->indices[j].vertex_index * 3 + 1],
                    attributes.vertices[mesh->indices[j].vertex_index * 3 + 2],
                };

                glm::vec3 normal = {
                    attributes.normals[mesh->indices[j].normal_index * 3],
                    attributes.normals[mesh->indices[j].normal_index * 3 + 1],
                    attributes.normals[mesh->indices[j].normal_index * 3 + 2],
                };
                glm::vec2 tex_coord = {
                    attributes.texcoords[mesh->indices[j].texcoord_index * 2],
                    1.0f - attributes.texcoords[mesh->indices[j].texcoord_index * 2 + 1],
                };

                Vertex vertex = {position, normal, tex_coord};

                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertex_buffer->size());
                    vertex_buffer->push_back(vertex);
                }

                index_buffer->push_back(unique_vertices[vertex]);
            }
        }
    }
} // namespace model
