#include "camera.h"

#include <algorithm>

vec3 Camera::get_position() const {
    return m_position;
}

void Camera::set_position(vec3 value) {
    m_position = value;
    m_is_dirty = true;
}

vec3 Camera::forward() const {
    return m_view.forward();
}

vec3 Camera::backward() const {
    return m_view.backward();
}

vec3 Camera::up() const {
    return m_view.up();
}

vec3 Camera::down() const {
    return m_view.down();
}

vec3 Camera::left() const {
    return m_view.left();
}

vec3 Camera::right() const {
    return m_view.right();
}

void Camera::move(vec3 velocity) {
    m_position += velocity;
    m_is_dirty = true;
}

void Camera::yaw(float amount) {
    m_euler_rotation.y += amount;
    m_is_dirty = true;
}

void Camera::pitch(float amount) {
    const float CAMERA_PITCH_LIMIT = 89.0f * ZEN_DEG_TO_RAD;
    m_euler_rotation.x = std::clamp(m_euler_rotation.x + amount, -CAMERA_PITCH_LIMIT, CAMERA_PITCH_LIMIT);
    m_is_dirty = true;
}

mat4 Camera::get_calculated_view() {
    if (m_is_dirty) {
        m_view = (mat4::euler_vec3(m_euler_rotation) * mat4::translation(m_position)).inversed();
        m_is_dirty = false;
    }
    return m_view;
}
