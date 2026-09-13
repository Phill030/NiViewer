#include "FileMapping.hpp"
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(_WIN32)

namespace {
	struct WindowsHandles
	{
		HANDLE file = INVALID_HANDLE_VALUE;
		HANDLE mapping = nullptr;
	};
}

FileMapping::FileMapping(const std::filesystem::path& path) : m_data(nullptr), m_size(0) {
	HANDLE file = CreateFileW(
		path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (file == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("FileMapping: failed to open file: " + path.string());
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(file, &fileSize)) {
		CloseHandle(file);
		throw std::runtime_error("FileMapping: failed to get file size: " + path.string());
	}

	if (fileSize.QuadPart == 0) {
		CloseHandle(file);
		throw std::runtime_error("FileMapping: cannot map empty file: " + path.string());
	}

	HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (mapping == nullptr) {
		CloseHandle(file);
		throw std::runtime_error("FileMapping: CreateFileMapping failed: " + path.string());
	}

	void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	if (view == nullptr) {
		CloseHandle(mapping);
		CloseHandle(file);
		throw std::runtime_error("FileMapping: MapViewOfFile failed: " + path.string());
	}

	// File handle is not needed once the mapping+view exist, but the
	// mapping handle must stay alive until UnmapViewOfFile is called.
	CloseHandle(file);

	m_data = static_cast<const std::byte*>(view);
	m_size = static_cast<std::size_t>(fileSize.QuadPart);
	m_mappingHandle = mapping;
}

FileMapping::~FileMapping() {
	if (m_data != nullptr) {
		UnmapViewOfFile(m_data);
	}

	if (m_mappingHandle != nullptr) {
		CloseHandle(m_mappingHandle);
	}
}

#else // POSIX

FileMapping::FileMapping(const std::filesystem::path& path) : m_data(nullptr), m_size(0) {
	const int fd = open(path.c_str(), O_RDONLY);
	if (fd == -1) {
		throw std::runtime_error("FileMapping: failed to open file: " + path.string());
	}

	struct stat st {};
	if (fstat(fd, &st) == -1) {
		close(fd);
		throw std::runtime_error("FileMapping: fstat failed: " + path.string());
	}

	if (st.st_size == 0) {
		close(fd);
		throw std::runtime_error("FileMapping: cannot map empty file: " + path.string());
	}

	void* mapped = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);

	// fd is not needed after mmap; the mapping stays valid once established.
	close(fd);

	if (mapped == MAP_FAILED) {
		throw std::runtime_error("FileMapping: mmap failed: " + path.string());
	}

	m_data = static_cast<const std::byte*>(mapped);
	m_size = static_cast<std::size_t>(st.st_size);
}

FileMapping::~FileMapping() {
	if (m_data != nullptr) {
		munmap(const_cast<std::byte*>(m_data), m_size);
	}
}

#endif

std::span<const std::byte> FileMapping::data() const noexcept {
	return std::span<const std::byte>(m_data, m_size);
}