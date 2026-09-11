#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Vertex
{
    glm::vec3 position{ 0.0f };
    glm::vec3 normal{ 0.0f };
    glm::vec2 texCoords{ 0.0f };
    glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct RenderableMesh
{
    std::string name;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    unsigned int indexCount = 0;
    unsigned int vertexCount = 0;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 baseColor = glm::vec3(0.75f, 0.78f, 0.82f);
    glm::vec3 worldCenter{ 0.0f };
    bool visible = true;
    std::string texturePath;
    int embeddedPixelDataIndex = -1;
    bool hasTexture = false;
    GLuint textureId = 0;
    bool isHidden = false;
    bool hiddenByFlag = false;
    bool hiddenByMissingProperty = false;

    // Alpha / Blending properties
    bool hasAlphaBlend = false;
    GLenum srcBlend = 0x0302;  // GL_SRC_ALPHA
    GLenum destBlend = 0x0303; // GL_ONE_MINUS_SRC_ALPHA
    bool hasAlphaTest = false;
    uint32_t alphaTestFunc = 0;
    float alphaTestRef = 0.0f;
    bool isAdditive = false;
    bool hasVertexColors = false;

    // Cached UI strings to avoid per-frame allocations
    std::string textureFilename;
    std::string hiddenTag;
    std::string tooltipReason;

    void destroy() {
        if (vao != 0) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (ebo != 0) { glDeleteBuffers(1, &ebo); ebo = 0; }
        indexCount = 0;
        vertexCount = 0;
        texturePath.clear();
        textureFilename.clear();
        hiddenTag.clear();
        tooltipReason.clear();
        embeddedPixelDataIndex = -1;
        hasTexture = false;
        textureId = 0;
        isHidden = false;
        hiddenByFlag = false;
        hiddenByMissingProperty = false;
        hasAlphaBlend = false;
        hasAlphaTest = false;
        isAdditive = false;
        hasVertexColors = false;
    }
};

struct SceneNodeInfo
{
    std::string name;
    std::string typeName;
    int blockIndex = -1;
    glm::vec3 translation{ 0.0f };
    float scale = 1.0f;
    uint16_t flags = 0;
    bool isHidden = false;
    std::vector<SceneNodeInfo> children;
};

struct NifBlockEntry
{
    int index = 0;
    std::string typeName;
    std::string name;
    std::string details;
    uint32_t sizeBytes = 0;
};

struct SceneData
{
    std::vector<RenderableMesh> meshes;
    glm::vec3 minBound{ 1e9f, 1e9f, 1e9f };
    glm::vec3 maxBound{ -1e9f, -1e9f, -1e9f };
    glm::vec3 visibleMinBound{ 1e9f, 1e9f, 1e9f };
    glm::vec3 visibleMaxBound{ -1e9f, -1e9f, -1e9f };
    std::vector<SceneNodeInfo> rootNodes;
    std::vector<NifBlockEntry> allBlocks;
    unsigned int totalTriangles = 0;
    unsigned int totalVertices = 0;
    size_t hiddenMeshCount = 0;

    void clear() {
        for (auto& mesh : meshes) {
            mesh.destroy();
        }
        meshes.clear();
        rootNodes.clear();
        allBlocks.clear();
        minBound = glm::vec3(1e9f);
        maxBound = glm::vec3(-1e9f);
        visibleMinBound = glm::vec3(1e9f);
        visibleMaxBound = glm::vec3(-1e9f);
        totalTriangles = 0;
        totalVertices = 0;
        hiddenMeshCount = 0;
    }

    glm::vec3 getEffectiveMinBound() const {
        if (visibleMinBound.x <= visibleMaxBound.x) return visibleMinBound;
        return minBound;
    }

    glm::vec3 getEffectiveMaxBound() const {
        if (visibleMinBound.x <= visibleMaxBound.x) return visibleMaxBound;
        return maxBound;
    }
};
