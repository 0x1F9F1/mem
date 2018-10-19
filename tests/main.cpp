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

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#else
# include <cstdlib>
#endif

#include <mem/mem.h>
#include <mem/pattern.h>
#include <mem/utils.h>

#include <string>
#include <unordered_set>

#include <gtest/gtest.h>

void check_pattern(const mem::pattern& pattern, size_t original_size, size_t trimmed_size, bool needs_masks, const void* bytes, const void* masks)
{
    EXPECT_EQ(pattern.size(), original_size);
    EXPECT_EQ(pattern.trimmed_size(), trimmed_size);
    EXPECT_EQ(pattern.needs_masks(), needs_masks);

    EXPECT_EQ(memcmp(bytes, pattern.bytes(), pattern.size()), 0);
    EXPECT_EQ(memcmp(masks, pattern.masks(), pattern.size()), 0);
}

TEST(pattern, parse)
{
    check_pattern(mem::pattern("01 02 03 04 05"), 5, 5, false, "\x01\x02\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF");
    check_pattern(mem::pattern("1 2 3 4 5"),      5, 5, false, "\x01\x02\x03\x04\x05", "\xFF\xFF\xFF\xFF\xFF");
    check_pattern(mem::pattern("1 ?2 3 4? 5"),    5, 5, true,  "\x01\x02\x03\x40\x05", "\xFF\x0F\xFF\xF0\xFF");
    check_pattern(mem::pattern("1? ? 3 ?? 5?"),   5, 5, true,  "\x10\x00\x03\x00\x50", "\xF0\x00\xFF\x00\xF0");
    check_pattern(mem::pattern("?1 ? 3 ?? ?5"),   5, 5, true,  "\x01\x00\x03\x00\x05", "\x0F\x00\xFF\x00\x0F");
    check_pattern(mem::pattern("01?12???34"),     5, 5, true,  "\x01\x01\x20\x00\x34", "\xFF\x0F\xF0\x00\xFF");

    check_pattern(mem::pattern("12345678"), 4, 4, false, "\x12\x34\x56\x78", "\xFF\xFF\xFF\xFF");

    check_pattern(mem::pattern("? 01 02 03 04 ? ? ?"), 8, 5, true, "\x00\x01\x02\x03\x04\x00\x00\x00", "\x00\xFF\xFF\xFF\xFF\x00\x00\x00");

    check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", "x?xx?"), 5, 4, true, "\x12\x00\x56\x78\x00", "\xFF\x00\xFF\xFF\x00");

    check_pattern(mem::pattern("Hello", nullptr), 5, 5, false, "\x48\x65\x6C\x6C\x6F", "\xFF\xFF\xFF\xFF\xFF");

    check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", "\xFF\x00\xFF\xFF\x00", 5), 5, 4, true, "\x12\x00\x56\x78\x00", "\xFF\x00\xFF\xFF\x00");
    check_pattern(mem::pattern("\x12\x34\x56\x78\xAB", nullptr, 5), 5, 5, false, "\x12\x34\x56\x78\xAB", "\xFF\xFF\xFF\xFF\xFF");
}

void check_pattern_results(const mem::region& whole_region, const mem::pattern& pattern, const std::vector<uint8_t>& scan_data, const std::unordered_set<size_t>& offsets)
{
    EXPECT_LE(scan_data.size(), whole_region.size);

    mem::region scan_region = whole_region.sub_region(whole_region.start.add(whole_region.size - scan_data.size()));

    EXPECT_EQ(scan_region.start, whole_region.start.add(whole_region.size - scan_data.size()));
    EXPECT_EQ(scan_region.size, scan_data.size());

    scan_region.copy(scan_data.data());

    auto scan_results = pattern.scan_all(scan_region);

    EXPECT_EQ(scan_results.size(), offsets.size());

    std::unordered_set<size_t> scan_results_set;

    for (auto result : scan_results)
    {
        scan_results_set.emplace(result - scan_region.start);
    }

    EXPECT_EQ(scan_results_set.size(), offsets.size());

    for (auto expected : offsets)
    {
        EXPECT_NE(scan_results_set.find(expected), scan_results_set.end());
    }
}

