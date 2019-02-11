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

#if !defined(MEM_SIMD_SCANNER_BRICK_H)
#define MEM_SIMD_SCANNER_BRICK_H

#include "pattern.h"

#if !defined(MEM_SIMD_SCANNER_USE_MEMCHR)
# if defined(MEM_SIMD_AVX2)
#  include <immintrin.h>
# elif defined(MEM_SIMD_SSE3)
#  include <emmintrin.h>
#  include <pmmintrin.h>
# elif defined(MEM_SIMD_SSE2)
#  include <emmintrin.h>
# else
#  define MEM_SIMD_SCANNER_USE_MEMCHR
# endif
#endif

#if !defined(MEM_SIMD_SCANNER_USE_MEMCHR)
# include "arch.h"
#endif

namespace mem
{
    class simd_scanner
        : public scanner_base<simd_scanner>
    {
    private:
        const pattern* pattern_ {nullptr};
        std::size_t skip_pos_ {SIZE_MAX};

    public:
        simd_scanner() = default;

        simd_scanner(const pattern& pattern);

        pointer scan(region range) const;
    };

    const byte* find_byte(const byte* ptr, byte value, std::size_t num);

    inline simd_scanner::simd_scanner(const pattern& _pattern)
        : pattern_(&_pattern)
        , skip_pos_(_pattern.get_skip_pos())
    { }

    inline pointer simd_scanner::scan(region range) const
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

        const byte* current = region_base;
        const byte* const end = region_end - original_size + 1;

        const std::size_t last = trimmed_size - 1;

        const byte* const pat_bytes = pattern_->bytes();

        const std::size_t skip_pos = skip_pos_;

        if (skip_pos != SIZE_MAX)
        {
            if (pattern_->needs_masks())
            {
                const byte* const pat_masks = pattern_->masks();

                while (MEM_LIKELY(current < end))
                {
                    std::size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                            break;

                        if (MEM_UNLIKELY(i == 0))
                            return current;

                        --i;
                    } while (true);

                    ++current;
                    current = find_byte(current + skip_pos, pat_bytes[skip_pos], static_cast<std::size_t>(end - current)) - skip_pos;
                }

                return nullptr;
            }
            else
            {
                while (MEM_LIKELY(current < end))
                {
                    std::size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                            break;

                        if (MEM_UNLIKELY(i == 0))
                            return current;

                        --i;
                    } while (true);

                    ++current;
                    current = find_byte(current + skip_pos, pat_bytes[skip_pos], static_cast<std::size_t>(end - current)) - skip_pos;
                }

                return nullptr;
            }
        }
        else
        {
            const byte* const pat_masks = pattern_->masks();

            while (MEM_LIKELY(current < end))
            {
                std::size_t i = last;

                do
                {
                    if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                        break;

                    if (MEM_UNLIKELY(i == 0))
                        return current;

                    --i;
                    break;
                } while (true);

                ++current;
            }

            return nullptr;
        }
    }

    MEM_STRONG_INLINE const byte* find_byte(const byte* ptr, byte value, std::size_t num)
    {
#if !defined(MEM_SIMD_SCANNER_USE_MEMCHR)
# if defined(MEM_SIMD_AVX2)
#  define l_SIMD_TYPE __m256i
#  define l_SIMD_FILL(x) _mm256_set1_epi8(static_cast<char>(x))
#  define l_SIMD_LOAD(x) _mm256_lddqu_si256(x)
#  define l_SIMD_CMPEQ_MASK(x, y) _mm256_movemask_epi8(_mm256_cmpeq_epi8(x, y))
# elif defined(MEM_SIMD_SSE3)
#  define l_SIMD_TYPE __m128i
#  define l_SIMD_FILL(x) _mm_set1_epi8(static_cast<char>(x))
#  define l_SIMD_LOAD(x) _mm_lddqu_si128(x)
#  define l_SIMD_CMPEQ_MASK(x, y) _mm_movemask_epi8(_mm_cmpeq_epi8(x, y))
# elif defined(MEM_SIMD_SSE2)
#  define l_SIMD_TYPE __m128i
#  define l_SIMD_FILL(x) _mm_set1_epi8(static_cast<char>(x))
#  define l_SIMD_LOAD(x) _mm_loadu_si128(x)
#  define l_SIMD_CMPEQ_MASK(x, y) _mm_movemask_epi8(_mm_cmpeq_epi8(x, y))
# else
#  error Sorry, No Potatoes
# endif


        if (MEM_LIKELY(num >= sizeof(l_SIMD_TYPE)))
        {
            const l_SIMD_TYPE simd_value = l_SIMD_FILL(value);

            do
            {
                const int mask = l_SIMD_CMPEQ_MASK(l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(ptr)), simd_value);

                if (MEM_UNLIKELY(mask != 0))
                    return ptr + bsf(static_cast<unsigned int>(mask));

                num -= sizeof(l_SIMD_TYPE);
                ptr += sizeof(l_SIMD_TYPE);
            } while (MEM_LIKELY(num >= sizeof(l_SIMD_TYPE)));
        }

        while (MEM_LIKELY(num != 0))
        {
            if (MEM_UNLIKELY(*ptr == value))
                return ptr;

            --num;
            ++ptr;
        }

        return ptr;

# undef l_SIMD_TYPE
# undef l_SIMD_FILL
# undef l_SIMD_LOAD
# undef l_SIMD_CMPEQ_MASK
#else
        const byte* result = static_cast<const byte*>(std::memchr(ptr, value, num));

        if (MEM_UNLIKELY(result == nullptr))
            result = ptr + num;

        return result;
#endif
    }
}

#endif // MEM_SIMD_SCANNER_BRICK_H
