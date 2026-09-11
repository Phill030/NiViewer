#pragma once
#include "UI/IViewerWindow.hpp"
#include "Core/SceneTypes.hpp"
#include <string>
#include <vector>

class BlockListView : public IViewerWindow
{
public:
    const char* getTitle() const override { return "Block List"; }

    void render(ViewerContext& ctx, ImGuiCond layoutCond) override;

private:
    char m_blockFilter[128] = "";
    std::string m_selectedTypeFilter = "ALL";
    int m_selectedBlockIndex = -1;
    std::vector<int> m_filteredIndices;

    void renderFilterBar();
    void rebuildFilteredIndices(const SceneData& sceneData);
    void renderTable(const SceneData& sceneData);
    void renderInspector(const SceneData& sceneData);
};