#include "ViewerUI.hpp"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void ViewerUI::renderSceneHierarchyTreeNode(const SceneNodeInfo& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    std::string label;
    if (node.blockIndex >= 0) {
        label = "[" + std::to_string(node.blockIndex) + "] [" + node.typeName + "] " + node.name;
    }
    else {
        label = "[" + node.typeName + "] " + node.name;
    }
    if (node.isHidden) {
        label += " (Hidden)";
    }

    ImGui::PushID(node.blockIndex >= 0 ? node.blockIndex : (int)(intptr_t)&node);

    if (node.isHidden) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
    }
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    if (node.isHidden) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Block: %d\nTranslation: (%.2f, %.2f, %.2f)\nScale: %.2f\nFlags: 0x%04X%s",
                          node.blockIndex,
                          node.translation.x, node.translation.y, node.translation.z, node.scale,
                          node.flags, node.isHidden ? " [Bit 0 Hidden]" : "");
    }

    if (nodeOpen) {
        for (const auto& child : node.children) {
            renderSceneHierarchyTreeNode(child);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void ViewerUI::renderMainMenuBar(GLFWwindow* window, SceneData& sceneData, const std::string& currentFilePath) {
    menuBarHeight = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        menuBarHeight = ImGui::GetWindowSize().y;
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
            ImGui::MenuItem("Viewer Controls", nullptr, &showViewerControls);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showSceneHierarchy);
            ImGui::MenuItem("Block List", nullptr, &showBlockList);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                layoutNeedsReset = true;
                showViewerControls = true;
                showSceneHierarchy = true;
                showBlockList = true;
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

void ViewerUI::renderBackgroundViewport(Framebuffer& fbo, OrbitCamera& camera) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 bgViewportPos = ImVec2(0.0f, menuBarHeight);
    ImVec2 bgViewportSize = ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight);
    if (bgViewportSize.x < 1.0f) bgViewportSize.x = 1.0f;
    if (bgViewportSize.y < 1.0f) bgViewportSize.y = 1.0f;

    ImGui::SetNextWindowPos(bgViewportPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(bgViewportSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags bgFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav;
    ImGui::Begin("##BackgroundViewport", nullptr, bgFlags);

    // Resize framebuffer if viewport changed
    if (bgViewportSize.x > 0 && bgViewportSize.y > 0) {
        fbo.resize(static_cast<int>(bgViewportSize.x), static_cast<int>(bgViewportSize.y));
    }

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Render FBO texture to ImGui (flip vertically for OpenGL coordinates)
    ImGui::Image(
        (ImTextureID)fbo.textureColor,
        bgViewportSize,
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    // Invisible button to capture mouse interaction for camera navigation
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("viewport_canvas", bgViewportSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    bool isViewportHovered = ImGui::IsItemHovered();
    bool isViewportActive = ImGui::IsItemActive();
    camera.handleInputs(isViewportHovered, isViewportActive);

    // Drag & drop visual badge
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 badgePos = ImVec2(canvasPos.x + 12.0f, canvasPos.y + 12.0f);
    drawList->AddRectFilled(badgePos, ImVec2(badgePos.x + 220.0f, badgePos.y + 26.0f),
                            IM_COL32(20, 22, 28, 200), 4.0f);
    drawList->AddText(ImVec2(badgePos.x + 8.0f, badgePos.y + 5.0f),
                      IM_COL32(180, 210, 240, 230), "Drop .nif file here to load");

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ViewerUI::renderViewerControlsWindow(SceneData& sceneData,
                                          RenderSettings& renderSettings,
                                          OrbitCamera& camera,
                                          TextureManager& textureManager,
                                          const std::string& currentFilePath,
                                          ImGuiCond layoutCond) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 440.0f, menuBarHeight + 10.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(420, 480), layoutCond);
    if (ImGui::Begin("Viewer Controls", &showViewerControls)) {
        if (ImGui::CollapsingHeader("File & Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Model: %s", fs::path(currentFilePath).filename().string().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", currentFilePath.c_str());
            }

            if (ImGui::Button("Reset Camera")) {
                if (onResetCamera) onResetCamera();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(Drop .nif onto viewer)");

            ImGui::Separator();
            ImGui::Checkbox("Enable Textures", &renderSettings.enableTextures);
            ImGui::InputTextWithHint("Texture Dir", "Drop folder or enter path...", customTextureDir, sizeof(customTextureDir));
            if (ImGui::Button("Reload Textures")) {
                if (onReloadTextures) onReloadTextures();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Dir")) {
                customTextureDir[0] = '\0';
                if (onReloadTextures) onReloadTextures();
            }

            ImGui::Text("Loaded Textures: %zu", textureManager.getLoadedTextureCount());
            if (textureManager.getMissingTextureCount() > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing Textures: %zu", textureManager.getMissingTextureCount());
                if (ImGui::TreeNode("Missing Texture List")) {
                    for (const auto& missing : textureManager.getMissingTextures()) {
                        ImGui::BulletText("%s", missing.c_str());
                    }
                    ImGui::TreePop();
                }
            }
        }

        if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Wireframe", &renderSettings.wireframe);
            ImGui::SameLine();
            ImGui::Checkbox("Backface Culling", &renderSettings.culling);
            ImGui::Checkbox("Show Normal Colors", &renderSettings.useNormalsColor);

            ImGui::Checkbox("Show Hidden", &renderSettings.showHidden);
            if (sceneData.hiddenMeshCount > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu hidden)", sceneData.hiddenMeshCount);
            }
        }

        if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Total NIF Blocks: %zu", sceneData.allBlocks.size());
            ImGui::Text("Meshes: %zu (%zu visible, %zu hidden)", sceneData.meshes.size(),
                        sceneData.meshes.size() - sceneData.hiddenMeshCount, sceneData.hiddenMeshCount);
            ImGui::Text("Vertices: %u", sceneData.totalVertices);
            ImGui::Text("Triangles: %u", sceneData.totalTriangles);
            ImGui::Text("Framerate: %.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        }
    }
    ImGui::End();
}

void ViewerUI::renderSceneHierarchyWindow(SceneData& sceneData, ImGuiCond layoutCond) {
    ImGui::SetNextWindowPos(ImVec2(10.0f, menuBarHeight + 10.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(440, 520), layoutCond);
    if (ImGui::Begin("Scene Hierarchy", &showSceneHierarchy)) {
        if (ImGui::CollapsingHeader("Node Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (sceneData.rootNodes.empty()) {
                ImGui::TextDisabled("No root nodes found.");
            }
            else {
                for (const auto& root : sceneData.rootNodes) {
                    renderSceneHierarchyTreeNode(root);
                }
            }
        }

        if (ImGui::CollapsingHeader("Meshes List", ImGuiTreeNodeFlags_DefaultOpen)) {
            static char meshFilter[128] = "";
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputTextWithHint("##meshFilter", "Filter meshes...", meshFilter, sizeof(meshFilter));
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear##meshFilter")) {
                meshFilter[0] = '\0';
            }

            bool hasFilter = meshFilter[0] != '\0';
            static std::vector<int> filteredMeshIndices;
            if (hasFilter) {
                filteredMeshIndices.clear();
                std::string filterStr = meshFilter;
                std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
                for (size_t i = 0; i < sceneData.meshes.size(); ++i) {
                    const auto& mesh = sceneData.meshes[i];
                    std::string nameLower = mesh.name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    std::string texLower = mesh.textureFilename;
                    std::transform(texLower.begin(), texLower.end(), texLower.begin(), ::tolower);
                    if (nameLower.find(filterStr) != std::string::npos ||
                        texLower.find(filterStr) != std::string::npos ||
                        std::to_string(i).find(filterStr) != std::string::npos) {
                        filteredMeshIndices.push_back(static_cast<int>(i));
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu / %zu)", filteredMeshIndices.size(), sceneData.meshes.size());
            }

            int itemCount = hasFilter ? static_cast<int>(filteredMeshIndices.size()) : static_cast<int>(sceneData.meshes.size());
            ImGuiListClipper clipper;
            clipper.Begin(itemCount);
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    int i = hasFilter ? filteredMeshIndices[row] : row;
                    auto& mesh = sceneData.meshes[i];
                    ImGui::PushID(i);
                    ImGui::Checkbox("##vis", &mesh.visible);
                    ImGui::SameLine();
                    ImGui::ColorEdit3("##col", &mesh.baseColor.x, ImGuiColorEditFlags_NoInputs);
                    ImGui::SameLine();

                    if (mesh.hasTexture) {
                        if (mesh.textureId != 0) {
                            ImGui::TextColored(mesh.isHidden ? ImVec4(0.55f, 0.85f, 0.55f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                                               "%s (%u tris)%s [Tex: %s]",
                                               mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str(),
                                               mesh.textureFilename.c_str());
                        }
                        else {
                            ImGui::TextColored(mesh.isHidden ? ImVec4(0.75f, 0.55f, 0.35f, 1.0f) : ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                               "%s (%u tris)%s [Missing: %s]",
                                               mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str(),
                                               mesh.textureFilename.c_str());
                        }
                    }
                    else {
                        if (mesh.isHidden) {
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s (%u tris)%s",
                                               mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str());
                        }
                        else {
                            ImGui::Text("%s (%u tris)", mesh.name.c_str(), mesh.indexCount / 3);
                        }
                    }

                    if (mesh.isHidden && !mesh.tooltipReason.empty() && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", mesh.tooltipReason.c_str());
                    }

                    ImGui::PopID();
                }
            }
        }
    }
    ImGui::End();
}

void ViewerUI::renderBlockListWindow(const SceneData& sceneData, ImGuiCond layoutCond) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10.0f, io.DisplaySize.y - 310.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20.0f, 300.0f), layoutCond);
    if (ImGui::Begin("Block List", &showBlockList)) {
        static char blockFilter[128] = "";
        static std::string selectedTypeFilter = "ALL";
        static int selectedBlockIndex = -1;

        // Search bar
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##blockSearch", "Search by type, name, or ID...", blockFilter, sizeof(blockFilter));
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            blockFilter[0] = '\0';
            selectedTypeFilter = "ALL";
        }
        ImGui::SameLine();

        // Quick Category Filter Buttons
        const char* filterChips[] = { "ALL", "NiNode", "NiMesh", "NiTriShape", "NiSourceTexture", "NiTexturingProperty", "NiMaterialProperty", "NiDataStream" };
        for (const char* chip : filterChips) {
            bool isSelected = (selectedTypeFilter == chip);
            if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.9f, 1.0f));
            if (ImGui::SmallButton(chip)) {
                selectedTypeFilter = chip;
            }
            if (isSelected) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();

        // Filter indices
        std::vector<int> filteredIndices;
        filteredIndices.reserve(sceneData.allBlocks.size());
        std::string filterStr = blockFilter;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        for (size_t i = 0; i < sceneData.allBlocks.size(); ++i) {
            const auto& b = sceneData.allBlocks[i];
            if (selectedTypeFilter != "ALL" && b.typeName != selectedTypeFilter) {
                continue;
            }
            if (!filterStr.empty()) {
                std::string typeLower = b.typeName;
                std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
                std::string nameLower = b.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::string detailsLower = b.details;
                std::transform(detailsLower.begin(), detailsLower.end(), detailsLower.begin(), ::tolower);
                std::string idxStr = std::to_string(b.index);

                if (typeLower.find(filterStr) == std::string::npos &&
                    nameLower.find(filterStr) == std::string::npos &&
                    detailsLower.find(filterStr) == std::string::npos &&
                    idxStr.find(filterStr) == std::string::npos) {
                    continue;
                }
            }
            filteredIndices.push_back(static_cast<int>(i));
        }

        ImGui::Text("Showing %zu / %zu blocks", filteredIndices.size(), sceneData.allBlocks.size());

        // Virtual scroll table with ImGuiListClipper
        float tableHeight = (selectedBlockIndex >= 0 && selectedBlockIndex < (int)sceneData.allBlocks.size()) ? 150.0f : 210.0f;
        if (ImGui::BeginTable("BlockTable", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                              ImVec2(0, tableHeight))) {

            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(filteredIndices.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    int blockIdx = filteredIndices[row];
                    const auto& block = sceneData.allBlocks[blockIdx];

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    char label[32];
                    snprintf(label, sizeof(label), "[%d]", block.index);
                    bool isSelected = (selectedBlockIndex == block.index);
                    if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                        selectedBlockIndex = block.index;
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(block.typeName.c_str());

                    ImGui::TableSetColumnIndex(2);
                    if (!block.name.empty()) {
                        ImGui::TextUnformatted(block.name.c_str());
                    }
                    else {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableSetColumnIndex(3);
                    if (!block.details.empty()) {
                        ImGui::TextUnformatted(block.details.c_str());
                    }
                    else {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", block.sizeBytes);
                }
            }
            ImGui::EndTable();
        }

        // Inspector card for selected block
        if (selectedBlockIndex >= 0 && selectedBlockIndex < (int)sceneData.allBlocks.size()) {
            const auto& sel = sceneData.allBlocks[selectedBlockIndex];
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Block [%d] - %s", sel.index, sel.typeName.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(Size: %u bytes)", sel.sizeBytes);
            if (!sel.name.empty()) {
                ImGui::Text("Object Name: %s", sel.name.c_str());
            }
            if (!sel.details.empty()) {
                ImGui::Text("Properties / Info: %s", sel.details.c_str());
            }
        }
    }
    ImGui::End();
}

void ViewerUI::render(GLFWwindow* window,
                      SceneData& sceneData,
                      RenderSettings& renderSettings,
                      OrbitCamera& camera,
                      Framebuffer& fbo,
                      TextureManager& textureManager,
                      const std::string& currentFilePath) {
    renderMainMenuBar(window, sceneData, currentFilePath);
    renderBackgroundViewport(fbo, camera);

    ImGuiCond layoutCond = layoutNeedsReset ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

    if (showViewerControls) {
        renderViewerControlsWindow(sceneData, renderSettings, camera, textureManager, currentFilePath, layoutCond);
    }
    if (showSceneHierarchy) {
        renderSceneHierarchyWindow(sceneData, layoutCond);
    }
    if (showBlockList) {
        renderBlockListWindow(sceneData, layoutCond);
    }

    layoutNeedsReset = false;
}
