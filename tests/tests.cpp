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

#include <mem/mem.h>
#include <mem/utils.h>
#include <mem/macros.h>

#include <mem/data_buffer.h>
#include <mem/slice.h>

#include <mem/init_function.h>

#include <mem/cmd_param.h>
#include <mem/cmd_param-inl.h>

#include <mem/pattern.h>
#include <mem/pattern_cache.h>

#include <mem/simd_scanner.h>
#include <mem/boyer_moore_scanner.h>

#include <mem/prot_flags.h>
#include <mem/protect.h>

#include <mem/module.h>
#include <mem/aligned_alloc.h>
#include <mem/execution_handler.h>

#include <mem/macros.h>

#if defined(_WIN32)
# include <mem/rtti.h>
#endif

#include <string>
#include <unordered_set>

#include "doctest.h"

void check_pattern(const mem::pattern& pattern, size_t original_size, size_t trimmed_size, bool needs_masks, const void* bytes, const void* masks)
{
    REQUIRE(pattern.size() == original_size);
    REQUIRE(pattern.trimmed_size() == trimmed_size);
    REQUIRE(pattern.needs_masks() == needs_masks);

    REQUIRE(memcmp(bytes, pattern.bytes(), pattern.size()) == 0);
    REQUIRE(memcmp(masks, pattern.masks(), pattern.size()) == 0);
}

