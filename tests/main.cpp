#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MEM_AUTO_PLATFORM
#include "..\mem.h"
#include "..\mem_scoped_seh.h"
#include "..\mem_pattern.h"

#include <string>

#include <unordered_set>

void Assert(bool value, const char* format, ...)
{
    if (!value)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        putchar('\n');
        va_end(args);

        DebugBreak();

        throw std::runtime_error("Assertion Failed");
    }
}

void check_ida_pattern(const char* pattern, const void* bytes, const void* masks, const size_t original_length, const size_t actual_length)
{
    mem::pattern pat(pattern);

    Assert(pat.size() == original_length, "Orignal Length: Expected %zu, Got %zu", original_length, pat.size());

    Assert(pat.bytes().size() == actual_length, "Actual Bytes Length: Expected %zu, Got %zu", actual_length, pat.bytes().size());

    Assert(!memcmp(bytes, pat.bytes().data(), pat.bytes().size()), "Invalid Pattern Bytes");

    Assert(pat.masks().empty() == (masks == nullptr), "Invalid Masks");

    if (!pat.masks().empty())
    {
        Assert(pat.masks().size() == actual_length, "Actual Masks Length: Expected %zu, Got %zu", actual_length, pat.masks().size());

        Assert(!memcmp(masks, pat.masks().data(), pat.masks().size()), "Invalid Pattern Masks");
    }
}

void check_pattern_results(const mem::region& whole_region, const char* pattern, const std::vector<uint8_t>& scan_data, const std::unordered_set<size_t>& offsets)
{
    Assert(scan_data.size() <= whole_region.size, "Data Too Large");

    mem::pattern pat(pattern);

    mem::region scan_region = whole_region.sub_region(whole_region.start.add(whole_region.size - scan_data.size()));

    Assert(scan_region.start == whole_region.start.add(whole_region.size - scan_data.size()), "Invalid Scan Region Address");
    Assert(scan_region.size == scan_data.size(), "Invalid Scan Region Size");

    scan_region.copy(scan_data.data());

    auto scan_results = pat.scan_all(scan_region);

    Assert(scan_results.size() == offsets.size(), "Incorrect Result Count");

    std::unordered_set<size_t> scan_results_set;

    for (auto result : scan_results)
    {
        scan_results_set.emplace(result - scan_region.start);
    }

    Assert(scan_results_set.size() == offsets.size(), "Invalid Result Count");

    for (auto expected : offsets)
    {
        Assert(scan_results_set.find(expected) != scan_results_set.end(), "Missing Result");
    }
}

void check_patterns()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    size_t size = si.dwPageSize * (4 + 2); // 4 Pages + Guard Page Before/After
    uint8_t* data = (uint8_t*) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    memset(data, 0, size);

    DWORD dwOld = 0;
    VirtualProtect(data, si.dwPageSize, PAGE_NOACCESS, &dwOld);
    VirtualProtect(data + size - si.dwPageSize, si.dwPageSize, PAGE_NOACCESS, &dwOld);

    mem::region scan_region(data + si.dwPageSize, size - (2 * si.dwPageSize));

    try
    {
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
    }
    catch (...)
    {
        Assert(false, "Exception Scanning");
    }

    VirtualFree(data, 0, MEM_RELEASE);
}

void check_pattern_parsing()
{
    check_ida_pattern("01 02 03 04 05", "\x01\x02\x03\x04\x05", nullptr, 5, 5);
    check_ida_pattern("1 2 3 4 5", "\x01\x02\x03\x04\x05", nullptr, 5, 5);
    check_ida_pattern("1 ?2 3 4? 5", "\x01\x02\x03\x40\x05", "\xFF\x0F\xFF\xF0\xFF", 5, 5);
    check_ida_pattern("1? ? 3 ?? 5?", "\x10\x00\x03\x00\x50", "\xF0\x00\xFF\x00\xF0", 5, 5);
    check_ida_pattern("?1 ? 3 ?? ?5", "\x01\x00\x03\x00\x05", "\x0F\x00\xFF\x00\x0F", 5, 5);
    check_ida_pattern("12345678", "\x12\x34\x56\x78", nullptr, 4, 4);
    check_ida_pattern("01?12???34", "\x01\x01\x20\x00\x34", "\xFF\x0F\xF0\x00\xFF", 5, 5);
    check_ida_pattern("? 01 02 03 04 ? ? ?", "\x00\x01\x02\x03\x04", "\x00\xFF\xFF\xFF\xFF", 8, 5);
}

void check_bounds()
{
    /*
        MEM_CONSTEXPR bool contains(const region& value) const noexcept;

        MEM_CONSTEXPR bool contains(const pointer& value) const noexcept;
        MEM_CONSTEXPR bool contains(const pointer& value, const size_t size) const noexcept;
    */

    static_assert(mem::region(0x1234, 0x10).contains(mem::region(0x1234, 0x10)), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x10)), "Region Checking Failed");
    static_assert(mem::region(0x1234, 0x10).contains(mem::region(0x1235, 0x09)), "Region Checking Failed");

    static_assert(mem::region(0x1234, 0x10).contains(0x1234, 0x10), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 0x10).contains(0x1235, 0x10), "Region Checking Failed");
    static_assert(mem::region(0x1234, 0x10).contains(0x1235, 0x09), "Region Checking Failed");

    static_assert(mem::region(0x1234, 0x10).contains(0x1234), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 0x10).contains(0x1233), "Region Checking Failed");
    static_assert(mem::region(0x1234, 0x10).contains(0x1234 + 0x9), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 0x10).contains(0x1234 + 0x10), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 0).contains(0x1234), "Region Checking Failed");
    static_assert(mem::region(0x1234, 1).contains(0x1234), "Region Checking Failed");

    static_assert(mem::region(0x1234, 4).contains<int>(0x1234), "Region Checking Failed");
    static_assert(!mem::region(0x1234, 3).contains<int>(0x1234), "Region Checking Failed");
    static_assert(!mem::region(0x1235, 3).contains<int>(0x1234), "Region Checking Failed");
    static_assert(mem::region(0x1234, 4).contains<int>(0x1234), "Region Checking Failed");
}

void check_aligning()
{
    static_assert(mem::pointer(13).align_down(1) == 13, "Bad Alignment");
    static_assert(mem::pointer(13).align_up(1)   == 13, "Bad Alignment");

    static_assert(mem::pointer(13).align_down(2) == 12, "Bad Alignment");
    static_assert(mem::pointer(13).align_up(2)   == 14, "Bad Alignment");

    static_assert(mem::pointer(13).align_down(5) == 10, "Bad Alignment");
    static_assert(mem::pointer(13).align_up(5)   == 15, "Bad Alignment");
}

int main()
{
    check_pattern_parsing();

    check_patterns();

    check_bounds();

    check_aligning();

    printf("Done\n");

    return 0;
}
