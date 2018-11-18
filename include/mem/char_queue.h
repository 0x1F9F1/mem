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

#if !defined(MEM_CHAR_QUEUE_BRICK_H)
#define MEM_CHAR_QUEUE_BRICK_H

#include "defines.h"

#include <cstring>

namespace mem
{
    class char_queue
    {
    private:
        const char* start {nullptr};
        const char* end {nullptr};
        const char* current {nullptr};

    public:
        char_queue(const char* string);
        MEM_CONSTEXPR char_queue(const char* string, size_t length);

        MEM_CONSTEXPR int peek() const noexcept;

        MEM_CONSTEXPR_14 void pop() noexcept;
        MEM_CONSTEXPR size_t pos() const noexcept;

        MEM_CONSTEXPR explicit operator bool() const noexcept;
    };

    MEM_CONSTEXPR int xctoi(int value) noexcept;
    MEM_CONSTEXPR int dctoi(int value) noexcept;
    MEM_CONSTEXPR int octoi(int value) noexcept;

    MEM_STRONG_INLINE char_queue::char_queue(const char* string)
        : char_queue(string, strlen(string))
    { }

    MEM_CONSTEXPR MEM_STRONG_INLINE char_queue::char_queue(const char* string, size_t length)
        : start(string)
        , end(start + length)
        , current(start)
    { }

    MEM_CONSTEXPR MEM_STRONG_INLINE int char_queue::peek() const noexcept
    {
        return (current < end) ? byte(*current) : -1;
    }

    MEM_CONSTEXPR_14 MEM_STRONG_INLINE void char_queue::pop() noexcept
    {
        if (current < end)
        {
            ++current;
        }
    }

    MEM_CONSTEXPR MEM_STRONG_INLINE size_t char_queue::pos() const noexcept
    {
        return current - start;
    }

    MEM_CONSTEXPR MEM_STRONG_INLINE char_queue::operator bool() const noexcept
    {
        return current < end;
    }

    MEM_CONSTEXPR MEM_STRONG_INLINE int xctoi(int value) noexcept
    {
        return (value >= '0' && value <= '9') ? (value - '0')
             : (value >= 'a' && value <= 'f') ? (value - 'a' + 10)
             : (value >= 'A' && value <= 'F') ? (value - 'A' + 10)
             : -1;
    }

    MEM_CONSTEXPR MEM_STRONG_INLINE int dctoi(int value) noexcept
    {
        return (value >= '0' && value <= '9') ? (value - '0') : -1;
    }

    MEM_CONSTEXPR MEM_STRONG_INLINE int octoi(int value) noexcept
    {
        return (value >= '0' && value <= '7') ? (value - '0') : -1;
    }
}

#endif // MEM_CHAR_QUEUE_BRICK_H
