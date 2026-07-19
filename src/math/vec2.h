#pragma once

#include "math/constants.h"
#include <cmath>

struct vec2 {
    float x;
    float y;

    vec2() = default;
    vec2(float v) {
        x = v;
        y = v;
    }
    vec2(float p_x, float p_y) {
        x = p_x;
        y = p_y;
    }

    static vec2 up() { return vec2(0.0f, 1.0f); }
    static vec2 right() { return vec2(1.0f, 0.0f); }
    static vec2 down() { return vec2(0.0f, -1.0f); }
    static vec2 left() { return vec2(-1.0f, 0.0f); }

    bool operator==(const vec2& other) const {
        if (std::abs(x - other.x) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        if (std::abs(y - other.y) > ZEN_FLOAT_EPSILON) {
            return false;
        }
        return true;
    }
    bool operator!=(const vec2& other) const { return !((*this) == other); }

    vec2 operator+(const vec2& other) const {
        return vec2(x + other.x, y + other.y);
    }
    vec2& operator+=(const vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    vec2 operator-(const vec2& other) const {
        return vec2(x - other.x, y - other.y);
    }
    vec2& operator-=(const vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    vec2 operator*(float scaler) const { return vec2(x * scaler, y * scaler); }
    vec2& operator*=(float scaler) {
        x *= scaler;
        y *= scaler;
        return *this;
    }

    vec2 operator/(float scaler) const { return vec2(x / scaler, y / scaler); }
    vec2& operator/=(float scaler) {
        x /= scaler;
        y /= scaler;
        return *this;
    }

    float length_squared() const { return (x * x) + (y * y); }
    float length() const { return std::sqrt(length_squared()); }

    void normalize() {
        const float _length = length();
        x /= _length;
        y /= _length;
    }
    vec2 normalized() const { return (*this) / length(); }

    static float distance(const vec2& a, const vec2& b) {
        return (a - b).length();
    }
};
