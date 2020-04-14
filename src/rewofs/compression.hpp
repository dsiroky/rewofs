/// Compression/decompression.
///
/// @file

#pragma once
#ifndef COMPRESSION_HPP__AZWPSEFL
#define COMPRESSION_HPP__AZWPSEFL

#include <cstdint>
#include <vector>

#include <gsl/span>

//==========================================================================
namespace rewofs {
//==========================================================================

std::vector<uint8_t> compress(const gsl::span<const uint8_t> buf);
std::vector<uint8_t> decompress(const gsl::span<const uint8_t> cbuf);

//==========================================================================
} // namespace rewofs

#endif /* include guard */