TEST_CASE("mem::pattern constructor")
{
    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03 04 05"), 5, 5, false, "\x01\x02\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03 04 ?"), 5, 4, false, "\x01\x02\x03\x04\x00", "\xFF\xFF\xFF\xFF\x00"));
    CHECK_NOTHROW(check_pattern(mem::pattern(" 01    02        03 04 05 "), 5, 5, false, "\x01\x02\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("1 2 3 4 5"),      5, 5, false, "\x01\x02\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("1 ?2 3 4? 5"),    5, 5, true,  "\x01\x02\x03\x40\x05", "\xFF\x0F\xFF\xF0\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("1? ? 3 ?? 5?"),   5, 5, true,  "\x10\x00\x03\x00\x50", "\xF0\x00\xFF\x00\xF0"));
    CHECK_NOTHROW(check_pattern(mem::pattern("?1 ? 3 ?? ?5"),   5, 5, true,  "\x01\x00\x03\x00\x05", "\x0F\x00\xFF\x00\x0F"));
    CHECK_NOTHROW(check_pattern(mem::pattern("01?12???34"),     5, 5, true,  "\x01\x01\x20\x00\x34", "\xFF\x0F\xF0\x00\xFF"));

    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03#3 04 05"),    7, 7, false, "\x01\x02\x03\x03\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF\xFF\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03&F#3 04 05"),  7, 7, true,  "\x01\x02\x03\x03\x03\x04\x05", "\xFF\xFF\x0F\x0F\x0F\xFF\xFF"));
    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 33&F0#3 04 05"), 7, 7, true,  "\x01\x02\x30\x30\x30\x04\x05", "\xFF\xFF\xF0\xF0\xF0\xFF\xFF"));

    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03&F"), 3, 3, true, "\x01\x02\x03", "\xFF\xFF\x0F"));
    CHECK_NOTHROW(check_pattern(mem::pattern("01 02 03#2"), 4, 4, false, "\x01\x02\x03\x03", "\xFF\xFF\xFF\xFF"));

    CHECK_NOTHROW(check_pattern(mem::pattern("12345678"), 4, 4, false, "\x12\x34\x56\x78", "\xFF\xFF\xFF\xFF"));

    CHECK_NOTHROW(check_pattern(mem::pattern("? 01 02 03 04 ? ? ?"), 8, 5, true, "\x00\x01\x02\x03\x04\x00\x00\x00", "\x00\xFF\xFF\xFF\xFF\x00\x00\x00"));

    CHECK_NOTHROW(check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", "x?xx?"), 5, 4, true, "\x12\x00\x56\x78\x00", "\xFF\x00\xFF\xFF\x00"));

    CHECK_NOTHROW(check_pattern(mem::pattern("Hello", nullptr), 5, 5, false, "\x48\x65\x6C\x6C\x6F", "\xFF\xFF\xFF\xFF\xFF"));

    CHECK_NOTHROW(check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", "\xFF\x00\xFF\xFF\x00", 5), 5, 4, true, "\x12\x00\x56\x78\x00", "\xFF\x00\xFF\xFF\x00"));
    CHECK_NOTHROW(check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", nullptr, 5), 5, 5, false, "\x12\x34\x56\x78\xAB", "\xFF\xFF\xFF\xFF\xFF"));
}

void check_pattern_results(mem::region whole_region, const mem::pattern& pattern, const std::vector<uint8_t>& scan_data, const std::unordered_set<size_t>& offsets)
{
    REQUIRE(scan_data.size() <= whole_region.size);

    mem::region scan_region = whole_region.sub_region(whole_region.start.add(whole_region.size - scan_data.size()));

    REQUIRE(scan_region.start == whole_region.start.add(whole_region.size - scan_data.size()));
    REQUIRE(scan_region.size == scan_data.size());

    scan_region.copy(scan_data.data());

    mem::default_scanner scanner(pattern);

    auto scan_results = scanner.scan_all(whole_region);

    REQUIRE(scan_results.size() == offsets.size());

    std::unordered_set<size_t> scan_results_set;

    for (auto result : scan_results)
    {
        scan_results_set.emplace(result - scan_region.start);
    }

    REQUIRE(scan_results_set.size() == offsets.size());

    for (auto expected : offsets)
    {
        REQUIRE(scan_results_set.find(expected) != scan_results_set.end());
    }
}

TEST_CASE("mem::pattern scan")
{
    size_t page_size = mem::page_size();

    size_t raw_size = page_size * (4 + 2); // 4 Pages + Guard Page Before/After
    uint8_t* raw_data = static_cast<uint8_t*>(mem::protect_alloc(raw_size, mem::prot_flags::RW));

    memset(raw_data, 0, raw_size);

    mem::protect_modify(raw_data, page_size, mem::prot_flags::NONE);
    mem::protect_modify(raw_data + raw_size - page_size, page_size, mem::prot_flags::NONE);

    mem::region scan_region(raw_data + page_size, raw_size - (2 * page_size));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 02 03 04 05"), {
        0x01, 0x02, 0x03, 0x04, 0x05
    },{
        0
    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 02 03 04 ?"), {
        0x01, 0x02, 0x03, 0x04, 0x05
    },{
        0
    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 02 03 04 ?"), {
        0x01, 0x02, 0x03, 0x04
    },{

    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 02 01 02 01"), {
        0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01
    }, {
        0, 2, 4, 6
    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern(""), {

    }, {

    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern(""), {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06
    }, {

    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 ?2 3? 45"), {
        0x02, 0x59, 0x72, 0x01, 0x01, 0x02, 0x34, 0x45, 0x59, 0x92
    }, {
        4
    }));

    CHECK_NOTHROW(check_pattern_results(scan_region, mem::pattern("01 ?2 3? 45"), {
        0x02, 0x59, 0x72, 0x01, 0x01, 0x02, 0x43, 0x45, 0x59, 0x92
    }, {

    }));

    mem::protect_free(raw_data, raw_size);
}

TEST_CASE("mem::region contains")
{
    REQUIRE(mem::region(0x1234, 0x10).contains(mem::region(0x1234, 0x10)));
    REQUIRE(mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x09)));
    REQUIRE(!mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x10)));

    REQUIRE(mem::region(0x1234, 0x10).contains(0x1234, 0x10));
    REQUIRE(mem::region(0x1234, 0x10).contains(0x1235, 0x09));
    REQUIRE(!mem::region(0x1234, 0x10).contains(0x1235, 0x10));

    REQUIRE(mem::region(0x1234, 0x10).contains(0x1234));
    REQUIRE(mem::region(0x1234, 0x10).contains(0x1234 + 0x9));
    REQUIRE(mem::region(0x1234, 1).contains(0x1234));
    REQUIRE(!mem::region(0x1234, 0x10).contains(0x1233));
    REQUIRE(!mem::region(0x1234, 0x10).contains(0x1234 + 0x10));
    REQUIRE(!mem::region(0x1234, 0).contains(0x1234));

    REQUIRE(mem::region(0x1234, 4).contains<int>(0x1234));
    REQUIRE(mem::region(0x1234, 4).contains<int>(0x1234));
    REQUIRE(!mem::region(0x1234, 3).contains<int>(0x1234));
    REQUIRE(!mem::region(0x1235, 3).contains<int>(0x1234));
}

void check_pointer_aligment(mem::pointer address, size_t alignment, mem::pointer aligned_down, mem::pointer aligned_up)
{
    REQUIRE(address.align_down(alignment) == aligned_down);
    REQUIRE(address.align_up(alignment) == aligned_up);
}

