#pragma once
#include <span>
#include <string>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <type_traits>

class ByteReader
{
public:
	explicit ByteReader(std::span<const std::byte> data, size_t offset = 0) : _data(data), _offset(offset) {}
	
	size_t position() const { return _offset; }
	size_t size() const { return _data.size(); }
	size_t remaining() const { return _data.size() - _offset; }

	void seek(size_t pos) {
		if (pos > _data.size()) {
			throw std::out_of_range("Seek position is out of range");
		}
		_offset = pos;
	}

	void skip(size_t count) {
		requireBytes(count);
		_offset += count;
	}

	template <typename T>
	T read() {
		static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
		requireBytes(sizeof(T));
		T value;
		std::memcpy(&value, data_.data() + offset_, sizeof(T));
		offset_ += sizeof(T);
		return value;
	}

	uint8_t readUInt8() { return read<uint8_t>(); }
	int8_t readInt8() { return read<int8_t>(); }
	uint16_t readUInt16() { return read<uint16_t>(); }
	int16_t readInt16() { return read<int16_t>(); }
	uint32_t readUInt32() { return read<uint32_t>(); }
	int32_t readInt32() { return read<int32_t>(); }
	uint64_t readUInt64() { return read<uint64_t>(); }
	int64_t readInt64() { return read<int64_t>(); }
	float readFloat() { return read<float>(); }
	double readDouble() { return read<double>(); }

	// returned span is only valid as long as the original buffer is.
	std::span<const std::byte> readBytes(size_t length) {
		requireBytes(length);
		auto result = _data.subspan(_offset, length);
		_offset += length;
		return result;
	}

	std::string_view readString() {
		const auto length = static_cast<size_t>(readUInt32());
		auto bytes = readBytes(length);
		std::string_view view(reinterpret_cast<const char*>(bytes.data()), bytes.size());

		const auto pos = view.find_last_not_of('\0');
		view = (pos == std::string_view::npos) ? std::string_view{} : view.substr(0, pos + 1);

		return view;
	}

private:
	std::span<const std::byte> _data;
	size_t _offset;

	void requireBytes(size_t count) const {
		if (count > remaining()) {
			throw std::runtime_error("ByteReader: attempt to read past end of buffer");
		}
	}
};