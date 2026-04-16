#pragma once

#include<cstdint>
namespace utils 
{
    class Xoshiro256pp 
    {
        uint64_t s[4];
        static inline uint64_t rotl(const uint64_t x, int k) {
            return (x << k) | (x >> (64 - k));
        }
        
    public:
        explicit Xoshiro256pp(uint64_t seed) 
        {
            uint64_t z = (seed += 0x9e3779b97f4a7c15);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            s[0] = z ^ (z >> 31);
            
            s[1] = s[0] + 0x9e3779b97f4a7c15;
            s[2] = s[1] + 0x9e3779b97f4a7c15;
            s[3] = s[2] + 0x9e3779b97f4a7c15;
        }

        inline uint64_t next() 
        {
            const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
            const uint64_t t = s[1] << 17;
            s[2] ^= s[0];
            s[3] ^= s[1];
            s[1] ^= s[2];
            s[0] ^= s[3];
            s[2] ^= t;
            s[3] = rotl(s[3], 45);
            return result;
        }
        inline uint64_t operator()() {return next();}

        inline uint32_t nextInt(uint32_t bound) {
            return static_cast<uint32_t>((static_cast<__uint128_t>(next()) * bound) >> 64);
        }
        
        inline bool nextBool() {
            return static_cast<int64_t>(next()) < 0; 
        }
        
        inline double nextDouble(){
            return (next() >> 11) * 0x1.0p-53;
        }
    };
}
