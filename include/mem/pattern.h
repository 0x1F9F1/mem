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

#include <vector>

/*
    PATTERN SCANNING
    ================

    +------+--------------------------------------------------+
    | Arch | Regs                                             |
    +------+-----+--------------------------------------------+
    |  x86 |   7 | eax, rbx, ecx, edx, esi, edi, ebp          |
    |  x64 |  15 | rax, rbx, rcx, rdx, rsi, rdi, rbp, r[8-15] |
    |  ARM | 13+ | r[0-12]                                    |
    +------+-----+--------------------------------------------+

    +---------------+-------+----------------------------------------------------------+
    | Type          | Reads |  Vars                                                    |
    +---------------+---+---+---+------------------------------------------------------+
    | Masks + Skips | 3 | 2 | 8 | current, end, i, last, bytes, masks, skips, skip_pos |
    | Masks         | 3 | 0 | 6 | current, end, i, last, bytes, masks                  |
    | Skips         | 2 | 2 | 6 | current, end, i, last, bytes,        skips           |
    |               | 2 | 0 | 5 | current, end, i, last, bytes                         |
    | Suffixes      | 2 | 3 | 7 | current, end, i, last, bytes,        skips, suffixes |
    +---------------+---+---+---+------------------------------------------------------+

    To avoid accessing the stack during the main scap loop, you would need at least 1 register per variable plus at least 1 or 2 temporary registers for operations.
    This is of course assuming the compiler is smart enough to use all of the registers efficiently. Lots of them aren't.
*/

namespace mem
{
    struct pattern_settings
    {
        // 0 = Disabled
        size_t min_bad_char_skip
        {
#if defined(MEM_ARCH_X86)
            10
#elif defined(MEM_ARCH_X86_64)
            4
#else
            8
#endif
        };

        size_t min_good_suffix_skip {min_bad_char_skip};

        char wildcard {'?'};
    };

    class pattern
    {
    protected:
        std::vector<byte> bytes_ {};
        std::vector<byte> masks_ {};

        // Boyer–Moore + Boyer–Moore–Horspool Implementation
        std::vector<size_t> bad_char_skips_ {};
        std::vector<size_t> good_suffix_skips_ {};

        size_t skip_pos_ {0};
        size_t trimmed_size_ {0};

        bool needs_masks_ {true};

        size_t get_longest_run(size_t& length) const;

        bool is_prefix(size_t pos) const;
        size_t get_suffix_length(size_t pos) const;

        void finalize(const pattern_settings& settings);

    public:
        pattern() = default;

        pattern(const char* bytes, const pattern_settings& settings = {});
        pattern(const char* bytes, const char* masks, const pattern_settings& settings = {});
        pattern(const void* bytes, const void* masks, size_t length, const pattern_settings& settings = {});

        template <typename UnaryPredicate>
        pointer scan_predicate(region range, UnaryPredicate pred) const;

        pointer scan(region range) const noexcept;

        bool match(pointer address) const noexcept;

        std::vector<pointer> scan_all(region range) const;

        const byte* bytes() const noexcept;
        const byte* masks() const noexcept;

        const size_t* bad_char_skips() const noexcept;
        const size_t* good_suffix_skips() const noexcept;

        size_t size() const noexcept;
        size_t trimmed_size() const noexcept;

        size_t skip_pos() const noexcept;

        bool needs_masks() const noexcept;
    };

    namespace detail
    {
        // '0'-'9' => 0x0-0x9
        // 'a'-'f' => 0xA-0xF
        // 'A'-'F' => 0xA-0xF
        static MEM_CONSTEXPR const int8_t hex_char_table[256]
        {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x00 => 0x0F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x10 => 0x1F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x20 => 0x2F
             0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, // 0x30 => 0x3F
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x40 => 0x4F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x50 => 0x5F
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x60 => 0x6F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x70 => 0x7F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x80 => 0x8F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x90 => 0x9F
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xA0 => 0xAF
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xB0 => 0xBF
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xC0 => 0xCF
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xD0 => 0xDF
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xE0 => 0xEF
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xF0 => 0xFF
        };
    }

    inline pattern::pattern(const char* bytes, const pattern_settings& settings)
    {
        while (true)
        {
            uint8_t b = 0x00;
            uint8_t m = 0x00;

            char c = 0;
            size_t i = 0;

            while ((c = *bytes++) != '\0')
            {
                const int8_t value = detail::hex_char_table[static_cast<uint8_t>(c)];
                const bool is_wildcard = c == settings.wildcard;

                if (((value >= 0) && (value <= 0xF)) || is_wildcard)
                {
                    b <<= 4;
                    m <<= 4;

                    if (!is_wildcard)
                    {
                        b |= value;
                        m |= 0xF;
                    }

                    if (++i >= 2)
                    {
                        break;
                    }
                }
                else if (i)
                {
                    break;
                }
            }

            if (i)
            {
                if ((i == 1) && (m != 0))
                {
                    m |= 0xF0;
                }

                bytes_.push_back(b);
                masks_.push_back(m);
            }

            if (!c)
            {
                break;
            }
        }

        finalize(settings);
    }

