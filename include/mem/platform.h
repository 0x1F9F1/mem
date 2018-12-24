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
#include "slice.h"

#include <memory>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
# include <malloc.h>
# include <eh.h>
# include <stdexcept>
# include <intrin.h>
# if defined(_WIN64)
#  pragma intrinsic(__readgsqword)
# else
#  pragma intrinsic(__readfsdword)
# endif
#elif defined(__unix__)
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <cstring>
# include <cinttypes>
# include <unistd.h>
# include <sys/mman.h>
# include <link.h>
# if defined(MEM_USE_DLFCN) // Requires -ldl linker flag
#  include <dlfcn.h>
# endif
# include <setjmp.h>
# include <signal.h>
#else
# error Unknown Platform
#endif

namespace mem
{
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
}

MEM_DEFINE_ENUM_FLAG_OPERATORS(mem::prot_flags)

namespace mem
{
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

    class module
        : public region
    {
    public:
        using region::region;

#if defined(_WIN32)
        static module nt(pointer address);
        static module named(const wchar_t* name);

        const IMAGE_DOS_HEADER& dos_header();
        const IMAGE_NT_HEADERS& nt_headers();
        slice<const IMAGE_SECTION_HEADER> section_headers();
#elif defined(__unix__)
        static module elf(pointer address);

        const ElfW(Ehdr)& elf_header();
        slice<const ElfW(Phdr)> program_headers();
        slice<const ElfW(Shdr)> section_headers();
#endif

        static module named(const char* name);

        static module main();
        static module self();

        template <typename Func>
        void enum_segments(Func func);
    };

#if defined(_WIN32)
    class scoped_seh
    {
    private:
        _se_translator_function old_handler_ {nullptr};

    public:
        scoped_seh();
        ~scoped_seh();

        scoped_seh(const scoped_seh&) = delete;
        scoped_seh(scoped_seh&&) = delete;

        template <typename Func, typename... Args>
        auto execute(Func func, Args&&... args) -> decltype(func(std::forward<Args>(args)...))
        {
            return func(std::forward<Args>(args)...);
        }
    };

    using execution_handler = scoped_seh;
#elif defined(__unix__)
    class signal_handler
    {
    private:
        std::unique_ptr<char[ ]> sig_stack_;
        stack_t old_stack_;
        struct sigaction old_actions_[3];
        sigjmp_buf jmp_buffer_;

        static void sig_handler(int sig, siginfo_t* info, void* ucontext);
        static signal_handler*& current_handler();

        class scoped_handler
        {
        private:
            signal_handler* prev_ {nullptr};

        public:
            scoped_handler(signal_handler* handler);
            ~scoped_handler();

            scoped_handler(const scoped_handler&) = delete;
            scoped_handler(scoped_handler&&) = delete;
        };

    public:
        signal_handler();
        ~signal_handler();

        signal_handler(const signal_handler&) = delete;
        signal_handler(signal_handler&&) = delete;

        template <typename Func, typename... Args>
        auto execute(Func func, Args&&... args) -> decltype(func(std::forward<Args>(args)...))
        {
            scoped_handler scope(this);

            if (sigsetjmp(jmp_buffer_, 1))
            {
                throw std::runtime_error("Execution Error");
            }

            return func(std::forward<Args>(args)...);
        }
    };

    using execution_handler = signal_handler;
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

#if defined(_WIN32) || defined(__unix__)
    MEM_CONSTEXPR_14 inline uint32_t from_prot_flags(prot_flags flags) noexcept
    {
#if defined(_WIN32)
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
#elif defined(__unix__)
        uint32_t result = 0;

        if (flags & prot_flags::R)
            result |= PROT_READ;
        if (flags & prot_flags::W)
            result |= PROT_WRITE;
        if (flags & prot_flags::X)
            result |= PROT_EXEC;

        return result;
#endif
    }

    MEM_CONSTEXPR_14 inline prot_flags to_prot_flags(uint32_t flags) noexcept
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

    MEM_STRONG_INLINE size_t page_size()
    {
#if defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return static_cast<size_t>(si.dwPageSize);
#elif defined(__unix__)
        return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
    }

