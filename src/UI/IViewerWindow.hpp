#pragma once
#include <imgui.h>

struct ViewerContext;

// Common interface implemented by every window ViewerUI manages.
// (construct one ViewerContext per frame, loop, call render on open windows)
class IViewerWindow
{
public:
    virtual ~IViewerWindow() = default;

    // Window title, also used as the ImGui::Begin name and as the label in the "Windows" menu.
    virtual const char* getTitle() const = 0;

    // Draw this window's contents. ViewerUI only calls this while isOpen().
    virtual void render(ViewerContext& ctx, ImGuiCond layoutCond) = 0;

    // Whether this window should get a checkbox in the "Windows" menu and
    // participate in "Reset Layout". Windows that are always present
    // (e.g. the background viewport) return false here.
    virtual bool isToggleable() const { return true; }

    bool isOpen() const { return m_open; }
    void setOpen(bool open) { m_open = open; }
    bool* openPtr() { return &m_open; }

protected:
    bool m_open = true;
};