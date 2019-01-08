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

#if !defined(MEM_MACROS_BRICK_H)
#define MEM_MACROS_BRICK_H

#include "mem.h"
#include "init_function.h"

#define decl_static(TYPE, NAME) static typename std::add_lvalue_reference<TYPE>::type NAME
#define defn_static(ADDRESS, NAME) decltype(NAME) NAME = mem::pointer(ADDRESS).as<decltype(NAME)>()
#define static_var(ADDRESS, TYPE, NAME) static typename std::add_lvalue_reference<TYPE>::type NAME = mem::pointer(ADDRESS).as<typename std::add_lvalue_reference<TYPE>::type>()

#if __cpp_inline_variables >= 201606
# define inline_var(ADDRESS, TYPE, NAME) inline static_var(ADDRESS, TYPE, NAME)
#else
# define inline_var(ADDRESS, TYPE, NAME) static_assert(false, "inline_var requires C++17 inline variables")
#endif

#define check_size(type, size) static_assert(sizeof(type) == (size), "sizeof(" #type ") != " #size)

#define mem_paste_(a,b) a ## b
#define mem_paste(a,b) mem_paste_(a,b)

#define mem_str_(X) #X
#define mem_str(X) mem_str_(X)

#define run_once(body) static mem::init_function mem_paste(run_once_, __LINE__)(body);

#if defined(_MSC_VER)
# define define_dummy_symbol(NAME) extern "C" void mem_paste(dummy_symbol_, NAME)() {}
# if defined(MEM_ARCH_X86_64)
#  define include_dummy_symbol(NAME) __pragma(comment(linker, "/INCLUDE:" mem_str(mem_paste(dummy_symbol_, NAME))))
# else
#  define include_dummy_symbol(NAME) __pragma(comment(linker, "/INCLUDE:" mem_str(mem_paste(_dummy_symbol_, NAME))))
# endif
#endif

#endif // MEM_MACROS_BRICK_H
