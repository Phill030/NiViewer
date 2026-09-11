#include "ViewerControlsView.hpp"
#include "UI/ViewerContext.hpp"
#include <filesystem>

namespace fs = std::filesystem;

void ViewerControlsView::render(ViewerContext& ctx, ImGuiCond layoutCond) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 440.0f, ctx.menuBarHeight + 10.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(420, 480), layoutCond);
    if (ImGui::Begin(getTitle(), openPtr())) {
        if (ImGui::CollapsingHeader("File & Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Model: %s", fs::path(ctx.currentFilePath).filename().string().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", ctx.currentFilePath.c_str());
            }

            if (ImGui::Button("Reset Camera")) {
                if (ctx.onResetCamera) ctx.onResetCamera();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(Drop .nif onto viewer)");

            ImGui::Separator();
            ImGui::Checkbox("Enable Textures", &ctx.renderSettings.enableTextures);
            ImGui::InputTextWithHint("Texture Dir", "Drop folder or enter path...", m_customTextureDir, sizeof(m_customTextureDir));
            if (ImGui::Button("Reload Textures")) {
                if (ctx.onReloadTextures) ctx.onReloadTextures();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Dir")) {
                m_customTextureDir[0] = '\0';
                if (ctx.onReloadTextures) ctx.onReloadTextures();
            }

            ImGui::Text("Loaded Textures: %zu", ctx.textureManager.getLoadedTextureCount());
            if (ctx.textureManager.getMissingTextureCount() > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing Textures: %zu", ctx.textureManager.getMissingTextureCount());
                if (ImGui::TreeNode("Missing Texture List")) {
                    for (const auto& missing : ctx.textureManager.getMissingTextures()) {
                        ImGui::BulletText("%s", missing.c_str());
                    }
                    ImGui::TreePop();
                }
            }
        }

        if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Wireframe", &ctx.renderSettings.wireframe);
            ImGui::SameLine();
            ImGui::Checkbox("Backface Culling", &ctx.renderSettings.culling);
            ImGui::Checkbox("Show Normal Colors", &ctx.renderSettings.useNormalsColor);

            ImGui::Checkbox("Show Hidden", &ctx.renderSettings.showHidden);
            if (ctx.sceneData.hiddenMeshCount > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu hidden)", ctx.sceneData.hiddenMeshCount);
            }
        }

        if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Total NIF Blocks: %zu", ctx.sceneData.allBlocks.size());
            ImGui::Text("Meshes: %zu (%zu visible, %zu hidden)", ctx.sceneData.meshes.size(),
                        ctx.sceneData.meshes.size() - ctx.sceneData.hiddenMeshCount, ctx.sceneData.hiddenMeshCount);
            ImGui::Text("Vertices: %u", ctx.sceneData.totalVertices);
            ImGui::Text("Triangles: %u", ctx.sceneData.totalTriangles);
            ImGui::Text("Framerate: %.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        }
    }
    ImGui::End();
}