#include "SceneHierarchyView.hpp"
#include "UI/ViewerContext.hpp"
#include <algorithm>

void SceneHierarchyView::renderTreeNode(const SceneNodeInfo& node) {
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
            renderTreeNode(child);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneHierarchyView::renderMeshList(SceneData& sceneData) {
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##meshFilter", "Filter meshes...", m_meshFilter, sizeof(m_meshFilter));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##meshFilter")) {
        m_meshFilter[0] = '\0';
    }

    bool hasFilter = m_meshFilter[0] != '\0';
    if (hasFilter) {
        m_filteredMeshIndices.clear();
        std::string filterStr = m_meshFilter;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
        for (size_t i = 0; i < sceneData.meshes.size(); ++i) {
            const auto& mesh = sceneData.meshes[i];
            std::string nameLower = mesh.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string texLower = mesh.textureFilename;
            std::transform(texLower.begin(), texLower.end(), texLower.begin(), ::tolower);
            std::string glowLower = mesh.glowTextureFilename;
            std::transform(glowLower.begin(), glowLower.end(), glowLower.begin(), ::tolower);
            if (nameLower.find(filterStr) != std::string::npos ||
                texLower.find(filterStr) != std::string::npos ||
                glowLower.find(filterStr) != std::string::npos ||
                std::to_string(i).find(filterStr) != std::string::npos) {
                m_filteredMeshIndices.push_back(static_cast<int>(i));
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu / %zu)", m_filteredMeshIndices.size(), sceneData.meshes.size());
    }

    int itemCount = hasFilter ? static_cast<int>(m_filteredMeshIndices.size()) : static_cast<int>(sceneData.meshes.size());
    ImGuiListClipper clipper;
    clipper.Begin(itemCount);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            int i = hasFilter ? m_filteredMeshIndices[row] : row;
            auto& mesh = sceneData.meshes[i];
            ImGui::PushID(i);
            ImGui::Checkbox("##vis", &mesh.visible);
            ImGui::SameLine();
            ImGui::ColorEdit3("##col", &mesh.baseColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();

            std::string glowTag;
            if (mesh.hasGlowTexture) {
                glowTag = " [Glow: " + mesh.glowTextureFilename + (mesh.glowTextureId != 0 ? "" : " (missing)") + "]";
            }

            if (mesh.hasTexture) {
                if (mesh.textureId != 0) {
                    ImGui::TextColored(mesh.isHidden ? ImVec4(0.55f, 0.85f, 0.55f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                                       "%s (%u tris)%s [Tex: %s]%s",
                                       mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str(),
                                       mesh.textureFilename.c_str(), glowTag.c_str());
                }
                else {
                    ImGui::TextColored(mesh.isHidden ? ImVec4(0.75f, 0.55f, 0.35f, 1.0f) : ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                       "%s (%u tris)%s [Missing: %s]%s",
                                       mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str(),
                                       mesh.textureFilename.c_str(), glowTag.c_str());
                }
            }
            else if (mesh.hasGlowTexture) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
                                   "%s (%u tris)%s%s",
                                   mesh.name.c_str(), mesh.indexCount / 3, mesh.hiddenTag.c_str(), glowTag.c_str());
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

void SceneHierarchyView::render(ViewerContext& ctx, ImGuiCond layoutCond) {
    ImGui::SetNextWindowPos(ImVec2(10.0f, ctx.menuBarHeight + 10.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(440, 520), layoutCond);
    if (ImGui::Begin(getTitle(), openPtr())) {
        if (ImGui::CollapsingHeader("Node Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ctx.sceneData.rootNodes.empty()) {
                ImGui::TextDisabled("No root nodes found.");
            }
            else {
                for (const auto& root : ctx.sceneData.rootNodes) {
                    renderTreeNode(root);
                }
            }
        }

        if (ImGui::CollapsingHeader("Meshes List", ImGuiTreeNodeFlags_DefaultOpen)) {
            renderMeshList(ctx.sceneData);
        }
    }
    ImGui::End();
}