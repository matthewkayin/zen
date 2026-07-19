#pragma once

#include "math/vertex3d.h"
#include <vector>

bool obj_load(const char* path, std::vector<Vertex3d>* out_vertices, std::vector<uint32_t>* out_indices);
