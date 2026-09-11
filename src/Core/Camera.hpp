#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

class OrbitCamera
{
public:
    glm::vec3 target{ 0.0f, 0.0f, 0.0f };
    float distance = 100.0f;
    float yaw = 45.0f;
    float pitch = 25.0f;

    glm::vec3 getEyePosition() const;
    glm::mat4 getViewMatrix() const;
    void handleInputs(bool isHovered, bool isActive);
    void frameBounds(const glm::vec3& minBound, const glm::vec3& maxBound);
};

