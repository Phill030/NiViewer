#pragma once
#include "UI/IViewerWindow.hpp"
#include "Core/Zone.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>

enum class GroupingMode
{
    ByPath,
    ByAccessPass,
    ByZoneType
};

struct NavNode
{
    std::string label;
    const Zone* zone = nullptr;
    std::map<std::string, NavNode> children;

    bool isLeaf() const { return zone != nullptr; }
};

class ZoneExplorerView : public IViewerWindow
{
public:
    std::function<void(const Zone&)> onZoneSelected;

    // Fired when the user enters a path and clicks "Load" (or presses Enter).
    // The view doesn't know how to parse XML itself - the app wires this up
    // to its ZoneParser and then calls setPasses() with the result.
    std::function<void(const std::string&)> onLoadXmlRequested;

    const char* getTitle() const override { return "Zone Explorer"; }
    void render(ViewerContext& ctx, ImGuiCond layoutCond) override;

    // Data feed and selection API, unrelated to the generic window lifecycle
    // and so kept outside of IViewerWindow: the app calls setPasses() once
    // after loading a WAD/pass set, and can read back the current pick.
    void setPasses(const std::vector<AccessPass>& passes);
    const Zone* getSelectedZone() const { return m_selectedZone; }
    void setLoadStatus(bool success, const std::string& message);

private:
    std::vector<AccessPass> m_passes;
    NavNode m_root;
    GroupingMode m_currentMode = GroupingMode::ByPath;
    const Zone* m_selectedZone = nullptr;
    ImGuiTextFilter m_searchFilter;
    bool m_dirty = true;

    char m_xmlPathBuffer[512] = "";
    std::string m_loadStatusMessage;
    bool m_loadStatusIsError = false;

    void renderXmlLoader();
    void rebuildTree();
    void renderNode(const std::string& name, const NavNode& node);
    void insertPath(NavNode& root, const std::string& fullPath, const Zone* zone);
};