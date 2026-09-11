#include "MeshExtractor.hpp"
#include "Core/NiFile.hpp"
#include "Blocks/NiNode.hpp"
#include "Blocks/NiTriShape.hpp"
#include "Blocks/NiMesh.hpp"
#include "Blocks/NiSourceTexture.hpp"
#include "Blocks/Property/NiTexturingProperty.hpp"
#include "Blocks/Property/NiMaterialProperty.hpp"
#include "Blocks/Property/NiAlphaProperty.hpp"
#include "Blocks/NiDataStream.hpp"
#include "Blocks/Data/NiStringExtraData.hpp"
#include "Blocks/Data/NiTriShapeData.hpp"
#include "Blocks/DataStreamData/DataStreamPosition.hpp"
#include "Blocks/DataStreamData/DataStreamMorphPosition.hpp"
#include "Blocks/DataStreamData/DataStreamNormal.hpp"
#include "Blocks/DataStreamData/DataStreamTexCoord.hpp"
#include "Blocks/DataStreamData/DataStreamColor.hpp"
#include "Blocks/DataStreamData/DataStreamIndex.hpp"
#include "Blocks/Data/NiPixelData.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <unordered_set>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

static GLenum toGLBlendFunc(AlphaFunction func) {
    switch (func) {
        case AlphaFunction::ONE: return GL_ONE;
        case AlphaFunction::ZERO: return GL_ZERO;
        case AlphaFunction::SRC_COLOR: return GL_SRC_COLOR;
        case AlphaFunction::INV_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case AlphaFunction::DEST_COLOR: return GL_DST_COLOR;
        case AlphaFunction::INV_DEST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case AlphaFunction::SRC_ALPHA: return GL_SRC_ALPHA;
        case AlphaFunction::INV_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case AlphaFunction::DEST_ALPHA: return GL_DST_ALPHA;
        case AlphaFunction::INV_DEST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case AlphaFunction::SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
        default: return GL_SRC_ALPHA;
    }
}

glm::mat4 MeshExtractor::getLocalTransform(const NiAvObject& avObj) {
    // Translation
    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(avObj.translation.x, avObj.translation.y, avObj.translation.z));

    // Rotation (Matrix<3,3> is column-major: m[col][row])
    glm::mat4 R(1.0f);
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            R[col][row] = avObj.rotation.m[col][row];
        }
    }

    // Scale
    float s = (avObj.scale != 0.0f) ? avObj.scale : 1.0f;
    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(s));

    return T * R * S;
}

