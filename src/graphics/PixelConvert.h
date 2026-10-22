#pragma once

#include <cstddef>
#include <cstdint>
#include <tmmintrin.h>

// Headless Suyu frames are BGRA; the established D3D shared-texture path expects RGBA.
// Keep the conversion byte-identical to the scalar bridge, including alpha.
inline void bgraToRgba(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes) {
    const __m128i channels = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7,
                                            10, 9, 8, 11, 14, 13, 12, 15);
    std::size_t offset = 0;
    for (; offset + 16 <= bytes; offset += 16) {
        const auto pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + offset),
                         _mm_shuffle_epi8(pixels, channels));
    }
    for (; offset + 4 <= bytes; offset += 4) {
        destination[offset] = source[offset + 2];
        destination[offset + 1] = source[offset + 1];
        destination[offset + 2] = source[offset];
        destination[offset + 3] = source[offset + 3];
    }
}
