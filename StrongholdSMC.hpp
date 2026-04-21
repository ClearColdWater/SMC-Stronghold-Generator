#pragma once

#include"StrongholdStructure.hpp"
#include"StrongholdGammaMM.hpp"
#include<thread>
#include"OpenMPWrapper.hpp"

namespace StrongholdSMC
{
    using namespace utils;
    using namespace StrongholdStrucures;
    using namespace StrongholdAuxiliary;

    static inline void appendPendingSetCount(std::vector<PendingSetCount>& pendingSets, PendingSetCount&& nextSet)
    {
        if(!pendingSets.empty() && pendingSets.back() == nextSet)
            pendingSets.back().count += nextSet.count;
        else
            pendingSets.emplace_back(std::move(nextSet));
    }
    static inline std::vector<PendingSetCount> mergeTwoSortedPendingSetCounts(
        std::vector<PendingSetCount>&& pendingSetsA,
        std::vector<PendingSetCount>&& pendingSetsB
    ){
        std::vector<PendingSetCount> out;
        out.reserve(pendingSetsA.size() + pendingSetsB.size());
        size_t i = 0, j = 0;
        while(i < pendingSetsA.size() && j < pendingSetsB.size())
        {
            if(pendingSetsA[i] < pendingSetsB[j])
            {
                appendPendingSetCount(out, std::move(pendingSetsA[i]));
                i++;
            }
            else if(pendingSetsB[j] < pendingSetsA[i])
            {
                appendPendingSetCount(out, std::move(pendingSetsB[j]));
                j++;
            }
            else
            {
                pendingSetsA[i].count += pendingSetsB[j].count;
                appendPendingSetCount(out, std::move(pendingSetsA[i]));
                i++;
                j++;
            }
        }
        while(i < pendingSetsA.size())
        {
            appendPendingSetCount(out, std::move(pendingSetsA[i]));
            i++;
        }
        while(j < pendingSetsB.size())
        {
            appendPendingSetCount(out, std::move(pendingSetsB[j]));
            j++;
        }
        return out;
    }

    struct StrongholdBatch 
    {
        std::vector<Stronghold<observationGuided>> stronghold[2];
        uint32_t tick = 0;

        inline std::vector<Stronghold<observationGuided>>& get() {return stronghold[tick]; }

        const StrongholdObservation* observation;
        std::vector<Xoshiro256pp> rngs;

        uint32_t maxPieces, multiStep;
        const StrongholdAuxiliaryInfo* guidingInfo = nullptr;
        std::vector<StrongholdAuxiliaryInfo> nextGenInfos;
        void combineInfos()
        {
            uint32_t nodeCount = observation->tree.totalNodes;
            for(uint32_t i = 1; i < nextGenInfos.size(); i++)
            {
                for(uint32_t j = 0; j < nodeCount; j++)
                {
                    nextGenInfos[0].nodeLogMeanImportanceWeightDelta[j] = 
                        StrongholdAuxiliaryInfo::LogSumExp(nextGenInfos[0].nodeLogMeanImportanceWeightDelta[j], nextGenInfos[i].nodeLogMeanImportanceWeightDelta[j]);

                    nextGenInfos[0].nodeLogMeanImportanceWeightDeltaSq[j] = 
                        StrongholdAuxiliaryInfo::LogSumExp(nextGenInfos[0].nodeLogMeanImportanceWeightDeltaSq[j], nextGenInfos[i].nodeLogMeanImportanceWeightDeltaSq[j]);
                    
                    nextGenInfos[0].nodeTotalOmega[j] += nextGenInfos[i].nodeTotalOmega[j];
                }
            }

            for(uint32_t i = 1; i < nextGenInfos.size(); i++)
                for(uint32_t j = 0; j < nodeCount; j++)
                    nextGenInfos[0].winCount[j] += nextGenInfos[i].winCount[j];

            for(uint32_t stepSize = 1; stepSize < nextGenInfos.size(); stepSize <<= 1)
            {
                #pragma omp parallel for schedule(static, 1)
                for(int j = 0; j < static_cast<int>(nextGenInfos.size() - stepSize); j += (stepSize << 1))
                    nextGenInfos[j].pendingSets = mergeTwoSortedPendingSetCounts(std::move(nextGenInfos[j].pendingSets), std::move(nextGenInfos[j + stepSize].pendingSets));
            }

            nextGenInfos[0].build(nodeCount);
            nextGenInfos[0].fitGammaMM(nodeCount);
        }
        void contributeInfo()
        {
            auto& strongholds = get();

            #pragma omp parallel for schedule(dynamic, 16)
            for(uint32_t i = 0; i < strongholds.size(); i++)
            {
                int tid = omp_get_thread_num();
                
                for(uint32_t j = 0; j < observation->tree.totalNodes; j++)
                    nextGenInfos[tid].winCount[j] += strongholds[i].generationContext.pendingSets.winnerCount[j];

                for(uint32_t j = 0; j < strongholds[i].generationContext.pendingSets.sets.size(); j++)
                    nextGenInfos[tid].pendingSets.push_back({1, strongholds[i].generationContext.pendingSets.sets[j]});
            }

            #pragma omp parallel for schedule(static, 1)
            for(uint32_t i = 0; i < nextGenInfos.size(); i++)
            {
                int tid = omp_get_thread_num();
                std::sort(nextGenInfos[tid].pendingSets.begin(), nextGenInfos[tid].pendingSets.end());
            }
        }
        inline StrongholdAuxiliaryInfo getNextInfo() {return nextGenInfos[0];}

