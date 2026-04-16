#pragma once

#include<algorithm>
#include<vector>
namespace utils
{
    uint64_t updateHash(uint64_t previousHash, int value)
    {
        constexpr uint64_t c = 1000000007, m = (1ll << 61) - 1;
        return (previousHash * c + value) % m;
    }
}
