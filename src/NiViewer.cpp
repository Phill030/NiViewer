#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <Core/NiFile.hpp>
#include "Core/SceneTypes.hpp"
#include "Core/Camera.hpp"
#include "Core/Framebuffer.hpp"
#include "AssetExtraction/MeshExtractor.hpp"
#include "AssetExtraction/TextureManager.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "UI/ViewerUI.hpp"
#include "AssetExtraction/ZoneParser.hpp"

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

static std::vector<std::string> g_pendingDroppedFiles;

static void glfwDropCallback(GLFWwindow* /*window*/, int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        if (paths && paths[i]) {
            g_pendingDroppedFiles.push_back(paths[i]);
        }
    }
}

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1440, 900, "NiViewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync
    glfwSetDropCallback(window, glfwDropCallback);

    // Initialize GLAD
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Subsystems
    SceneRenderer renderer;
    if (!renderer.init()) {
        std::cerr << "Failed to initialize SceneRenderer shader program\n";
        return -1;
    }

    Framebuffer fbo;
    fbo.init(800, 600);

    OrbitCamera camera;
    SceneData sceneData;
    RenderSettings renderSettings;

    TextureManager textureManager;
    textureManager.init();

    ViewerUI ui;
    std::string currentFilePath = "data/WC_Unicorn_DSigns_A01_KH.nif";

    auto loadNifFile = [&](const std::string& path) -> bool {
        std::string resolvedPath = path;
        if (!fs::exists(resolvedPath)) {
            if (fs::exists("tests/" + path)) resolvedPath = "tests/" + path;
            else if (fs::exists("../" + path)) resolvedPath = "../" + path;
            else if (fs::exists("../../" + path)) resolvedPath = "../../" + path;
            else if (fs::exists("data/" + path)) resolvedPath = "data/" + path;
            else {
                std::cerr << "File does not exist: " << path << "\n";
                return false;
            }
        }
        try {
            std::cout << "Loading NIF: " << resolvedPath << "...\n";
            NiFile file(resolvedPath);
            sceneData.clear();
            textureManager.clear();
            sceneData = MeshExtractor::extractScene(file);

            // Bind textures from NiPixelData (embedded or external texture NIF)
            std::string modelDir = fs::path(resolvedPath).parent_path().string();
            for (auto& mesh : sceneData.meshes) {
                if (mesh.hasTexture) {
                    mesh.textureId = textureManager.getOrCreateTexture(
                        file, mesh.texturePath, mesh.embeddedPixelDataIndex, ui.getCustomTextureDir(), modelDir
                    );
                }

                if (mesh.hasGlowTexture) {
                    mesh.glowTextureId = textureManager.getOrCreateTexture(
                        file, mesh.glowTexturePath, mesh.glowEmbeddedPixelDataIndex, ui.getCustomTextureDir(), modelDir
                    );
                }
            }

            camera.frameBounds(sceneData.getEffectiveMinBound(), sceneData.getEffectiveMaxBound());
            currentFilePath = resolvedPath;
            strncpy_s(ui.filePathBuffer, resolvedPath.c_str(), sizeof(ui.filePathBuffer) - 1);
            std::cout << "Loaded " << sceneData.meshes.size() << " meshes, "
                << sceneData.totalTriangles << " triangles.\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading NIF: " << e.what() << "\n";
            return false;
        }
    };

    auto reloadTextures = [&]() {
        if (currentFilePath.empty() || !fs::exists(currentFilePath)) return;
        try {
            NiFile file(currentFilePath);
            textureManager.clear();
            std::string modelDir = fs::path(currentFilePath).parent_path().string();
            for (auto& mesh : sceneData.meshes) {
                if (mesh.hasTexture) {
                    mesh.textureId = textureManager.getOrCreateTexture(
                        file, mesh.texturePath, mesh.embeddedPixelDataIndex, ui.getCustomTextureDir(), modelDir
                    );
                }
                if (mesh.hasGlowTexture) {
                    mesh.glowTextureId = textureManager.getOrCreateTexture(
                        file, mesh.glowTexturePath, mesh.glowEmbeddedPixelDataIndex, ui.getCustomTextureDir(), modelDir
                    );
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to reload textures: " << e.what() << "\n";
        }
    };

    // UI Callbacks
    ui.onLoadFile = loadNifFile;
    ui.onReloadTextures = reloadTextures;
    ui.onResetCamera = [&]() {
        if (renderSettings.showHidden) {
            camera.frameBounds(sceneData.minBound, sceneData.maxBound);
        }
        else {
            camera.frameBounds(sceneData.getEffectiveMinBound(), sceneData.getEffectiveMaxBound());
        }
    };
    ui.getZoneExplorer().onLoadXmlRequested = [&](const std::string& path) {
        try {
            std::vector<AccessPass> passes = ZoneParser::ParseFromFile(path);
            ui.getZoneExplorer().setPasses(passes);
            ui.getZoneExplorer().setLoadStatus(true, "Loaded " + std::to_string(passes.size()) + " passes.");
        }
        catch (const std::exception& e) {
            ui.getZoneExplorer().setLoadStatus(false, std::string("Failed: ") + e.what());
        }
    };

    // Load initial sample
    loadNifFile(currentFilePath);

    // Main Application Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Process drag-and-dropped files
        if (!g_pendingDroppedFiles.empty()) {
            std::vector<std::string> dropped = std::move(g_pendingDroppedFiles);
            g_pendingDroppedFiles.clear();

            for (const auto& pathStr : dropped) {
                try {
                    if (fs::is_directory(pathStr)) {
                        ui.setCustomTextureDir(pathStr);
                        reloadTextures();
                    }
                    else if (pathStr.size() >= 4) {
                        std::string ext = pathStr.substr(pathStr.size() - 4);
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".nif") {
                            loadNifFile(pathStr);
                        }
                        else if (ext == ".dds") {
                            std::string dir = fs::path(pathStr).parent_path().string();
                            ui.setCustomTextureDir(dir);
                            reloadTextures();
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "Drop error: " << e.what() << "\n";
                }
            }
        }

        // Render 3D Scene to offscreen Framebuffer
        renderer.render(sceneData, camera, fbo, renderSettings);

        // Render ImGui UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ui.render(window, sceneData, renderSettings, camera, fbo, textureManager, currentFilePath);

        ImGui::Render();

        // Final presentation
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    sceneData.clear();
    fbo.cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

