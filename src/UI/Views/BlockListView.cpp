#include "BlockListView.hpp"
#include "UI/ViewerContext.hpp"
#include <algorithm>

void BlockListView::renderFilterBar() {
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##blockSearch", "Search by type, name, or ID...", m_blockFilter, sizeof(m_blockFilter));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_blockFilter[0] = '\0';
        m_selectedTypeFilter = "ALL";
    }
    ImGui::SameLine();

    const char* filterChips[] = { "ALL", "NiNode", "NiMesh", "NiTriShape", "NiSourceTexture", "NiTexturingProperty", "NiMaterialProperty", "NiDataStream" };
    for (const char* chip : filterChips) {
        bool isSelected = (m_selectedTypeFilter == chip);
        if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.9f, 1.0f));
        if (ImGui::SmallButton(chip)) {
            m_selectedTypeFilter = chip;
        }
        if (isSelected) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void BlockListView::rebuildFilteredIndices(const SceneData& sceneData) {
    m_filteredIndices.clear();
    m_filteredIndices.reserve(sceneData.allBlocks.size());
    std::string filterStr = m_blockFilter;
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

    for (size_t i = 0; i < sceneData.allBlocks.size(); ++i) {
        const auto& b = sceneData.allBlocks[i];
        if (m_selectedTypeFilter != "ALL" && b.typeName != m_selectedTypeFilter) {
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
        m_filteredIndices.push_back(static_cast<int>(i));
    }
}

void BlockListView::renderTable(const SceneData& sceneData) {
    float tableHeight = (m_selectedBlockIndex >= 0 && m_selectedBlockIndex < (int)sceneData.allBlocks.size()) ? 150.0f : 210.0f;
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
        clipper.Begin(static_cast<int>(m_filteredIndices.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                int blockIdx = m_filteredIndices[row];
                const auto& block = sceneData.allBlocks[blockIdx];

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                char label[32];
                snprintf(label, sizeof(label), "[%d]", block.index);
                bool isSelected = (m_selectedBlockIndex == block.index);
                if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_selectedBlockIndex = block.index;
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
}

void BlockListView::renderInspector(const SceneData& sceneData) {
    if (m_selectedBlockIndex >= 0 && m_selectedBlockIndex < (int)sceneData.allBlocks.size()) {
        const auto& sel = sceneData.allBlocks[m_selectedBlockIndex];
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

void BlockListView::render(ViewerContext& ctx, ImGuiCond layoutCond) {
    const SceneData& sceneData = ctx.sceneData;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10.0f, io.DisplaySize.y - 310.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20.0f, 300.0f), layoutCond);
    if (ImGui::Begin(getTitle(), openPtr())) {
        renderFilterBar();
        rebuildFilteredIndices(sceneData);

        ImGui::Text("Showing %zu / %zu blocks", m_filteredIndices.size(), sceneData.allBlocks.size());

        renderTable(sceneData);
        renderInspector(sceneData);
    }
    ImGui::End();
}