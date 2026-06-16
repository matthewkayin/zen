#pragma once

#include "math/mat4.h"
#include <cmath>

struct quat {
    float x;
    float y;
    float z;
    float w;

    quat() {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 0.0f;
    }

    quat(float p_x, float p_y, float p_z, float p_w) {
        x = p_x;
        y = p_y;
        z = p_z;
        w = p_w;
    }

    static quat identity() {
        return quat(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float normal() const {
        return std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
    }

    void normalize() {
        float _normal = normal();
        x /= _normal;
        y /= _normal;
        z /= _normal;
        w /= _normal;
    }

    quat normalized() const {
        float _normal = normal();
        return quat(
            x / _normal,
            y / _normal,
            z / _normal,
            w / _normal);
    }

    quat conjugate() const {
        return quat(-x, -y, -z, w);
    }

    quat inverse() const {
        return conjugate().normalized();
    }

    quat operator*(const quat& other) const {
        quat result;

        result.x =
            (x * other.w) +
            (y * other.z) -
            (z * other.y) +
            (w * other.x);
        result.y =
            (-x * other.z) +
            (y * other.w) +
            (z * other.x) +
            (w * other.y);
        result.z =
            (x * other.y) -
            (y * other.x) +
            (z * other.w) +
            (w * other.z);
        result.w =
            (-x * other.x) -
            (y * other.y) -
            (z * other.z) +
            (w * other.w);

        return result;
    }

    static float dot(const quat& a, const quat& b) {
        return
            (a.x * b.x) +
            (a.y * b.y) +
            (a.z * b.z) +
            (a.w * b.w);
    }

    mat4 to_mat4() const {
        mat4 result = mat4::identity();
        quat n = normalized();

        result.data[0] = 1.0f - (2.0f * n.y * n.y) - (2.0f * n.z * n.z);
        result.data[1] = (2.0f * n.x * n.y) - (2.0f * n.z * n.w);
        result.data[2] = 2.0f * n.x * n.z + 2.0f * n.y * n.w;

        result.data[4] = 2.0f * n.x * n.y + 2.0f * n.z * n.w;
        result.data[5] = 1.0f - 2.0f * n.x * n.x - 2.0f * n.z * n.z;
        result.data[6] = 2.0f * n.y * n.z - 2.0f * n.x * n.w;

        result.data[8] = 2.0f * n.x * n.z - 2.0f * n.y * n.w;
        result.data[9] = 2.0f * n.y * n.z + 2.0f * n.x * n.w;
        result.data[10] = 1.0f - 2.0f * n.x * n.x - 2.0f * n.y * n.y;

        return result;
    }

    mat4 to_rotation_matrix(vec3 center) const {
        mat4 result;

        result.data[0] = (x * x) - (y * y) - (z * z) + (w * w);
        result.data[1] = 2.0f * ((x * y) + (z * w));
        result.data[2] = 2.0f * ((x * z) - (y * w));
        result.data[3] = center.x - center.x * result.data[0] - center.y * result.data[1] - center.z * result.data[2];

        result.data[4] = 2.0f * ((x * y) - (z * w));
        result.data[5] = -(x * x) + (y * y) - (z * z) + (w * w);
        result.data[6] = 2.0f * ((y * z) + (x * w));
        result.data[7] = center.y - center.x * result.data[4] - center.y * result.data[5] - center.z * result.data[6];

        result.data[8] = 2.0f * ((x * z) + (y * w));
        result.data[9] = 2.0f * ((y * z) - (x * w));
        result.data[10] = -(x * x) - (y * y) + (z * z) + (w * w);
        result.data[11] = center.z - center.x * result.data[8] - center.y * result.data[9] - center.z * result.data[10];

        result.data[12] = 0.0f;
        result.data[13] = 0.0f;
        result.data[14] = 0.0f;
        result.data[15] = 1.0f;

        return result;
    }

    static quat from_axis_angle(vec3 axis, float angle, bool normalize) {
        float sin_angle = sin(0.5f * angle);
        float cos_angle = cos(0.5f * angle);

        quat result = quat(sin_angle * axis.x, sin_angle * axis.y, sin_angle * axis.z, cos_angle);
        if (normalize) {
            result.normalize();
        }

        return result;
    }

    quat slerp(const quat& dest, float percent) {
        quat result;
        quat from = normalized();
        quat to = dest.normalized();
        float _dot = quat::dot(from, to);

        if (_dot < 0.0f) {
            to.x = -to.x;
            to.y = -to.y;
            to.z = -to.z;
            to.w = -to.w;
            _dot = -_dot;
        }

        const float DOT_THRESHOLD = 0.9995f;
        if (_dot > DOT_THRESHOLD) {
            // If inputs are too close to safely acos, lerp and normalize the result
            return quat(
                from.x + ((to.x - from.x) * percent),
                from.y + ((to.y - from.y) * percent),
                from.z + ((to.z - from.z) * percent),
                from.w + ((to.w - from.w) * percent)).normalized();
        }

        float theta_0 = acos(_dot);
        float theta = theta_0 * percent;
        float sin_theta = sin(theta);
        float sin_theta_0 = sin(theta_0);
        float s0 = cos(theta) - (_dot * (sin_theta / sin_theta_0));
        float s1 = sin_theta / sin_theta_0;

        return quat(
            (from.x * s0) + (to.x * s1),
            (from.y * s0) + (to.y * s1),
            (from.z * s0) + (to.z * s1),
            (from.w * s0) + (to.w * s1));
    }
};
