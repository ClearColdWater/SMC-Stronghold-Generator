#pragma once

#include<iostream>
#include"StrongholdAuxiliaryInfo.hpp"
#include"PendingBitset.hpp"

namespace StrongholdAuxiliary
{
    using namespace utils;

    static constexpr double tol = 1e-6;
    static constexpr double minGamma = 1e-3, maxGamma = 1e3;
    static constexpr uint32_t maxIter = 256;
    inline bool StrongholdAuxiliaryInfo::fitGammaMM(uint32_t nodeCount, double lambda)
    {
        if(pendingSets.empty())
        {
            std::fill(nodeGamma, nodeGamma + 1024, 1.0);
            return true;
        }

        std::vector<double> setTotalAddend;
        setTotalAddend.resize(pendingSets.size());

        double gamma[2][1024];
        std::fill(gamma[0], gamma[0] + 1024, 1.0);
        std::fill(gamma[1], gamma[1] + 1024, 1.0);
        uint32_t tick = 0;

        bool converged = false;
        for(uint32_t i = 0; i < maxIter; i++)
        {
            #pragma omp parallel for schedule(dynamic, 16)
            for(int j = 0; j < static_cast<int>(pendingSets.size()); j++)
            {
                setTotalAddend[j] = static_cast<double>(pendingSets[j].set.unobservedCount);

                pendingSets[j].set.exists.forEachSetBit([&, tick, j](uint32_t nodeIndex){
                    setTotalAddend[j] += gamma[tick][nodeIndex];
                });

                setTotalAddend[j] = static_cast<double>(pendingSets[j].count) / setTotalAddend[j];
            }

            uint32_t nextTick = tick ^ 1;
            double maxDelta = -std::numeric_limits<double>::infinity();
            std::fill(gamma[nextTick], gamma[nextTick] + 1024, 0.0);

            #pragma omp parallel for schedule(dynamic, 16) reduction(+:gamma[nextTick][:1024])
            for(int j = 0; j < static_cast<int>(pendingSets.size()); j++)
            {
                pendingSets[j].set.exists.forEachSetBit([&, nextTick, j](uint32_t nodeIndex){
                    gamma[nextTick][nodeIndex] += setTotalAddend[j];
                });
            }

            for(int j = 0; j < static_cast<int>(nodeCount); j++)
            {
                if(gamma[nextTick][j] == 0)
                {
                    gamma[nextTick][j] = 1;
                    continue;
                }
                
                gamma[nextTick][j] = (winCount[j] + lambda) / (gamma[nextTick][j] + lambda);
                double delta = std::abs(
                    std::log(std::max(gamma[nextTick][j], minGamma)) -
                    std::log(std::max(gamma[tick][j], minGamma))
                );
                maxDelta = std::max(maxDelta, delta);
            }

            tick ^= 1;
            if(maxDelta < tol)
            {
                converged = true;
                break;
            }
        }

        for(uint32_t i = 0; i < nodeCount; i++)
            nodeGamma[i] = std::clamp(gamma[tick][i], minGamma, maxGamma);
        
        return converged;
    }
}