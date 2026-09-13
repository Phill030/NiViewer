#include "Archive.hpp"
#include <zlib.h>
#include "ByteReader.hpp"
#include "FileEntry.hpp"

Archive::Archive(const std::filesystem::path& path) : mapping(path) {
	ByteReader reader(mapping.data());

	auto kiwadHeader = reader.readBytes(5);
	if (kiwadHeader.size() != 5 || std::string_view(reinterpret_cast<const char*>(kiwadHeader.data()), 5) != "KIWAD") {
		throw std::runtime_error("Invalid WAD file: missing KIWAD signature");
	}

	version = reader.readUInt32();
	auto fileCount = reader.readUInt32();

	if (version > 1) {
		reader.readBytes(1); // Padding
	}

	for (uint32_t i = 0; i < fileCount; i++) {
		auto entry = getFileEntry(reader);
		if (!entry.filename.empty()) {
			fileEntries.emplace(entry.filename, std::move(entry));
		}
	}
}

FileEntry Archive::getFileEntry(ByteReader reader) const {
	uint32_t offset = reader.readUInt32();
	uint32_t uncompressedSize = reader.readUInt32();
	uint32_t compressedSize = reader.readUInt32();
	bool isCompressed = reader.readUInt8() != 0;
	uint32_t crc32 = reader.readUInt32();
	std::string_view filename = reader.readString();

	if (filename.ends_with(".mp3") || filename.ends_with(".ogg") {
		isCompressed = false; // Some audio files are not compressed
	}

	return FileEntry(
		offset,
		uncompressedSize,
		compressedSize,
		isCompressed,
		crc32,
		filename
	);
}

AssetData Archive::readAsset(std::string_view filename) const {
	const auto it = fileEntries.find(filename);

	if (it == fileEntries.end()) {
		throw std::runtime_error("File not found in archive: " + std::string(filename));
	}

	const FileEntry& entry = it->second;

	const auto data = mapping.data();
	if (entry.offset > data.size()) {
		throw std::runtime_error("Invalid asset offset");
	}

	if (!entry.isCompressed) {
		if (entry.uncompressedSize > data.size() - entry.offset) {
			throw std::runtime_error("Asset extends beyond WAD");
		}

		return AssetData(data.subspan(entry.offset, entry.uncompressedSize));
	}

	if (entry.compressedSize > data.size() - entry.offset) {
		throw std::runtime_error("Compressed asset extends beyond WAD");
	}

	const auto compressedData = data.subspan(entry.offset, entry.compressedSize);
	return AssetData(decompressData(compressedData, entry.uncompressedSize));
}

std::vector<std::byte> Archive::decompressData(const std::span<const std::byte>& compressedData, size_t uncompressedSize) const {
	std::vector<std::byte> buffer(uncompressedSize);

	unsigned long outputSize = static_cast<unsigned long>(uncompressedSize);

	const int result = uncompress(
		reinterpret_cast<Bytef*>(buffer.data()),
		&outputSize,
		reinterpret_cast<const Bytef*>(compressedData.data()),
		static_cast<unsigned long>(compressedData.size())
	);

	if (result != Z_OK) {
		throw std::runtime_error("Failed to decompress data");
	}

	buffer.resize(outputSize);
	return buffer;
}