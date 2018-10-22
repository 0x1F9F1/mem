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

#if !defined(MEM_PLATFORM_INL_BRICK_H)
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
# include <Windows.h>
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
    namespace internal
    {
        static MEM_CONSTEXPR MEM_STRONG_INLINE DWORD translate_prot_flags(prot_flags flags) noexcept
        {
            if (flags & prot_flags::X)
            {
                if      (flags & prot_flags::W) { return PAGE_EXECUTE_READWRITE; }
                else if (flags & prot_flags::R) { return PAGE_EXECUTE_READ;      }
                else                            { return PAGE_EXECUTE;           }
            }
            else
            {
                if      (flags & prot_flags::W) { return PAGE_READWRITE; }
                else if (flags & prot_flags::R) { return PAGE_READONLY;  }
                else                            { return PAGE_NOACCESS;  }
            }
        }
    }

    MEM_STRONG_INLINE protect::protect(region range, prot_flags flags)
        : region(range)
        , old_protect_(0)
        , success_(false)
    {
        DWORD old_protect = 0;
        success_ = VirtualProtect(start.as<void*>(), size, internal::translate_prot_flags(flags), &old_protect);
        old_protect_ = old_protect;
    }

    MEM_STRONG_INLINE protect::~protect()
    {
        if (success_)
        {
            DWORD old_protect = 0;
            VirtualProtect(start.as<void*>(), size, old_protect_, &old_protect);

            if (old_protect_ & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
            {
                FlushInstructionCache(GetCurrentProcess(), start.as<void*>(), size);
            }
        }
    }

    MEM_STRONG_INLINE protect::protect(protect&& rhs) noexcept
        : region(rhs)
        , old_protect_(rhs.old_protect_)
        , success_(rhs.success_)
    {
        rhs.old_protect_ = 0;
        rhs.success_ = false;
    }

    extern "C" namespace internal
    {
        IMAGE_DOS_HEADER __ImageBase;
    }

    MEM_STRONG_INLINE module module::nt(pointer address)
    {
        const IMAGE_DOS_HEADER& dos = address.at<const IMAGE_DOS_HEADER>(0);
        const IMAGE_NT_HEADERS& nt  = address.at<const IMAGE_NT_HEADERS>(dos.e_lfanew);

        return module(address, nt.OptionalHeader.SizeOfImage);
    }

    MEM_STRONG_INLINE module module::named(const char* name)
    {
        return nt(GetModuleHandleA(name));
    }

    MEM_STRONG_INLINE module module::named(const wchar_t* name)
    {
        return nt(GetModuleHandleW(name));
    }

    MEM_STRONG_INLINE module module::main()
    {
        return module::named(static_cast<const char*>(nullptr));
    }

    MEM_STRONG_INLINE module module::self()
    {
        return nt(&internal::__ImageBase);
    }

    namespace internal
    {
        static MEM_CONSTEXPR MEM_STRONG_INLINE const char* translate_exception_code(uint32_t code) noexcept
        {
            switch (code)
            {
                case 0x80000001: return "STATUS_GUARD_PAGE_VIOLATION";
                case 0x80000002: return "STATUS_DATATYPE_MISALIGNMENT";
                case 0x80000003: return "STATUS_BREAKPOINT";
                case 0x80000004: return "STATUS_SINGLE_STEP";
                case 0x80000026: return "STATUS_LONGJUMP";
                case 0x80000029: return "STATUS_UNWIND_CONSOLIDATE";
                case 0x80010001: return "DBG_EXCEPTION_NOT_HANDLED";
                case 0xC0000005: return "STATUS_ACCESS_VIOLATION";
                case 0xC0000006: return "STATUS_IN_PAGE_ERROR";
                case 0xC0000008: return "STATUS_INVALID_HANDLE";
                case 0xC000000D: return "STATUS_INVALID_PARAMETER";
                case 0xC0000017: return "STATUS_NO_MEMORY";
                case 0xC000001D: return "STATUS_ILLEGAL_INSTRUCTION";
                case 0xC0000025: return "STATUS_NONCONTINUABLE_EXCEPTION";
                case 0xC0000026: return "STATUS_INVALID_DISPOSITION";
                case 0xC000008C: return "STATUS_ARRAY_BOUNDS_EXCEEDED";
                case 0xC000008D: return "STATUS_FLOAT_DENORMAL_OPERAND";
                case 0xC000008E: return "STATUS_FLOAT_DIVIDE_BY_ZERO";
                case 0xC000008F: return "STATUS_FLOAT_INEXACT_RESULT";
                case 0xC0000090: return "STATUS_FLOAT_INVALID_OPERATION";
                case 0xC0000091: return "STATUS_FLOAT_OVERFLOW";
                case 0xC0000092: return "STATUS_FLOAT_STACK_CHECK";
                case 0xC0000093: return "STATUS_FLOAT_UNDERFLOW";
                case 0xC0000094: return "STATUS_INTEGER_DIVIDE_BY_ZERO";
                case 0xC0000095: return "STATUS_INTEGER_OVERFLOW";
                case 0xC0000096: return "STATUS_PRIVILEGED_INSTRUCTION";
                case 0xC00000FD: return "STATUS_STACK_OVERFLOW";
                case 0xC0000135: return "STATUS_DLL_NOT_FOUND";
                case 0xC0000138: return "STATUS_ORDINAL_NOT_FOUND";
                case 0xC0000139: return "STATUS_ENTRYPOINT_NOT_FOUND";
                case 0xC000013A: return "STATUS_CONTROL_C_EXIT";
                case 0xC0000142: return "STATUS_DLL_INIT_FAILED";
                case 0xC00002B4: return "STATUS_FLOAT_MULTIPLE_FAULTS";
                case 0xC00002B5: return "STATUS_FLOAT_MULTIPLE_TRAPS";
                case 0xC00002C9: return "STATUS_REG_NAT_CONSUMPTION";
                case 0xC0000374: return "STATUS_HEAP_CORRUPTION";
                case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN";
                case 0xC0000417: return "STATUS_INVALID_CRUNTIME_PARAMETER";
                case 0xC0000420: return "STATUS_ASSERTION_FAILURE";
                case 0xC00004A2: return "STATUS_ENCLAVE_VIOLATION";
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
    #elif defined(MEM_ARCH_X86)
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
    MEM_STRONG_INLINE scoped_seh::scoped_seh()
        : context_(_set_se_translator(&internal::translate_seh))
    { }

    MEM_STRONG_INLINE scoped_seh::~scoped_seh()
    {
        _set_se_translator(static_cast<_se_translator_function>(context_));
    }
#pragma warning(pop)

#endif // _WIN32

    MEM_STRONG_INLINE void* aligned_alloc(size_t size, size_t alignment)
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

    MEM_STRONG_INLINE void aligned_free(void* address)
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

#endif // MEM_PLATFORM_INL_BRICK_H
