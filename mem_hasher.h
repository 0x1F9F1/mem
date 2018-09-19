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

#if !defined(MEM_HASHER_BRICK_H)
#define MEM_HASHER_BRICK_H

#include <cstdint>

namespace mem
{
    class hasher
    {
    protected:
        uint32_t hash_;

    public:
        hasher(const uint32_t seed = 0)
            : hash_(seed)
        { }

        void update(const void* data, const size_t length)
        {
            uint32_t hash = hash_;

            for (size_t i = 0; i < length; ++i)
            {
                hash += static_cast<const uint8_t*>(data)[i];
                hash += (hash << 10);
                hash ^= (hash >> 6);
            }

            hash_ = hash;
        }

        template <typename T>
        void update(const T& value)
        {
            static_assert(std::is_integral<T>::value, "Invalid Type");

            update(&value, sizeof(value));
        }

        uint32_t digest() const
        {
            uint32_t hash = hash_;

            hash += (hash << 3);
            hash ^= (hash >> 11);
            hash += (hash << 15);

            return hash;
        }
    };
}

#endif // MEM_HASHER_BRICK_H
