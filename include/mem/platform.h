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

#if !defined(MEM_PLATFORM_BRICK_H)
#define MEM_PLATFORM_BRICK_H

#include "mem.h"
#include "bitwise_enum.h"

#include <memory>

namespace mem
{
#if defined(_WIN32) || defined(__unix__)
    namespace enums
    {
        enum prot_flags : uint32_t
        {
            INVALID = 0, // Invalid

            NONE = 1 << 0, // No Access

            R = 1 << 1, // Read
            W = 1 << 2, // Write
            X = 1 << 3, // Execute

            G  = 1 << 4, // Guard
            NC = 1 << 5, // No Cache
            WC = 1 << 6, // Write Combine

            RW  = R | W,
            RX  = R | X,
            RWX = R | W | X,
        };
    }

    using enums::prot_flags;

    MEM_DEFINE_ENUM_FLAG_OPERATORS(prot_flags);

    MEM_CONSTEXPR_14 uint32_t from_prot_flags(prot_flags flags) noexcept;
    MEM_CONSTEXPR_14 prot_flags to_prot_flags(uint32_t flags) noexcept;

    size_t page_size();

    void* protect_alloc(size_t length, prot_flags flags);
    void protect_free(void* memory, size_t length);

    prot_flags protect_query(void* memory);

    bool protect_modify(void* memory, size_t length, prot_flags flags, prot_flags* old_flags = nullptr);

    class protect
        : public region
    {
    private:
        prot_flags old_flags_ {prot_flags::INVALID};
        bool success_ {false};

    public:
        protect(region range, prot_flags flags = prot_flags::RWX);
        ~protect();

        protect(protect&& rhs) noexcept;
        protect(const protect&) = delete;

        MEM_STRONG_INLINE explicit operator bool() const noexcept
        {
            return success_;
        }

        MEM_STRONG_INLINE prot_flags release() noexcept
        {
            success_ = false;

            return old_flags_;
        }
    };
#endif

#if defined(_WIN32)
    class module
        : public region
    {
    public:
        using region::region;

        static module nt(pointer address);
        static module named(const char* name);
        static module named(const wchar_t* name);

        static module main();
        static module self();
    };

    class scoped_seh
    {
    private:
        void* context_;

    public:
        scoped_seh();
        ~scoped_seh();
    };
#endif

    void* aligned_alloc(size_t size, size_t alignment);
    void aligned_free(void* address);

    template <typename T, size_t Alignment>
    class aligned_allocator
        : public std::allocator<T>
    {
    public:
        inline T* allocate(size_t n)
        {
            return static_cast<T*>(aligned_alloc(n * sizeof(T), Alignment));
        }

        inline void deallocate(T* p, size_t n)
        {
            (void) n;

            aligned_free(p);
        }
    };
}

#endif // MEM_PLATFORM_BRICK_H
