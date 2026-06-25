#pragma once

#include "math/vec3.h"
#include <cmath>
#include <cstring>

struct mat4 {
    float data[16];

    mat4() { memset(data, 0, sizeof(data)); }

    static mat4 identity() {
        mat4 m;
        m.data[0] = 1.0f;
        m.data[5] = 1.0f;
        m.data[10] = 1.0f;
        m.data[15] = 1.0f;
        return m;
    }

    mat4 operator*(const mat4& other) const {
        mat4 result = mat4::identity();
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                result.data[col + (row * 4)] =
                    data[0 + (row * 4)] * other.data[col + 0] +
                    data[1 + (row * 4)] * other.data[col + 4] +
                    data[2 + (row * 4)] * other.data[col + 8] +
                    data[3 + (row * 4)] * other.data[col + 12];
            }
        }

        return result;
    }

    static mat4 ortho(float left, float right, float top, float bottom, float near, float far) {
        mat4 result = mat4::identity();

        float lr = 1.0f / (left - right);
        float bt = 1.0f / (bottom - top);
        float nf = 1.0f / (near - far);

        result.data[0] = -2.0f * lr;
        result.data[5] = -2.0f * bt;
        result.data[10] = 2.0f * nf;

        result.data[12] = (left + right) * lr;
        result.data[13] = (top + bottom) * bt;
        result.data[14] = (far + near) * nf;

        return result;
    }

    static mat4 perspective(float fov_radians, float aspect_ratio, float near, float far) {
        float half_tan_fov = tanf(fov_radians * 0.5f);

        mat4 result;
        result.data[0] = 1.0f / (aspect_ratio * half_tan_fov);
        result.data[5] = 1.0f / half_tan_fov;
        result.data[10] = -((far + near) / (far - near));
        result.data[11] = -1.0f;
        result.data[14] = -((2.0f * far * near) / (far - near));

        return result;
    }

    static mat4 look_at(vec3 position, vec3 target, vec3 up) {
        mat4 result;
        vec3 z_axis(target.x - position.x, target.y - position.y, target.z - position.z);
        z_axis.normalize();

        vec3 x_axis = vec3::cross(z_axis, up).normalized();
        vec3 y_axis = vec3::cross(x_axis, z_axis);

        result.data[0] = x_axis.x;
        result.data[1] = y_axis.x;
        result.data[2] = -z_axis.x;
        result.data[3] = 0.0f;
        result.data[4] = x_axis.y;
        result.data[5] = y_axis.y;
        result.data[6] = -z_axis.y;
        result.data[7] = 0.0f;
        result.data[8] = x_axis.z;
        result.data[9] = y_axis.z;
        result.data[10] = -z_axis.z;
        result.data[11] = 0.0f;
        result.data[12] = -vec3::dot(x_axis, position);
        result.data[13] = -vec3::dot(y_axis, position);
        result.data[14] = vec3::dot(z_axis, position);
        result.data[15] = 1.0f;

        return result;
    }

    mat4 transposed() const {
        mat4 result = mat4::identity();

        result.data[0] = data[0];
        result.data[1] = data[4];
        result.data[2] = data[8];
        result.data[3] = data[12];
        result.data[4] = data[1];
        result.data[5] = data[5];
        result.data[6] = data[9];
        result.data[7] = data[13];
        result.data[8] = data[2];
        result.data[9] = data[6];
        result.data[10] = data[10];
        result.data[11] = data[14];
        result.data[12] = data[3];
        result.data[13] = data[7];
        result.data[14] = data[11];
        result.data[15] = data[15];

        return result;
    }

    mat4 inversed() const {
        float t0 = data[10] * data[15];
        float t1 = data[14] * data[11];
        float t2 = data[6] * data[15];
        float t3 = data[14] * data[7];
        float t4 = data[6] * data[11];
        float t5 = data[10] * data[7];
        float t6 = data[2] * data[15];
        float t7 = data[14] * data[3];
        float t8 = data[2] * data[11];
        float t9 = data[10] * data[3];
        float t10 = data[2] * data[7];
        float t11 = data[6] * data[3];
        float t12 = data[8] * data[13];
        float t13 = data[12] * data[9];
        float t14 = data[4] * data[13];
        float t15 = data[12] * data[5];
        float t16 = data[4] * data[9];
        float t17 = data[8] * data[5];
        float t18 = data[0] * data[13];
        float t19 = data[12] * data[1];
        float t20 = data[0] * data[9];
        float t21 = data[8] * data[1];
        float t22 = data[0] * data[5];
        float t23 = data[4] * data[1];

        mat4 result;

        result.data[0] = (t0 * data[5] + t3 * data[9] + t4 * data[13]) - (t1 * data[5] + t2 * data[9] + t5 * data[13]);
        result.data[1] = (t1 * data[1] + t6 * data[9] + t9 * data[13]) - (t0 * data[1] + t7 * data[9] + t8 * data[13]);
        result.data[2] = (t2 * data[1] + t7 * data[5] + t10 * data[13]) - (t3 * data[1] + t6 * data[5] + t11 * data[13]); result.data[3] = (t5 * data[1] + t8 * data[5] + t11 * data[9]) - (t4 * data[1] + t9 * data[5] + t10 * data[9]);

        float d = 1.0f / (data[0] * result.data[0] + data[4] * result.data[1] + data[8] * result.data[2] + data[12] * result.data[3]);

        result.data[0] = d * result.data[0];
        result.data[1] = d * result.data[1];
        result.data[2] = d * result.data[2];
        result.data[3] = d * result.data[3];
        result.data[4] = d * ((t1 * data[4] + t2 * data[8] + t5 * data[12]) - (t0 * data[4] + t3 * data[8] + t4 * data[12]));
        result.data[5] = d * ((t0 * data[0] + t7 * data[8] + t8 * data[12]) - (t1 * data[0] + t6 * data[8] + t9 * data[12]));
        result.data[6] = d * ((t3 * data[0] + t6 * data[4] + t11 * data[12]) - (t2 * data[0] + t7 * data[4] + t10 * data[12]));
        result.data[7] = d * ((t4 * data[0] + t9 * data[4] + t10 * data[8]) - (t5 * data[0] + t8 * data[4] + t11 * data[8]));
        result.data[8] = d * ((t12 * data[7] + t15 * data[11] + t16 * data[15]) - (t13 * data[7] + t14 * data[11] + t17 * data[15]));
        result.data[9] = d * ((t13 * data[3] + t18 * data[11] + t21 * data[15]) - (t12 * data[3] + t19 * data[11] + t20 * data[15]));
        result.data[10] = d * ((t14 * data[3] + t19 * data[7] + t22 * data[15]) - (t15 * data[3] + t18 * data[7] + t23 * data[15]));
        result.data[11] = d * ((t17 * data[3] + t20 * data[7] + t23 * data[11]) - (t16 * data[3] + t21 * data[7] + t22 * data[11]));
        result.data[12] = d * ((t14 * data[10] + t17 * data[14] + t13 * data[6]) - (t16 * data[14] + t12 * data[6] + t15 * data[10]));
        result.data[13] = d * ((t20 * data[14] + t12 * data[2] + t19 * data[10]) - (t18 * data[10] + t21 * data[14] + t13 * data[2]));
        result.data[14] = d * ((t18 * data[6] + t23 * data[14] + t15 * data[2]) - (t22 * data[14] + t14 * data[2] + t19 * data[6]));
        result.data[15] = d * ((t22 * data[10] + t16 * data[2] + t21 * data[6]) - (t20 * data[6] + t23 * data[10] + t17 * data[2]));

        return result;
    }

    static mat4 translation(vec3 position) {
        mat4 result = mat4::identity();
        result.data[12] = position.x;
        result.data[13] = position.y;
        result.data[14] = position.z;
        return result;
    }

    static mat4 scale(vec3 scale) {
        mat4 result = mat4::identity();
        result.data[0] = scale.x;
        result.data[5] = scale.y;
        result.data[10] = scale.z;
        return result;
    }

    static mat4 euler_x(float angle_radians) {
        mat4 result = mat4::identity();
        float cos_angle = cos(angle_radians);
        float sin_angle = sin(angle_radians);

        result.data[5] = cos_angle;
        result.data[6] = sin_angle;
        result.data[9] = -sin_angle;
        result.data[10] = cos_angle;

        return result;
    }

    static mat4 euler_y(float angle_radians) {
        mat4 result = mat4::identity();
        float cos_angle = cos(angle_radians);
        float sin_angle = sin(angle_radians);

        result.data[0] = cos_angle;
        result.data[2] = -sin_angle;
        result.data[8] = sin_angle;
        result.data[10] = cos_angle;

        return result;
    }

    static mat4 euler_z(float angle_radians) {
        mat4 result = mat4::identity();
        float cos_angle = cos(angle_radians);
        float sin_angle = sin(angle_radians);

        result.data[0] = cos_angle;
        result.data[1] = sin_angle;
        result.data[4] = -sin_angle;
        result.data[5] = cos_angle;

        return result;
    }

    static mat4 euler_xyz(float x_radians, float y_radians, float z_radians) {
        mat4 result_x = mat4::euler_x(x_radians);
        mat4 result_y = mat4::euler_y(y_radians);
        mat4 result_z = mat4::euler_z(z_radians);

        return (result_x * result_y) * result_z;
    }

    vec3 forward() const {
        return vec3(-data[2], -data[6], -data[10]).normalized();
    }

    vec3 backward() const {
        return vec3(data[2], data[6], data[10]).normalized();
    }

    vec3 up() const { return vec3(data[1], data[5], data[9]).normalized(); }

    vec3 down() const {
        return vec3(-data[1], -data[5], -data[9]).normalized();
    }

    vec3 left() const {
        return vec3(-data[0], -data[4], -data[8]).normalized();
    }

    vec3 right() const { return vec3(data[0], data[4], data[8]).normalized(); }
};
