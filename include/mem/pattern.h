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

#if !defined(MEM_PATTERN_BRICK_H)
#define MEM_PATTERN_BRICK_H

#include "mem.h"
#include "char_queue.h"

#include <vector>

namespace mem
{
    class pattern
    {
    private:
        std::vector<byte> bytes_ {};
        std::vector<byte> masks_ {};
        size_t trimmed_size_ {0};
        bool needs_masks_ {true};

        void finalize();

        bool parse_chunk(char_queue& input, const char wildcard);

    public:
        explicit pattern() = default;

        enum class wildcard_t : char {};

        explicit pattern(const char* string, wildcard_t wildcard = wildcard_t('?'));
        explicit pattern(const void* bytes, const char* masks, wildcard_t wildcard = wildcard_t('?'));

        explicit pattern(const void* bytes, const void* masks, size_t length);

        bool match(pointer address) const noexcept;

        template <typename Scanner>
        pointer scan(region range, const Scanner& scanner) const;

        template <typename Scanner>
        std::vector<pointer> scan_all(region range, const Scanner& scanner) const;

        const byte* bytes() const noexcept;
        const byte* masks() const noexcept;

        size_t size() const noexcept;
        size_t trimmed_size() const noexcept;

        bool needs_masks() const noexcept;

        size_t get_skip_pos() const noexcept;

        explicit operator bool() const noexcept;
    };

    inline bool pattern::parse_chunk(char_queue& input, const char wildcard)
    {
        byte   value     = 0x00;
        byte   mask      = 0x00;
        byte   expl_mask = 0xFF;
        size_t count     = 1;

        int current = -1;
        int temp = -1;

        current = input.peek();
        if ((temp = xctoi(current)) != -1) { input.pop(); value = byte(temp); mask = 0xFF; }
        else if (current == wildcard)      { input.pop(); value = 0x00;       mask = 0x00; }
        else                               { return false;                                 }

        current = input.peek();
        if ((temp = xctoi(current)) != -1) { input.pop(); value = (value << 4) | byte(temp); mask = (mask << 4) | 0x0F; }
        else if (current == wildcard)      { input.pop(); value = (value << 4);              mask = (mask << 4);        }

        if (input.peek() == '&')
        {
            input.pop();

            if ((temp = xctoi(input.peek())) != -1) { input.pop(); expl_mask = byte(temp); }
            else                                    { return false; }

            if ((temp = xctoi(input.peek())) != -1) { input.pop(); expl_mask = (expl_mask << 4) | byte(temp); }
        }

        if (input.peek() == '#')
        {
            input.pop();

            count = 0;

            while (true)
            {
                if ((temp = dctoi(input.peek())) != -1) { input.pop(); count = (count * 10) + temp; }
                else if (count > 0)                     { break;                                    }
                else                                    { return false;                             }
            }
        }

        value &= (mask &= expl_mask);

        for (size_t i = 0; i < count; ++i)
        {
            bytes_.push_back(value);
            masks_.push_back(mask);
        }

        return true;
    }

    inline pattern::pattern(const char* string, wildcard_t wildcard)
    {
        char_queue input(string);

        while (input)
        {
            if (input.peek() == ' ')
            {
                input.pop();

                continue;
            }

            if (!parse_chunk(input, char(wildcard)))
            {
                masks_.clear();
                bytes_.clear();

                break;
            }
        }

        finalize();
    }

    inline pattern::pattern(const void* bytes, const char* mask, wildcard_t wildcard)
    {
        if (mask)
        {
            const size_t size = std::strlen(mask);

            bytes_.resize(size);
            masks_.resize(size);

            for (size_t i = 0; i < size; ++i)
            {
                if (mask[i] == char(wildcard))
                {
                    bytes_[i] = 0x00;
                    masks_[i] = 0x00;
                }
                else
                {
                    bytes_[i] = static_cast<const byte*>(bytes)[i];
                    masks_[i] = 0xFF;
                }
            }
        }
        else
        {
            const size_t size = std::strlen(static_cast<const char*>(bytes));

            bytes_.resize(size);
            masks_.resize(size);

            for (size_t i = 0; i < size; ++i)
            {
                bytes_[i] = static_cast<const byte*>(bytes)[i];
                masks_[i] = 0xFF;
            }
        }

        finalize();
    }

    inline pattern::pattern(const void* bytes, const void* mask, size_t length)
    {
        if (mask)
        {
            bytes_.resize(length);
            masks_.resize(length);

            for (size_t i = 0; i < length; ++i)
            {
                const byte v = static_cast<const byte*>(bytes)[i];
                const byte m = static_cast<const byte*>(mask)[i];

                bytes_[i] = v & m;
                masks_[i] = m;
            }
        }
        else
        {
            bytes_.resize(length);
            masks_.resize(length);

            for (size_t i = 0; i < length; ++i)
            {
                bytes_[i] = static_cast<const byte*>(bytes)[i];
                masks_[i] = 0xFF;
            }
        }

        finalize();
    }

    inline void pattern::finalize()
    {
        if (bytes_.size() != masks_.size())
        {
            bytes_.clear();
            masks_.clear();
            trimmed_size_ = 0;
            needs_masks_ = false;

            return;
        }

        for (size_t i = 0; i < bytes_.size(); ++i)
        {
            bytes_[i] &= masks_[i];
        }

        size_t trimmed_size = bytes_.size();

        while (trimmed_size && (masks_[trimmed_size - 1] == 0x00))
        {
            --trimmed_size;
        }

        trimmed_size_ = trimmed_size;

        needs_masks_ = false;

        for (size_t i = bytes_.size(); i--;)
        {
            if (masks_[i] != 0xFF)
            {
                needs_masks_ = true;

                break;
            }
        }
    }