    MEM_STRONG_INLINE void* protect_alloc(size_t length, prot_flags flags)
    {
#if defined(_WIN32)
        return VirtualAlloc(nullptr, length, MEM_RESERVE | MEM_COMMIT, from_prot_flags(flags));
#elif defined(__unix__)
        void* result = mmap(nullptr, length, (int) from_prot_flags(flags), MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (result == MAP_FAILED)
            result = nullptr;
        return result;
#endif
    }

    MEM_STRONG_INLINE void protect_free(void* memory, size_t length)
    {
        if (memory != nullptr)
        {
#if defined(_WIN32)
            (void) length;
            VirtualFree(memory, 0, MEM_RELEASE);
#elif defined(__unix__)
            munmap(memory, length);
#endif
        }
    }

    inline prot_flags protect_query(void* memory)
    {
#if defined(_WIN32)
        MEMORY_BASIC_INFORMATION info;

        if (VirtualQuery(memory, &info, sizeof(info)))
            return to_prot_flags(info.Protect);

        return prot_flags::INVALID;
#elif defined(__unix__)
        FILE* maps = std::fopen("/proc/self/maps", "r");

        prot_flags result = prot_flags::INVALID;
        uintptr_t address = reinterpret_cast<uintptr_t>(memory);

        if (maps != nullptr)
        {
            char buffer[256];

            while (std::fgets(buffer, 256, maps))
            {
                uintptr_t start;
                uintptr_t end;
                char perms[16];

                if (std::sscanf(buffer, "%" SCNxPTR "-%" SCNxPTR " %15s", &start, &end, perms) != 3)
                    continue;

                if (address >= start && address < end)
                {
                    result = prot_flags::NONE;

                    if (std::strchr(perms, 'r'))
                        result |= prot_flags::R;

                    if (std::strchr(perms, 'w'))
                        result |= prot_flags::W;

                    if (std::strchr(perms, 'x'))
                        result |= prot_flags::X;

                    break;
                }
            }

            std::fclose(maps);
        }

        return result;
#endif
    }

    inline bool protect_modify(void* memory, size_t length, prot_flags flags, prot_flags* old_flags)
    {
        if (flags == prot_flags::INVALID)
            return false;

#if defined(_WIN32)
        DWORD old_protect = 0;
        BOOL success = VirtualProtect(memory, length, static_cast<DWORD>(from_prot_flags(flags)), &old_protect);

        if (old_flags)
            *old_flags = success ? to_prot_flags(old_protect) : prot_flags::INVALID;

        return success;
#elif defined(__unix__)
        prot_flags current_protect = old_flags ? protect_query(memory) : prot_flags::INVALID;

        bool success = mprotect(memory, length, static_cast<int>(from_prot_flags(flags))) == 0;

        if (old_flags)
            *old_flags = current_protect;

        return success;
#endif
    }

    inline protect::protect(region range, prot_flags flags)
        : region(range)
        , old_flags_(prot_flags::INVALID)
        , success_(protect_modify(start.as<void*>(), size, flags, &old_flags_))
    { }

    inline protect::~protect()
    {
        if (success_)
        {
            protect_modify(start.as<void*>(), size, old_flags_, nullptr);
        }
    }

    inline protect::protect(protect&& rhs) noexcept
        : region(rhs)
        , old_flags_(rhs.old_flags_)
        , success_(rhs.success_)
    {
        rhs.old_flags_ = prot_flags::INVALID;
        rhs.success_ = false;
    }
#endif

#if defined(_WIN32)
    extern "C" namespace internal
    {
        IMAGE_DOS_HEADER __ImageBase;
    }

    namespace internal
    {
        struct PEB
        {
            UCHAR InheritedAddressSpace;
            UCHAR ReadImageFileExecOptions;
            UCHAR BeingDebugged;
            UCHAR Spare;
            PVOID Mutant;
            PVOID ImageBaseAddress;
        };

        static MEM_STRONG_INLINE PEB* get_peb() noexcept
        {
#if defined(_WIN64)
            return reinterpret_cast<PEB*>(__readgsqword(0x60));
#else
            return reinterpret_cast<PEB*>(__readfsdword(0x30));
#endif
        }
    }

    MEM_STRONG_INLINE module module::nt(pointer address)
    {
        if (!address)
            return module();

        const IMAGE_DOS_HEADER& dos = address.at<const IMAGE_DOS_HEADER>(0);

        if (dos.e_magic != IMAGE_DOS_SIGNATURE)
            return module();

        const IMAGE_NT_HEADERS& nt = address.at<const IMAGE_NT_HEADERS>(dos.e_lfanew);

        if (nt.Signature != IMAGE_NT_SIGNATURE)
            return module();

        if (nt.FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))
            return module();

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
        return nt(internal::get_peb()->ImageBaseAddress);
    }

    MEM_STRONG_INLINE module module::self()
    {
        return nt(&internal::__ImageBase);
    }

