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

#include <mem/module.h>
#include <mem/pattern.h>

#include <cstdio>

volatile char NeedleString[] = "Lorem ipsum dolor sit amet";

int main()
{
    mem::module main_module = mem::module::main();

    std::printf("Main Module: 0x%zX => 0x%zX\n", main_module.start.as<std::uintptr_t>(), main_module.start.add(main_module.size).as<std::uintptr_t>());

    mem::pattern needle("4C 6F 72 65 6D 20 69 70 73 75 6D 20 64 6F 6C 6F 72 20 73 69 74 20 61 6D 65 74");
    mem::default_scanner scanner(needle);

    main_module.enum_segments([&](mem::region range, mem::prot_flags prot) {
        std::printf("Scanning %c%c%c segment 0x%zX => 0x%zX \n", (prot & mem::prot_flags::R) ? 'R' : '-', (prot & mem::prot_flags::W) ? 'W' : '-',
            (prot & mem::prot_flags::X) ? 'X' : '-', range.start.as<std::uintptr_t>(), range.start.add(range.size).as<std::uintptr_t>());

        scanner(range, [&](mem::pointer address) {
            std::printf("Found needle at 0x%zX\n", address.as<std::uintptr_t>());

            return false;
        });

        return false;
    });
}
