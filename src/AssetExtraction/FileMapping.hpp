#pragma once
#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <span>

class FileMapping
{
public:
	explicit FileMapping(const std::filesystem::path& path);

	~FileMapping();

	FileMapping(const FileMapping&) = delete;
	FileMapping& operator=(const FileMapping&) = delete;

	std::span<const std::byte> data() const noexcept;

private:
	const std::byte* m_data;
	std::size_t m_size;
#if defined(_WIN32)
	void* m_mappingHandle = nullptr; // HANDLE, kept as void* to avoid <windows.h> in the header
#endif
};