#pragma once
#include <span>
#include <vector>
#include <variant>
#include <cstddef>

class AssetData
{
private:
	// Either borrowed (mmap) or owned (decompressed)
	std::variant<std::span<const std::byte>, std::vector<std::byte>> data;

public:
	AssetData(std::span<const std::byte> mappedData) : data(mappedData) {}
	AssetData(std::vector<std::byte> decompressedData) : data(std::move(decompressedData)) {}

	std::span<const std::byte> asSpan() const {
		if (std::holds_alternative<std::span<const std::byte>>(data)) {
			return std::get<std::span<const std::byte>>(data);
		}

		return std::get<std::vector<std::byte>>(data);
	}

};