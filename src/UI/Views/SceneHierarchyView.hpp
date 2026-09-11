#pragma once
#include "UI/IViewerWindow.hpp"
#include "Core/SceneTypes.hpp"
#include <string>
#include <vector>

class SceneHierarchyView : public IViewerWindow
{
public:
    const char* getTitle() const override { return "Scene Hierarchy"; }

    void render(ViewerContext& ctx, ImGuiCond layoutCond) override;

private:
    char m_meshFilter[128] = "";
    std::vector<int> m_filteredMeshIndices;

    void renderTreeNode(const SceneNodeInfo& node);
    void renderMeshList(SceneData& sceneData);
};