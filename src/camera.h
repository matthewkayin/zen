#pragma once

#include "math/mat4.h"

class Camera {
public:
    Camera() : m_is_dirty(true) {}

    vec3 get_position() const;
    void set_position(vec3 value);

    vec3 forward() const;
    vec3 backward() const;
    vec3 up() const;
    vec3 down() const;
    vec3 left() const;
    vec3 right() const;

    void move(vec3 velocity);
    void yaw(float amount);
    void pitch(float amount);

    mat4 get_calculated_view();
private:
    mat4 m_view;
    vec3 m_position;
    vec3 m_euler_rotation;
    bool m_is_dirty;
};
