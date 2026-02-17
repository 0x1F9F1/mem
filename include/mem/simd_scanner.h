/*
    Copyright 2018 Brick

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef MEM_SIMD_SCANNER_BRICK_H
#define MEM_SIMD_SCANNER_BRICK_H

#include "pattern.h"

#if !defined(MEM_SIMD_SCANNER_USE_STD_FIND)
#    if defined(MEM_SIMD_AVX2)
#        include <immintrin.h>
#    elif defined(MEM_SIMD_SSE2)
#        include <emmintrin.h>
#    else
#        define MEM_SIMD_SCANNER_USE_STD_FIND
#    endif
#endif

#if defined(MEM_SIMD_SCANNER_USE_STD_FIND)
#    include <algorithm>
#else
#    include "arch.h"
#endif

namespace mem
{
    class simd_scanner : public scanner_base<simd_scanner>
    {
    private:
        const pattern* pattern_ {nullptr};
        std::size_t skip_pos_ {SIZE_MAX};
        std::vector<std::size_t> suffix_skips_ {};

    public:
        simd_scanner() = default;

        simd_scanner(const pattern& pattern);
        simd_scanner(const pattern& pattern, const byte* frequencies);

        pointer scan(region range) const;

        static const byte* default_frequencies() noexcept;
    };

    inline simd_scanner::simd_scanner(const pattern& _pattern)
        : simd_scanner(_pattern, default_frequencies())
    {}

    inline simd_scanner::simd_scanner(const pattern& _pattern, const byte* frequencies)
        : pattern_(&_pattern)
    {
        const std::size_t trimmed_size = pattern_->trimmed_size();
        const byte* const bytes = pattern_->bytes();
        const byte* const masks = pattern_->masks();

        // Try to pick a byte which is uncommon in both the needle and haystack
        std::size_t hist[256] {};
        const std::size_t hist_factor = 50;

        for (std::size_t i = 0; i < trimmed_size; ++i)
        {
            if (masks[i] == 0xFF)
                hist[bytes[i]] += hist_factor;
        }

        std::size_t min_freq = SIZE_MAX;

        for (std::size_t i = 0; i < trimmed_size; ++i)
        {
            if (masks[i] == 0xFF)
            {
                std::uint8_t v = bytes[i];
                std::size_t f = hist[v] + frequencies[v];

                if (f <= min_freq)
                {
                    skip_pos_ = i;
                    min_freq = f;
                }
            }
        }

        if (skip_pos_ == SIZE_MAX)
            return;

        // Once we've found a match for data[skip_pos], we need to check the rest.
        // If it isn't a full match, calculate how far forward we can skip, based on how much of the suffix was correct.
        // This is a similar idea to the Boyer-Moore good-suffix rule, but also incorporates the match at skip_pos.

        suffix_skips_.resize(trimmed_size);

        std::size_t skip = 1;

        const auto matches = [&](std::size_t i) {
            if (i < skip)
                return true;
            std::size_t j = i - skip;
            return ((bytes[i] ^ bytes[j]) & (masks[i] & masks[j])) == 0;
        };

        for (std::size_t i = trimmed_size; i > 0; --i)
        {
            for (; skip < trimmed_size; ++skip)
            {
                if (!matches(skip_pos_))
                    continue;

                bool could_match = true;

                for (std::size_t j = i; j < trimmed_size; ++j)
                {
                    if (!matches(j))
                    {
                        could_match = false;
                        break;
                    }
                }

                if (could_match)
                    break;
            }

            suffix_skips_[i - 1] = skip;
        }
    }

    MEM_STRONG_INLINE const byte* simd_scanner::default_frequencies() noexcept
    {
        // clang-format off
        static constexpr const byte frequencies[256]
        {
            0xFF,0xFB,0xF2,0xEE,0xEC,0xE7,0xDC,0xC8,0xED,0xB7,0xCC,0xC0,0xD3,0xCD,0x89,0xFA,
            0xF3,0xD6,0x8D,0x83,0xC1,0xAA,0x7A,0x72,0xC6,0x60,0x3E,0x2E,0x98,0x69,0x39,0x7C,
            0xEB,0x76,0x24,0x34,0xF9,0x50,0x04,0x07,0xE5,0xAC,0x53,0x65,0x9B,0x4D,0x6D,0x5C,
            0xDA,0x93,0x7F,0xCB,0x92,0x49,0x43,0x09,0xBA,0x8E,0x1E,0x91,0x8A,0x5B,0x11,0xA1,
            0xE8,0xF5,0x9E,0xAD,0xEF,0xE6,0x79,0x7B,0xFE,0xE0,0x1F,0x54,0xE4,0xBD,0x7D,0x6A,
            0xDF,0x67,0x7E,0xA4,0xB6,0xAF,0x88,0xA0,0xC3,0xA9,0x26,0x77,0xD1,0x71,0x61,0xC2,
            0x9A,0xCA,0x29,0x9F,0xD8,0xE2,0xD0,0x6E,0xB4,0xB8,0x25,0x3C,0xBF,0x73,0xB5,0xCF,
            0xD4,0x01,0xCE,0xBE,0xF1,0xDB,0x52,0x37,0x9D,0x63,0x02,0x6B,0x80,0x45,0x2B,0x95,
            0xE1,0xC4,0x36,0xF0,0xD5,0xE3,0x57,0x9C,0xB1,0xF7,0x82,0xFC,0x42,0xF6,0x18,0x33,
            0xD2,0x48,0x05,0x0F,0x41,0x1D,0x03,0x27,0x70,0x10,0x00,0x08,0x55,0x16,0x2F,0x0E,
            0x94,0x35,0x2C,0x40,0x6F,0x3B,0x1C,0x28,0x90,0x68,0x81,0x4B,0x56,0x30,0x2A,0x3D,
            0x97,0x17,0x06,0x13,0x32,0x0B,0x5A,0x75,0xA5,0x86,0x78,0x4F,0x2D,0x51,0x46,0x5F,
            0xE9,0xDE,0xA2,0xDD,0xC9,0x4C,0xAB,0xBB,0xC7,0xB9,0x74,0x8F,0xF8,0x6C,0x85,0x8B,
            0xC5,0x84,0x8C,0x66,0x21,0x23,0x64,0x59,0xA3,0x87,0x44,0x58,0x3A,0x0D,0x12,0x19,
            0xAE,0x5E,0x3F,0x38,0x31,0x22,0x0A,0x14,0xF4,0xD9,0x20,0xB0,0xB2,0x1A,0x0C,0x15,
            0xB3,0x47,0x5D,0xEA,0x4A,0x1B,0x99,0xBC,0xD7,0xA6,0x62,0x4E,0xA8,0x96,0xA7,0xFD,
        };
        // clang-format on

        return frequencies;
    }

    MEM_NOINLINE inline pointer simd_scanner::scan(region range) const
    {
        const std::size_t trimmed_size = pattern_->trimmed_size();

        if (!trimmed_size)
            return nullptr;

        const std::size_t original_size = pattern_->size();
        const std::size_t region_size = range.size;

        if (original_size > region_size)
            return nullptr;

        const byte* const region_base = range.start.as<const byte*>();
        const byte* const region_end = region_base + region_size;

        const byte* ptr = region_base;
        const byte* end = region_end - original_size + 1;

        const std::size_t last = trimmed_size - 1;

        const byte* const pat_bytes = pattern_->bytes();
        const byte* const pat_masks = pattern_->masks();

        const std::size_t skip_pos = skip_pos_;

        if (skip_pos == SIZE_MAX)
        {
            while (MEM_LIKELY(ptr < end))
            {
                [[MEM_ATTR_LIKELY]];

                for (std::size_t i = last; MEM_LIKELY((ptr[i] & pat_masks[i]) == pat_bytes[i]); --i)
                {
                    [[MEM_ATTR_LIKELY]];

                    if (MEM_UNLIKELY(i == 0)) [[MEM_ATTR_UNLIKELY]]
                        return ptr;
                }

                ++ptr;
            }

            return nullptr;
        }

        const std::size_t* suffix_skips = suffix_skips_.data();
        const std::size_t last_skip = suffix_skips[last];

        ptr += skip_pos;
        end += skip_pos;
        const byte skip_value = pat_bytes[skip_pos];

#if !defined(MEM_SIMD_SCANNER_USE_STD_FIND)
#    if defined(MEM_SIMD_AVX2)
#        define l_SIMD_TYPE __m256i
#        define l_SIMD_FILL(x) _mm256_set1_epi8(static_cast<char>(x))
#        define l_SIMD_LOAD(x) _mm256_loadu_si256(x)
#        define l_SIMD_CMPEQ(x, y) _mm256_cmpeq_epi8(x, y)
#        define l_SIMD_MOVEMASK(x) static_cast<unsigned int>(_mm256_movemask_epi8(x))
#    elif defined(MEM_SIMD_SSE2)
#        define l_SIMD_TYPE __m128i
#        define l_SIMD_FILL(x) _mm_set1_epi8(static_cast<char>(x))
#        define l_SIMD_LOAD(x) _mm_loadu_si128(x)
#        define l_SIMD_CMPEQ(x, y) _mm_cmpeq_epi8(x, y)
#        define l_SIMD_MOVEMASK(x) static_cast<unsigned int>(_mm_movemask_epi8(x))
#    else
#        error Sorry, No Potatoes
#    endif

#    define l_SIMD_SIZEOF(N) (sizeof(l_SIMD_TYPE) * N)
#    define l_SIMD_CMPEQ_MASK(x, y) l_SIMD_MOVEMASK(l_SIMD_CMPEQ(x, y))

        const l_SIMD_TYPE simd_value = l_SIMD_FILL(skip_value);

    retry:
        while (MEM_LIKELY((end - ptr) >= l_SIMD_SIZEOF(4)))
        {
            [[MEM_ATTR_LIKELY]];

            const l_SIMD_TYPE value0 = l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr));
            const l_SIMD_TYPE value1 = l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr + l_SIMD_SIZEOF(1)));
            const l_SIMD_TYPE value2 = l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr + l_SIMD_SIZEOF(2)));
            const l_SIMD_TYPE value3 = l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr + l_SIMD_SIZEOF(3)));

            ptr += l_SIMD_SIZEOF(4);

            {
                const auto mask = l_SIMD_CMPEQ_MASK(value0, simd_value);

                if (MEM_UNLIKELY(mask != 0)) [[MEM_ATTR_UNLIKELY]]
                {
                    ptr = ptr - l_SIMD_SIZEOF(4) + bsf(mask);
                    goto match;
                }
            }

            {
                const auto mask = l_SIMD_CMPEQ_MASK(value1, simd_value);

                if (MEM_UNLIKELY(mask != 0)) [[MEM_ATTR_UNLIKELY]]
                {
                    ptr = ptr - l_SIMD_SIZEOF(3) + bsf(mask);
                    goto match;
                }
            }

            {
                const auto mask = l_SIMD_CMPEQ_MASK(value2, simd_value);

                if (MEM_UNLIKELY(mask != 0)) [[MEM_ATTR_UNLIKELY]]
                {
                    ptr = ptr - l_SIMD_SIZEOF(2) + bsf(mask);
                    goto match;
                }
            }

            {
                const auto mask = l_SIMD_CMPEQ_MASK(value3, simd_value);

                if (MEM_UNLIKELY(mask != 0)) [[MEM_ATTR_UNLIKELY]]
                {
                    ptr = ptr - l_SIMD_SIZEOF(1) + bsf(mask);
                    goto match;
                }
            }
        }

        while (MEM_LIKELY((end - ptr) >= l_SIMD_SIZEOF(1)))
        {
            [[MEM_ATTR_LIKELY]];

            const auto mask = l_SIMD_CMPEQ_MASK(l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr)), simd_value);

            ptr += l_SIMD_SIZEOF(1);

            if (MEM_UNLIKELY(mask != 0)) [[MEM_ATTR_UNLIKELY]]
            {
                ptr = ptr - l_SIMD_SIZEOF(1) + bsf(mask);
                goto match;
            };
        }

        while (MEM_LIKELY(ptr != end))
        {
            [[MEM_ATTR_LIKELY]];

            if (MEM_UNLIKELY(*ptr == skip_value)) [[MEM_ATTR_UNLIKELY]]
                goto match;

            ++ptr;
        }

        return nullptr;

#    undef l_SIMD_TYPE
#    undef l_SIMD_FILL
#    undef l_SIMD_LOAD
#    undef l_SIMD_LOADU
#    undef l_SIMD_CMPEQ
#    undef l_SIMD_MOVEMASK
#    undef l_SIMD_SIZEOF
#    undef l_SIMD_CMPEQ_MASK
#else
    retry:
        ptr = std::find(ptr, end, skip_value);

        if (ptr == end)
            return nullptr;

        goto match;
#endif

    match:
        const byte* here = ptr - skip_pos;

        if (MEM_LIKELY((here[last] & pat_masks[last]) != pat_bytes[last])) [[MEM_ATTR_LIKELY]]
        {
            if (MEM_UNLIKELY((end - ptr) <= last_skip)) [[MEM_ATTR_UNLIKELY]]
                return nullptr;

            ptr += last_skip;
            goto retry;
        }

        std::size_t i = last;

        while (true)
        {
            if (MEM_UNLIKELY(i == 0)) [[MEM_ATTR_UNLIKELY]]
                return here;

            --i;

            if (MEM_LIKELY((here[i] & pat_masks[i]) != pat_bytes[i])) [[MEM_ATTR_LIKELY]]
                break;
        }

        std::size_t skip = suffix_skips[i];

        if (MEM_UNLIKELY((end - ptr) <= skip)) [[MEM_ATTR_UNLIKELY]]
            return nullptr;

        ptr += skip;
        goto retry;
    }
} // namespace mem

#endif // MEM_SIMD_SCANNER_BRICK_H
