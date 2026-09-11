#pragma once
#include <string>
#include <functional>

#include "Core/SceneTypes.hpp"
#include "Core/Camera.hpp"
#include "Core/Framebuffer.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "AssetExtraction/TextureManager.hpp"

struct GLFWwindow;

// Bundles everything a window might need to render one frame. ViewerUI
// constructs one of these fresh each frame and passes it to every window;
// each view only reads the fields it actually cares about, so adding a new
// window never requires changing every other window's signature.
struct ViewerContext
{
    GLFWwindow* window;
    SceneData& sceneData;
    RenderSettings& renderSettings;
    OrbitCamera& camera;
    Framebuffer& fbo;
    TextureManager& textureManager;
    const std::string& currentFilePath;
    float menuBarHeight;

    std::function<void(const std::string&)>& onLoadFile;
    std::function<void()>& onReloadTextures;
    std::function<void()>& onResetCamera;
};