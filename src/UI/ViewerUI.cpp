#include "ViewerUI.hpp"

ViewerUI::ViewerUI() {
    auto viewport = std::make_unique<ViewportView>();
    auto controls = std::make_unique<ViewerControlsView>();
    auto hierarchy = std::make_unique<SceneHierarchyView>();
    auto blocks = std::make_unique<BlockListView>();
    auto zones = std::make_unique<ZoneExplorerView>();

    m_viewportView = viewport.get();
    m_viewerControlsView = controls.get();
    m_sceneHierarchyView = hierarchy.get();
    m_blockListView = blocks.get();
    m_zoneExplorerView = zones.get();

    m_windows.push_back(std::move(viewport));
    m_windows.push_back(std::move(controls));
    m_windows.push_back(std::move(hierarchy));
    m_windows.push_back(std::move(blocks));
    m_windows.push_back(std::move(zones));
}

void ViewerUI::renderMainMenuBar(GLFWwindow* window) {
    m_menuBarHeight = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        m_menuBarHeight = ImGui::GetWindowSize().y;
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Reset Camera", "F")) {
                if (onResetCamera) onResetCamera();
            }
            if (ImGui::MenuItem("Reload Textures", "R")) {
                if (onReloadTextures) onReloadTextures();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Quick Samples")) {
                if (ImGui::MenuItem("Sign (Small)")) {
                    if (onLoadFile) onLoadFile("data/WC_Unicorn_DSigns_A01_KH.nif");
                }
                if (ImGui::MenuItem("Hub (Medium)")) {
                    if (onLoadFile) onLoadFile("data/WC_Z00_Hub.nif");
                }
                if (ImGui::MenuItem("Unicorn Way (Large)")) {
                    if (onLoadFile) onLoadFile("data/WC_Z06_Unicorn_Way.nif");
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            for (auto& win : m_windows) {
                if (!win->isToggleable()) continue;
                bool open = win->isOpen();
                if (ImGui::MenuItem(win->getTitle(), nullptr, &open)) {
                    win->setOpen(open);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                layoutNeedsReset = true;
                for (auto& win : m_windows) {
                    if (win->isToggleable()) win->setOpen(true);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::BulletText("Drag & Drop: Drop any .nif file directly onto the 3D viewport.");
            ImGui::BulletText("Textures: Drop a texture folder or .dds file to load textures.");
            ImGui::BulletText("Controls: Left-drag to rotate, Right-drag to pan, Scroll to zoom.");
            ImGui::BulletText("Windows: Drag windows by their title bar. Click 'X' to close.");
            ImGui::BulletText("NiViewer by Phill030 is licensed under CC BY-NC-SA 4.0!");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void ViewerUI::render(GLFWwindow* window,
                      SceneData& sceneData,
                      RenderSettings& renderSettings,
                      OrbitCamera& camera,
                      Framebuffer& fbo,
                      TextureManager& textureManager,
                      const std::string& currentFilePath) {
    renderMainMenuBar(window);

    ImGuiCond layoutCond = layoutNeedsReset ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

    ViewerContext ctx {
        window,
        sceneData,
        renderSettings,
        camera,
        fbo,
        textureManager,
        currentFilePath,
        m_menuBarHeight,
        onLoadFile,
        onReloadTextures,
        onResetCamera
    };

    for (auto& win : m_windows) {
        if (win->isOpen()) {
            win->render(ctx, layoutCond);
        }
    }

    layoutNeedsReset = false;
}