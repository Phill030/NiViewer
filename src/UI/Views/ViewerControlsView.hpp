#pragma once
#include "UI/IViewerWindow.hpp"
#include <string>
#include <cstring>

class ViewerControlsView : public IViewerWindow
{
public:
    const char* getTitle() const override { return "Viewer Controls"; }

    void render(ViewerContext& ctx, ImGuiCond layoutCond) override;

    const char* getTextureDir() const { return m_customTextureDir; }

    void setTextureDir(const std::string& dir) {
        strncpy_s(m_customTextureDir, sizeof(m_customTextureDir), dir.c_str(), _TRUNCATE);
    }

private:
    char m_customTextureDir[512] = "";
};