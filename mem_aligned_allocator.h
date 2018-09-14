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

#if !defined(MEM_ALIGNED_ALLOCATOR_BRICK_H)
#define MEM_ALIGNED_ALLOCATOR_BRICK_H

#include "mem_defines.h"

#include <memory>
#include <malloc.h>

#if !defined(MEM_PLATFORM_WINDOWS)
# error mem::aligned_allocator only supports windows
#endif // !MEM_PLATFORM_WINDOWS

namespace mem
{
    template <typename T, size_t Alignment>
    class aligned_allocator
        : public std::allocator<T>
    {
    public:
        T* allocate(size_t n)
        {
            return _aligned_malloc(n * sizeof(T), Alignment);
        }

        void deallocate(T* p, size_t n)
        {
            (void) n;

            return _aligned_free(p);
        }
    };
}

#endif // MEM_ALIGNED_ALLOCATOR_BRICK_H
