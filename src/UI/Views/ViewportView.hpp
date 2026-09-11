#pragma once
#include "UI/IViewerWindow.hpp"

class ViewportView : public IViewerWindow
{
public:
    const char* getTitle() const override { return "##BackgroundViewport"; }
    bool isToggleable() const override { return false; }

    void render(ViewerContext& ctx, ImGuiCond layoutCond) override;
};