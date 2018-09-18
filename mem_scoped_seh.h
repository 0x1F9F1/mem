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
#include <iterator>

namespace mem
{
    namespace detail
    {
        struct named_exception_code
        {
            unsigned int code;
            const char* name;
        };

        static MEM_CONSTEXPR const named_exception_code exception_codes[ ] =
        {
            { 0x80000001, "STATUS_GUARD_PAGE_VIOLATION" },
            { 0x80000002, "STATUS_DATATYPE_MISALIGNMENT" },
            { 0x80000003, "STATUS_BREAKPOINT" },
            { 0x80000004, "STATUS_SINGLE_STEP" },
            { 0x80000026, "STATUS_LONGJUMP" },
            { 0x80000029, "STATUS_UNWIND_CONSOLIDATE" },
            { 0x80010001, "DBG_EXCEPTION_NOT_HANDLED" },
            { 0xC0000005, "STATUS_ACCESS_VIOLATION" },
            { 0xC0000006, "STATUS_IN_PAGE_ERROR" },
            { 0xC0000008, "STATUS_INVALID_HANDLE" },
            { 0xC000000D, "STATUS_INVALID_PARAMETER" },
            { 0xC0000017, "STATUS_NO_MEMORY" },
            { 0xC000001D, "STATUS_ILLEGAL_INSTRUCTION" },
            { 0xC0000025, "STATUS_NONCONTINUABLE_EXCEPTION" },
            { 0xC0000026, "STATUS_INVALID_DISPOSITION" },
            { 0xC000008C, "STATUS_ARRAY_BOUNDS_EXCEEDED" },
            { 0xC000008D, "STATUS_FLOAT_DENORMAL_OPERAND" },
            { 0xC000008E, "STATUS_FLOAT_DIVIDE_BY_ZERO" },
            { 0xC000008F, "STATUS_FLOAT_INEXACT_RESULT" },
            { 0xC0000090, "STATUS_FLOAT_INVALID_OPERATION" },
            { 0xC0000091, "STATUS_FLOAT_OVERFLOW" },
            { 0xC0000092, "STATUS_FLOAT_STACK_CHECK" },
            { 0xC0000093, "STATUS_FLOAT_UNDERFLOW" },
            { 0xC0000094, "STATUS_INTEGER_DIVIDE_BY_ZERO" },
            { 0xC0000095, "STATUS_INTEGER_OVERFLOW" },
            { 0xC0000096, "STATUS_PRIVILEGED_INSTRUCTION" },
            { 0xC00000FD, "STATUS_STACK_OVERFLOW" },
            { 0xC0000135, "STATUS_DLL_NOT_FOUND" },
            { 0xC0000138, "STATUS_ORDINAL_NOT_FOUND" },
            { 0xC0000139, "STATUS_ENTRYPOINT_NOT_FOUND" },
            { 0xC000013A, "STATUS_CONTROL_C_EXIT" },
            { 0xC0000142, "STATUS_DLL_INIT_FAILED" },
            { 0xC00002B4, "STATUS_FLOAT_MULTIPLE_FAULTS" },
            { 0xC00002B5, "STATUS_FLOAT_MULTIPLE_TRAPS" },
            { 0xC00002C9, "STATUS_REG_NAT_CONSUMPTION" },
            { 0xC0000374, "STATUS_HEAP_CORRUPTION" },
            { 0xC0000409, "STATUS_STACK_BUFFER_OVERRUN" },
            { 0xC0000417, "STATUS_INVALID_CRUNTIME_PARAMETER" },
            { 0xC0000420, "STATUS_ASSERTION_FAILURE" },
            { 0xC00004A2, "STATUS_ENCLAVE_VIOLATION" },
        };
        
        inline const char* get_exception_code_name(unsigned int code)
        {
            const named_exception_code* find = std::lower_bound(std::begin(exception_codes), std::end(exception_codes), code, [ ] (const named_exception_code& value, unsigned int code)
            {
                return value.code < code;
            });

            if ((find != std::end(exception_codes)) && (find->code == code))
            {
                return find->name;
            }

            return "Unhandled Exception";
        }
    }

    class scoped_seh
    {
    protected:
        static void translate_seh(unsigned int code, EXCEPTION_POINTERS* ep)
        {
            const char* code_name = detail::get_exception_code_name(code);

            char buffer[2048];

            std::snprintf(buffer, sizeof(buffer),
    #if defined(MEM_ARCH_X64)
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
