#include "ViewportView.hpp"
#include "UI/ViewerContext.hpp"

void ViewportView::render(ViewerContext& ctx, ImGuiCond layoutCond) {
    (void)layoutCond; // this window always pins itself full-screen with ImGuiCond_Always

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 bgViewportPos = ImVec2(0.0f, ctx.menuBarHeight);
    ImVec2 bgViewportSize = ImVec2(io.DisplaySize.x, io.DisplaySize.y - ctx.menuBarHeight);
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
    ImGui::Begin(getTitle(), nullptr, bgFlags);

    // Resize framebuffer if viewport changed
    if (bgViewportSize.x > 0 && bgViewportSize.y > 0) {
        ctx.fbo.resize(static_cast<int>(bgViewportSize.x), static_cast<int>(bgViewportSize.y));
    }

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Render FBO texture to ImGui (flip vertically for OpenGL coordinates)
    ImGui::Image(
        (ImTextureID)ctx.fbo.textureColor,
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
    ctx.camera.handleInputs(isViewportHovered, isViewportActive);

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