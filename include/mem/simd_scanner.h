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
# if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(_BitScanForward)
# endif
#endif

namespace mem
{
    class simd_scanner
    {
    private:
        const pattern* pattern_ {nullptr};
        size_t skip_pos_ {SIZE_MAX};

    public:
        simd_scanner(const pattern& pattern);

        template <typename UnaryPredicate>
        pointer operator()(region range, UnaryPredicate pred) const;
    };

    const byte* find_byte(const byte* b, const byte* e, byte c);

    MEM_STRONG_INLINE simd_scanner::simd_scanner(const pattern& _pattern)
        : pattern_(&_pattern)
    {
        const byte* masks = pattern_->masks();

        for (size_t i = pattern_->trimmed_size(); i--;)
        {
            if (masks[i] == 0xFF)
            {
                skip_pos_ = i;

                break;
            }
        }
    }

    template <typename UnaryPredicate>
    inline pointer simd_scanner::operator()(region range, UnaryPredicate pred) const
    {
        const size_t trimmed_size = pattern_->trimmed_size();

        if (!trimmed_size)
            return nullptr;

        const size_t original_size = pattern_->size();
        const size_t region_size = range.size;

        if (original_size > region_size)
            return nullptr;

        const byte* const region_base = range.start.as<const byte*>();
        const byte* const region_end = region_base + region_size;

        const byte* current = region_base;
        const byte* const end = region_end - original_size + 1;

        const size_t last = trimmed_size - 1;

        const byte* const pat_bytes = pattern_->bytes();

        const size_t skip_pos = skip_pos_;

        if (skip_pos != SIZE_MAX)
        {
            if (pattern_->needs_masks())
            {
                const byte* const pat_masks = pattern_->masks();

                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    current = find_byte(current + 1 + skip_pos, end + skip_pos, pat_bytes[skip_pos]) - skip_pos;
                }

                return nullptr;
            }
            else
            {
                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    current = find_byte(current + 1 + skip_pos, end + skip_pos, pat_bytes[skip_pos]) - skip_pos;
                }

                return nullptr;
            }
        }
        else
        {
            const byte* const pat_masks = pattern_->masks();

            while (MEM_LIKELY(current < end))
            {
                size_t i = last;

                do
                {
                    if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                        break;

                    if (MEM_LIKELY(i))
                    {
                        --i;

                        continue;
                    }

                    if (MEM_UNLIKELY(pred(current)))
                        return current;

                    break;
                } while (true);

                ++current;
            }

            return nullptr;
        }
    }

#if !defined(MEM_SIMD_SCANNER_USE_MEMCHR)
# if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4)))
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        return __builtin_ctz(x);
    }
# elif defined(_MSC_VER)
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        unsigned long result;
        _BitScanForward(&result, static_cast<unsigned long>(x));
        return static_cast<int>(result);
    }
# else
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        int result;
        asm("bsf %1, %0" : "=r" (result) : "rm" (x));
        return result;
    }
# endif
#endif

    MEM_STRONG_INLINE const byte* find_byte(const byte* b, const byte* e, byte c)
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

        const l_SIMD_TYPE q = l_SIMD_FILL(c);

        for (; MEM_LIKELY(b + sizeof(l_SIMD_TYPE) <= e); b += sizeof(l_SIMD_TYPE))
        {
            const int mask = l_SIMD_CMPEQ_MASK(l_SIMD_LOAD(reinterpret_cast<const l_SIMD_TYPE*>(b)), q);

            if (MEM_UNLIKELY(mask))
                return b + bsf(static_cast<unsigned int>(mask));
        }

        for (; MEM_LIKELY(b < e); ++b)
            if (MEM_UNLIKELY(*b == c))
                return b;

        return e;

#undef l_SIMD_TYPE
#undef l_SIMD_FILL
#undef l_SIMD_LOAD
#undef l_SIMD_CMPEQ_MASK
#else
        if (b < e)
        {
            b = static_cast<const byte*>(std::memchr(b, c, e - b));

            if (b == nullptr)
                b = e;
        }

        return b;
#endif
    }
}

#endif // MEM_SIMD_SCANNER_BRICK_H
