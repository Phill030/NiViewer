#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <functional>

#include "Core/SceneTypes.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "AssetExtraction/TextureManager.hpp"
#include "Core/Camera.hpp"
#include "Core/Framebuffer.hpp"

class ViewerUI
{
public:
    bool showViewerControls = true;
    bool showSceneHierarchy = true;
    bool showBlockList = true;
    bool layoutNeedsReset = false;

    char customTextureDir[512] = "";
    char filePathBuffer[512] = "data/WC_Unicorn_DSigns_A01_KH.nif";

    // Callbacks to communicate with main application
    std::function<void(const std::string&)> onLoadFile;
    std::function<void()> onReloadTextures;
    std::function<void()> onResetCamera;

    ViewerUI() = default;

    void render(GLFWwindow* window,
                SceneData& sceneData,
                RenderSettings& renderSettings,
                OrbitCamera& camera,
                Framebuffer& fbo,
                TextureManager& textureManager,
                const std::string& currentFilePath);

private:
    float menuBarHeight = 0.0f;

    void renderMainMenuBar(GLFWwindow* window, SceneData& sceneData, const std::string& currentFilePath);
    void renderBackgroundViewport(Framebuffer& fbo, OrbitCamera& camera);
    void renderViewerControlsWindow(SceneData& sceneData,
                                    RenderSettings& renderSettings,
                                    OrbitCamera& camera,
                                    TextureManager& textureManager,
                                    const std::string& currentFilePath,
                                    ImGuiCond layoutCond);
    void renderSceneHierarchyWindow(SceneData& sceneData, ImGuiCond layoutCond);
    void renderBlockListWindow(const SceneData& sceneData, ImGuiCond layoutCond);
    void renderSceneHierarchyTreeNode(const SceneNodeInfo& node);
};
