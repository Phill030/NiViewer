#pragma once
#include <glad/gl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

class NiFile;
struct NiPixelData;

class TextureManager
{
public:
    TextureManager();
    ~TextureManager();

    void init();
    void clear();

    GLuint loadFromPixelData(const NiPixelData& pixelData);

    GLuint getOrCreateTexture(NiFile& currentFile,
                              const std::string& texturePath,
                              int embeddedPixelDataIndex,
                              const std::string& customTextureDir,
                              const std::string& modelDirectory);

    GLuint getDefaultWhiteTexture() const { return defaultWhiteTexture; }
    GLuint getDefaultCheckerboardTexture() const { return defaultCheckerTexture; }

    size_t getLoadedTextureCount() const { return textureCache.size(); }
    size_t getMissingTextureCount() const { return missingTextures.size(); }
    const std::vector<std::string>& getMissingTextures() const { return missingTextures; }

private:
    GLuint defaultWhiteTexture = 0;
    GLuint defaultCheckerTexture = 0;

    std::unordered_map<std::string, GLuint> textureCache;
    std::unordered_map<int, GLuint> embeddedCache;
    std::vector<std::string> missingTextures;

    std::string resolveTextureFile(const std::string& rawPath,
                                   const std::string& modelDir,
                                   const std::string& customDir);

    GLuint uploadPixelDataToGL(const NiPixelData& pixelData);
};
