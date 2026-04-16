#pragma once

#include"StrongholdPieces.hpp"

namespace StrongholdStrucures
{
    using namespace StrongholdPieces;
    using namespace StrongholdObservations;

    static constexpr double logp5 = -0.6931471805599453;
    template<nextPieceSelectionMethod method = vanilla>
    struct Stronghold
    {
        StrongholdPieceList builtPieces;
        StrongholdGenerationContext generationContext;
        bool finished;
        
        Stronghold() = default;
        Stronghold(Xoshiro256pp& rng){ generate(rng); }

        uint32_t order = 0;
        uint32_t rootAncestorId = 0;

        bool generate(Xoshiro256pp& rng)
        {
            if constexpr(method == constraintRestricted)
                std::cerr << "generate should not be called for constraintRestricted" << std::endl;

            while(true)
            {
                initGeneration(rng);
                while(!stepGeneration(rng));
                if(generationContext.portalRoomGenerated)return true;
            }
        }

        void initGeneration(
            Xoshiro256pp& rng, const StrongholdObservation* observation = nullptr, 
            const StrongholdAuxiliaryInfo* guidingInfo = nullptr, StrongholdAuxiliaryInfo* nextGenInfo = nullptr
        ){
            finished = false;
            builtPieces.list.clear();
            builtPieces.list.reserve(512);
            generationContext.prepareStructurePieces();

            generationContext.observation = observation;
            if constexpr(method == observationGuided)
            {
                generationContext.guidingInfo = guidingInfo;
                generationContext.nextGenInfo = nextGenInfo;
                generationContext.omega = 1;

                if(observation != nullptr)
                {
                    generationContext.observedBoxes = observation->observedBoxes;
                    std::copy(observation->instancesSpawned, observation->instancesSpawned + 16, generationContext.priorInstancesSpawned);
                }

                if(guidingInfo != nullptr)
                {
                    generationContext.remainingLogImportanceDebt = guidingInfo->totalLogImportanceDebt;
                    // doesn't change anything since root never contribute weight anyways
                    // generationContext.remainingLogImportanceDebt -= guidingInfo->nodeLogMeanImportanceWeightDelta[0]; 
                }

                if(nextGenInfo != nullptr)
                    generationContext.poEvents.reserve(4096);
                    
            }

            order = 0;
            Direction starterDirection = observation == nullptr ? getRandomDirection(rng) : observation->starterDirection;

            uint32_t starterId = builtPieces.add(ConcreteStrongholdPieces::StarterStairs(generationContext.getNextId(), starterDirection, 2, 2));
            generationContext.existingBoxes.push_back(getBase(builtPieces.list[starterId])->getBoundingBox());
            getBase(builtPieces.list[starterId])->ancestryHash = 42;
            if constexpr(method == observationGuided)
                generationContext.observedBoxes.remove(0 /*index of starter is always 0*/);

            getBase(builtPieces.list[starterId])->expansionOrder = order++;
            dispatchBuildComponent<method>(builtPieces.list[starterId], generationContext, builtPieces, rng);
        }

        inline bool firstChildBlockedByLastPlaced(uint32_t pieceId)
        {
            const auto* observation = generationContext.observation;
            if(observation == nullptr)return false;
            if(generationContext.lastPlaced == NONE || generationContext.lastPlaced == UNKNOWN)return false;

            auto& pieceVariant = builtPieces.list[pieceId];
            auto* piece = getBase(pieceVariant);
            switch(piece->getPieceType())
            {
                case LIBRARY:
                case PORTAL_ROOM:
                case SMALL_CORRIDOR:
                    return false;
                default:
                    break;
            }

            uint64_t childHash = utils::updateHash(piece->ancestryHash, 0);
            int childObservationIndex = observation->branchHashToIndexMap.find(childHash);
            if(childObservationIndex == -1)return false;

            StructurePieceType determinedType = observation->tree.nodes[childObservationIndex].determinedType;
            return determinedType != UNKNOWN && determinedType != NONE && determinedType == generationContext.lastPlaced;
        }
        std::vector<double>& getLastPlacedProposal()
        {
            static thread_local std::vector<double> proposal;
            const auto& pending = generationContext.pendingChildren;
            proposal.resize(pending.size());
            for(uint32_t i = 0; i < pending.size(); i++)
                proposal[i] = !firstChildBlockedByLastPlaced(pending[i]);
            return proposal;
        }
        
