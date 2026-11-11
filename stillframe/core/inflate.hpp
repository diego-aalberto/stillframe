// DEFLATE decoder (RFC 1951/1950) and CRC-32. Used by the ZIP reader and the PNG decoder.
#pragma once
#include "common.hpp"

namespace sf {
constexpr size_t kMaxInflate = size_t(1) << 30; // 1 GiB guard against decompression bombs
Bytes inflate_raw(const uint8_t* data, size_t n, size_t size_hint = 0, size_t max_out = kMaxInflate);
Bytes zlib_decompress(const uint8_t* data, size_t n, size_t size_hint = 0, size_t max_out = kMaxInflate);
uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0);
} // namespace sf