void MeshExtractor::uploadMeshToGPU(const std::string& name,
                                    const std::vector<Vertex>& vertices,
                                    const std::vector<uint32_t>& indices,
                                    const glm::mat4& worldTransform,
                                    const std::string& texturePath,
                                    int embeddedPixelDataIndex,
                                    bool isHidden,
                                    bool hiddenByFlag,
                                    bool hiddenByMissingProperty,
                                    const NiAlphaProperty* alphaProp,
                                    bool hasVertexColors,
                                    SceneData& outScene) {
    if (vertices.empty() || indices.empty()) return;

    RenderableMesh mesh;
    mesh.name = name.empty() ? "Mesh_" + std::to_string(outScene.meshes.size()) : name;
    mesh.transform = worldTransform;
    mesh.texturePath = texturePath;
    mesh.embeddedPixelDataIndex = embeddedPixelDataIndex;
    mesh.hasTexture = (!texturePath.empty() || embeddedPixelDataIndex >= 0);
    mesh.vertexCount = static_cast<unsigned int>(vertices.size());
    mesh.indexCount = static_cast<unsigned int>(indices.size());
    mesh.isHidden = isHidden;
    mesh.hiddenByFlag = hiddenByFlag;
    mesh.hiddenByMissingProperty = hiddenByMissingProperty;
    mesh.hasVertexColors = hasVertexColors;

    if (!texturePath.empty()) {
        try {
            mesh.textureFilename = fs::path(texturePath).filename().string();
        }
        catch (...) {
            mesh.textureFilename = texturePath;
        }
    }
    else if (embeddedPixelDataIndex >= 0) {
        mesh.textureFilename = "PixelData_" + std::to_string(embeddedPixelDataIndex);
    }

    if (mesh.isHidden) {
        outScene.hiddenMeshCount++;
        if (mesh.hiddenByFlag && mesh.hiddenByMissingProperty) {
            mesh.hiddenTag = " [Hidden: Flag & No Tex]";
            mesh.tooltipReason = "Reason: Node flag bit 0 (Hidden) | Missing texturing/shader lighting property";
        }
        else if (mesh.hiddenByFlag) {
            mesh.hiddenTag = " [Hidden: Flag]";
            mesh.tooltipReason = "Reason: Node flag bit 0 (Hidden)";
        }
        else {
            mesh.hiddenTag = " [Hidden: No Tex]";
            mesh.tooltipReason = "Reason: Missing texturing/shader lighting property";
        }
    }

    if (alphaProp) {
        mesh.hasAlphaBlend = alphaProp->alphaBlend();
        mesh.srcBlend = toGLBlendFunc(alphaProp->srcBlend());
        mesh.destBlend = toGLBlendFunc(alphaProp->destBlend());
        mesh.hasAlphaTest = alphaProp->alphaTest();
        mesh.alphaTestFunc = static_cast<uint32_t>(alphaProp->testFunc());
        mesh.alphaTestRef = alphaProp->testThreshold();
        mesh.isAdditive = (mesh.hasAlphaBlend && (mesh.destBlend == GL_ONE || mesh.srcBlend == GL_ONE));
    }

    if (mesh.hasAlphaBlend) {
        if (mesh.isAdditive) mesh.hiddenTag += " [Additive]";
        else mesh.hiddenTag += " [AlphaBlend]";
    }

    // Update scene bounding box (track both visible and total bounds)
    glm::vec3 meshMin(1e9f), meshMax(-1e9f);
    for (const auto& v : vertices) {
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z) ||
            std::abs(v.position.x) > 1e7f || std::abs(v.position.y) > 1e7f || std::abs(v.position.z) > 1e7f) {
            continue;
        }
        glm::vec4 worldPos = worldTransform * glm::vec4(v.position, 1.0f);
        if (!std::isfinite(worldPos.x) || !std::isfinite(worldPos.y) || !std::isfinite(worldPos.z) ||
            std::abs(worldPos.x) > 1e7f || std::abs(worldPos.y) > 1e7f || std::abs(worldPos.z) > 1e7f) {
            continue;
        }
        glm::vec3 wp(worldPos);
        meshMin = glm::min(meshMin, wp);
        meshMax = glm::max(meshMax, wp);

        if (!mesh.isHidden) {
            outScene.visibleMinBound = glm::min(outScene.visibleMinBound, wp);
            outScene.visibleMaxBound = glm::max(outScene.visibleMaxBound, wp);
        }
        outScene.minBound = glm::min(outScene.minBound, wp);
        outScene.maxBound = glm::max(outScene.maxBound, wp);
    }
    if (meshMin.x <= meshMax.x) {
        mesh.worldCenter = 0.5f * (meshMin + meshMax);
    }
    else {
        mesh.worldCenter = glm::vec3(worldTransform[3]);
    }

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // 0: Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // 1: Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // 2: TexCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    // 3: Color
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    glBindVertexArray(0);

    // Vary the mesh base color slightly to distinguish submeshes
    float hue = std::fmod(outScene.meshes.size() * 0.618033988749895f, 1.0f);
    mesh.baseColor = glm::vec3(0.6f + 0.3f * std::sin(hue * 6.28f),
                               0.6f + 0.3f * std::sin((hue + 0.33f) * 6.28f),
                               0.6f + 0.3f * std::sin((hue + 0.67f) * 6.28f));

    outScene.totalVertices += mesh.vertexCount;
    outScene.totalTriangles += mesh.indexCount / 3;
    outScene.meshes.push_back(mesh);
}