        static double getRaceProb(int a, int b)
        {
            static double H[64][64] = {0};
            static bool init = false;
            if(!init)
            {
                for(int j = 0; j < 64; ++j)H[0][j] = 1.0;
                for(int i = 1; i < 64; ++i)H[i][0] = 0.0;
                for(int i = 1; i < 64; ++i)
                    for(int j = 1; j < 64; ++j)
                        H[i][j] = 0.5 * (H[i - 1][j] + H[i][j - 1]);
                init = true;
            }
            return H[a][b];
        }

        inline int getPendingObservationIndex(uint32_t pendingIndex) const
        {
            if(generationContext.observation == nullptr)return -1;
            uint32_t pieceId = generationContext.pendingChildren[pendingIndex];
            uint64_t hash = getBase(builtPieces.list[pieceId])->ancestryHash;
            return generationContext.observation->branchHashToIndexMap.find(hash);
        }

        std::vector<double>& getTypePriorityProposal(std::vector<int>& pendingIndexToObservationIndex)
        {
            static thread_local std::vector<double> proposal;
            const auto& pending = generationContext.pendingChildren;
            const uint32_t leavesCount = pending.size();

            pendingIndexToObservationIndex.resize(leavesCount);
            for(uint32_t i = 0; i < leavesCount; ++i)
                pendingIndexToObservationIndex[i] = getPendingObservationIndex(i);

            proposal.resize(leavesCount);
            if(generationContext.guidingInfo == nullptr)
            {
                std::fill(proposal.begin(), proposal.end(), 1);
                return proposal;
            }

            /*double maxLogProposal = -std::numeric_limits<double>::infinity();
            for(uint32_t i = 0; i < leavesCount; i++)
            {
                proposal[i] = 0;
                if(pendingIndexToObservationIndex[i] == -1)
                {
                    proposal[i] += logp5 * leavesCount;
                    continue;
                }
                for(uint32_t j = 0; j < leavesCount; j++)
                {
                    if(i == j || pendingIndexToObservationIndex[j] == -1)proposal[i] += logp5;
                    else proposal[i] += std::log(generationContext.guidingInfo->
                        pairwisePendingWinRate[pendingIndexToObservationIndex[i]][pendingIndexToObservationIndex[j]]);
                }
                maxLogProposal = std::max(maxLogProposal, proposal[i]);
            }

            if(!std::isfinite(maxLogProposal))
            {
                std::fill(proposal.begin(), proposal.end(), 1.0);
                return proposal;
            }

            for(uint32_t i = 0; i < pending.size(); i++)
                proposal[i] = std::exp(proposal[i] - maxLogProposal);

            static int cnt = 0;
            cnt++;
            if(cnt % 100000 == 0)
            {
                for(uint32_t i = 0; i < pending.size(); i++)
                    std::cerr << proposal[i] << ' ';
                std::cerr << std::endl;
            }*/
            

            for(uint32_t i = 0; i < leavesCount; i++)
            {
                proposal[i] = 0;
                if(pendingIndexToObservationIndex[i] == -1)
                {
                    proposal[i] += 0.5 * leavesCount;
                    continue;
                }
                for(uint32_t j = 0; j < leavesCount; j++)
                {
                    if(i == j || pendingIndexToObservationIndex[j] == -1)proposal[i] += 0.5;
                    else proposal[i] += generationContext.guidingInfo->
                        pairwisePendingWinRate[pendingIndexToObservationIndex[i]][pendingIndexToObservationIndex[j]];
                }/**/
            }
                
            return proposal;
        }
        
        uint32_t getNextPendingIndexGuided(Xoshiro256pp& rng)
        {
            static thread_local std::vector<int> pendingIndexToObservationIndex;
            std::vector<double>& nextPieceProposal = getLastPlacedProposal();
            std::vector<double>& typePriorityProposal = getTypePriorityProposal(pendingIndexToObservationIndex);
            static thread_local std::vector<double> proposal;
            const auto& pending = generationContext.pendingChildren;
            const uint32_t leavesCount = pending.size();
            proposal.resize(leavesCount);

            double sum = 0, sumBeforeType = 0;
            for(uint32_t i = 0; i < leavesCount; i++)
            {
                sum += proposal[i] = nextPieceProposal[i] * typePriorityProposal[i];
                sumBeforeType += nextPieceProposal[i];
            }
            
            if(sum == 0)
            {
                generationContext.logImportanceWeight = -1e9;
                return 0xffffffff;
            }

            double r = rng.nextDouble() * sum;
            double cumulative = 0;
            for(uint32_t i = 0; i < leavesCount; i++)
            {
                cumulative += proposal[i];
                if(cumulative >= r)
                {
                    // = log(p/q) = log p - log q = log(1/n) - log(p[i] / sum) = log(sum) - log(p[i]) - log(n)
                    double deltaImportanceWeight = std::log(sum) - std::log(proposal[i]) - std::log(static_cast<double>(leavesCount));
                    generationContext.logImportanceWeight += deltaImportanceWeight;

                    double deltaImportanceWeightType = std::log(sum) - std::log(sumBeforeType) - std::log(typePriorityProposal[i]);
                    generationContext.pendingTypePriorityAuxCarry -= deltaImportanceWeightType;

                    if(generationContext.nextGenInfo != nullptr)[[likely]]
                    {
                        uint8_t comparableCount = 0;
                        for(uint32_t j = 0; j < leavesCount; j++)
                            if(j != i && pendingIndexToObservationIndex[j] != -1)
                                comparableCount++;

                        if(comparableCount == 0)return i;
                        for(uint32_t j = 0; j < leavesCount; j++)
                        {
                            if(i == j || pendingIndexToObservationIndex[i] == -1 || pendingIndexToObservationIndex[j] == -1)continue;
                            generationContext.poEvents.push_back(pendingIndexToObservationIndex[i], pendingIndexToObservationIndex[j], comparableCount);
                        }
                    }
                    
                    return i;
                }
            }

            return 0; // fallback, shouldn't happen
        }

