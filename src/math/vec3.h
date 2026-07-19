#pragma once

#include "math/constants.h"
#include <cmath>

struct vec3 {
    float x;
    float y;
    float z;

    vec3() = default;
    vec3(float v) {
        x = v;
        y = v;
        z = v;
    }
    vec3(float p_x, float p_y, float p_z) {
        x = p_x;
        y = p_y;
        z = p_z;
    }

    static vec3 up() { return vec3(0.0f, 1.0f, 0.0f); }
    static vec3 right() { return vec3(1.0f, 0.0f, 0.0f); }
    static vec3 down() { return vec3(0.0f, -1.0f, 0.0f); }
    static vec3 left() { return vec3(-1.0f, 0.0f, 0.0f); }
    static vec3 forward() { return vec3(0.0f, 0.0f, -1.0f); }
    static vec3 back() { return vec3(0.0f, 0.0f, 1.0f); }

    bool operator==(const vec3& other) const {
        if (std::abs(x - other.x) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(y - other.y) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(z - other.z) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        return true;
    }
    bool operator!=(const vec3& other) const { return !((*this) == other); }

    vec3 operator+(const vec3& other) const {
        return vec3(x + other.x, y + other.y, z + other.z);
    }
    vec3& operator+=(const vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    vec3 operator-(const vec3& other) const {
        return vec3(x - other.x, y - other.y, z - other.z);
    }
    vec3& operator-=(const vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    vec3 operator*(float scaler) const {
        return vec3(x * scaler, y * scaler, z * scaler);
    }
    vec3& operator*=(float scaler) {
        x *= scaler;
        y *= scaler;
        z *= scaler;
        return *this;
    }

    vec3 operator/(float scaler) const {
        return vec3(x / scaler, y / scaler, z / scaler);
    }
    vec3& operator/=(float scaler) {
        x /= scaler;
        y /= scaler;
        z /= scaler;
        return *this;
    }

    float length_squared() const { return (x * x) + (y * y) + (z * z); }
    float length() const { return std::sqrt(length_squared()); }

    void normalize() {
        const float _length = length();
        x /= _length;
        y /= _length;
        z /= _length;
    }
    vec3 normalized() const { return (*this) / length(); }

    static float distance(const vec3& a, const vec3& b) {
        return (a - b).length();
    }

    static float dot(const vec3& a, const vec3& b) {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    }
    static vec3 cross(const vec3& a, const vec3& b) {
        return vec3((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z),
                    (a.x * b.y) - (a.y * b.x));
    }
};