    inline pattern::pattern(const char* bytes, const char* mask, const pattern_settings& settings)
    {
        if (mask)
        {
            const size_t size = strlen(mask);

            bytes_.resize(size);
            masks_.resize(size);

            for (size_t i = 0; i < size; ++i)
            {
                if (mask[i] == settings.wildcard)
                {
                    bytes_[i] = 0x00;
                    masks_[i] = 0x00;
                }
                else
                {
                    const char c = bytes[i];

                    bytes_[i] = static_cast<byte>(c);
                    masks_[i] = byte(~0);
                }
            }
        }
        else
        {
            const size_t size = strlen(bytes);

            bytes_.resize(size);
            masks_.resize(size);

            for (size_t i = 0; i < size; ++i)
            {
                const char c = bytes[i];

                bytes_[i] = static_cast<byte>(c);
                masks_[i] = byte(~0);
            }
        }

        finalize(settings);
    }

    inline pattern::pattern(const void* bytes, const void* mask, size_t length, const pattern_settings& settings)
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
                masks_[i] = byte(~0);
            }
        }

        finalize(settings);
    }

    inline size_t pattern::get_longest_run(size_t& length) const
    {
        size_t max_skip = 0;
        size_t skip_pos = 0;

        {
            size_t current_skip = 0;

            for (size_t i = 0; i < trimmed_size(); ++i)
            {
                if (masks_[i] != 0xFF)
                {
                    if (current_skip > max_skip)
                    {
                        max_skip = current_skip;
                        skip_pos = i - current_skip;
                    }

                    current_skip = 0;
                }
                else
                {
                    ++current_skip;
                }
            }

            if (current_skip > max_skip)
            {
                max_skip = current_skip;
                skip_pos = trimmed_size() - current_skip;
            }
        }

        length = max_skip;

        return skip_pos;
    }

    inline bool pattern::is_prefix(size_t pos) const
    {
        const size_t suffix_length = trimmed_size() - pos;

        for (size_t i = 0; i < suffix_length; ++i)
        {
            if (bytes_[i] != bytes_[pos + i])
            {
                return false;
            }
        }

        return true;
    }

    inline size_t pattern::get_suffix_length(size_t pos) const
    {
        const size_t last = trimmed_size() - 1;

        size_t i = 0;

        while ((i < pos) && (bytes_[pos - i] == bytes_[last - i]))
        {
            ++i;
        }

        return i;
    }

    inline void pattern::finalize(const pattern_settings& settings)
    {
        if (bytes_.empty() || masks_.empty() || (bytes_.size() != masks_.size()))
        {
            bytes_.clear();
            masks_.clear();
            bad_char_skips_.clear();
            good_suffix_skips_.clear();

            skip_pos_ = 0;
            trimmed_size_ = 0;

            return;
        }

        {
            size_t trimmed_size = bytes_.size();

            while (trimmed_size && (masks_[trimmed_size - 1] == 0x00))
            {
                --trimmed_size;
            }

            trimmed_size_ = trimmed_size;
        }

        size_t max_skip = 0;
        size_t skip_pos = get_longest_run(max_skip);

        if ((settings.min_bad_char_skip > 0) && (max_skip > settings.min_bad_char_skip))
        {
            bad_char_skips_.resize(256, max_skip);
            skip_pos_ = skip_pos + max_skip - 1;

            for (size_t i = skip_pos, last = skip_pos + max_skip - 1; i < last; ++i)
            {
                bad_char_skips_[bytes_[i]] = last - i;
            }
        }

        if ((skip_pos == 0) && (max_skip == trimmed_size()))
        {
            needs_masks_ = false;

            if (!bad_char_skips_.empty() && (settings.min_good_suffix_skip > 0) && (max_skip > settings.min_good_suffix_skip))
            {
                good_suffix_skips_.resize(trimmed_size());

                const size_t last = trimmed_size() - 1;

                size_t last_prefix = last;

                for (size_t i = trimmed_size(); i--;)
                {
                    if (is_prefix(i + 1))
                    {
                        last_prefix = i + 1;
                    }

                    good_suffix_skips_[i] = last_prefix + (last - i);
                }

                for (size_t i = 0; i < last; ++i)
                {
                    size_t suffix_length = get_suffix_length(i);
                    size_t pos = last - suffix_length;

                    if (bytes_[i - suffix_length] != bytes_[pos])
                    {
                        good_suffix_skips_[pos] = suffix_length + (last - i);
                    }
                }
            }
        }
    }

    template <typename UnaryPredicate>
    MEM_NOINLINE pointer pattern::scan_predicate(region range, UnaryPredicate pred) const
    {
        if (!trimmed_size())
        {
            return nullptr;
        }

        const size_t original_size = size();
        const size_t region_size = range.size;

        if (original_size > region_size)
        {
            return nullptr;
        }

        const byte* const region_base = range.start.as<const byte*>();
        const byte* const region_end = region_base + region_size;

        const byte* current = region_base;
        const byte* const end = region_end - original_size;

        const size_t last = trimmed_size() - 1;

        const byte* const pat_bytes = bytes();
        const size_t* const pat_skips = bad_char_skips();

        if (needs_masks())
        {
            const byte* const pat_masks = masks();

            if (pat_skips)
            {
                const size_t pat_skip_pos = skip_pos();

                for (; MEM_LIKELY(current <= end); current += pat_skips[current[pat_skip_pos]])
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                        {
                            goto mask_skip_next;
                        }
                    } while (MEM_LIKELY(i--));

                    if (pred(current))
                    {
                        return current;
                    }

                mask_skip_next:;
                }

                return nullptr;
            }
            else
            {
                for (; MEM_LIKELY(current <= end); ++current)
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                        {
                            goto mask_noskip_next;
                        }
                    } while (MEM_LIKELY(i--));

                    if (pred(current))
                    {
                        return current;
                    }

                mask_noskip_next:;
                }

                return nullptr;
            }
        }
        else
        {
            const size_t* const pat_suffixes = good_suffix_skips();

            if (pat_suffixes)
            {
                current += last;
                const byte* const end_plus_last = end + last;

                for (; MEM_LIKELY(current <= end_plus_last);)
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(*current != pat_bytes[i]))
                        {
                            goto suffix_skip_next;
                        }
                        else if (MEM_LIKELY(i))
                        {
                            --current;
                            --i;
                        }
                        else
                        {
                            break;
                        }
                    } while (true);

                    if (pred(current))
                    {
                        return current;
                    }

                suffix_skip_next:
                    const size_t bc_skip = pat_skips[*current];
                    const size_t gs_skip = pat_suffixes[i];

                    current += (bc_skip > gs_skip) ? bc_skip : gs_skip;
                }

                return nullptr;
            }
            else if (pat_skips)
            {
                for (; MEM_LIKELY(current <= end); current += pat_skips[current[last]])
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                        {
                            goto nomask_skip_next;
                        }
                    } while (MEM_LIKELY(i--));

                    if (pred(current))
                    {
                        return current;
                    }

                nomask_skip_next:;
                }

                return nullptr;
            }
            else
            {
                for (; MEM_LIKELY(current <= end); ++current)
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                        {
                            goto nomask_noskip_next;
                        }
                    } while (MEM_LIKELY(i--));

                    if (pred(current))
                    {
                        return current;
                    }

                nomask_noskip_next:;
                }

                return nullptr;
            }
        }
    }

    namespace detail
    {
        struct always_true
        {
            template <typename... Args>
            MEM_CONSTEXPR MEM_STRONG_INLINE bool operator()(Args&&...) const noexcept
            {
                return true;
            }
        };
    }

    inline pointer pattern::scan(region range) const noexcept
    {
        return scan_predicate(range, detail::always_true {});
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

    inline std::vector<pointer> pattern::scan_all(region range) const
    {
        std::vector<pointer> results;

        scan_predicate(range, [&results] (pointer result)
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

    MEM_STRONG_INLINE const size_t* pattern::bad_char_skips() const noexcept
    {
        return !bad_char_skips_.empty() ? bad_char_skips_.data() : nullptr;
    }

    MEM_STRONG_INLINE const size_t* pattern::good_suffix_skips() const noexcept
    {
        return !good_suffix_skips_.empty() ? good_suffix_skips_.data() : nullptr;
    }

    MEM_STRONG_INLINE size_t pattern::size() const noexcept
    {
        return bytes_.size();
    }

    MEM_STRONG_INLINE size_t pattern::trimmed_size() const noexcept
    {
        return trimmed_size_;
    }

    MEM_STRONG_INLINE size_t pattern::skip_pos() const noexcept
    {
        return skip_pos_;
    }

    MEM_STRONG_INLINE bool pattern::needs_masks() const noexcept
    {
        return needs_masks_;
    }
}

#endif // MEM_PATTERN_BRICK_H