        uint32_t N;
        uint32_t uniqueAncestors;

        void init(uint64_t seed)
        {
            stronghold[tick].reserve(N + 4 * std::sqrt(N));
            stronghold[tick].resize(N);
            stronghold[tick ^ 1].reserve(N + 4 * std::sqrt(N));

            rngs.clear();
            int numThreads = omp_get_max_threads();
            for(int i = 0; i < numThreads; i++)
                rngs.emplace_back(Xoshiro256pp(seed + i));
            
            nextGenInfos.resize(numThreads);
            for(int i = 0; i < numThreads; i++)
                nextGenInfos[i].clear();

            #pragma omp parallel for schedule(dynamic, 16)
            for(int i = 0; i < static_cast<int>(N); i++)
            {
                int tid = omp_get_thread_num();
                stronghold[tick][i].initGeneration(rngs[tid], observation, guidingInfo, &nextGenInfos[tid]);
                stronghold[tick][i].rootAncestorId = i;
            }
        }

        std::pair<double, bool> step(std::vector<double>& auxiliaryWeights, std::vector<double>& trueWeights, uint32_t currentSize)
        {
            int finished = 1;
            double maxLogWeight = -std::numeric_limits<double>::infinity();
            #pragma omp parallel for schedule(dynamic, 16) reduction(max:maxLogWeight) reduction(&:finished)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                int tid = omp_get_thread_num();
                
                stronghold[tick][i].generationContext.nextGenInfo = &nextGenInfos[tid]; // rebind for dynamic schedule
                if(!stronghold[tick][i].finished)
                {
                    for(uint32_t j = 0; j < multiStep; j++)
                        if(stronghold[tick][i].generationContext.logImportanceWeight != -1e9)
                            stronghold[tick][i].stepGeneration(rngs[tid], maxPieces);
                    finished = 0;
                }
                trueWeights[i] = stronghold[tick][i].generationContext.logImportanceWeight;
                auxiliaryWeights[i] = trueWeights[i] + 
                    stronghold[tick][i].generationContext.remainingLogImportanceDebt +
                    stronghold[tick][i].generationContext.pendingTypePriorityAuxCarry;
                
                if(auxiliaryWeights[i] > maxLogWeight)
                    maxLogWeight = auxiliaryWeights[i];
            }

            return std::make_pair(maxLogWeight, finished);
        }

