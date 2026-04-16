#pragma once

#include"StrongholdObservations.hpp"
#include"StrongholdStructure.hpp"
#include"SortedVectorMap.hpp"
#include"StrongholdObservationJson.hpp"
#include"Hash.hpp"
#include<cstring>
#include<stdexcept>
#include<limits>

namespace StrongholdObservations
{
    using namespace utils;
    using namespace StructurePieces;

    inline uint32_t extractCustomData(const StrongholdPieceVariant& v)
    {
        return std::visit([](const auto& piece) -> uint32_t {
            using T = std::decay_t<decltype(piece)>;
            if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::RoomCrossing>)
                return piece.roomType;
            else if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::Library>)
                return piece.getBoundingBox().getYSize() == 6; // small library
            else if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::SmallCorridor>)
                return piece.steps;
            else 
                return 0;
        }, v);
    }

    inline void getDecidedBranches(const StrongholdPieceVariant& v, bool decided[5])
    {
        std::fill(decided, decided + 5, false);
        std::visit([&](const auto& piece){
            using T = std::decay_t<decltype(piece)>;
            if constexpr(
                std::is_same_v<T, ConcreteStrongholdPieces::ChestCorridor> ||
                std::is_same_v<T, ConcreteStrongholdPieces::StraightStairs> ||
                std::is_same_v<T, ConcreteStrongholdPieces::PrisonCell> ||
                std::is_same_v<T, ConcreteStrongholdPieces::LeftTurn> ||
                std::is_same_v<T, ConcreteStrongholdPieces::RightTurn> ||
                std::is_same_v<T, ConcreteStrongholdPieces::SpiralStairs> ||
                std::is_same_v<T, ConcreteStrongholdPieces::StarterStairs>
            ){
                decided[0] = true;
            }
            else if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::RoomCrossing>)
            {
                decided[0] = decided[1] = decided[2] = true;
            }
            else if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::BranchableCorridor>)
            {
                decided[0] = true;
                decided[1] = piece.expandsX;
                decided[2] = piece.expandsZ;
            }
            else if constexpr(std::is_same_v<T, ConcreteStrongholdPieces::FiveWayCrossing>)
            {
                decided[0] = true;
                decided[1] = piece.confusedLeftLow;
                decided[2] = piece.confusedLeftHigh;
                decided[3] = piece.confusedRightLow;
                decided[4] = piece.confusedRightHigh;
            }
        }, v);
    }

    inline StrongholdObservation generateObservation(uint64_t seed, uint32_t nodeCount, bool excludePortalRoom)
    {
        if(nodeCount == 0)nodeCount = 1;
        Xoshiro256pp rng(seed);
        Stronghold<vanilla> stronghold(rng);

        auto& list = stronghold.builtPieces.list;
        SortedVectorMap<uint64_t, uint32_t> hashToIndexMap;
        for(uint32_t i = 0; i < list.size(); i++)
        {
            uint64_t hash = getBase(list[i])->ancestryHash;
            hashToIndexMap.delayedInsert(hash, i);
        }
        hashToIndexMap.build();

        int parent[1024], whichChild[1024];
        int pieceIndexToObservationIndex[1024];
        int children[1024][5];
        memset(parent, -1, sizeof(parent));
        memset(whichChild, -1, sizeof(whichChild));
        memset(pieceIndexToObservationIndex, -1, sizeof(pieceIndexToObservationIndex));
        memset(children, -1, sizeof(children));
        for(uint32_t i = 0; i < list.size(); i++)
        {
            AbstractStrongholdPiece* parentPiece = getBase(list[i]);
            uint64_t parentHash = parentPiece->ancestryHash;
            for(uint32_t j = 0; j < 5; j++)
            {
                uint64_t childHash = updateHash(parentHash, j);
                int childIndex = hashToIndexMap.find(childHash);
                if(childIndex != -1)
                {
                    parent[childIndex] = i;
                    whichChild[childIndex] = j;
                    children[i][j] = childIndex;
                }
            }
        }

        StrongholdObservation observation;
        observation.starterDirection = getBase(list[0])->getFacingDirection();
        std::vector<std::pair<uint32_t, double>> observedLeaf;
        observedLeaf.push_back(std::make_pair(0, 1));

        double branchGenerateWeights[5];
        std::fill(branchGenerateWeights, branchGenerateWeights + 5, 1.0);
        double weights[16];

        bool noPortalEnding = false;
        for(uint32_t i = 0; i < nodeCount && !observedLeaf.empty() && !noPortalEnding; i++)
        {
            double totalWeight = 0;
            for(uint32_t j = 0; j < observedLeaf.size(); j++)
                totalWeight += (observedLeaf[j].second = std::max(1e-2, observedLeaf[j].second * 0.9));

            double r = rng.nextDouble() * totalWeight;
            for(uint32_t j = 0; j < observedLeaf.size(); j++)
            {
                r -= observedLeaf[j].second;
                if(r < 0)
                {
                    int pieceIndex = observedLeaf[j].first;

                    if(excludePortalRoom && getBase(list[pieceIndex])->getPieceType() == PORTAL_ROOM)
                    {
                        if(observedLeaf.size() == 1)noPortalEnding = true;
                        continue;
                    }

                    std::swap(observedLeaf[j], observedLeaf.back());
                    observedLeaf.pop_back();

                    memset(weights, 0, sizeof(weights));
                    weights[getBase(list[pieceIndex])->getPieceType()] = 1;

                    int observedParent = parent[pieceIndex] == -1 ? -1 : pieceIndexToObservationIndex[parent[pieceIndex]];
                    uint32_t observationIndex = observation.tree.insertNode(weights, branchGenerateWeights, observedParent, whichChild[pieceIndex], extractCustomData(list[pieceIndex]));
                    pieceIndexToObservationIndex[pieceIndex] = observationIndex;

                    for(uint32_t k = 0; k < 5; k++)
                    {
                        if(children[pieceIndex][k] == -1)continue;
                        observedLeaf.push_back(std::make_pair(children[pieceIndex][k], 1));
                    }

                    break;
                }
            }
        }

        for(uint32_t i = 0; i < 16; i++)weights[i] = 1;
        for(uint32_t i = 0; i < observedLeaf.size(); i++)
        {
            int pieceIndex = observedLeaf[i].first;
            int observedParent = parent[pieceIndex] == -1 ? -1 : pieceIndexToObservationIndex[parent[pieceIndex]];
            observation.tree.insertNode(weights, branchGenerateWeights, observedParent, whichChild[pieceIndex]);
        }

        double noneWeights[16] = {0};
        noneWeights[NONE] = 1;
        double defaultBranchWeights[5];
        std::fill(defaultBranchWeights, defaultBranchWeights + 5, 1.0);

        for(uint32_t pieceIndex = 0; pieceIndex < list.size(); pieceIndex++)
        {
            int observationIndex = pieceIndexToObservationIndex[pieceIndex];
            if(observationIndex == -1)continue;

            auto& node = observation.tree.nodes[observationIndex];
            if(node.determinedType == UNKNOWN || node.determinedType == NONE)continue;

            bool decided[5];
            getDecidedBranches(list[pieceIndex], decided);

            StructurePieceType type = getBase(list[pieceIndex])->getPieceType();
            for(uint32_t j = StrongholdObservation::optionalChildIndexStart(type); j < StrongholdObservation::nonexistantChildIndexStart(type); j++)
                if(decided[j])node.branchGenerateWeights[j] = std::numeric_limits<double>::infinity();

            for(uint32_t j = 0; j < StrongholdObservation::nonexistantChildIndexStart(type); j++)
            {
                if(!decided[j])continue; // !decided == decided not fair logic right
                if(node.ch[j] >= 0)continue;
                
                observation.tree.insertNode(noneWeights, defaultBranchWeights, observationIndex, j);
            }
        }

        if(!observation.build(rng))
            throw std::runtime_error("generateObservation: observation.build failed");
        return observation;
    }
}