    MEM_STRONG_INLINE const IMAGE_DOS_HEADER& module::dos_header()
    {
        return start.at<const IMAGE_DOS_HEADER>(0);
    }

    MEM_STRONG_INLINE const IMAGE_NT_HEADERS& module::nt_headers()
    {
        return start.at<const IMAGE_NT_HEADERS>(dos_header().e_lfanew);
    }

    MEM_STRONG_INLINE slice<const IMAGE_SECTION_HEADER> module::section_headers()
    {
        const IMAGE_NT_HEADERS& nt = nt_headers();
        const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(&nt);

        return { sections, nt.FileHeader.NumberOfSections };
    }

    template <typename Func>
    MEM_STRONG_INLINE void module::enum_segments(Func func)
    {
        for (const IMAGE_SECTION_HEADER& section : section_headers())
        {
            mem::region range(start.add(section.VirtualAddress), section.Misc.VirtualSize);

            if (!range.size)
                continue;

            prot_flags prot = prot_flags::NONE;

            if (section.Characteristics & IMAGE_SCN_MEM_READ)
                prot |= prot_flags::R;

            if (section.Characteristics & IMAGE_SCN_MEM_WRITE)
                prot |= prot_flags::W;

            if (section.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                prot |= prot_flags::X;

            if (func(range, prot))
                return;
        }
    }

#elif defined(__unix__)
    namespace internal
    {
        // https://github.com/torvalds/linux/blob/master/fs/binfmt_elf.c
        static size_t total_mapping_size(const ElfW(Phdr)* cmds, size_t count)
        {
            size_t first_idx = SIZE_MAX, last_idx = SIZE_MAX;

            for (size_t i = 0; i < count; ++i)
            {
                if (cmds[i].p_type == PT_LOAD)
                {
                    last_idx = i;

                    if (first_idx == SIZE_MAX)
                        first_idx = i;
                }
            }

            if (first_idx == SIZE_MAX)
                return 0;

            return cmds[last_idx].p_vaddr + cmds[last_idx].p_memsz - (cmds[first_idx].p_vaddr & ~(cmds[first_idx].p_align - 1));
        }
    }

    MEM_STRONG_INLINE module module::elf(pointer address)
    {
        if (!address)
            return module();

        const ElfW(Ehdr)& ehdr = address.at<const ElfW(Ehdr)&>(0);

        if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
            ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
            ehdr.e_ident[EI_MAG3] != ELFMAG3)
            return module();

        if (ehdr.e_phentsize != sizeof(ElfW(Phdr)) ||
            ehdr.e_shentsize != sizeof(ElfW(Shdr)))
            return module();

        const ElfW(Phdr)* phdr = address.at<const ElfW(Phdr)[ ]>(ehdr.e_phoff);
        const size_t mapping_size = internal::total_mapping_size(phdr, ehdr.e_phnum);

        return module(address, mapping_size);
    }

    MEM_STRONG_INLINE const ElfW(Ehdr)& module::elf_header()
    {
        return start.at<const ElfW(Ehdr)>(0);
    }

    MEM_STRONG_INLINE slice<const ElfW(Phdr)> module::program_headers()
    {
        const ElfW(Ehdr)& ehdr = elf_header();
        const ElfW(Phdr)* phdr = start.at<const ElfW(Phdr)[ ]>(ehdr.e_phoff);

        return { phdr, ehdr.e_phnum };
    }

    MEM_STRONG_INLINE slice<const ElfW(Shdr)> module::section_headers()
    {
        const ElfW(Ehdr)& ehdr = elf_header();
        const ElfW(Shdr)* shdr = start.at<const ElfW(Shdr)[ ]>(ehdr.e_shoff);

        return { shdr, ehdr.e_shnum };
    }

    template <typename Func>
    MEM_STRONG_INLINE void module::enum_segments(Func func)
    {
        for (const ElfW(Phdr)& section : program_headers())
        {
            if (section.p_type != PT_LOAD)
                continue;

            mem::region range(start.add(section.p_vaddr), section.p_memsz);

            if (!range.size)
                continue;

            prot_flags prot = prot_flags::NONE;

            if (section.p_flags & PF_R)
                prot |= prot_flags::R;

            if (section.p_flags & PF_W)
                prot |= prot_flags::W;

            if (section.p_flags & PF_X)
                prot |= prot_flags::X;

            if (func(range, prot))
                return;
        }
    }

    MEM_STRONG_INLINE module module::main()
    {
        return named(nullptr);
    }

    extern "C" namespace internal
    {
        ElfW(Ehdr) __attribute__((weak)) __ehdr_start;
    }

