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

#include "char_queue.h"

namespace mem
{
    template <typename T>
    typename std::add_lvalue_reference<T>::type field(pointer base, const ptrdiff_t offset = 0) noexcept;

    bool is_ascii(region range) noexcept;
    bool is_utf8(region range) noexcept;

    std::string as_string(region range);
    std::string as_hex(region range, bool upper_case = true, bool padded = true);

    std::string unescape(const char* string, size_t length);

    template <typename T>
    MEM_STRONG_INLINE typename std::add_lvalue_reference<T>::type field(pointer base, const ptrdiff_t offset) noexcept
    {
        return base.at<T>(offset);
    }

    MEM_STRONG_INLINE bool is_ascii(region range) noexcept
    {
        for (size_t i = 0; i < range.size; ++i)
        {
            if (range.start.at<const byte>(i) >= 0x80)
            {
                return false;
            }
        }

        return true;
    }

    namespace internal
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

    inline bool is_utf8(region range) noexcept
    {
        for (size_t i = 0; i < range.size;)
        {
            const size_t length = internal::utf8_length_table[range.start.at<const byte>(i)];

            if (length == 0)
            {
                return false;
            }

            if ((i + length) > range.size)
            {
                return false;
            }

            for (size_t j = 1; j < length; ++j)
            {
                if ((range.start.at<const byte>(i + j) & 0xC0) != 0x80)
                {
                    return false;
                }
            }

            i += length;
        }

        return true;
    }

    inline std::string as_string(region range)
    {
        return std::string(range.start.as<const char*>(), range.size);
    }

    inline std::string as_hex(region range, bool upper_case, bool padded)
    {
        const char* const char_hex_table = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";

        std::string result;

        result.reserve(range.size * (padded ? 3 : 2));

        for (size_t i = 0; i < range.size; ++i)
        {
            if (i && padded)
            {
                result.push_back(' ');
            }

            const byte value = range.start.at<const byte>(i);

            result.push_back(char_hex_table[(value >> 4) & 0xF]);
            result.push_back(char_hex_table[(value >> 0) & 0xF]);
        }

        return result;
    }

    inline std::string unescape(const char* string, size_t length)
    {
        std::string results;

        char_queue input(string, length);

        while (input)
        {
            size_t result = SIZE_MAX;
            size_t count = 0;

            int current = -1;
            int temp = -1;

            current = input.peek();

            if (current == '\\')
            {
                input.pop();

                current = input.peek();
                if      (current == '\'') { input.pop(); result = '\''; }
                else if (current == '\"') { input.pop(); result = '\"'; }
                else if (current == '\\') { input.pop(); result = '\\'; }
                else if (current == '?')  { input.pop(); result = '\?'; }
                else if (current == 'a')  { input.pop(); result = '\a'; }
                else if (current == 'b')  { input.pop(); result = '\b'; }
                else if (current == 'f')  { input.pop(); result = '\f'; }
                else if (current == 'n')  { input.pop(); result = '\n'; }
                else if (current == 'r')  { input.pop(); result = '\r'; }
                else if (current == 't')  { input.pop(); result = '\t'; }
                else if (current == 'v')  { input.pop(); result = '\v'; }
                else if (current == 'x')
                {
                    input.pop();

                    result = 0;
                    count = 0;

                    while (true)
                    {
                        current = input.peek();
                        if ((temp = xctoi(current)) != -1) { input.pop(); result = (result * 16) + temp; ++count; }
                        else { if (count == 0) { result = SIZE_MAX; } break; }
                    }
                }
                else if (octoi(current) != -1)
                {
                    result = 0;
                    count = 0;

                    while (true)
                    {
                        current = input.peek();
                        if ((temp = octoi(current)) != -1) { input.pop(); result = (result * 8) + temp; if (++count == 3) { break; } }
                        else { if (count == 0) { result = SIZE_MAX; } break; }
                    }
                }
            }
            else
            {
                input.pop();

                result = size_t(current);
            }

            if (result <= UCHAR_MAX)
            {
                results.push_back(static_cast<char>(result));
            }
            else
            {
                results.clear();

                break;
            }
        }

        return results;
    }
}

#endif // MEM_UTILS_BRICK_H