void MeshExtractor::traverseNode(NiFile& file, NiAvObject* obj, int blockIndex, const glm::mat4& parentTransform,
                                 SceneData& outScene, SceneNodeInfo* parentNodeInfo,
                                 std::unordered_set<const NiAvObject*>& visitedObjects,
                                 const std::string& parentTexturePath,
                                 int parentEmbeddedPixelDataIndex,
                                 bool parentHidden,
                                 bool parentHasTexturingOrShaderLighting,
                                 std::shared_ptr<NiAlphaProperty> parentAlphaProperty) {
    if (!obj) return;
    if (!visitedObjects.insert(obj).second) {
        return;
    }

    glm::mat4 localTransform = getLocalTransform(*obj);
    glm::mat4 currentTransform = parentTransform * localTransform;

    // Node flag bit 0: Hidden (least significant bit)
    bool nodeFlagHidden = (obj->flags & 0x0001) != 0;
    bool isHiddenByFlag = parentHidden || nodeFlagHidden;

    SceneNodeInfo nodeInfo;
    nodeInfo.name = obj->name.empty() ? "(unnamed)" : obj->name;
    nodeInfo.blockIndex = blockIndex;
    nodeInfo.translation = glm::vec3(obj->translation.x, obj->translation.y, obj->translation.z);
    nodeInfo.scale = obj->scale;
    nodeInfo.flags = obj->flags;
    nodeInfo.isHidden = isHiddenByFlag;

    if (blockIndex >= 0 && blockIndex < (int)file.header.blockTypeIndex.size()) {
        uint16_t tIdx = file.header.blockTypeIndex[blockIndex];
        if (tIdx < file.header.blockTypes.size()) {
            nodeInfo.typeName = file.header.blockTypes[tIdx];
        }
    }

    // Extract texture path, embedded pixel data, and alpha property
    std::string texturePath = parentTexturePath;
    int embeddedPixelDataIndex = parentEmbeddedPixelDataIndex;
    bool hasTexturingOrShaderLighting = parentHasTexturingOrShaderLighting;
    std::shared_ptr<NiAlphaProperty> alphaProperty = parentAlphaProperty;

    for (uint32_t propIdx : obj->properties) {
        if (propIdx < file.blocks.size() && file.blocks[propIdx]) {
            if (auto ap = dynamic_pointer_cast<NiAlphaProperty>(file.blocks[propIdx])) {
                alphaProperty = ap;
            }
            if (auto texProp = dynamic_pointer_cast<NiTexturingProperty>(file.blocks[propIdx])) {
                hasTexturingOrShaderLighting = true;
                if (texProp->hasBaseTexture && texProp->baseTexture) {
                    int srcIdx = texProp->baseTexture->source.value;
                    if (srcIdx >= 0 && srcIdx < (int)file.blocks.size() && file.blocks[srcIdx]) {
                        if (auto srcTex = dynamic_pointer_cast<NiSourceTexture>(file.blocks[srcIdx])) {
                            texturePath = srcTex->filePath;
                            if (srcTex->unknownLink >= 0 && srcTex->unknownLink < (int)file.blocks.size()) {
                                if (dynamic_pointer_cast<NiPixelData>(file.blocks[srcTex->unknownLink])) {
                                    embeddedPixelDataIndex = srcTex->unknownLink;
                                }
                            }
                        }
                    }
                }
            }
        }
        // Also check header block type name in case block type is BSShaderLightingProperty or similar
        if (propIdx < file.header.blockTypeIndex.size()) {
            uint16_t tIdx = file.header.blockTypeIndex[propIdx];
            if (tIdx < file.header.blockTypes.size()) {
                const std::string& tName = file.header.blockTypes[tIdx];
                if (tName == "NiTexturingProperty" ||
                    tName == "BSShaderLightingProperty" ||
                    tName == "BSLightingShaderProperty" ||
                    tName.find("TexturingProperty") != std::string::npos ||
                    tName.find("ShaderLightingProperty") != std::string::npos ||
                    tName.find("LightingShaderProperty") != std::string::npos) {
                    hasTexturingOrShaderLighting = true;
                }
            }
        }
    }

    if (!texturePath.empty() || embeddedPixelDataIndex >= 0) {
        hasTexturingOrShaderLighting = true;
    }

    if (auto node = dynamic_cast<NiNode*>(obj)) {
        if (nodeInfo.typeName.empty()) nodeInfo.typeName = "NiNode";
        for (const auto& childRef : node->children) {
            auto child = childRef.getReference(file);
            if (child) {
                traverseNode(file, child, childRef.value, currentTransform, outScene, &nodeInfo, visitedObjects,
                             texturePath, embeddedPixelDataIndex, isHiddenByFlag, hasTexturingOrShaderLighting,
                             alphaProperty);
            }
        }
    }
    else if (auto triShape = dynamic_cast<NiTriShape*>(obj)) {
        if (nodeInfo.typeName.empty()) nodeInfo.typeName = "NiTriShape";
        bool isHiddenByMissingProperty = !hasTexturingOrShaderLighting;
        bool isHidden = isHiddenByFlag || isHiddenByMissingProperty;

        auto triData = dynamic_cast<NiTriShapeData*>(triShape->data.getReference(file));
        if (triData && triData->hasVertices && triData->hasTriangles) {
            std::vector<Vertex> vertices;
            vertices.resize(triData->numVertices);

            for (size_t i = 0; i < triData->numVertices; ++i) {
                vertices[i].position = glm::vec3(triData->vertices[i].x, triData->vertices[i].y, triData->vertices[i].z);
                if (triData->hasNormals && i < triData->normals.size()) {
                    vertices[i].normal = glm::vec3(triData->normals[i].x, triData->normals[i].y, triData->normals[i].z);
                }
                else {
                    vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                if (!triData->uvSets.empty() && i < triData->uvSets.size()) {
                    vertices[i].texCoords = glm::vec2(triData->uvSets[i].u, triData->uvSets[i].v);
                }
                else {
                    vertices[i].texCoords = glm::vec2(0.0f);
                }
                if (triData->hasVertexColors && i < triData->vertexColors.size()) {
                    vertices[i].color = glm::vec4(triData->vertexColors[i].r, triData->vertexColors[i].g,
                                                  triData->vertexColors[i].b, triData->vertexColors[i].a);
                }
                else {
                    vertices[i].color = glm::vec4(1.0f);
                }
            }

            // Compute missing normals from triangle faces
            if (!triData->hasNormals || triData->normals.empty()) {
                for (const auto& tri : triData->triangles) {
                    if (tri.v1 < vertices.size() && tri.v2 < vertices.size() && tri.v3 < vertices.size()) {
                        glm::vec3 edge1 = vertices[tri.v2].position - vertices[tri.v1].position;
                        glm::vec3 edge2 = vertices[tri.v3].position - vertices[tri.v1].position;
                        glm::vec3 faceNorm = glm::cross(edge1, edge2);
                        if (glm::length(faceNorm) > 1e-6f) {
                            faceNorm = glm::normalize(faceNorm);
                            vertices[tri.v1].normal += faceNorm;
                            vertices[tri.v2].normal += faceNorm;
                            vertices[tri.v3].normal += faceNorm;
                        }
                    }
                }
                for (auto& v : vertices) {
                    if (glm::length(v.normal) > 1e-6f) {
                        v.normal = glm::normalize(v.normal);
                    }
                }
            }

            std::vector<uint32_t> indices;
            indices.reserve(triData->triangles.size() * 3);
            for (const auto& tri : triData->triangles) {
                indices.push_back(tri.v1);
                indices.push_back(tri.v2);
                indices.push_back(tri.v3);
            }

            bool hasVertColors = triData->hasVertexColors && !triData->vertexColors.empty();
            uploadMeshToGPU(triShape->name, vertices, indices, currentTransform,
                            texturePath, embeddedPixelDataIndex,
                            isHidden, isHiddenByFlag, isHiddenByMissingProperty,
                            alphaProperty.get(), hasVertColors, outScene);
        }
    }
    else if (auto meshObj = dynamic_cast<NiMesh*>(obj)) {
        if (nodeInfo.typeName.empty()) nodeInfo.typeName = "NiMesh";
        bool isHiddenByMissingProperty = !hasTexturingOrShaderLighting;
        bool isHidden = isHiddenByFlag || isHiddenByMissingProperty;

        std::vector<glm::vec3> primaryPositions;
        std::vector<glm::vec3> morphPositions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> colors;
        std::vector<uint32_t> indices;

        const DataStreamRef* idxRef = nullptr;
        const DataStreamRef* posRef = nullptr;
        NiDataStream* idxDataStream = nullptr;
        NiDataStream* posDataStream = nullptr;

        for (const auto& dsRef : meshObj->dataStreams) {
            auto ds = dsRef.stream.getReference(file);
            if (!ds) continue;

            for (const auto& s : ds->semanticData) {
                if (auto posStream = dynamic_cast<DataStreamPosition*>(s.get())) {
                    for (const auto& p : posStream->values) {
                        primaryPositions.push_back(glm::vec3(p.x, p.y, p.z));
                    }
                    if (!posRef) {
                        posRef = &dsRef;
                        posDataStream = ds;
                    }
                }
                else if (auto morphStream = dynamic_cast<DataStreamMorphPosition*>(s.get())) {
                    for (const auto& p : morphStream->values) {
                        morphPositions.push_back(glm::vec3(p.x, p.y, p.z));
                    }
                }
                else if (auto normStream = dynamic_cast<DataStreamNormal*>(s.get())) {
                    for (const auto& n : normStream->values) {
                        normals.push_back(glm::vec3(n.x, n.y, n.z));
                    }
                }
                else if (auto uvStream = dynamic_cast<DataStreamTexCoord*>(s.get())) {
                    for (const auto& t : uvStream->values) {
                        uvs.push_back(glm::vec2(t.u, t.v));
                    }
                }
                else if (auto colStream = dynamic_cast<DataStreamColor*>(s.get())) {
                    for (const auto& c : colStream->values) {
                        colors.push_back(glm::vec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f));
                    }
                }
                else if (auto idxStream = dynamic_cast<DataStreamIndex*>(s.get())) {
                    if (!idxRef) {
                        idxRef = &dsRef;
                        idxDataStream = ds;
                    }
                }
            }
        }

        // Handle indices with proper region offsets when multiple submeshes are present
        if (idxRef && idxDataStream && meshObj->numSubmeshes > 1 && !idxDataStream->regions.empty()) {
            std::vector<uint32_t> rawIndices;
            for (const auto& s : idxDataStream->semanticData) {
                if (auto idxStream = dynamic_cast<DataStreamIndex*>(s.get())) {
                    rawIndices.reserve(rawIndices.size() + idxStream->values.size());
                    for (const auto& idx : idxStream->values) {
                        rawIndices.push_back(idx);
                    }
                }
            }

            for (size_t sub = 0; sub < meshObj->numSubmeshes; ++sub) {
                uint16_t rIdx = (sub < idxRef->submeshToRegionMap.size()) ? idxRef->submeshToRegionMap[sub] : 0;
                uint16_t rPos = (posRef && sub < posRef->submeshToRegionMap.size()) ? posRef->submeshToRegionMap[sub] : 0;

                uint32_t idxStart = (rIdx < idxDataStream->regions.size()) ? idxDataStream->regions[rIdx].startIndex : 0;
                uint32_t idxCount = (rIdx < idxDataStream->regions.size()) ? idxDataStream->regions[rIdx].numIndices : 0;
                uint32_t posOffset = (posDataStream && rPos < posDataStream->regions.size()) ? posDataStream->regions[rPos].startIndex : 0;

                for (uint32_t k = 0; k < idxCount && (idxStart + k) < rawIndices.size(); ++k) {
                    indices.push_back(posOffset + rawIndices[idxStart + k]);
                }
            }
        }
        else {
            // Standard / single-submesh fallback
            for (const auto& dsRef : meshObj->dataStreams) {
                auto ds = dsRef.stream.getReference(file);
                if (!ds) continue;
                for (const auto& s : ds->semanticData) {
                    if (auto idxStream = dynamic_cast<DataStreamIndex*>(s.get())) {
                        for (const auto& idx : idxStream->values) {
                            indices.push_back(idx);
                        }
                    }
                }
            }
        }

        auto arePositionsValid = [](const std::vector<glm::vec3>& posList) -> bool {
            if (posList.empty()) return false;
            for (const auto& p : posList) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    std::abs(p.x) > 1e7f || std::abs(p.y) > 1e7f || std::abs(p.z) > 1e7f) {
                    return false;
                }
            }
            return true;
        };

        std::vector<glm::vec3> positions;
        if (arePositionsValid(primaryPositions)) {
            positions = std::move(primaryPositions);
        }
        else if (arePositionsValid(morphPositions)) {
            positions = std::move(morphPositions);
        }
        else if (!primaryPositions.empty()) {
            positions = std::move(primaryPositions);
        }

        if (!positions.empty()) {
            if (indices.empty()) {
                indices.resize(positions.size());
                for (size_t i = 0; i < positions.size(); ++i) {
                    indices[i] = static_cast<uint32_t>(i);
                }
            }

            std::vector<Vertex> vertices(positions.size());
            bool hasVertColors = !colors.empty();
            for (size_t i = 0; i < positions.size(); ++i) {
                vertices[i].position = positions[i];
                vertices[i].normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f);
                vertices[i].texCoords = (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f);
                vertices[i].color = (i < colors.size()) ? colors[i] : glm::vec4(1.0f);
            }

            // Compute missing normals from triangle faces
            if (normals.empty() || normals.size() < positions.size()) {
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    uint32_t i1 = indices[i];
                    uint32_t i2 = indices[i + 1];
                    uint32_t i3 = indices[i + 2];
                    if (i1 < vertices.size() && i2 < vertices.size() && i3 < vertices.size()) {
                        glm::vec3 edge1 = vertices[i2].position - vertices[i1].position;
                        glm::vec3 edge2 = vertices[i3].position - vertices[i1].position;
                        glm::vec3 faceNorm = glm::cross(edge1, edge2);
                        if (glm::length(faceNorm) > 1e-6f) {
                            faceNorm = glm::normalize(faceNorm);
                            vertices[i1].normal += faceNorm;
                            vertices[i2].normal += faceNorm;
                            vertices[i3].normal += faceNorm;
                        }
                    }
                }
                for (auto& v : vertices) {
                    if (glm::length(v.normal) > 1e-6f) {
                        v.normal = glm::normalize(v.normal);
                    }
                    else {
                        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    }
                }
            }

            uploadMeshToGPU(meshObj->name, vertices, indices, currentTransform,
                            texturePath, embeddedPixelDataIndex,
                            isHidden, isHiddenByFlag, isHiddenByMissingProperty,
                            alphaProperty.get(), hasVertColors, outScene);
        }
    }
    else {
        if (nodeInfo.typeName.empty()) nodeInfo.typeName = "NiAvObject";
    }

    if (parentNodeInfo) {
        parentNodeInfo->children.push_back(nodeInfo);
    }
    else {
        outScene.rootNodes.push_back(nodeInfo);
    }
}

