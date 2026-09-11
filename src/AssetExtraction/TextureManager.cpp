#include "TextureManager.hpp"
#include "Core/NiFile.hpp"
#include "Blocks/Data/NiPixelData.hpp"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

TextureManager::TextureManager() = default;

TextureManager::~TextureManager() {
    clear();
    if (defaultWhiteTexture != 0) {
        glDeleteTextures(1, &defaultWhiteTexture);
        defaultWhiteTexture = 0;
    }
    if (defaultCheckerTexture != 0) {
        glDeleteTextures(1, &defaultCheckerTexture);
        defaultCheckerTexture = 0;
    }
}

void TextureManager::init() {
    if (defaultWhiteTexture == 0) {
        glGenTextures(1, &defaultWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
        uint32_t white = 0xFFFFFFFF;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (defaultCheckerTexture == 0) {
        glGenTextures(1, &defaultCheckerTexture);
        glBindTexture(GL_TEXTURE_2D, defaultCheckerTexture);
        uint32_t checker[16] = {
            0xFFE0E0E0, 0xFFB0B0B0, 0xFFE0E0E0, 0xFFB0B0B0,
            0xFFB0B0B0, 0xFFE0E0E0, 0xFFB0B0B0, 0xFFE0E0E0,
            0xFFE0E0E0, 0xFFB0B0B0, 0xFFE0E0E0, 0xFFB0B0B0,
            0xFFB0B0B0, 0xFFE0E0E0, 0xFFB0B0B0, 0xFFE0E0E0,
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void TextureManager::clear() {
    for (auto& [path, id] : textureCache) {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
    textureCache.clear();

    for (auto& [idx, id] : embeddedCache) {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
    embeddedCache.clear();
    missingTextures.clear();
}

std::string TextureManager::resolveTextureFile(const std::string& rawPath,
                                               const std::string& modelDir,
                                               const std::string& customDir) {
    if (rawPath.empty()) return "";

    std::string filename = fs::path(rawPath).filename().string();

    std::vector<fs::path> searchDirs;
    if (!modelDir.empty()) {
        searchDirs.push_back(modelDir);
        searchDirs.push_back(fs::path(modelDir) / "Textures");
        searchDirs.push_back(fs::path(modelDir) / "textures");
    }
    if (!customDir.empty()) {
        searchDirs.push_back(customDir);
        searchDirs.push_back(fs::path(customDir) / "Textures");
        searchDirs.push_back(fs::path(customDir) / "textures");
    }
    searchDirs.push_back(".");
    searchDirs.push_back("Textures");
    searchDirs.push_back("textures");
    searchDirs.push_back("data");
    searchDirs.push_back("data/Textures");
    searchDirs.push_back("tests/data");
    searchDirs.push_back("../data");
    searchDirs.push_back("../../data");

    // Also check if rawPath directly exists
    if (fs::exists(rawPath)) return rawPath;

    // Check in each directory for exact filename
    for (const auto& dir : searchDirs) {
        fs::path p = dir / filename;
        if (fs::exists(p)) return p.string();

        fs::path pRaw = dir / rawPath;
        if (fs::exists(pRaw)) return pRaw.string();
    }

    return "";
}

GLuint TextureManager::uploadPixelDataToGL(const NiPixelData& pixelData) {
    if (pixelData.mipMaps.empty() || pixelData.pixelData.empty()) {
        return 0;
    }

    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (pixelData.numMipMaps > 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Anisotropic filtering if supported
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 8.0f));
    }

    uint32_t levelsUploaded = 0;

    for (uint32_t level = 0; level < pixelData.numMipMaps; ++level) {
        const auto& mm = pixelData.mipMaps[level];
        if (mm.offset >= pixelData.pixelData.size()) break;

        const uint8_t* dataPtr = pixelData.pixelData.data() + mm.offset;
        size_t availableBytes = pixelData.pixelData.size() - mm.offset;

        if (pixelData.pixelFormat == PixelFormat::PX_FMT_DXT1) {
            uint32_t imageSize = std::max(1u, (mm.width + 3) / 4) * std::max(1u, (mm.height + 3) / 4) * 8;
            if (imageSize <= availableBytes) {
                glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                       mm.width, mm.height, 0, imageSize, dataPtr);
                levelsUploaded++;
            }
        }
        else if (pixelData.pixelFormat == PixelFormat::PX_FMT_DXT3) {
            uint32_t imageSize = std::max(1u, (mm.width + 3) / 4) * std::max(1u, (mm.height + 3) / 4) * 16;
            if (imageSize <= availableBytes) {
                glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
                                       mm.width, mm.height, 0, imageSize, dataPtr);
                levelsUploaded++;
            }
        }
        else if (pixelData.pixelFormat == PixelFormat::PX_FMT_DXT5) {
            uint32_t imageSize = std::max(1u, (mm.width + 3) / 4) * std::max(1u, (mm.height + 3) / 4) * 16;
            if (imageSize <= availableBytes) {
                glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                                       mm.width, mm.height, 0, imageSize, dataPtr);
                levelsUploaded++;
            }
        }
        else if (pixelData.pixelFormat == PixelFormat::PX_FMT_RGBA) {
            size_t expectedSize = static_cast<size_t>(mm.width) * mm.height * 4;
            if (expectedSize <= availableBytes) {
                glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mm.width, mm.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dataPtr);
                levelsUploaded++;
            }
        }
        else if (pixelData.pixelFormat == PixelFormat::PX_FMT_RGB) {
            size_t expectedSize = static_cast<size_t>(mm.width) * mm.height * 3;
            if (expectedSize <= availableBytes) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, mm.width, mm.height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataPtr);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                levelsUploaded++;
            }
        }
    }

    if (levelsUploaded == 1 && pixelData.numMipMaps <= 1) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    if (levelsUploaded == 0) {
        glDeleteTextures(1, &texId);
        return 0;
    }

    return texId;
}

