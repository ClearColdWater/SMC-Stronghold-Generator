#pragma once

#include<algorithm>
#include<vector>
namespace utils
{
    template<typename keyType, typename valueType>
    struct SortedVectorMap
    {
        inline void delayedInsert(keyType key, valueType value) { d.push_back({key, value}); }
        inline void build()
        {
            std::sort(d.begin(), d.end(), [](const auto& pairA, const auto& pairB){
               return pairA.first < pairB.first; 
            });
        }
        
        inline int find(keyType key)const
        {
            auto it = std::lower_bound(d.begin(), d.end(), key, [](const auto& pair, keyType k){
               return pair.first < k; 
            });
            
            if(it != d.end() && it->first == key)
                return it->second;
           
            return -1;
        }
        
        private:
            std::vector<std::pair<keyType, valueType>> d; 
    };
}
