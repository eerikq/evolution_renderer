#pragma once

#include "types/vertex.h"

namespace model {
    void Load(const char* filename, std::vector<Vertex>* vertex_buffer, std::vector<uint16_t>* index_buffer);
}
