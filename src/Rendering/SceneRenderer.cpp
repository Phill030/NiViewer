#include "SceneRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <algorithm>

static const char* vertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 VertexColor;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    VertexColor = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

static const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 VertexColor;

uniform vec3 lightDir;
uniform vec3 meshColor;
uniform bool useNormalsColor;
uniform sampler2D diffuseTexture;
uniform sampler2D glowTexture;
uniform bool hasTexture;
uniform bool hasGlowTexture;
uniform bool enableTextures;
uniform bool isAdditive;
uniform float alphaCutoff;

void main() {
    if (useNormalsColor) {
        vec3 norm = normalize(Normal);
        FragColor = vec4(norm * 0.5 + 0.5, 1.0);
        return;
    }

    vec4 texColor = vec4(1.0);
    if (hasTexture && enableTextures) {
        texColor = texture(diffuseTexture, TexCoords);
    }

    vec4 finalColor = texColor * VertexColor;

    float effectiveCutoff = max(alphaCutoff, (hasTexture && enableTextures) ? 0.005 : 0.0);
    if (effectiveCutoff > 0.0 && finalColor.a < effectiveCutoff) {
        discard;
    }

    vec3 glow = vec3(0.0);
    if (hasGlowTexture && enableTextures) {
        glow = texture(glowTexture, TexCoords).rgb;
    }

    if (isAdditive) {
        FragColor = vec4(finalColor.rgb + glow, finalColor.a);
    } else {
        vec3 norm = normalize(Normal);
        vec3 light = normalize(-lightDir);
        float diff = max(dot(norm, light), 0.0);
        float diffBack = max(dot(-norm, light), 0.0) * 0.4;
        float lightIntensity = max(diff, diffBack);

        vec3 base = (hasTexture && enableTextures) ? finalColor.rgb : (meshColor * VertexColor.rgb);
        vec3 ambient = base * 0.35;
        vec3 diffuse = base * lightIntensity * 0.65;
        FragColor = vec4(ambient + diffuse + glow, finalColor.a);
    }
}
)";

void RenderUniforms::init(GLuint prog) {
    model = glGetUniformLocation(prog, "model");
    view = glGetUniformLocation(prog, "view");
    projection = glGetUniformLocation(prog, "projection");
    lightDir = glGetUniformLocation(prog, "lightDir");
    useNormalsColor = glGetUniformLocation(prog, "useNormalsColor");
    diffuseTexture = glGetUniformLocation(prog, "diffuseTexture");
    glowTexture = glGetUniformLocation(prog, "glowTexture");
    enableTextures = glGetUniformLocation(prog, "enableTextures");
    hasTexture = glGetUniformLocation(prog, "hasTexture");
    hasGlowTexture = glGetUniformLocation(prog, "hasGlowTexture");
    meshColor = glGetUniformLocation(prog, "meshColor");
    isAdditive = glGetUniformLocation(prog, "isAdditive");
    alphaCutoff = glGetUniformLocation(prog, "alphaCutoff");
}

SceneRenderer::SceneRenderer() = default;

bool SceneRenderer::init() {
    if (!shader.init(vertexShaderSource, fragmentShaderSource)) {
        return false;
    }
    uniforms.init(shader.id);
    return true;
}

