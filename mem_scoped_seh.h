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

#if !defined(MEM_SCOPED_SEH_BRICK_H)
#define MEM_SCOPED_SEH_BRICK_H

#include "mem_defines.h"

#if defined(MEM_ARCH_X86) || defined(MEM_ARCH_X64)
# if !defined(MEM_PLATFORM_WINDOWS)
#  error mem::scoped_seh only supports windows
# endif // !MEM_PLATFORM_WINDOWS
#else
# error mem::scoped_seh only supports x86 and x64
#endif

#include <eh.h>

#include <stdexcept>
#include <cstdio>

namespace mem
{
    class scoped_seh
    {
    protected:
        static void translate_seh(unsigned int code, EXCEPTION_POINTERS* ep)
        {
            char buffer[2048];

            std::snprintf(buffer, sizeof(buffer),
    #if defined(MEM_ARCH_X64)
                "Unhandled exception 0x%08X at 0x%016llX\n"
                "RAX = 0x%016llX RBX = 0x%016llX RCX = 0x%016llX RDX = 0x%016llX\n"
                "RSP = 0x%016llX RBP = 0x%016llX RSI = 0x%016llX RDI = 0x%016llX\n"
                "R8  = 0x%016llX R9  = 0x%016llX R10 = 0x%016llX R11 = 0x%016llX\n"
                "R12 = 0x%016llX R13 = 0x%016llX R14 = 0x%016llX R15 = 0x%016llX\n",
                code,
                reinterpret_cast<DWORD64>(ep->ExceptionRecord->ExceptionAddress),
                ep->ContextRecord->Rax, ep->ContextRecord->Rbx, ep->ContextRecord->Rcx, ep->ContextRecord->Rdx,
                ep->ContextRecord->Rsp, ep->ContextRecord->Rbp, ep->ContextRecord->Rsi, ep->ContextRecord->Rdi,
                ep->ContextRecord->R8,  ep->ContextRecord->R9,  ep->ContextRecord->R10, ep->ContextRecord->R11,
                ep->ContextRecord->R12, ep->ContextRecord->R13, ep->ContextRecord->R14, ep->ContextRecord->R15
    #elif defined(MEM_ARCH_X86)
                "Unhandled exception 0x%08X at 0x%08X\n"
                "EAX = 0x%08lX EBX = 0x%08lX ECX = 0x%08lX EDX = 0x%08lX\n"
                "ESP = 0x%08lX EBP = 0x%08lX ESI = 0x%08lX EDI = 0x%08lX\n",
                code,
                reinterpret_cast<DWORD>(ep->ExceptionRecord->ExceptionAddress),
                ep->ContextRecord->Eax, ep->ContextRecord->Ebx, ep->ContextRecord->Ecx, ep->ContextRecord->Edx,
                ep->ContextRecord->Esp, ep->ContextRecord->Ebp, ep->ContextRecord->Esi, ep->ContextRecord->Edi
    #endif
            );

            throw std::runtime_error(buffer);
        }

        _se_translator_function _old_se_translator;

    public:

        scoped_seh()
            : _old_se_translator(_set_se_translator(translate_seh))
        { }

        ~scoped_seh()
        {
            _set_se_translator(_old_se_translator);
        }
    };
}

#endif // MEM_SCOPED_SEH_BRICK_H
