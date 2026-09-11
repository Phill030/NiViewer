#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "Core/SceneTypes.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "AssetExtraction/TextureManager.hpp"
#include "Core/Camera.hpp"
#include "Core/Framebuffer.hpp"
#include "UI/IViewerWindow.hpp"
#include "UI/ViewerContext.hpp"
#include "UI/Views/ViewportView.hpp"
#include "UI/Views/ViewerControlsView.hpp"
#include "UI/Views/SceneHierarchyView.hpp"
#include "UI/Views/BlockListView.hpp"
#include "UI/Views/ZoneExplorerView.hpp"

class ViewerUI
{
public:
    bool layoutNeedsReset = false;
    char filePathBuffer[512] = "data/WC_Unicorn_DSigns_A01_KH.nif";

    // Callbacks to communicate with main application
    std::function<void(const std::string&)> onLoadFile;
    std::function<void()> onReloadTextures;
    std::function<void()> onResetCamera;

    ViewerUI();

    void render(GLFWwindow* window,
                SceneData& sceneData,
                RenderSettings& renderSettings,
                OrbitCamera& camera,
                Framebuffer& fbo,
                TextureManager& textureManager,
                const std::string& currentFilePath);

    // Typed access for the app code to reach specific windows directly
    // (e.g. feeding ZoneExplorerView data after a WAD loads, or reading
    // the texture directory typed into Viewer Controls).
    ZoneExplorerView& getZoneExplorer() { return *m_zoneExplorerView; }
    const char* getCustomTextureDir() const { return m_viewerControlsView->getTextureDir(); }
    void setCustomTextureDir(const std::string& dir) { m_viewerControlsView->setTextureDir(dir); }

private:
    float m_menuBarHeight = 0.0f;

    // Ownership lives in m_windows; these are non-owning pointers cached
    // at construction time purely for the typed accessors above.
    ViewportView* m_viewportView = nullptr;
    ViewerControlsView* m_viewerControlsView = nullptr;
    SceneHierarchyView* m_sceneHierarchyView = nullptr;
    BlockListView* m_blockListView = nullptr;
    ZoneExplorerView* m_zoneExplorerView = nullptr;

    std::vector<std::unique_ptr<IViewerWindow>> m_windows;

    void renderMainMenuBar(GLFWwindow* window);
};