void SceneRenderer::render(const SceneData& scene, const OrbitCamera& camera, Framebuffer& fbo, const RenderSettings& settings) {
    fbo.bind();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    if (settings.culling) {
        glEnable(GL_CULL_FACE);
    }
    else {
        glDisable(GL_CULL_FACE);
    }

    if (settings.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glClearColor(settings.clearColor[0], settings.clearColor[1], settings.clearColor[2], settings.clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (fbo.width <= 0 || fbo.height <= 0) {
        fbo.unbind();
        return;
    }

    shader.use();

    float aspect = static_cast<float>(fbo.width) / static_cast<float>(fbo.height);
    float farClip = std::max(100000.0f, camera.distance * 10.0f);
    float nearClip = std::max(0.1f, camera.distance * 0.0001f);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, nearClip, farClip);
    glm::mat4 view = camera.getViewMatrix();

    // Set frame-level uniforms directly via cached locations
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniform3fv(uniforms.lightDir, 1, glm::value_ptr(settings.lightDir));
    glUniform1i(uniforms.useNormalsColor, settings.useNormalsColor ? 1 : 0);
    glUniform1i(uniforms.diffuseTexture, 0);
    glUniform1i(uniforms.glowTexture, 1);
    glUniform1i(uniforms.enableTextures, settings.enableTextures ? 1 : 0);

    // Base transform (e.g. rotate Z-up to Y-up)
    glm::mat4 baseModel(1.0f);
    if (settings.rotateZtoY) {
        baseModel = glm::rotate(baseModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    // Partition meshes into persistent vectors (zero heap allocations per frame)
    struct TransparentEntry
    {
        const RenderableMesh* mesh;
        float viewDepth;
    };
    static std::vector<const RenderableMesh*> opaqueMeshes;
    static std::vector<TransparentEntry> transparentSorted;
    opaqueMeshes.clear();
    transparentSorted.clear();

    for (const auto& mesh : scene.meshes) {
        if (!mesh.visible) continue;
        if (mesh.isHidden && !settings.showHidden) continue;
        if (mesh.hasAlphaBlend) {
            glm::vec3 worldPos = settings.rotateZtoY ? glm::vec3(baseModel * glm::vec4(mesh.worldCenter, 1.0f)) : mesh.worldCenter;
            glm::vec4 viewPos = view * glm::vec4(worldPos, 1.0f);
            float viewDepth = -viewPos.z; // In OpenGL view space, depth along camera forward axis is -Z
            transparentSorted.push_back({ &mesh, viewDepth });
        }
        else {
            opaqueMeshes.push_back(&mesh);
        }
    }

    // State cache to avoid redundant OpenGL driver calls
    GLuint boundTexId = 0;
    GLuint boundGlowTexId = 0;
    int currentHasTexture = -1;
    int currentHasGlowTexture = -1;
    float currentAlphaCutoff = -999.0f;
    int currentIsAdditive = -1;
    bool currentDepthTest = true;
    GLenum currentDepthFunc = GL_LEQUAL;
    bool currentDepthWrite = true;

    auto applyMeshState = [&](const RenderableMesh& mesh, float targetCutoff, int targetAdditive) {
        glm::mat4 modelMatrix = baseModel * mesh.transform;
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, glm::value_ptr(modelMatrix));

        bool texActive = (settings.enableTextures && mesh.hasTexture && mesh.textureId != 0);
        int hasTexInt = texActive ? 1 : 0;
        if (hasTexInt != currentHasTexture) {
            glUniform1i(uniforms.hasTexture, hasTexInt);
            currentHasTexture = hasTexInt;
        }

        if (texActive) {
            if (mesh.textureId != boundTexId) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.textureId);
                boundTexId = mesh.textureId;
            }
        }
        else {
            glUniform3fv(uniforms.meshColor, 1, glm::value_ptr(mesh.baseColor));
        }

        bool glowActive = (settings.enableTextures && mesh.hasGlowTexture && mesh.glowTextureId != 0);
        int hasGlowInt = glowActive ? 1 : 0;
        if (hasGlowInt != currentHasGlowTexture) {
            glUniform1i(uniforms.hasGlowTexture, hasGlowInt);
            currentHasGlowTexture = hasGlowInt;
        }

        if (glowActive) {
            if (mesh.glowTextureId != boundGlowTexId) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mesh.glowTextureId);
                boundGlowTexId = mesh.glowTextureId;
            }
        }

        if (targetCutoff != currentAlphaCutoff) {
            glUniform1f(uniforms.alphaCutoff, targetCutoff);
            currentAlphaCutoff = targetCutoff;
        }

        if (targetAdditive != currentIsAdditive) {
            glUniform1i(uniforms.isAdditive, targetAdditive);
            currentIsAdditive = targetAdditive;
        }

        if (mesh.depthTest != currentDepthTest) {
            if (mesh.depthTest) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);
            currentDepthTest = mesh.depthTest;
        }
        if (mesh.depthTest && mesh.depthFunc != currentDepthFunc) {
            glDepthFunc(mesh.depthFunc);
            currentDepthFunc = mesh.depthFunc;
        }

        bool targetDepthWrite = mesh.depthWrite && (targetAdditive == 0);
        if (targetDepthWrite != currentDepthWrite) {
            glDepthMask(targetDepthWrite ? GL_TRUE : GL_FALSE);
            currentDepthWrite = targetDepthWrite;
        }

        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    };

    // Pass 1: Opaque meshes (depth write enabled, blending disabled)
    glDisable(GL_BLEND);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    currentDepthWrite = true;

    for (const auto* m : opaqueMeshes) {
        float cutoff = m->hasAlphaTest ? std::max(m->alphaTestRef, 0.001f) : 0.0f;
        applyMeshState(*m, cutoff, 0);
    }

    // Pass 2: Transparent / blended meshes (sorted back-to-front)
    if (!transparentSorted.empty()) {
        std::sort(transparentSorted.begin(), transparentSorted.end(),
                  [](const TransparentEntry& a, const TransparentEntry& b) {
            return a.viewDepth > b.viewDepth;
        });

        glEnable(GL_BLEND);

        GLenum curSrc = 0, curDest = 0;
        bool cullingDisabledForAdditive = false;
        bool polyOffsetActive = false;

        for (const auto& entry : transparentSorted) {
            const auto* m = entry.mesh;

            if (m->srcBlend != curSrc || m->destBlend != curDest) {
                glBlendFunc(m->srcBlend, m->destBlend);
                curSrc = m->srcBlend;
                curDest = m->destBlend;
            }

            if (m->isAdditive) {
                if (!cullingDisabledForAdditive) {
                    glDisable(GL_CULL_FACE);
                    cullingDisabledForAdditive = true;
                }
            }
            else {
                if (cullingDisabledForAdditive) {
                    if (settings.culling) glEnable(GL_CULL_FACE);
                    else glDisable(GL_CULL_FACE);
                    cullingDisabledForAdditive = false;
                }
            }

            // Polygon offset for co-planar decals (e.g. ground textures with alpha blend but no alpha test)
            bool needsPolyOffset = (m->hasAlphaBlend && !m->hasAlphaTest && !m->isAdditive);
            if (needsPolyOffset != polyOffsetActive) {
                if (needsPolyOffset) {
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(-1.0f, -1.0f);
                }
                else {
                    glDisable(GL_POLYGON_OFFSET_FILL);
                }
                polyOffsetActive = needsPolyOffset;
            }

            float cutoff = m->hasAlphaTest ? m->alphaTestRef : 0.0f;
            applyMeshState(*m, cutoff, m->isAdditive ? 1 : 0);
        }

        if (polyOffsetActive) {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        // Restore culling, depth mask, depth test, and blend
        if (settings.culling) {
            glEnable(GL_CULL_FACE);
        }
        else {
            glDisable(GL_CULL_FACE);
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    fbo.unbind();
}
