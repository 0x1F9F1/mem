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

#if !defined(MEM_PROT_FLAGS_BRICK_H)
#define MEM_PROT_FLAGS_BRICK_H

#include "defines.h"
#include "bitwise_enum.h"

#if defined(_WIN32)
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#elif defined(__unix__)
# include <sys/mman.h>
#else
# error Unknown Platform
#endif

namespace mem
{
    namespace enums
    {
        enum prot_flags : std::uint32_t
        {
            INVALID = 0, // Invalid

            NONE = 1 << 0, // No Access

            R = 1 << 1, // Read
            W = 1 << 2, // Write
            X = 1 << 3, // Execute

            RW  = R | W,
            RX  = R | X,
            RWX = R | W | X,
        };
    }

    using enums::prot_flags;
}

MEM_DEFINE_ENUM_FLAG_OPERATORS(mem::prot_flags)

namespace mem
{
    MEM_CONSTEXPR_14 std::uint32_t from_prot_flags(prot_flags flags) noexcept;
    MEM_CONSTEXPR_14 prot_flags to_prot_flags(std::uint32_t flags) noexcept;

    MEM_CONSTEXPR_14 inline std::uint32_t from_prot_flags(prot_flags flags) noexcept
    {
#if defined(_WIN32)
        std::uint32_t result = PAGE_NOACCESS;

        if (flags & prot_flags::X)
        {
            if      (flags & prot_flags::W) { result = PAGE_EXECUTE_READWRITE; }
            else if (flags & prot_flags::R) { result = PAGE_EXECUTE_READ;      }
            else                            { result = PAGE_EXECUTE;           }
        }
        else
        {
            if      (flags & prot_flags::W) { result = PAGE_READWRITE; }
            else if (flags & prot_flags::R) { result = PAGE_READONLY;  }
            else                            { result = PAGE_NOACCESS;  }
        }

        return result;
#elif defined(__unix__)
        std::uint32_t result = 0;

        if (flags & prot_flags::R)
            result |= PROT_READ;
        if (flags & prot_flags::W)
            result |= PROT_WRITE;
        if (flags & prot_flags::X)
            result |= PROT_EXEC;

        return result;
#endif
    }

    MEM_CONSTEXPR_14 inline prot_flags to_prot_flags(std::uint32_t flags) noexcept
    {
#if defined(_WIN32)
        prot_flags result = prot_flags::INVALID;

        if (flags & PAGE_EXECUTE_READWRITE)
            result = prot_flags::RWX;
        else if (flags & PAGE_EXECUTE_READ)
            result = prot_flags::RX;
        else if (flags & PAGE_EXECUTE)
            result = prot_flags::X;
        else if (flags & PAGE_READWRITE)
            result = prot_flags::RW;
        else if (flags & PAGE_READONLY)
            result = prot_flags::R;
        else
            result = prot_flags::NONE;

        return result;
#elif defined(__unix__)
        prot_flags result = prot_flags::NONE;

        if (flags & PROT_READ)
            result |= prot_flags::R;

        if (flags & PROT_WRITE)
            result |= prot_flags::W;

        if (flags & PROT_EXEC)
            result |= prot_flags::X;

        return result;
#endif
    }
}

#endif // MEM_PROT_FLAGS_BRICK_H
