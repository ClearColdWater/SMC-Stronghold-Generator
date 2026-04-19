#pragma once

#include<cinttypes>
#include<array>

namespace utils
{
    template<uint16_t bitCount>
    struct PendingBitset
    {
        static_assert(bitCount % 64 == 0, "PendingBitset bitCount must be multiple of 64");
        static constexpr uint16_t wordBits = 64;
        static constexpr uint16_t wordCount = bitCount / wordBits;
        std::array<uint64_t, wordCount> words{};

        PendingBitset() = default;
        inline void clear() noexcept {
            words.fill(0);
        }

        inline bool empty() const noexcept
        {
            for(uint64_t x : words)
                if(x)return false;
            return true;
        }

        inline void set(uint16_t index) noexcept {
            words[index >> 6] |= (1ull << (index & 63));
        }
        inline void reset(uint16_t index) noexcept {
            words[index >> 6] &= ~(1ull << (index & 63));
        }
        inline bool test(uint16_t index) const noexcept {
            return (words[index >> 6] >> (index & 63)) & 1ull;
        }

        template<typename Func>
        inline void forEachSetBit(Func&& f) const
        {
            for(uint32_t w = 0; w < wordCount; w++)
            {
                uint64_t x = words[w];
                while(x)
                {
                #if defined(__GNUC__) || defined(__clang__)
                    uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(x));
                #else
                    uint32_t bit = 0;
                    uint64_t t = x;
                    while((t & 1ull) == 0)
                    {
                        t >>= 1;
                        bit++;
                    }
                #endif
                    f(static_cast<uint16_t>((w << 6) + bit));
                    x &= (x - 1); // clear lowest set bit
                }
            }
        }

        inline bool operator ==(const PendingBitset& other) const noexcept {
            return words == other.words;
        }

        inline bool operator !=(const PendingBitset& other) const noexcept {
            return !(*this == other);
        }

        inline bool operator <(const PendingBitset& other) const noexcept
        {
            for(int i = static_cast<int>(wordCount) - 1; i >= 0; i--)
            {
                if(words[i] != other.words[i])
                    return words[i] < other.words[i];
            }
            return false;
        }
    };

    struct PendingSet
    {
        uint32_t unobservedCount = 0;
        PendingBitset<1024> exists;

        inline bool operator ==(const PendingSet& other) const noexcept {
            return unobservedCount == other.unobservedCount && exists == other.exists;
        }
        inline bool operator !=(const PendingSet& other) const noexcept {
            return !(*this == other);
        }
        inline bool operator <(const PendingSet& other)const noexcept
        {
            if(unobservedCount != other.unobservedCount)return unobservedCount < other.unobservedCount;
            return exists < other.exists;
        }
    };

    struct PendingSetCount
    {
        uint64_t count;
        PendingSet set;

        inline bool operator ==(const PendingSetCount& other) const noexcept {
            return set == other.set;
        }
        inline bool operator !=(const PendingSetCount& other) const noexcept {
            return set != other.set;
        }
        inline bool operator <(const PendingSetCount& other) const noexcept {
            return set < other.set;
        }
    };
}