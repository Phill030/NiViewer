#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include "Core/SceneTypes.hpp"
#include "Core/Shader.hpp"
#include "Core/Camera.hpp"
#include "Core/Framebuffer.hpp"

struct RenderSettings
{
    bool wireframe = false;
    bool culling = false;
    bool rotateZtoY = true;
    bool useNormalsColor = false;
    bool enableTextures = true;
    bool showHidden = false;
    float clearColor[4] = { 0.12f, 0.13f, 0.15f, 1.0f };
    glm::vec3 lightDir = glm::vec3(-0.4f, -1.0f, -0.6f);
};

struct RenderUniforms
{
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint lightDir = -1;
    GLint useNormalsColor = -1;
    GLint diffuseTexture = -1;
    GLint glowTexture = -1;
    GLint enableTextures = -1;
    GLint hasTexture = -1;
    GLint hasGlowTexture = -1;
    GLint meshColor = -1;
    GLint isAdditive = -1;
    GLint alphaCutoff = -1;

    void init(GLuint prog);
};

class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer() = default;

    bool init();
    void render(const SceneData& scene, const OrbitCamera& camera, Framebuffer& fbo, const RenderSettings& settings);

    Shader& getShader() { return shader; }

private:
    Shader shader;
    RenderUniforms uniforms;
};
