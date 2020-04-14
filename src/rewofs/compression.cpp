/// @copydoc compression.hpp
///
/// @file

#include <cassert>

#include <zstd.h>

#include "rewofs/compression.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

std::vector<uint8_t> compress(const gsl::span<const uint8_t> buf)
{
    std::vector<uint8_t> cbuf(ZSTD_compressBound(buf.size()));
    const auto csize
        = ZSTD_compress(cbuf.data(), cbuf.size(), buf.data(), buf.size(), 1);
    if (ZSTD_isError(csize))
    {
        throw std::runtime_error{ZSTD_getErrorName(csize)};
    }
    cbuf.resize(csize);
    return cbuf;
}

//--------------------------------------------------------------------------

std::vector<uint8_t> decompress(const gsl::span<const uint8_t> cbuf)
{
    const auto unsize = ZSTD_getFrameContentSize(cbuf.data(), cbuf.size());
    if (ZSTD_isError(unsize))
    {
        throw std::runtime_error{ZSTD_getErrorName(unsize)};
    }
    std::vector<uint8_t> unbuf(unsize);
    const auto dsize
        = ZSTD_decompress(unbuf.data(), unbuf.size(), cbuf.data(), cbuf.size());
    if (ZSTD_isError(dsize))
    {
        throw std::runtime_error{ZSTD_getErrorName(dsize)};
    }
    assert(dsize == unsize);
    return unbuf;
}

//==========================================================================
} // namespace rewofs
