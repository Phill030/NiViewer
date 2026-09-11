#pragma once
#include "Core/Zone.hpp"
#include <filesystem>
#include <vector>

class ZoneParser
{
public:
	/// @brief Parse Privileges.xml from disk.
	/// @param resolveCopies If true, resolves <CopyZonesFrom> references for Sample passes.
	static std::vector<AccessPass> ParseFromFile(const std::filesystem::path& filePath, bool resolveCopies = true);

	/// @brief Parse Privileges.xml from an in-memory buffer (e.g. extracted from a .wad archive).
	static std::vector<AccessPass> ParseFromMemory(std::string_view xmlData, bool resolveCopies = true);
};