        inline uint32_t getNextPendingIndexUniform(Xoshiro256pp& rng)
        {
            std::vector<uint32_t>& pending = generationContext.pendingChildren;
            return rng.nextInt(pending.size()); 
        }

        bool stepGeneration(Xoshiro256pp& rng, uint32_t maxPieceCount = 1024)
        {
            if(finished)return true;
            if(builtPieces.list.size() >= maxPieceCount)
            {
                generationContext.logImportanceWeight = -1e9;
                return finished = true;
            }

            generationContext.pendingTypePriorityAuxCarry *= 0.9;

            std::vector<uint32_t>& pending = generationContext.pendingChildren;
            if(pending.empty())
            {
                if constexpr(method == observationGuided)
                {
                    if(!generationContext.portalRoomGenerated)
                    {
                        generationContext.logImportanceWeight = -1e9;
                        //std::cerr << 'f' << std::endl;
                    }
                }
                return finished = true;
            }

            uint32_t pendingIndex;
            if constexpr(method == observationGuided)
            {
                pendingIndex = getNextPendingIndexGuided(rng);
                if(pendingIndex == 0xffffffff)return finished = true;
            }
            else pendingIndex = getNextPendingIndexUniform(rng); 

            uint32_t chosenId = pending[pendingIndex];
            std::swap(pending[pendingIndex], pending[pending.size() - 1]);
            pending.pop_back();
            
            getBase(builtPieces.list[chosenId])->expansionOrder = order++;
            dispatchBuildComponent<method>(builtPieces.list[chosenId], generationContext, builtPieces, rng);

            if constexpr(method == observationGuided)
            {
                if(generationContext.logImportanceWeight <= -1e9)
                    return finished = true;
            }
            return false;
        }

        void prepareCapacity()
        {
            if(builtPieces.list.size() == builtPieces.list.capacity())
                builtPieces.list.reserve(builtPieces.list.size() + 256);

            if(generationContext.existingBoxes.minX.size() == generationContext.existingBoxes.minX.capacity())
                generationContext.existingBoxes.reserve(generationContext.existingBoxes.minX.size() + 256);
        }
    };
}

namespace StrongholdObservations
{
    using namespace utils;
    using namespace StrongholdStrucures;
    bool StrongholdObservation::build(Xoshiro256pp& rng)
    {
        this->dfsBuild();
        this->branchHashToIndexMap.build();

        std::memset(this->instancesSpawned, 0, sizeof(this->instancesSpawned));
        for(uint32_t i = 2; i < this->tree.totalNodes; i++)
            this->instancesSpawned[this->tree.nodes[i].determinedType]++;

        Stronghold<nextPieceSelectionMethod::constraintRestricted> stronghold;
        stronghold.initGeneration(rng, this);
        while(!stronghold.stepGeneration(rng));
        if(stronghold.generationContext.logImportanceWeight == -1e9)return false;

        this->observedBoxes = stronghold.generationContext.existingBoxes;
        memset(this->nodeIndexToBoxIndex, -1, sizeof(this->nodeIndexToBoxIndex));
        for(uint32_t boxIndex = 0; boxIndex < stronghold.builtPieces.list.size(); boxIndex++)
        {
            uint64_t hash = getBase(stronghold.builtPieces.list[boxIndex])->ancestryHash;
            uint32_t nodeIndex = this->branchHashToIndexMap.find(hash);
            this->nodeIndexToBoxIndex[nodeIndex] = boxIndex;
        }
        return true;
    }
}