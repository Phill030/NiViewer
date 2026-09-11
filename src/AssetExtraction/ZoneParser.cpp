#include "ZoneParser.hpp"
#include <iostream>
#include <pugixml.hpp>
#include <algorithm>
#include <unordered_map>

// Helper: Convert string attribute to ZoneType enum
ZoneType ParseZoneType(const pugi::xml_attribute& typeAttr) {
	if (!typeAttr) {
		return ZoneType::Normal; // Default when Type attribute is omitted
	}

	std::string_view typeStr = typeAttr.as_string();
	if (typeStr == "Timed")  return ZoneType::Timed;
	if (typeStr == "Sample") return ZoneType::Sample;
	return ZoneType::Normal;
}

// Helper: Construct Zone from raw XML text
Zone CreateZone(std::string rawPath) {
    std::string name = rawPath;
    const size_t lastSlash = rawPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        name = rawPath.substr(lastSlash + 1);
    }

    std::string filename = rawPath;
    std::replace(filename.begin(), filename.end(), '/', '-');

    Zone zone(std::move(name), std::move(filename));
    zone.rawPath = std::move(rawPath); // preserve the original "A/B/C" form for grouping/tooltips
    return zone;
}

std::vector<AccessPass> ParseDoc(const pugi::xml_document& doc, bool resolveCopies) {
    std::vector<AccessPass> passes;

    // Maps pass Key -> vector of copied pass Keys (to resolve <CopyZonesFrom>)
    std::unordered_map<std::string, std::vector<std::string>> copyDirectives;
    // Maps pass Key -> index in `passes` vector for fast lookup
    std::unordered_map<std::string, size_t> passLookup;

    for (pugi::xml_node passNode : doc.child("Privileges").children("AccessPass")) {
        std::string key = passNode.attribute("Key").as_string();
        ZoneType type = ParseZoneType(passNode.attribute("Type"));

        AccessPass pass(key, type);

        // 1. Parse direct <Zone> elements
        for (pugi::xml_node zoneNode : passNode.children("Zone")) {
            pass.zones.push_back(CreateZone(zoneNode.child_value()));
        }

        // 2. Track <CopyZonesFrom> directives if any exist
        if (resolveCopies) {
            for (pugi::xml_node copyNode : passNode.children("CopyZonesFrom")) {
                copyDirectives[key].emplace_back(copyNode.child_value());
            }
        }

        passLookup[key] = passes.size();
        passes.push_back(std::move(pass));
    }

    // 3. Resolve <CopyZonesFrom>
    if (resolveCopies) {
        for (const auto& [targetKey, sourceKeys] : copyDirectives) {
            auto targetIt = passLookup.find(targetKey);
            if (targetIt == passLookup.end()) continue;

            AccessPass& targetPass = passes[targetIt->second];

            for (const std::string& srcKey : sourceKeys) {
                auto srcIt = passLookup.find(srcKey);
                if (srcIt == passLookup.end()) continue;

                const AccessPass& srcPass = passes[srcIt->second];
                
                targetPass.zones.insert(
                    targetPass.zones.end(),
                    srcPass.zones.begin(),
                    srcPass.zones.end()
                );
            }
        }
    }

    return passes;
}

std::vector<AccessPass> ZoneParser::ParseFromFile(const std::filesystem::path& filePath, bool resolveCopies) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());

    if (!result) {
        std::cerr << "[PrivilegeParser] Failed to parse file '" << filePath.string()
            << "': " << result.description() << '\n';
        return {};
    }

    return ParseDoc(doc, resolveCopies);
}

std::vector<AccessPass> ZoneParser::ParseFromMemory(std::string_view xmlData, bool resolveCopies) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xmlData.data(), xmlData.size());

    if (!result) {
        std::cerr << "[PrivilegeParser] Failed to parse XML from buffer: "
            << result.description() << '\n';
        return {};
    }

    return ParseDoc(doc, resolveCopies);
}