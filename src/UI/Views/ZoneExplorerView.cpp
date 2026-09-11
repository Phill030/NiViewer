#include "ZoneExplorerView.hpp"
#include "UI/ViewerContext.hpp"
#include <sstream>

void ZoneExplorerView::setPasses(const std::vector<AccessPass>& passes) {
    m_passes = passes;
    m_dirty = true;
}

void ZoneExplorerView::setLoadStatus(bool success, const std::string& message) {
    m_loadStatusIsError = !success;
    m_loadStatusMessage = message;
}

void ZoneExplorerView::renderXmlLoader() {
    ImGui::TextUnformatted("XML File:");
    ImGui::SetNextItemWidth(-70.0f);
    bool enterPressed = ImGui::InputTextWithHint("##xmlPath", "Path to zones.xml...",
                                                 m_xmlPathBuffer, sizeof(m_xmlPathBuffer),
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool loadClicked = ImGui::Button("Load");

    if ((enterPressed || loadClicked) && m_xmlPathBuffer[0] != '\0') {
        if (onLoadXmlRequested) {
            onLoadXmlRequested(m_xmlPathBuffer);
        }
    }

    if (!m_loadStatusMessage.empty()) {
        ImVec4 color = m_loadStatusIsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        ImGui::TextColored(color, "%s", m_loadStatusMessage.c_str());
    }

    ImGui::Separator();
}

void ZoneExplorerView::insertPath(NavNode& root, const std::string& fullPath, const Zone* zone) {
    std::stringstream ss(fullPath);
    std::string segment;
    NavNode* current = &root;

    std::vector<std::string> segments;
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) segments.push_back(segment);
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& part = segments[i];
        current = &current->children[part];
        current->label = part;
        if (i == segments.size() - 1) {
            current->zone = zone;
        }
    }
}

void ZoneExplorerView::rebuildTree() {
    m_root = NavNode{};
    m_root.label = "Root";

    for (const auto& pass : m_passes) {
        for (const auto& zone : pass.zones) {
            if (!m_searchFilter.PassFilter(zone.name.c_str()) &&
                !m_searchFilter.PassFilter(zone.filename.c_str()) &&
                !m_searchFilter.PassFilter(pass.key.c_str())) {
                continue;
            }

            switch (m_currentMode) {
                case GroupingMode::ByPath: {
                    std::string path = zone.rawPath.empty() ? zone.filename : zone.rawPath;
                    insertPath(m_root, path, &zone);
                    break;
                }
                case GroupingMode::ByAccessPass: {
                    NavNode& passNode = m_root.children[pass.key];
                    passNode.label = pass.key;
                    NavNode& leafNode = passNode.children[zone.name];
                    leafNode.label = zone.name;
                    leafNode.zone = &zone;
                    break;
                }
                case GroupingMode::ByZoneType: {
                    std::string typeName;
                    switch (pass.type) {
                        case ZoneType::Normal: typeName = "Normal"; break;
                        case ZoneType::Timed:  typeName = "Timed"; break;
                        case ZoneType::Sample: typeName = "Sample"; break;
                    }
                    NavNode& typeNode = m_root.children[typeName];
                    typeNode.label = typeName;
                    NavNode& passNode = typeNode.children[pass.key];
                    passNode.label = pass.key;
                    NavNode& leafNode = passNode.children[zone.name];
                    leafNode.label = zone.name;
                    leafNode.zone = &zone;
                    break;
                }
            }
        }
    }
    m_dirty = false;
}

void ZoneExplorerView::renderNode(const std::string& name, const NavNode& node) {
    ImGui::PushID(name.c_str());

    if (node.isLeaf()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (m_selectedZone == node.zone) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx(name.c_str(), flags, "%s", name.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_selectedZone = node.zone;
            if (onZoneSelected && node.zone) {
                onZoneSelected(*node.zone);
            }
        }

        if (ImGui::IsItemHovered() && node.zone) {
            ImGui::SetTooltip("WAD: %s.wad\nPath: %s",
                              node.zone->filename.c_str(),
                              node.zone->rawPath.c_str());
        }
    }
    else {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_searchFilter.IsActive()) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        bool isOpen = ImGui::TreeNodeEx(name.c_str(), flags, "%s (%zu)",
                                        name.c_str(), node.children.size());
        if (isOpen) {
            for (const auto& [childName, childNode] : node.children) {
                renderNode(childName, childNode);
            }
            ImGui::TreePop();
        }
    }

    ImGui::PopID();
}

void ZoneExplorerView::render(ViewerContext& ctx, ImGuiCond layoutCond) {
    ImGui::SetNextWindowPos(ImVec2(460.0f, ctx.menuBarHeight + 10.0f), layoutCond);
    ImGui::SetNextWindowSize(ImVec2(380, 520), layoutCond);

    if (!ImGui::Begin(getTitle(), openPtr())) {
        ImGui::End();
        return;
    }

    renderXmlLoader();

    ImGui::TextUnformatted("Group By:");
    ImGui::SameLine();
    int mode = static_cast<int>(m_currentMode);
    if (ImGui::RadioButton("Path", &mode, 0)) { m_currentMode = GroupingMode::ByPath; m_dirty = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Pass", &mode, 1)) { m_currentMode = GroupingMode::ByAccessPass; m_dirty = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Type", &mode, 2)) { m_currentMode = GroupingMode::ByZoneType; m_dirty = true; }

    if (m_searchFilter.Draw("Filter (inc,-exc)")) {
        m_dirty = true;
    }

    ImGui::Separator();

    if (m_dirty) {
        rebuildTree();
    }

    ImGui::BeginChild("ZoneTreeRegion", ImVec2(0, 0), true);
    for (const auto& [childName, childNode] : m_root.children) {
        renderNode(childName, childNode);
    }
    ImGui::EndChild();

    ImGui::End();
}