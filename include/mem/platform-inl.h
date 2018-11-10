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

#if defined(MEM_PLATFORM_INL_BRICK_H)
# error mem/platform-inl.h should only be included once
#endif // MEM_PLATFORM_INL_BRICK_H

#define MEM_PLATFORM_INL_BRICK_H

#include "platform.h"

#if defined(_WIN32)
# if defined(WIN32_LEAN_AND_MEAN)
#  include <Windows.h>
# else
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#  undef WIN32_LEAN_AND_MEAN
# endif
# include <malloc.h>
# include <eh.h>
# include <stdexcept>
# include <cstdio>
#else
# include <cstdlib>
#endif

namespace mem
{
#if defined(_WIN32)
    MEM_CONSTEXPR uint32_t from_prot_flags(prot_flags flags) noexcept
    {
        uint32_t result = PAGE_NOACCESS;

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

        if (flags & prot_flags::G)
            result |= PAGE_GUARD;
        if (flags & prot_flags::NC)
            result |= PAGE_NOCACHE;
        if (flags & prot_flags::WC)
            result |= PAGE_WRITECOMBINE;

        return result;
    }

    MEM_CONSTEXPR prot_flags to_prot_flags(uint32_t flags) noexcept
    {
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

        if (result != prot_flags::INVALID)
        {
            if (flags & PAGE_GUARD)
                result |= prot_flags::G;
            if (flags & PAGE_NOCACHE)
                result |= prot_flags::NC;
            if (flags & PAGE_WRITECOMBINE)
                result |= prot_flags::WC;
        }

        return result;
    }

