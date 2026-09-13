#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <span>
#include <cstddef>
#include <vector>
#include "FileEntry.hpp"
#include "AssetExtraction/FileMapping.hpp"
#include "ByteReader.hpp"
#include "AssetData.hpp"


class Archive
{
private:
	// mmap
	FileMapping mapping;
	uint32_t version;
	std::unordered_map<std::string, FileEntry, std::hash<std::string_view>, std::equal_to<>> fileEntries;


	std::vector<std::byte> decompressData(const std::span<const std::byte>& compressedData, size_t uncompressedSize) const;

public:
	 explicit Archive(const std::filesystem::path& path);

	 FileEntry getFileEntry(ByteReader reader) const;

	 AssetData readAsset(
		 std::string_view filename
	 ) const;
};