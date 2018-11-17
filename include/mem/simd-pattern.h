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

#if !defined(MEM_SIMD_PATTERN_BRICK_H)
#define MEM_SIMD_PATTERN_BRICK_H

#include "pattern.h"

#if !defined(MEM_SIMD_SSE2) && !defined(MEM_SIMD_PATTERN_USE_MEMCHR)
# define MEM_SIMD_PATTERN_USE_MEMCHR
#endif

#if !defined(MEM_SIMD_PATTERN_USE_MEMCHR)
# if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(_BitScanForward)
# endif
# if defined(MEM_SIMD_AVX2)
#  include <immintrin.h>
# elif defined(MEM_SIMD_SSE2)
#  include <emmintrin.h>
#  if defined(MEM_SIMD_SSE3)
#   include <pmmintrin.h>
#  endif
# else
#  error Sorry, No Potatoes
# endif
#endif

namespace mem
{
    class simd_pattern
    {
    private:
        const byte* bytes_ {nullptr};
        const byte* masks_ {nullptr};
        size_t size_ {0};
        size_t trimmed_size_ {0};
        bool needs_masks_ {true};
        size_t skip_pos_ {SIZE_MAX};

        size_t get_skip_pos() const noexcept;

    public:
        simd_pattern(const mem::pattern& pattern);

        template <typename UnaryPredicate>
        pointer scan_predicate(region range, UnaryPredicate pred) const;
    };

    const byte* find_byte(const byte* b, const byte* e, byte c);

    MEM_STRONG_INLINE simd_pattern::simd_pattern(const mem::pattern& pattern)
        : bytes_(pattern.bytes())
        , masks_(pattern.masks())
        , size_(pattern.size())
        , trimmed_size_(pattern.trimmed_size())
        , needs_masks_(pattern.needs_masks())
        , skip_pos_(get_skip_pos())
    { }

    MEM_STRONG_INLINE size_t simd_pattern::get_skip_pos() const noexcept
    {
        for (size_t i = trimmed_size_; i--;)
        {
            if (masks_[i] == 0xFF)
            {
                return i;
            }
        }

        return SIZE_MAX;
    }

    template <typename UnaryPredicate>
    MEM_NOINLINE pointer simd_pattern::scan_predicate(region range, UnaryPredicate pred) const
    {
        if (!trimmed_size_)
        {
            return nullptr;
        }

        const size_t original_size = size_;
        const size_t region_size = range.size;

        if (original_size > region_size)
        {
            return nullptr;
        }

        const byte* const region_base = range.start.as<const byte*>();
        const byte* const region_end = region_base + region_size;

        const byte* current = region_base;
        const byte* const end = region_end - original_size + 1;

        const size_t last = trimmed_size_ - 1;

        const byte* const pat_bytes = bytes_;

        const size_t skip_pos = skip_pos_;

        if (skip_pos != SIZE_MAX)
        {
            if (needs_masks_)
            {
                const byte* const pat_masks = masks_;

                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                        {
                            break;
                        }

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                        {
                            return current;
                        }

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
                        {
                            break;
                        }

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                        {
                            return current;
                        }

                        break;
                    } while (true);

                    current = find_byte(current + 1 + skip_pos, end + skip_pos, pat_bytes[skip_pos]) - skip_pos;
                }

                return nullptr;
            }
        }
        else
        {
            const byte* const pat_masks = masks_;

            while (MEM_LIKELY(current < end))
            {
                size_t i = last;

                do
                {
                    if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                    {
                        break;
                    }

                    if (MEM_LIKELY(i))
                    {
                        --i;

                        continue;
                    }

                    if (MEM_UNLIKELY(pred(current)))
                    {
                        return current;
                    }

                    break;
                } while (true);

                ++current;
            }

            return nullptr;
        }
    }

#if !defined(MEM_SIMD_PATTERN_USE_MEMCHR)
# if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4)))
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        return __builtin_ctz(x);
    }
# elif defined(_MSC_VER)
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        unsigned long result = 0;
        _BitScanForward(&result, static_cast<unsigned long>(x));
        return static_cast<int>(result);
    }
# else
    MEM_STRONG_INLINE int bsf(unsigned int x) noexcept
    {
        int result = 0;
        asm("bsf %1, %0" : "=r" (result) : "rm" (x));
        return result;
    }
# endif
#endif

    MEM_STRONG_INLINE const byte* find_byte(const byte* b, const byte* e, byte c)
    {
#if !defined(MEM_SIMD_PATTERN_USE_MEMCHR)
# if defined(MEM_SIMD_AVX2)
        const __m256i q = _mm256_set1_epi8(c);

        for (; MEM_LIKELY(b + 32 <= e); b += 32)
        {
            int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_lddqu_si256(reinterpret_cast<const __m256i*>(b)), q));

            if (MEM_UNLIKELY(mask))
            {
                return b + bsf(static_cast<unsigned int>(mask));
            }
        }

        for (; MEM_LIKELY(b < e); ++b)
            if (MEM_UNLIKELY(*b == c))
                return b;

        return e;
# elif defined(MEM_SIMD_SSE2)
        const __m128i q = _mm_set1_epi8(c);

        for (; MEM_LIKELY(b + 16 <= e); b += 16)
        {
            int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(
# if defined(MEM_SIMD_SSE3)
                _mm_lddqu_si128
# else
                _mm_loadu_si128
# endif
                (reinterpret_cast<const __m128i*>(b)), q));

            if (MEM_UNLIKELY(mask))
            {
                return b + bsf(static_cast<unsigned int>(mask));
            }
        }

        for (; MEM_LIKELY(b < e); ++b)
            if (MEM_UNLIKELY(*b == c))
                return b;

        return e;
# else
#  error Sorry, No Potatoes
# endif
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

#endif // MEM_SIMD_PATTERN_BRICK_H
