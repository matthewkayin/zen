#pragma once

#include "math/vec2.h"

struct ivec2 {
    int x;
    int y;

    ivec2() {
        x = 0.0f;
        y = 0.0f;
    }
    ivec2(int v) {
        x = v;
        y = v;
    }
    ivec2(int p_x, int p_y) {
        x = p_x;
        y = p_y;
    }

    static ivec2 from_vec2(vec2 v) {
        return ivec2((int)v.x, (int)v.y);
    }
    vec2 to_vec2() const {
        return vec2((float)x, (float)y);
    }

    bool operator==(const ivec2& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const ivec2& other) const { return !((*this) == other); }

    ivec2 operator+(const ivec2& other) const {
        return ivec2(x + other.x, y + other.y);
    }
    ivec2& operator+=(const ivec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    ivec2 operator-(const ivec2& other) const {
        return ivec2(x - other.x, y - other.y);
    }
    ivec2& operator-=(const ivec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    ivec2 operator*(float scaler) const {
        return ivec2(x * scaler, y * scaler);
    }
    ivec2& operator*=(float scaler) {
        x *= scaler;
        y *= scaler;
        return *this;
    }

    ivec2 operator/(float scaler) const {
        return ivec2(x / scaler, y / scaler);
    }
    ivec2& operator/=(float scaler) {
        x /= scaler;
        y /= scaler;
        return *this;
    }

    float length_squared() const {
        return (x * x) + (y * y);
    }
};
