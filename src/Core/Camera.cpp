#include "Camera.hpp"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

glm::vec3 OrbitCamera::getEyePosition() const {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    return target + glm::vec3(
        distance * std::cos(radPitch) * std::sin(radYaw),
        distance * std::sin(radPitch),
        distance * std::cos(radPitch) * std::cos(radYaw)
    );
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    return glm::lookAt(getEyePosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::handleInputs(bool isHovered, bool isActive) {
    ImGuiIO& io = ImGui::GetIO();

    // Zoom (Mouse Wheel) - when hovered over the viewport
    if (isHovered && io.MouseWheel != 0.0f) {
        float speed = std::max(distance * 0.1f, 1.0f);
        distance -= io.MouseWheel * speed;
        if (distance < 0.5f) distance = 0.5f;
    }

    // Orbit Rotation (Left Mouse Button Drag) - when canvas is active
    if (isActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
            yaw -= io.MouseDelta.x * 0.4f;
            pitch += io.MouseDelta.y * 0.4f;
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }
    }

    // Pan (Right Mouse Button Drag) - when canvas is active
    if (isActive && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
            float panSpeed = std::max(distance * 0.002f, 0.05f);

            // Compute right and up vectors relative to camera
            glm::vec3 forward = glm::normalize(target - getEyePosition());
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));

            target += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * panSpeed;
        }
    }
}

void OrbitCamera::frameBounds(const glm::vec3& minBound, const glm::vec3& maxBound) {
    if (!std::isfinite(minBound.x) || !std::isfinite(maxBound.x) ||
        !std::isfinite(minBound.y) || !std::isfinite(maxBound.y) ||
        !std::isfinite(minBound.z) || !std::isfinite(maxBound.z) ||
        std::abs(minBound.x) > 1e7f || std::abs(maxBound.x) > 1e7f ||
        std::abs(minBound.y) > 1e7f || std::abs(maxBound.y) > 1e7f ||
        std::abs(minBound.z) > 1e7f || std::abs(maxBound.z) > 1e7f) {
        target = glm::vec3(0.0f);
        distance = 100.0f;
        return;
    }
    target = (minBound + maxBound) * 0.5f;
    float radius = glm::length(maxBound - minBound) * 0.5f;
    if (radius < 1.0f || !std::isfinite(radius) || radius > 1e7f) radius = 100.0f;
    distance = radius * 2.2f;
    yaw = 45.0f;
    pitch = 25.0f;
}