GLuint TextureManager::loadFromPixelData(const NiPixelData& pixelData) {
    return uploadPixelDataToGL(pixelData);
}

GLuint TextureManager::getOrCreateTexture(NiFile& currentFile,
                                          const std::string& texturePath,
                                          int embeddedPixelDataIndex,
                                          const std::string& customTextureDir,
                                          const std::string& modelDirectory) {
    // 1. Check embedded NiPixelData if valid
    if (embeddedPixelDataIndex >= 0 && embeddedPixelDataIndex < (int)currentFile.blocks.size()) {
        auto it = embeddedCache.find(embeddedPixelDataIndex);
        if (it != embeddedCache.end()) {
            return it->second;
        }

        if (auto pxData = std::dynamic_pointer_cast<NiPixelData>(currentFile.blocks[embeddedPixelDataIndex])) {
            GLuint texId = uploadPixelDataToGL(*pxData);
            if (texId != 0) {
                embeddedCache[embeddedPixelDataIndex] = texId;
                return texId;
            }
        }
    }

    // 2. Check external texture path
    if (texturePath.empty()) {
        return 0;
    }

    auto cacheIt = textureCache.find(texturePath);
    if (cacheIt != textureCache.end()) {
        return cacheIt->second;
    }

    std::string resolved = resolveTextureFile(texturePath, modelDirectory, customTextureDir);
    if (resolved.empty()) {
        if (std::find(missingTextures.begin(), missingTextures.end(), texturePath) == missingTextures.end()) {
            missingTextures.push_back(texturePath);
        }
        textureCache[texturePath] = 0;
        return 0;
    }

    auto resIt = textureCache.find(resolved);
    if (resIt != textureCache.end()) {
        textureCache[texturePath] = resIt->second;
        return resIt->second;
    }

    // Load as texture .nif containing NiPixelData
    try {
        NiFile texNif(resolved);
        for (const auto& block : texNif.blocks) {
            if (auto pxData = std::dynamic_pointer_cast<NiPixelData>(block)) {
                GLuint texId = uploadPixelDataToGL(*pxData);
                if (texId != 0) {
                    textureCache[texturePath] = texId;
                    textureCache[resolved] = texId;
                    std::cout << "Loaded texture from NIF: " << resolved << " (ID: " << texId << ")\n";
                    return texId;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to parse texture NIF " << resolved << ": " << e.what() << "\n";
    }

    textureCache[texturePath] = 0;
    return 0;
}