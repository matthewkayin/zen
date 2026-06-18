#pragma once

#include "math/constants.h"
#include "math/vec3.h"
#include <cmath>

struct vec4 {
    float x;
    float y;
    float z;
    float w;

    vec4() {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 0.0f;
    }
    vec4(float v) {
        x = v;
        y = v;
        z = v;
        w = v;
    }
    vec4(float p_x, float p_y, float p_z, float p_w) {
        x = p_x;
        y = p_y;
        z = p_z;
        w = p_w;
    }

    vec3 to_vec3() const { return vec3(x, y, z); }
    static vec4 from_vec3(const vec3& src, float p_w) {
        return vec4(src.x, src.y, src.z, p_w);
    }

    bool operator==(const vec4& other) const {
        if (std::abs(x - other.x) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(y - other.y) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(z - other.z) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(w - other.w) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        return true;
    }
    bool operator!=(const vec4& other) const { return !((*this) == other); }

    vec4 operator+(const vec4& other) const {
        return vec4(x + other.x, y + other.y, z + other.z, w + other.w);
    }
    vec4& operator+=(const vec4& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    vec4 operator-(const vec4& other) const {
        return vec4(x - other.x, y - other.y, z - other.z, w - other.w);
    }
    vec4& operator-=(const vec4& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    vec4 operator*(float scaler) const {
        return vec4(x * scaler, y * scaler, z * scaler, w * scaler);
    }
    vec4& operator*=(float scaler) {
        x *= scaler;
        y *= scaler;
        z *= scaler;
        w *= scaler;
        return *this;
    }

    vec4 operator/(float scaler) const {
        return vec4(x / scaler, y / scaler, z / scaler, w / scaler);
    }
    vec4& operator/=(float scaler) {
        x /= scaler;
        y /= scaler;
        z /= scaler;
        w /= scaler;
        return *this;
    }

    float length_squared() const {
        return (x * x) + (y * y) + (z * z) + (w * w);
    }
    float length() const { return std::sqrt(length_squared()); }

    void normalize() {
        const float _length = length();
        x /= _length;
        y /= _length;
        z /= _length;
        w /= _length;
    }
    vec4 normalized() const { return (*this) / length(); }

    static float dot(const vec4& a, const vec4& b) {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    }
};