        double getTotalWeight(std::vector<double>& auxiliaryWeights, double maxLogWeight, uint32_t currentSize) // logAuxiliaryWeights -> auxiliaryWeights
        {
            double totalWeight = 0;
            #pragma omp parallel for schedule(dynamic, 16) reduction(+:totalWeight)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                auxiliaryWeights[i] = std::exp(auxiliaryWeights[i] - maxLogWeight);
                totalWeight += auxiliaryWeights[i];
            }
            return totalWeight;
        }

        double normalizeAndGetESS(std::vector<double>& auxiliaryWeights, double totalWeight, uint32_t currentSize)
        {
            double squareSum = 0;
            #pragma omp parallel for schedule(dynamic, 16) reduction(+:squareSum)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                auxiliaryWeights[i] /= totalWeight;
                squareSum += auxiliaryWeights[i] * auxiliaryWeights[i];
            }

            return 1 / squareSum;
        }

        void chopthin(std::vector<double>& auxiliaryWeights, 
            std::vector<uint32_t>& duplicateCounts, std::vector<uint32_t>& prefixCounts, uint32_t currentSize, bool finalStep = false
        ){
            double thresholdWeight = 1.0 / N;
            #pragma omp parallel for schedule(dynamic, 16)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                int tid = omp_get_thread_num();

                double expected = auxiliaryWeights[i] / thresholdWeight;
                duplicateCounts[i] = expected;
                double residual = expected - duplicateCounts[i];
                duplicateCounts[i] += rngs[tid].nextDouble() < residual;
            }

            prefixCounts[0] = 0;
            for(uint32_t i = 0; i < currentSize; i++)
                prefixCounts[i + 1] = prefixCounts[i] + duplicateCounts[i];

            stronghold[tick ^ 1].resize(prefixCounts[currentSize]);

            #pragma omp parallel for schedule(dynamic, 16)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                for(uint32_t j = 0; j < duplicateCounts[i]; j++)
                {
                    Stronghold<observationGuided>& nextStronghold = stronghold[tick ^ 1][prefixCounts[i] + j];
                    nextStronghold = stronghold[tick][i];
                    nextStronghold.generationContext.logImportanceWeight = -nextStronghold.generationContext.remainingLogImportanceDebt;
                    if(!finalStep)
                        nextStronghold.generationContext.logImportanceWeight -= nextStronghold.generationContext.pendingTypePriorityAuxCarry;
                    //else
                        //assert(std::abs(nextStronghold.generationContext.remainingLogImportanceDebt) < 1e-6);
                    nextStronghold.generationContext.omega /= duplicateCounts[i];
                    nextStronghold.prepareCapacity();
                }
            }
        }

        void renewOmega(uint32_t currentSize)
        {
            #pragma omp parallel for schedule(dynamic, 16)
            for(int i = 0; i < static_cast<int>(currentSize); i++)
            {
                double omega = stronghold[tick][i].generationContext.omega;
                stronghold[tick][i].generationContext.omega += 0.5 * omega * (1 - omega);
            }
        }

        bool generateStrongholds(
            const StrongholdObservation* strongholdObservation, 
            uint32_t initN = 2048, double resampleThreshold = 0.8, uint64_t seed = 42, 
            uint32_t maxPieceCount = 1024, uint32_t multiSteps = 3,
            const StrongholdAuxiliaryInfo* guidingStrongholdInfo = nullptr
        ){
            observation = strongholdObservation;
            guidingInfo = guidingStrongholdInfo;
            maxPieces = maxPieceCount;
            multiStep = multiSteps;
            N = initN;

            init(seed);

            std::vector<double> trueWeights, auxiliaryWeights;
            std::vector<uint32_t> duplicateCounts, prefixCounts;

            bool finalChopthin = false;
            for(uint32_t stepCount = 0; true; stepCount++)
            {
                std::cerr << '\r' << "Step: " << stepCount;
                uint32_t currentSize = stronghold[tick].size();
                trueWeights.resize(currentSize);
                auxiliaryWeights.resize(currentSize);
                duplicateCounts.resize(currentSize);
                prefixCounts.resize(currentSize + 1);

                auto pair = step(auxiliaryWeights, trueWeights, currentSize);
                renewOmega(currentSize);
                double maxLogWeight = pair.first;
                bool finished = pair.second;
                
                if(finished)
                {
                    if(finalChopthin)
                    {
                        std::cerr << std::endl;
                        break;
                    }
                    finalChopthin = true;

                    maxLogWeight = -std::numeric_limits<double>::infinity();
                    for(uint32_t i = 0; i < currentSize; i++)
                    {
                        auxiliaryWeights[i] = trueWeights[i];
                        if(auxiliaryWeights[i] > maxLogWeight)
                            maxLogWeight = auxiliaryWeights[i];
                    }
                }

                double totalWeight = getTotalWeight(auxiliaryWeights, maxLogWeight, currentSize);
                if(totalWeight == 0)return false;

                double ESS = normalizeAndGetESS(auxiliaryWeights, totalWeight, currentSize);
                if(!finalChopthin && ESS >= resampleThreshold * initN)continue;
                chopthin(auxiliaryWeights, duplicateCounts, prefixCounts, currentSize);

                tick ^= 1;

                uniqueAncestors = 0;
                std::vector<uint8_t> ancestorExists(initN, 0);
                for(uint32_t i = 0; i < stronghold[tick].size(); i++) {
                    uint32_t ancId = stronghold[tick][i].rootAncestorId;
                    if(ancestorExists[ancId] == 0) {
                        ancestorExists[ancId] = 1;
                        uniqueAncestors++;
                    }
                }
                std::cerr << "\rStep: " << stepCount 
                    << " | ESS: " << ESS 
                    << " | True Unique Ancestors: " << uniqueAncestors << std::endl;
            }

            contributeInfo();
            combineInfos();
            return true;
        }

        bool generateStrongholdsWithBootstrapInfo(
            const StrongholdObservation* strongholdObservation, uint32_t epochs = 12,
            uint32_t initN = 2048, double resampleThreshold = 0.8, uint64_t seed = 42,
            const StrongholdAuxiliaryInfo* guidingStrongholdInfo = nullptr // in case multiple runs was needed
        );
    };
}