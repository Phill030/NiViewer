#pragma once
#include <cstdint>
#include <string>
#include <utility>


class FileEntry
{
public:
	uint32_t offset;
	uint32_t uncompressedSize;
	uint32_t compressedSize;
	bool isCompressed;
	uint32_t crc32;
	std::string filename;

	FileEntry(uint32_t offset, uint32_t uncompressedSize, uint32_t compressedSize, bool isCompressed, uint32_t crc32, std::string filename)
		: offset(offset), uncompressedSize(uncompressedSize), compressedSize(compressedSize), isCompressed(isCompressed), crc32(crc32), filename(std::move(filename)) {
	}
};