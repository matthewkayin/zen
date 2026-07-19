#include "obj.h"

#include "core/asserts.h"
#include "core/logger.h"
#include "util/string.h"
#include <unordered_map>

enum ObjEntryType {
    OBJ_ENTRY_TYPE_COMMENT,
    OBJ_ENTRY_TYPE_OBJECT,
    OBJ_ENTRY_TYPE_MTL_LIB,
    OBJ_ENTRY_TYPE_MTL_USE,
    OBJ_ENTRY_TYPE_SMOOTH_SHADING,
    OBJ_ENTRY_TYPE_POSITION,
    OBJ_ENTRY_TYPE_NORMAL,
    OBJ_ENTRY_TYPE_TEX_COORD,
    OBJ_ENTRY_TYPE_FACE
};

struct ObjVertex {
    uint32_t position_index;
    uint32_t normal_index;
    uint32_t tex_coord_index;

    bool operator==(const ObjVertex& other) const {
        return position_index == other.position_index &&
            normal_index == other.normal_index &&
            tex_coord_index == other.tex_coord_index;
    }
};

namespace std {
    template <>
    struct hash<ObjVertex> {
        size_t operator()(const ObjVertex& vertex) const {
            size_t h = 17;
            h *= 31 + std::hash<int>()(vertex.position_index);
            h *= 31 + std::hash<int>()(vertex.normal_index);
            h *= 31 + std::hash<int>()(vertex.tex_coord_index);
            return h;
        }
    };
}

ObjEntryType obj_entry_type_from_str(const char* str);

bool obj_load(const char* path, std::vector<Vertex3d>* out_vertices, std::vector<uint32_t>* out_indices) {
    FILE* file = fopen(path, "r");
    if (!file) {
        log_error("Failed to open obj file %s.", path);
        return false;
    }

    std::vector<vec3> positions;
    std::vector<vec3> normals;
    std::vector<vec2> tex_coords;
    std::vector<ObjVertex> vertices;

    const uint32_t MAX_WORD_COUNT = 8;
    char* words[MAX_WORD_COUNT];
    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        uint32_t word_count;
        string_split(line, ' ', MAX_WORD_COUNT, words, &word_count);
        if (word_count == 0) {
            continue;
        }

        ObjEntryType entry_type = obj_entry_type_from_str(words[0]);
        switch (entry_type) {
            case OBJ_ENTRY_TYPE_POSITION: {
                if (word_count < 4) {
                    log_error("Improper position in %s: %s.", path, line);
                    return false;
                }
                positions.push_back(vec3(
                    std::stof(words[1]),
                    std::stof(words[2]),
                    std::stof(words[3])));
                break;
            }
            case OBJ_ENTRY_TYPE_NORMAL: {
                if (word_count < 4) {
                    log_error("Improper normal in %s: %s.", path, line);
                    return false;
                }
                normals.push_back(vec3(
                    std::stof(words[1]),
                    std::stof(words[2]),
                    std::stof(words[3])));
                break;
            }
            case OBJ_ENTRY_TYPE_TEX_COORD: {
                if (word_count < 3) {
                    log_error("Improper tex_coord in %s: %s.", path, line);
                    return false;
                }
                tex_coords.push_back(vec2(
                    std::stof(words[1]),
                    std::stof(words[2])));
                break;
            }
            case OBJ_ENTRY_TYPE_FACE: {
                if (word_count < 4) {
                    log_error("Improper face in %s: %s.", path, line);
                    return false;
                }

                for (uint32_t index = 0; index < 3; index++) {
                    char* index_strs[3];
                    uint32_t index_str_count;
                    string_split(words[index], '/', 3, index_strs, &index_str_count);
                    if (index_str_count != 3) {
                        log_error("Improper face vertex in %s: %s.", path, words[index]);
                        return false;
                    }

                    vertices.push_back({
                        .position_index = (uint32_t)std::stoul(index_strs[0]),
                        .tex_coord_index = (uint32_t)std::stoul(index_strs[1]),
                        .normal_index = (uint32_t)std::stoul(index_strs[2])
                    });
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    out_vertices->clear();
    out_indices->clear();
    out_indices->reserve(vertices.size());

    std::unordered_map<ObjVertex, uint32_t> vertex_indices;
    for (const ObjVertex& vertex : vertices) {
        auto index_it = vertex_indices.find(vertex);
        if (index_it == vertex_indices.end()) {
            out_vertices->push_back({
                .position = positions[vertex.position_index],
                .normal = normals[vertex.normal_index],
                .tex_coord = tex_coords[vertex.tex_coord_index]
            });
            vertex_indices[vertex] = (uint32_t)out_vertices->size() - 1U;
        }

        out_indices->push_back(vertex_indices[vertex]);
    }

    fclose(file);
    return true;
}

ObjEntryType obj_entry_type_from_str(const char* str) {
    if (strcmp(str, "#") == 0) {
        return OBJ_ENTRY_TYPE_COMMENT;
    }
    if (strcmp(str, "o") == 0) {
        return OBJ_ENTRY_TYPE_OBJECT;
    }
    if (strcmp(str, "mtllib") == 0) {
        return OBJ_ENTRY_TYPE_OBJECT;
    }
    if (strcmp(str, "usemtl") == 0) {
        return OBJ_ENTRY_TYPE_OBJECT;
    }
    if (strcmp(str, "s") == 0) {
        return OBJ_ENTRY_TYPE_SMOOTH_SHADING;
    }
    if (strcmp(str, "v") == 0) {
        return OBJ_ENTRY_TYPE_POSITION;
    }
    if (strcmp(str, "vn") == 0) {
        return OBJ_ENTRY_TYPE_NORMAL;
    }
    if (strcmp(str, "vt") == 0) {
        return OBJ_ENTRY_TYPE_TEX_COORD;
    }
    if (strcmp(str, "f") == 0) {
        return OBJ_ENTRY_TYPE_FACE;
    }

    log_warn("Unrecognized obj entry %s.", str);
    ZEN_ASSERT(false);
    return OBJ_ENTRY_TYPE_COMMENT;
}
