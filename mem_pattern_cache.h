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

#if !defined(MEM_PATTERN_CACHE_BRICK_H)
#define MEM_PATTERN_CACHE_BRICK_H

#include "mem_pattern.h"
#include "mem_hasher.h"

#include <unordered_map>

#include <ostream>
#include <istream>

namespace mem
{
    namespace stream
    {
        template <typename T>
        static inline void write(std::ostream& output, const T& value)
        {
            static_assert(std::is_trivial<T>::value, "Invalid Value");

            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        template <typename T>
        static inline T read(std::istream& input)
        {
            static_assert(std::is_trivial<T>::value, "Invalid Value");

            T result;

            input.read(reinterpret_cast<char*>(&result), sizeof(result));

            return result;
        }
    }

    class pattern_cache
    {
    protected:
        struct pattern_results
        {
            bool checked {false};
            std::vector<pointer> results {};
        };

        region region_;
        std::unordered_map<uint32_t, pattern_results> results_;

        static uint32_t hash_pattern(const pattern& pattern)
        {
            hasher hash;

            hash.update(pattern.size());

            const auto& bytes = pattern.bytes();

            if (!bytes.empty())
            {
                hash.update(0x435E89AB7);
                hash.update(bytes.data(), bytes.size());
            }

            const auto& masks = pattern.masks();

            if (!masks.empty())
            {
                hash.update(0xAE1E9528);
                hash.update(masks.data(), masks.size());
            }

            return hash.digest();
        }

    public:
        pattern_cache(const region& region)
            : region_(region)
        { }

        pointer scan(const pattern& pattern, const size_t index = 0, const size_t expected = 1)
        {
            const auto& results = scan_all(pattern);

            if (results.size() != expected)
            {
                return nullptr;
            }

            if (index >= results.size())
            {
                return nullptr;
            }

            return results[index];
        }

        const std::vector<pointer>& scan_all(const pattern& pattern)
        {
            const uint32_t hash = hash_pattern(pattern);

            auto find = results_.find(hash);

            if (find != results_.end())
            {
                if (!find->second.checked)
                {
                    bool changed = false;

                    for (mem::pointer result : find->second.results)
                    {
                        if (!pattern.match(result))
                        {
                            changed = true;

                            break;
                        }
                    }

                    if (changed)
                    {
                        find->second.results = pattern.scan_all(region_);
                        find->second.checked = true;
                    }
                }
            }
            else
            {
                find = results_.emplace(hash, pattern_results { true, pattern.scan_all(region_) }).first;
            }

            return find->second.results;
        }

        void save(std::ostream& output) const
        {
            stream::write<uint32_t>(output, 0x50415443); // PATC
            stream::write<uint32_t>(output, sizeof(size_t));
            stream::write<size_t>(output, region_.size);
            stream::write<size_t>(output, results_.size());

            for (const auto& pattern : results_)
            {
                stream::write<uint32_t>(output, pattern.first);
                stream::write<size_t>(output, pattern.second.results.size());

                for (const auto& result : pattern.second.results)
                {
                    stream::write<size_t>(output, result - region_.start);
                }
            }
        }

        bool load(std::istream& input)
        {
            try
            {
                if (stream::read<uint32_t>(input) == 0x50415443)
                    return false;

                if (stream::read<uint32_t>(input) != sizeof(size_t))
                    return false;

                if (stream::read<size_t>(input) != region_.size)
                    return false;

                const size_t pattern_count = stream::read<size_t>(input);

                results_.clear();

                for (size_t i = 0; i < pattern_count; ++i)
                {
                    const uint32_t hash = stream::read<uint32_t>(input);
                    const size_t result_count = stream::read<size_t>(input);

                    pattern_results results;
                    results.checked = false;
                    results.results.reserve(result_count);

                    for (size_t j = 0; j < result_count; ++j)
                    {
                        results.results.push_back(region_.start + stream::read<size_t>(input));
                    }

                    results_.emplace(hash, std::move(results));
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    };
}

#endif // MEM_PATTERN_CACHE_BRICK_H