    inline bool pattern::match(pointer address) const noexcept
    {
        const byte* const pat_bytes = bytes();

        if (!pat_bytes)
        {
            return false;
        }

        const byte* current = address.as<const byte*>();

        const size_t last = trimmed_size() - 1;

        if (needs_masks())
        {
            const byte* const pat_masks = masks();

            size_t i = last;

            do
            {
                if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                {
                    return false;
                }
            } while (MEM_LIKELY(i--));

            return true;
        }
        else
        {
            size_t i = last;

            do
            {
                if (MEM_LIKELY(current[i] != pat_bytes[i]))
                {
                    return false;
                }
            } while (MEM_LIKELY(i--));

            return true;
        }
    }

    template <typename Scanner>
    inline pointer pattern::scan(region range, const Scanner& scanner) const
    {
        return scanner(range, [ ] (pointer result) noexcept
        {
            return true;
        });
    }

    template <typename Scanner>
    inline std::vector<pointer> pattern::scan_all(region range, const Scanner& scanner) const
    {
        std::vector<pointer> results;

        scanner(range, [&results] (pointer result)
        {
            results.emplace_back(result);

            return false;
        });

        return results;
    }

    MEM_STRONG_INLINE const byte* pattern::bytes() const noexcept
    {
        return !bytes_.empty() ? bytes_.data() : nullptr;
    }

    MEM_STRONG_INLINE const byte* pattern::masks() const noexcept
    {
        return !masks_.empty() ? masks_.data() : nullptr;
    }

    MEM_STRONG_INLINE size_t pattern::size() const noexcept
    {
        return bytes_.size();
    }

    MEM_STRONG_INLINE size_t pattern::trimmed_size() const noexcept
    {
        return trimmed_size_;
    }

    MEM_STRONG_INLINE bool pattern::needs_masks() const noexcept
    {
        return needs_masks_;
    }

    namespace internal
    {
        static MEM_CONSTEXPR const byte frequencies[256]
        {
            0xFF,0xFB,0xF2,0xEE,0xEC,0xE7,0xDC,0xC8,0xED,0xB7,0xCC,0xC0,0xD3,0xCD,0x89,0xFA,
            0xF3,0xD6,0x8D,0x83,0xC1,0xAA,0x7A,0x72,0xC6,0x60,0x3E,0x2E,0x98,0x69,0x39,0x7C,
            0xEB,0x76,0x24,0x34,0xF9,0x50,0x04,0x07,0xE5,0xAC,0x53,0x65,0x9B,0x4D,0x6D,0x5C,
            0xDA,0x93,0x7F,0xCB,0x92,0x49,0x43,0x09,0xBA,0x8E,0x1E,0x91,0x8A,0x5B,0x11,0xA1,
            0xE8,0xF5,0x9E,0xAD,0xEF,0xE6,0x79,0x7B,0xFE,0xE0,0x1F,0x54,0xE4,0xBD,0x7D,0x6A,
            0xDF,0x67,0x7E,0xA4,0xB6,0xAF,0x88,0xA0,0xC3,0xA9,0x26,0x77,0xD1,0x71,0x61,0xC2,
            0x9A,0xCA,0x29,0x9F,0xD8,0xE2,0xD0,0x6E,0xB4,0xB8,0x25,0x3C,0xBF,0x73,0xB5,0xCF,
            0xD4,0x01,0xCE,0xBE,0xF1,0xDB,0x52,0x37,0x9D,0x63,0x02,0x6B,0x80,0x45,0x2B,0x95,
            0xE1,0xC4,0x36,0xF0,0xD5,0xE3,0x57,0x9C,0xB1,0xF7,0x82,0xFC,0x42,0xF6,0x18,0x33,
            0xD2,0x48,0x05,0x0F,0x41,0x1D,0x03,0x27,0x70,0x10,0x00,0x08,0x55,0x16,0x2F,0x0E,
            0x94,0x35,0x2C,0x40,0x6F,0x3B,0x1C,0x28,0x90,0x68,0x81,0x4B,0x56,0x30,0x2A,0x3D,
            0x97,0x17,0x06,0x13,0x32,0x0B,0x5A,0x75,0xA5,0x86,0x78,0x4F,0x2D,0x51,0x46,0x5F,
            0xE9,0xDE,0xA2,0xDD,0xC9,0x4C,0xAB,0xBB,0xC7,0xB9,0x74,0x8F,0xF8,0x6C,0x85,0x8B,
            0xC5,0x84,0x8C,0x66,0x21,0x23,0x64,0x59,0xA3,0x87,0x44,0x58,0x3A,0x0D,0x12,0x19,
            0xAE,0x5E,0x3F,0x38,0x31,0x22,0x0A,0x14,0xF4,0xD9,0x20,0xB0,0xB2,0x1A,0x0C,0x15,
            0xB3,0x47,0x5D,0xEA,0x4A,0x1B,0x99,0xBC,0xD7,0xA6,0x62,0x4E,0xA8,0x96,0xA7,0xFD,
        };
    }

    MEM_STRONG_INLINE size_t pattern::get_skip_pos() const noexcept
    {
        size_t min = SIZE_MAX;
        size_t result = SIZE_MAX;

        for (size_t i = 0; i < size(); ++i)
        {
            if (masks_[i] == 0xFF)
            {
                size_t f = internal::frequencies[bytes_[i]];

                if (f <= min)
                {
                    result = i;
                    min = f;
                }
            }
        }

        return result;
    }

    MEM_STRONG_INLINE pattern::operator bool() const noexcept
    {
        return !bytes_.empty() && !masks_.empty();
    }
}

#include "simd_scanner.h"

namespace mem
{
    using default_scanner = simd_scanner;
}

#endif // MEM_PATTERN_BRICK_H
