#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

#include <unordered_set>

class NiFile;
struct NiAvObject;
struct NiNode;

struct NiAlphaProperty;
struct NiZBufferProperty;

#include "Core/SceneTypes.hpp"

class MeshExtractor
{
public:
    static SceneData extractScene(NiFile& file);

private:
    static glm::mat4 getLocalTransform(const NiAvObject& avObj);
    static void uploadMeshToGPU(const std::string& name,
                                const std::vector<Vertex>& vertices,
                                const std::vector<uint32_t>& indices,
                                const glm::mat4& worldTransform,
                                const std::string& texturePath,
                                int embeddedPixelDataIndex,
                                const std::string& glowTexturePath,
                                int glowEmbeddedPixelDataIndex,
                                bool isHidden,
                                bool hiddenByFlag,
                                bool hiddenByMissingProperty,
                                const NiAlphaProperty* alphaProp,
                                const NiZBufferProperty* zbufProp,
                                bool hasVertexColors,
                                SceneData& outScene);
    static void traverseNode(NiFile& file, NiAvObject* obj, int blockIndex, const glm::mat4& parentTransform,
                             SceneData& outScene, SceneNodeInfo* parentNodeInfo,
                             std::unordered_set<const NiAvObject*>& visitedObjects,
                             const std::string& parentTexturePath = "",
                             int parentEmbeddedPixelDataIndex = -1,
                             const std::string& parentGlowTexturePath = "",
                             int parentGlowEmbeddedPixelDataIndex = -1,
                             bool parentHidden = false,
                             bool parentHasTexturingOrShaderLighting = false,
                             std::shared_ptr<NiAlphaProperty> parentAlphaProperty = nullptr,
                             std::shared_ptr<NiZBufferProperty> parentZBufferProperty = nullptr);
};