SceneData MeshExtractor::extractScene(NiFile& file) {
    SceneData scene;

    // 1. Populate allBlocks list for UI inspection
    scene.allBlocks.reserve(file.header.numBlocks);
    for (uint32_t i = 0; i < file.header.numBlocks; ++i) {
        NifBlockEntry entry;
        entry.index = static_cast<int>(i);

        if (i < file.header.blockTypeIndex.size() && file.header.blockTypeIndex[i] < file.header.blockTypes.size()) {
            std::string rawType = file.header.blockTypes[file.header.blockTypeIndex[i]];
            for (char c : rawType) {
                if (c == '\0' || !isprint((unsigned char)c)) break;
                entry.typeName += c;
            }
        }
        if (i < file.header.blockSize.size()) {
            entry.sizeBytes = file.header.blockSize[i];
        }

        if (i < file.blocks.size() && file.blocks[i]) {
            const auto& block = file.blocks[i];
            if (auto objNet = dynamic_pointer_cast<NiObjectNet>(block)) {
                entry.name = objNet->name;
            }

            if (auto srcTex = dynamic_pointer_cast<NiSourceTexture>(block)) {
                entry.details = "file: \"" + srcTex->filePath + "\"" + (srcTex->useExternal ? " (ext)" : " (int)");
            }
            else if (auto texProp = dynamic_pointer_cast<NiTexturingProperty>(block)) {
                if (texProp->hasBaseTexture && texProp->baseTexture) {
                    entry.details = "baseTex -> Block " + std::to_string(texProp->baseTexture->source.value);
                }
                else {
                    entry.details = "no base texture";
                }
            }
            else if (auto tri = dynamic_pointer_cast<NiTriShape>(block)) {
                entry.details = "triData -> Block " + std::to_string(tri->data.value);
            }
            else if (auto triData = dynamic_pointer_cast<NiTriShapeData>(block)) {
                entry.details = std::to_string(triData->numVertices) + " verts, " + std::to_string(triData->numTriangles) + " tris";
            }
            else if (auto mesh = dynamic_pointer_cast<NiMesh>(block)) {
                entry.details = std::to_string(mesh->numSubmeshes) + " submeshes, " + std::to_string(mesh->dataStreams.size()) + " streams";
            }
            else if (auto node = dynamic_pointer_cast<NiNode>(block)) {
                entry.details = std::to_string(node->children.size()) + " children";
            }
            else if (auto stream = dynamic_pointer_cast<NiDataStream>(block)) {
                entry.details = std::to_string(stream->numBytes) + " bytes, " + std::to_string(stream->semanticData.size()) + " semantics";
            }
            else if (auto mat = dynamic_pointer_cast<NiMaterialProperty>(block)) {
                entry.details = "alpha: " + std::to_string(mat->alpha);
            }
            else if (auto ap = dynamic_pointer_cast<NiAlphaProperty>(block)) {
                entry.details = "blend: " + std::string(ap->alphaBlend() ? "yes" : "no") +
                    ", test: " + std::string(ap->alphaTest() ? "yes" : "no");
            }
            else if (auto strExtra = dynamic_pointer_cast<NiStringExtraData>(block)) {
                entry.details = "str: \"" + strExtra->stringData + "\"";
            }
        }
        else {
            entry.details = "(unparsed block, " + std::to_string(entry.sizeBytes) + " bytes)";
        }

        scene.allBlocks.push_back(std::move(entry));
    }

    // 2. Identify which objects are children of nodes to determine true root objects
    std::unordered_set<const NiAvObject*> childObjects;
    for (const auto& block : file.blocks) {
        if (auto node = dynamic_cast<NiNode*>(block.get())) {
            for (const auto& childRef : node->children) {
                if (auto child = childRef.getReference(file)) {
                    childObjects.insert(child);
                }
            }
        }
    }

    std::unordered_set<const NiAvObject*> visitedObjects;

    // 3. Traverse all root NiAvObjects (objects that are not referenced as children of any NiNode)
    for (size_t i = 0; i < file.blocks.size(); ++i) {
        if (auto avObj = dynamic_cast<NiAvObject*>(file.blocks[i].get())) {
            if (childObjects.find(avObj) == childObjects.end()) {
                traverseNode(file, avObj, static_cast<int>(i), glm::mat4(1.0f), scene, nullptr, visitedObjects);
            }
        }
    }

    // 4. Fallback: Traverse any remaining unvisited NiAvObject blocks
    for (size_t i = 0; i < file.blocks.size(); ++i) {
        if (auto avObj = dynamic_cast<NiAvObject*>(file.blocks[i].get())) {
            if (visitedObjects.find(avObj) == visitedObjects.end()) {
                traverseNode(file, avObj, static_cast<int>(i), glm::mat4(1.0f), scene, nullptr, visitedObjects);
            }
        }
    }

    // Fallback bounding boxes if no geometry found
    if (scene.visibleMinBound.x > scene.visibleMaxBound.x) {
        scene.visibleMinBound = scene.minBound;
        scene.visibleMaxBound = scene.maxBound;
    }
    if (scene.minBound.x > scene.maxBound.x) {
        scene.minBound = glm::vec3(-10.0f);
        scene.maxBound = glm::vec3(10.0f);
        scene.visibleMinBound = scene.minBound;
        scene.visibleMaxBound = scene.maxBound;
    }

    return scene;
}