    MEM_STRONG_INLINE module module::self()
    {
        return elf(&internal::__ehdr_start);
    }

#if defined(MEM_USE_DLFCN)
    MEM_STRONG_INLINE module module::named(const char* name)
    {
        void* handle = dlopen(name, RTLD_LAZY | RTLD_NOLOAD);

        if (handle)
        {
#if defined(MEM_USE_DLINFO)
            const link_map* lm = nullptr;

            if (dlinfo(handle, RTLD_DI_LINKMAP, &lm))
                lm = nullptr;
#else
            const link_map* lm = static_cast<const link_map*>(handle);
#endif

            void* base_addr = nullptr;

            if (lm && lm->l_ld)
            {
                Dl_info info;

                if (dladdr(lm->l_ld, &info))
                {
                    base_addr = info.dli_fbase;
                }
            }

            dlclose(handle);

            return elf(base_addr);
        }

        return module();
    }
#else
    namespace internal
    {
        struct dl_search_info
        {
            const char* name {nullptr};
            void* result {nullptr};
        };

        static int dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* data)
        {
            (void) size;

            dl_search_info* search_info = static_cast<dl_search_info*>(data);

            const char* file_path = info->dlpi_name;
            const char* file_name = std::strrchr(file_path, '/');

            if (file_name)
            {
                ++file_name;
            }
            else
            {
                file_name = file_path;
            }

            if (!std::strcmp(search_info->name, file_name))
            {
                for (int i = 0; i < info->dlpi_phnum; ++i)
                {
                    if (info->dlpi_phdr[i].p_type == PT_LOAD)
                    {
                        search_info->result = reinterpret_cast<void*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);

                        return 1;
                    }
                }

                return 2;
            }

            return 0;
        }
    }

    MEM_STRONG_INLINE module module::named(const char* name)
    {
        internal::dl_search_info search;

        search.name = name ? name : "";

        if (dl_iterate_phdr(&internal::dl_iterate_callback, &search) == 1)
        {
            return elf(search.result);
        }

        return module();
    }
#endif
#endif

#if defined(_WIN32)
# if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4535) // warning C4535: calling _set_se_translator() requires /EHa
# endif

    namespace internal
    {
        static MEM_CONSTEXPR_14 const char* translate_exception_code(uint32_t code) noexcept
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

    inline scoped_seh::scoped_seh()
        : old_handler_(_set_se_translator(&internal::translate_seh))
    { }

    inline scoped_seh::~scoped_seh()
    {
        _set_se_translator(old_handler_);
    }

# if defined(_MSC_VER)
#  pragma warning(pop)
# endif
#elif defined(__unix__)
    inline signal_handler::signal_handler()
        : sig_stack_(new char[MINSIGSTKSZ])
    {
        stack_t new_stack {};

        new_stack.ss_sp = sig_stack_.get();
        new_stack.ss_size = MINSIGSTKSZ;
        new_stack.ss_flags = 0;
        sigaltstack(&new_stack, &old_stack_);

        struct sigaction sa {};

        sa.sa_sigaction = &sig_handler;
        sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGSEGV, &sa, &old_actions_[0]);
        sigaction(SIGILL,  &sa, &old_actions_[1]);
        sigaction(SIGFPE,  &sa, &old_actions_[2]);
    }

    inline signal_handler::~signal_handler()
    {
        sigaction(SIGSEGV, &old_actions_[0], nullptr);
        sigaction(SIGILL,  &old_actions_[1], nullptr);
        sigaction(SIGFPE,  &old_actions_[2], nullptr);

        sigaltstack(&old_stack_, nullptr);
    }

    inline void signal_handler::sig_handler(int /*sig*/, siginfo_t* /*info*/, void* /*ucontext*/)
    {
        signal_handler* current = current_handler();

        if (!current)
        {
            std::abort();
        }

        siglongjmp(current->jmp_buffer_, 1);
    }

    inline signal_handler*& signal_handler::current_handler()
    {
        static thread_local signal_handler* current {nullptr};

        return current;
    }

    inline signal_handler::scoped_handler::scoped_handler(signal_handler* handler)
        : prev_(current_handler())
    {
        current_handler() = handler;
    }

    inline signal_handler::scoped_handler::~scoped_handler()
    {
        current_handler() = prev_;
    }
#endif

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
        void* result = std::malloc(size + max_offset);

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
        std::free(reinterpret_cast<void**>(address)[-1]);
#endif
    }
}

#endif // MEM_PLATFORM_BRICK_H
