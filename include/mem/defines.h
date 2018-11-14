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

#if !defined(MEM_DEFINES_BRICK_H)
#define MEM_DEFINES_BRICK_H

#if defined(__x86_64__) || defined(_M_X64)
# define MEM_ARCH_X86_64
#elif defined(__i386) || defined(_M_IX86)
# define MEM_ARCH_X86
#endif

#if !defined(MEM_CONSTEXPR)
# if (defined(__cpp_constexpr) && (__cpp_constexpr >= 200704)) || (defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 190024210))
#  define MEM_CONSTEXPR constexpr
# else
#  define MEM_CONSTEXPR
# endif
#endif

#if !defined(MEM_CONSTEXPR_14)
# if (defined(__cpp_constexpr) && (__cpp_constexpr >= 201304)) || (defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 191426433))
#  define MEM_CONSTEXPR_14 constexpr
# else
#  define MEM_CONSTEXPR_14
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
# define MEM_LIKELY(x) __builtin_expect(static_cast<bool>(x), 1)
# define MEM_UNLIKELY(x) __builtin_expect(static_cast<bool>(x), 0)
#else
# define MEM_LIKELY(x) static_cast<bool>(x)
# define MEM_UNLIKELY(x) static_cast<bool>(x)
#endif

#if defined(__GNUC__) || defined(__clang__)
# define MEM_STRONG_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
# define MEM_STRONG_INLINE __pragma(warning(suppress:4714)) inline __forceinline
#else
# define MEM_STRONG_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
# define MEM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define MEM_NOINLINE __declspec(noinline)
#else
# define MEM_NOINLINE
#endif

#include <climits>

#if CHAR_BIT != 8
# error Only 8-bit bytes are supported
#endif

namespace mem
{
    using byte = unsigned char;
}

#include <stdint.h>
#include <stddef.h>

#endif // MEM_DEFINES_BRICK_H