TEST_CASE("mem::pointer align")
{
    CHECK_NOTHROW(check_pointer_aligment(13, 1, 13, 13));
    CHECK_NOTHROW(check_pointer_aligment(13, 2, 12, 14));
    CHECK_NOTHROW(check_pointer_aligment(13, 5, 10, 15));
    CHECK_NOTHROW(check_pointer_aligment(16, 5, 15, 20));
    CHECK_NOTHROW(check_pointer_aligment(14, 5, 10, 15));
    CHECK_NOTHROW(check_pointer_aligment(15, 15, 15, 15));
}

template <typename T>
void check_any_pointer()
{
    mem::pointer result(0x1234);

    REQUIRE(result.as<T>() == static_cast<T>(result.any()));
}

TEST_CASE("mem::pointer any")
{
    CHECK_NOTHROW(check_any_pointer<uintptr_t>());
    CHECK_NOTHROW(check_any_pointer<int*>());
    CHECK_NOTHROW(check_any_pointer<void*>());
}

void check_hex_conversion(const void* data, size_t length, bool upper_case, bool padded, const char* expected)
{
    REQUIRE(mem::as_hex({ data, length }, upper_case, padded) == expected);
}

TEST_CASE("mem::as_hex")
{
    CHECK_NOTHROW(check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true,  true, "01 23 45 67 89 AB CD EF"));
    CHECK_NOTHROW(check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, false, true, "01 23 45 67 89 ab cd ef"));
    CHECK_NOTHROW(check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true,  false, "0123456789ABCDEF"));
    CHECK_NOTHROW(check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, false, false, "0123456789abcdef"));
}

void check_unescape_string(const char* string, const void* data, size_t length, bool strict)
{
    std::vector<mem::byte> unescaped = mem::unescape(string, strlen(string), strict);

    REQUIRE(unescaped.size() == length);
    REQUIRE(memcmp(unescaped.data(), data, length) == 0);
}

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4125) // warning C4125: decimal digit terminates octal escape sequence
#endif

TEST_CASE("mem::unescape")
{
    CHECK_NOTHROW(check_unescape_string(R"(\x12\x34)", "\x12\x34", 2, true));
    CHECK_NOTHROW(check_unescape_string(R"(\0\1\10)", "\0\1\10", 3, true));
    CHECK_NOTHROW(check_unescape_string(R"(\0\1\1011)", "\0\1\1011", 4, true));
    CHECK_NOTHROW(check_unescape_string(R"(\1\2\3)", "\1\2\3", 3, true));
    CHECK_NOTHROW(check_unescape_string(R"(Hello There)", "Hello There", 11, true));
    CHECK_NOTHROW(check_unescape_string(R"(Hello\nThere)", "Hello\nThere", 11, true));
    CHECK_NOTHROW(check_unescape_string(R"(Hello \"Bob)", "Hello \"Bob", 10, true));

    CHECK_NOTHROW(check_unescape_string(R"(I am a \U0001F9F1)", "I am a \xF0\x9F\xA7\xB1", 11, true));

    CHECK_NOTHROW(check_unescape_string(R"(\x123456!)", "V!", 2, false));
    CHECK_NOTHROW(check_unescape_string(R"(\567ABC)", "wABC", 4, false));
    CHECK_NOTHROW(check_unescape_string(R"(\xz)", "\0z", 2, false));
    CHECK_NOTHROW(check_unescape_string(R"(\yz)", "yz", 2, false));

    CHECK_NOTHROW(check_unescape_string(R"(\x123456)", "", 0, true));
    CHECK_NOTHROW(check_unescape_string(R"(\567ABC)", "", 0, true));
    CHECK_NOTHROW(check_unescape_string(R"(\xz)", "", 0, true));
    CHECK_NOTHROW(check_unescape_string(R"(\yz)", "", 0, true));
}

#if defined(_MSC_VER)
# pragma warning(pop)
#endif

void check_prot_flags_roundtrip(mem::prot_flags flags)
{
    mem::prot_flags new_flags = mem::to_prot_flags(mem::from_prot_flags(flags));

    REQUIRE(flags == new_flags);
}

TEST_CASE("mem::prot_flags")
{
    CHECK_NOTHROW(check_prot_flags_roundtrip(mem::prot_flags::NONE));
    CHECK_NOTHROW(check_prot_flags_roundtrip(mem::prot_flags::R));
    CHECK_NOTHROW(check_prot_flags_roundtrip(mem::prot_flags::RW));
    CHECK_NOTHROW(check_prot_flags_roundtrip(mem::prot_flags::RX));
    CHECK_NOTHROW(check_prot_flags_roundtrip(mem::prot_flags::RWX));
}
