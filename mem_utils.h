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

#if !defined(MEM_UTILS_BRICK_H)
#define MEM_UTILS_BRICK_H

#include "mem.h"

#include <string>

namespace mem
{
    template <typename T>
    typename std::add_lvalue_reference<T>::type field(const pointer& base, const ptrdiff_t offset = 0) noexcept;

    bool is_ascii(const region& region) noexcept;
    bool is_utf8(const region& region) noexcept;

    std::string as_string(const region& region);
    std::string as_hex(const region& region, bool upper_case = true, bool padded = true);

    template <typename T>
    MEM_STRONG_INLINE typename std::add_lvalue_reference<T>::type field(const pointer& base, const ptrdiff_t offset) noexcept
    {
        return base.at<T>(offset);
    }

    namespace detail
    {
        static MEM_CONSTEXPR const int8_t utf8_length_table[256]
        {
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
            4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,
        };
    }

    inline bool is_ascii(const region& region) noexcept
    {
        for (size_t i = 0; i < region.size;)
        {
            const size_t length = detail::utf8_length_table[region.start.at<const uint8_t>(i)];

            if (length != 1)
            {
                return false;
            }

            i += length;
        }

        return true;
    }

    inline bool is_utf8(const region& region) noexcept
    {
        for (size_t i = 0; i < region.size;)
        {
            const size_t length = detail::utf8_length_table[region.start.at<const uint8_t>(i)];

            if (length == 0)
            {
                return false;
            }

            if ((i + length) > region.size)
            {
                return false;
            }

            for (size_t j = 1; j < length; ++j)
            {
                if ((region.start.at<const uint8_t>(i + j) & 0xC0) != 0x80)
                {
                    return false;
                }
            }

            i += length;
        }

        return true;
    }

    inline std::string as_string(const region& region)
    {
        return std::string(region.start.as<const char*>(), region.size);
    }

    inline std::string as_hex(const region& region, bool upper_case, bool padded)
    {
        const char* const char_hex_table = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";

        std::string result;

        result.reserve(region.size * (padded ? 3 : 2));

        for (size_t i = 0; i < region.size; ++i)
        {
            if (i && padded)
            {
                result.push_back(' ');
            }

            const uint8_t value = region.start.at<const uint8_t>(i);

            result.push_back(char_hex_table[(value >> 4) & 0xF]);
            result.push_back(char_hex_table[(value >> 0) & 0xF]);
        }

        return result;
    }
}

#endif // MEM_UTILS_BRICK_H