    size_t page_size()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
    }

    void* protect_alloc(size_t length, prot_flags flags)
    {
        return VirtualAlloc(nullptr, length, MEM_RESERVE | MEM_COMMIT, from_prot_flags(flags));
    }

    void protect_free(void* memory)
    {
        if (memory != nullptr)
        {
            VirtualFree(memory, 0, MEM_RELEASE);
        }
    }

    bool protect_modify(void* memory, size_t length, prot_flags flags, prot_flags* old_flags)
    {
        if (flags == prot_flags::INVALID)
            return false;

        DWORD old_protect = 0;
        BOOL success = VirtualProtect(memory, length, from_prot_flags(flags), &old_protect);

        if (old_flags)
        {
            *old_flags = success ? to_prot_flags(old_protect) : prot_flags::INVALID;
        }

        return success;
    }

    protect::protect(region range, prot_flags flags)
        : region(range)
        , old_flags_(prot_flags::INVALID)
        , success_(protect_modify(start.as<void*>(), size, flags, &old_flags_))
    { }

    protect::~protect()
    {
        if (success_)
        {
            protect_modify(start.as<void*>(), size, old_flags_, nullptr);
        }
    }

    protect::protect(protect&& rhs) noexcept
        : region(rhs)
        , old_flags_(rhs.old_flags_)
        , success_(rhs.success_)
    {
        rhs.old_flags_ = prot_flags::INVALID;
        rhs.success_ = false;
    }

    extern "C" namespace internal
    {
        IMAGE_DOS_HEADER __ImageBase;
    }

    module module::nt(pointer address)
    {
        if (!address)
        {
            return module();
        }

        const IMAGE_DOS_HEADER& dos = address.at<const IMAGE_DOS_HEADER>(0);

        if (dos.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return module();
        }

        const IMAGE_NT_HEADERS& nt = address.at<const IMAGE_NT_HEADERS>(dos.e_lfanew);

        if (nt.Signature != IMAGE_NT_SIGNATURE)
        {
            return module();
        }

        return module(address, nt.OptionalHeader.SizeOfImage);
    }

    module module::named(const char* name)
    {
        return nt(GetModuleHandleA(name));
    }

    module module::named(const wchar_t* name)
    {
        return nt(GetModuleHandleW(name));
    }

    module module::main()
    {
        return nt(GetModuleHandleA(nullptr));
    }

    module module::self()
    {
        return nt(&internal::__ImageBase);
    }

    namespace internal
    {
        static MEM_CONSTEXPR const char* translate_exception_code(uint32_t code) noexcept
        {
            switch (code)
            {
            #define X(code) case code: return #code;
                X(STATUS_GUARD_PAGE_VIOLATION)
                X(STATUS_DATATYPE_MISALIGNMENT)
                X(STATUS_BREAKPOINT)
                X(STATUS_SINGLE_STEP)
                X(STATUS_LONGJUMP)
                X(STATUS_UNWIND_CONSOLIDATE)
                X(DBG_EXCEPTION_NOT_HANDLED)
                X(STATUS_ACCESS_VIOLATION)
                X(STATUS_IN_PAGE_ERROR)
                X(STATUS_INVALID_HANDLE)
                X(STATUS_INVALID_PARAMETER)
                X(STATUS_NO_MEMORY)
                X(STATUS_ILLEGAL_INSTRUCTION)
                X(STATUS_NONCONTINUABLE_EXCEPTION)
                X(STATUS_INVALID_DISPOSITION)
                X(STATUS_ARRAY_BOUNDS_EXCEEDED)
                X(STATUS_FLOAT_DENORMAL_OPERAND)
                X(STATUS_FLOAT_DIVIDE_BY_ZERO)
                X(STATUS_FLOAT_INEXACT_RESULT)
                X(STATUS_FLOAT_INVALID_OPERATION)
                X(STATUS_FLOAT_OVERFLOW)
                X(STATUS_FLOAT_STACK_CHECK)
                X(STATUS_FLOAT_UNDERFLOW)
                X(STATUS_INTEGER_DIVIDE_BY_ZERO)
                X(STATUS_INTEGER_OVERFLOW)
                X(STATUS_PRIVILEGED_INSTRUCTION)
                X(STATUS_STACK_OVERFLOW)
                X(STATUS_DLL_NOT_FOUND)
                X(STATUS_ORDINAL_NOT_FOUND)
                X(STATUS_ENTRYPOINT_NOT_FOUND)
                X(STATUS_CONTROL_C_EXIT)
                X(STATUS_DLL_INIT_FAILED)
                X(STATUS_FLOAT_MULTIPLE_FAULTS)
                X(STATUS_FLOAT_MULTIPLE_TRAPS)
                X(STATUS_REG_NAT_CONSUMPTION)
                X(STATUS_HEAP_CORRUPTION)
                X(STATUS_STACK_BUFFER_OVERRUN)
                X(STATUS_INVALID_CRUNTIME_PARAMETER)
                X(STATUS_ASSERTION_FAILURE)
                X(STATUS_ENCLAVE_VIOLATION)
            #undef X
            }

            return "UNKNOWN_EXCEPTION";
        }

        static void translate_seh(unsigned int code, EXCEPTION_POINTERS* ep)
        {
            const char* code_name = translate_exception_code(code);

            char buffer[2048];

            std::snprintf(buffer, sizeof(buffer),
#if defined(MEM_ARCH_X86_64)
                "%s (0x%08X) at 0x%016llX\n"
                "RAX = 0x%016llX RBX = 0x%016llX RCX = 0x%016llX RDX = 0x%016llX\n"
                "RSP = 0x%016llX RBP = 0x%016llX RSI = 0x%016llX RDI = 0x%016llX\n"
                "R8  = 0x%016llX R9  = 0x%016llX R10 = 0x%016llX R11 = 0x%016llX\n"
                "R12 = 0x%016llX R13 = 0x%016llX R14 = 0x%016llX R15 = 0x%016llX\n",
                code_name, code,
                reinterpret_cast<DWORD64>(ep->ExceptionRecord->ExceptionAddress),
                ep->ContextRecord->Rax, ep->ContextRecord->Rbx, ep->ContextRecord->Rcx, ep->ContextRecord->Rdx,
                ep->ContextRecord->Rsp, ep->ContextRecord->Rbp, ep->ContextRecord->Rsi, ep->ContextRecord->Rdi,
                ep->ContextRecord->R8,  ep->ContextRecord->R9,  ep->ContextRecord->R10, ep->ContextRecord->R11,
                ep->ContextRecord->R12, ep->ContextRecord->R13, ep->ContextRecord->R14, ep->ContextRecord->R15
#else /*if defined(MEM_ARCH_X86)*/
                "%s (0x%08X) at 0x%08X\n"
                "EAX = 0x%08lX EBX = 0x%08lX ECX = 0x%08lX EDX = 0x%08lX\n"
                "ESP = 0x%08lX EBP = 0x%08lX ESI = 0x%08lX EDI = 0x%08lX\n",
                code_name, code,
                reinterpret_cast<DWORD>(ep->ExceptionRecord->ExceptionAddress),
                ep->ContextRecord->Eax, ep->ContextRecord->Ebx, ep->ContextRecord->Ecx, ep->ContextRecord->Edx,
                ep->ContextRecord->Esp, ep->ContextRecord->Ebp, ep->ContextRecord->Esi, ep->ContextRecord->Edi
#endif
            );

            throw std::runtime_error(buffer);
        }
    }

#pragma warning(push)
#pragma warning(disable: 4535) // warning C4535: calling _set_se_translator() requires /EHa
    scoped_seh::scoped_seh()
        : context_(_set_se_translator(&internal::translate_seh))
    { }

    scoped_seh::~scoped_seh()
    {
        _set_se_translator(static_cast<_se_translator_function>(context_));
    }
#pragma warning(pop)

#endif // _WIN32

    void* aligned_alloc(size_t size, size_t alignment)
    {
#if defined(_WIN32)
        return _aligned_malloc(size, alignment);
#elif _POSIX_C_SOURCE >= 200112L
        void* result = nullptr;

        if (posix_memalign(&result, size, alignment) != 0)
        {
            return nullptr;
        }

        return result;
#else
        const size_t max_offset = alignment - 1 + sizeof(void*);
        void* result = malloc(size + max_offset);

        if (result)
        {
            void* aligned_result = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(result) + max_offset) / alignment * alignment);
            reinterpret_cast<void**>(aligned_result)[-1] = result;
            result = aligned_result;
        }

        return result;
#endif
    }

    void aligned_free(void* address)
    {
        if (!address)
        {
            return;
        }

#if defined(_WIN32)
        _aligned_free(address);
#elif _POSIX_C_SOURCE >= 200112L
        free(address);
#else
        free(reinterpret_cast<void**>(address)[-1]);
#endif
    }
}
