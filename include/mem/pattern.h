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

        explicit operator bool() const noexcept;
    };

    inline pattern::pattern(const char* string, wildcard_t wildcard)
    {
        char_queue input(string);

        while (input)
        {
            byte   value     = 0x00;
            byte   mask      = 0x00;
            byte   expl_mask = 0xFF;
            size_t count     = 1;

            int current = -1;
            int temp = -1;

        start:
            current = input.peek();
            if ((temp = xctoi(current)) != -1)  { input.pop(); value = byte(temp); mask = 0xFF; }
            else if (current == char(wildcard)) { input.pop(); value = 0x00;       mask = 0x00; }
            else if (current == ' ')            { input.pop(); goto start; }
            else                                {              goto error; }

            current = input.peek();
            if ((temp = xctoi(current)) != -1)  { input.pop(); value = (value << 4) | byte(temp); mask = (mask << 4) | 0x0F; }
            else if (current == char(wildcard)) { input.pop(); value = (value << 4);              mask = (mask << 4);        }
            else if (current == '&')            { input.pop(); goto masks;   }
            else if (current == '#')            { input.pop(); goto repeats; }
            else                                {              goto end;     }

            current = input.peek();
            if (current == '&')      { input.pop(); goto masks;   }
            else if (current == '#') { input.pop(); goto repeats; }
            else                     {              goto end;     }

        masks:
            current = input.peek();
            if ((temp = xctoi(current)) != -1) { input.pop(); expl_mask = byte(temp); }
            else                               { goto error; }

            current = input.peek();
            if ((temp = xctoi(current)) != -1) { input.pop(); expl_mask = (expl_mask << 4) | byte(temp); }
            else if (current == '#')           { input.pop(); goto repeats; }
            else                               {              goto end;     }

            current = input.peek();
            if (current == '#') { input.pop(); goto repeats; }
            else                {              goto end;     }

        repeats:
            count = 0;

            while (true)
            {
                current = input.peek();
                if ((temp = dctoi(current)) != -1) { input.pop(); count = (count * 10) + temp; }
                else if (count > 0)                { goto end;   }
                else                               { goto error; }
            }

        end:
            value &= (mask &= expl_mask);

            for (size_t i = 0; i < count; ++i)
            {
                bytes_.push_back(value);
                masks_.push_back(mask);
            }

            continue;

        error:
            masks_.clear();
            bytes_.clear();

            break;
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