TEST(pattern, scan)
{
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    size_t size = si.dwPageSize * (4 + 2); // 4 Pages + Guard Page Before/After
    uint8_t* data = (uint8_t*) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    memset(data, 0, size);

    DWORD dwOld = 0;
    VirtualProtect(data, si.dwPageSize, PAGE_NOACCESS, &dwOld);
    VirtualProtect(data + size - si.dwPageSize, si.dwPageSize, PAGE_NOACCESS, &dwOld);

    mem::region scan_region(data + si.dwPageSize, size - (2 * si.dwPageSize));
#else
    uint8_t* data = static_cast<uint8_t*>(std::malloc(8192));

    mem::region scan_region(data, 8192);
#endif

    check_pattern_results(scan_region, "01 02 03 04 05", {
        0x01, 0x02, 0x03, 0x04, 0x05
    },{
        0
    });

    check_pattern_results(scan_region, "01 02 03 04 ?", {
        0x01, 0x02, 0x03, 0x04, 0x05
    },{
        0
    });

    check_pattern_results(scan_region, "01 02 03 04 ?", {
        0x01, 0x02, 0x03, 0x04
    },{

    });

    check_pattern_results(scan_region, "01 02 01 02 01", {
        0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01
    }, {
        0, 2, 4, 6
    });

    check_pattern_results(scan_region, "", {

    }, {

    });

    check_pattern_results(scan_region, "", {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06
    }, {

    });

    check_pattern_results(scan_region, "01 ?2 3? 45", {
        0x02, 0x59, 0x72, 0x01, 0x01, 0x02, 0x34, 0x45, 0x59, 0x92
    }, {
        4
    });

    check_pattern_results(scan_region, "01 ?2 3? 45", {
        0x02, 0x59, 0x72, 0x01, 0x01, 0x02, 0x43, 0x45, 0x59, 0x92
    }, {

    });

#if defined(_WIN32)
    VirtualFree(data, 0, MEM_RELEASE);
#else
    std::free(data);
#endif
}

TEST(region, contains)
{
    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(mem::region(0x1234, 0x10)));
    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x09)));
    EXPECT_FALSE(mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x10)));

    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(0x1234, 0x10));
    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(0x1235, 0x09));
    EXPECT_FALSE(mem::region(0x1234, 0x10).contains(0x1235, 0x10));

    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(0x1234));
    EXPECT_TRUE(mem::region(0x1234, 0x10).contains(0x1234 + 0x9));
    EXPECT_TRUE(mem::region(0x1234, 1).contains(0x1234));
    EXPECT_FALSE(mem::region(0x1234, 0x10).contains(0x1233));
    EXPECT_FALSE(mem::region(0x1234, 0x10).contains(0x1234 + 0x10));
    EXPECT_FALSE(mem::region(0x1234, 0).contains(0x1234));

    EXPECT_TRUE(mem::region(0x1234, 4).contains<int>(0x1234));
    EXPECT_TRUE(mem::region(0x1234, 4).contains<int>(0x1234));
    EXPECT_FALSE(mem::region(0x1234, 3).contains<int>(0x1234));
    EXPECT_FALSE(mem::region(0x1235, 3).contains<int>(0x1234));
}

void check_pointer_aligment(mem::pointer address, size_t alignment, mem::pointer aligned_down, mem::pointer aligned_up)
{
    EXPECT_EQ(address.align_down(alignment), aligned_down);
    EXPECT_EQ(address.align_up(alignment), aligned_up);
}

TEST(pointer, align)
{
    check_pointer_aligment(13, 1, 13, 13);
    check_pointer_aligment(13, 2, 12, 14);
    check_pointer_aligment(13, 5, 10, 15);
    check_pointer_aligment(16, 5, 15, 20);
    check_pointer_aligment(14, 5, 10, 15);
    check_pointer_aligment(15, 15, 15, 15);
}

void check_hex_conversion(const void* data, size_t length, bool upper_case, bool padded, const char* expected)
{
    EXPECT_EQ(mem::as_hex({ data, length }, upper_case, padded), expected);
}

TEST(utils, as_hex)
{
    check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true,  true, "01 23 45 67 89 AB CD EF");
    check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, false, true, "01 23 45 67 89 ab cd ef");
    check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true,  false, "0123456789ABCDEF");
    check_hex_conversion("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, false, false, "0123456789abcdef");
}
