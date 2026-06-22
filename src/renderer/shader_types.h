#pragma once

#include "math/mat4.h"
#include <cstdint>

struct GlobalUniformObject {
    mat4 projection;
    mat4 view;
    uint8_t padding[128];
};
static_assert(sizeof(GlobalUniformObject) == 256ULL, "Some Nvidia cards require this to be exactly 256 bytes");
