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

#include "mem.h"
#include "mem_hasher.h"

namespace mem
{
    class pattern_cache
    {
    protected:
        region region_;
        std::unordered_map<uint32_t, std::vector<pointer>> results_;

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

        pointer scan(const pattern& pattern)
        {
            const auto& results = scan_all(pattern);

            return !results.empty() ? results[0] : nullptr;
        }

        const std::vector<pointer>& scan_all(const pattern& pattern)
        {
            const uint32_t hash = hash_pattern(pattern);

            auto find = results_.find(hash);

            if (find != results_.end())
            {
                bool changed = false;

                for (mem::pointer result : find->second)
                {
                    if (!pattern.match(result))
                    {
                        changed = true;

                        break;
                    }
                }

                if (changed)
                {
                    find->second = pattern.scan_all(region_);
                }
            }
            else
            {
                find = results_.emplace(hash, pattern.scan_all(region_)).first;
            }

            return find->second;
        }
    };
}

#endif // MEM_PATTERN_CACHE_BRICK_H
