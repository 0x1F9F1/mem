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

#if !defined(MEM_BOYER_MOORE_SCANNER_BRICK_H)
#define MEM_BOYER_MOORE_SCANNER_BRICK_H

#include "pattern.h"

namespace mem
{
    class boyer_moore_scanner
    {
    private:
        const pattern* pattern_ {nullptr};

        // Boyer–Moore + Boyer–Moore–Horspool Implementation
        std::vector<size_t> bad_char_skips_ {};
        std::vector<size_t> good_suffix_skips_ {};

        size_t skip_pos_ {SIZE_MAX};

        size_t get_longest_run(size_t& length) const;

        bool is_prefix(size_t pos) const;
        size_t get_suffix_length(size_t pos) const;

    public:
        boyer_moore_scanner(const pattern& pattern);

        template <typename UnaryPredicate>
        pointer operator()(region range, UnaryPredicate pred) const;
    };

    inline boyer_moore_scanner::boyer_moore_scanner(const pattern& _pattern)
        : pattern_(&_pattern)
    {
        constexpr const size_t min_bad_char_skip
        {
#if defined(MEM_ARCH_X86)
            10
#elif defined(MEM_ARCH_X86_64)
            4
#else
            8
#endif
        };

        constexpr const size_t min_good_suffix_skip {min_bad_char_skip};

        size_t max_skip = 0;
        size_t skip_pos = get_longest_run(max_skip);

        const byte* const bytes = pattern_->bytes();
        const size_t trimmed_size = pattern_->trimmed_size();

        if (max_skip > min_bad_char_skip)
        {
            bad_char_skips_.resize(256, max_skip);
            skip_pos_ = skip_pos + max_skip - 1;

            for (size_t i = skip_pos, last = skip_pos + max_skip - 1; i < last; ++i)
                bad_char_skips_[bytes[i]] = last - i;
        }

        if ((skip_pos == 0) && (max_skip == trimmed_size))
        {
            if (!bad_char_skips_.empty() && (max_skip > min_good_suffix_skip))
            {
                good_suffix_skips_.resize(trimmed_size);

                const size_t last = trimmed_size - 1;

                size_t last_prefix = last;

                for (size_t i = trimmed_size; i--;)
                {
                    if (is_prefix(i + 1))
                        last_prefix = i + 1;

                    good_suffix_skips_[i] = last_prefix + (last - i);
                }

                for (size_t i = 0; i < last; ++i)
                {
                    size_t suffix_length = get_suffix_length(i);
                    size_t pos = last - suffix_length;

                    if (bytes[i - suffix_length] != bytes[pos])
                        good_suffix_skips_[pos] = suffix_length + (last - i);
                }
            }
        }
    }

    inline size_t boyer_moore_scanner::get_longest_run(size_t& length) const
    {
        size_t max_skip = 0;
        size_t skip_pos = 0;

        size_t current_skip = 0;

        const byte* const masks = pattern_->masks();

        for (size_t i = 0; i < pattern_->trimmed_size(); ++i)
        {
            if (masks[i] != 0xFF)
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
            skip_pos = pattern_->trimmed_size() - current_skip;
        }

        length = max_skip;

        return skip_pos;
    }

    inline bool boyer_moore_scanner::is_prefix(size_t pos) const
    {
        const size_t suffix_length = pattern_->trimmed_size() - pos;

        const byte* const bytes = pattern_->bytes();

        for (size_t i = 0; i < suffix_length; ++i)
            if (bytes[i] != bytes[pos + i])
                return false;

        return true;
    }

    inline size_t boyer_moore_scanner::get_suffix_length(size_t pos) const
    {
        const size_t last = pattern_->trimmed_size() - 1;

        const byte* const bytes = pattern_->bytes();

        size_t i = 0;

        while ((i < pos) && (bytes[pos - i] == bytes[last - i]))
            ++i;

        return i;
    }

    template <typename UnaryPredicate>
    inline pointer boyer_moore_scanner::operator()(region range, UnaryPredicate pred) const
    {
        const size_t trimmed_size = pattern_->trimmed_size();

        if (!trimmed_size)
            return nullptr;

        const size_t original_size = pattern_->size();
        const size_t region_size = range.size;

        if (original_size > region_size)
            return nullptr;

        const byte* const region_base = range.start.as<const byte*>();
        const byte* const region_end = region_base + region_size;

        const byte* current = region_base;
        const byte* const end = region_end - original_size + 1;

        const size_t last = trimmed_size - 1;

        const byte* const pat_bytes = pattern_->bytes();
        const size_t* const pat_skips = !bad_char_skips_.empty() ? bad_char_skips_.data() : nullptr;

        if (pattern_->needs_masks())
        {
            const byte* const pat_masks = pattern_->masks();

            if (pat_skips)
            {
                const size_t pat_skip_pos = skip_pos_;

                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    current += pat_skips[current[pat_skip_pos]];
                }

                return nullptr;
            }
            else
            {
                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY((current[i] & pat_masks[i]) != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    ++current;
                }

                return nullptr;
            }
        }
        else
        {
            if (!good_suffix_skips_.empty())
            {
                const size_t* const pat_suffixes = good_suffix_skips_.data();

                current += last;
                const byte* const end_plus_last = end + last;

                while (MEM_LIKELY(current < end_plus_last))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(*current != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --current;
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    const size_t bc_skip = pat_skips[*current];
                    const size_t gs_skip = pat_suffixes[i];

                    current += (bc_skip > gs_skip) ? bc_skip : gs_skip;
                }

                return nullptr;
            }
            else if (pat_skips)
            {
                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    current += pat_skips[current[last]];
                }

                return nullptr;
            }
            else
            {
                while (MEM_LIKELY(current < end))
                {
                    size_t i = last;

                    do
                    {
                        if (MEM_LIKELY(current[i] != pat_bytes[i]))
                            break;

                        if (MEM_LIKELY(i))
                        {
                            --i;

                            continue;
                        }

                        if (MEM_UNLIKELY(pred(current)))
                            return current;

                        break;
                    } while (true);

                    ++current;
                }

                return nullptr;
            }
        }
    }
}

#endif // MEM_BOYER_MOORE_SCANNER_BRICK_H
