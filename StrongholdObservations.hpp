#pragma once

#include"AbstractStrongholdPieces.hpp"
#include"SortedVectorMap.hpp"
#include"Hash.hpp"
#include<cstring>
#include<stdexcept>

namespace StrongholdObservations
{
    using namespace utils;
    using namespace StructurePieces;

    static constexpr int branchNotGenerate = -1, unsureIfBranchGenerateWithoutChildObservation = -2;
    struct StrongholdObservation
    {
        SortedVectorMap<uint64_t, uint32_t> branchHashToIndexMap;
        Direction starterDirection = EAST;
        GenerationContext::SoABoundingBox observedBoxes;
        uint32_t instancesSpawned[16];
        uint32_t nodeIndexToBoxIndex[1024];
        struct ObservedStrongholdTree
        {
            struct Node
            {
                // -1: branch does not even attempt to generate (no opening)
                // -2: unsure if branch generates(unsure opening, decided by branchGenerateWeights); no further observation for its room type info(no type info).
                // >= 0: branch may or may not generate(unsure opening, decided by branchGenerateWeights); child node is at index stored(type info in child, only happens if branch generates); NONE type is stored in child.
                // mandatory to fill in for every single child because default is branch not attempt to generate but some branches always generate. at least fill in -2
                int ch[5]; 
                // only for BranchableCorridor / FiveWayCrossing, weight of not generating is set to 1.
                double branchGenerateWeights[5];
                uint32_t customData = 0;
                StructurePieceType determinedType = UNKNOWN;
                double roomTypeLikelihoodWeights[16];

                Node()
                {
                    std::memset(ch, branchNotGenerate, sizeof(ch)); 
                    std::fill(branchGenerateWeights, branchGenerateWeights + 5, 1.0);
                }
            }nodes[1024];

            uint32_t insertNode(double* typeLikelihoodWeights, double *branchGenerateWeights, int parent = -1, int whichChild = -1, uint32_t customData = 0)
            {
                uint32_t id = totalNodes++;
                std::copy(branchGenerateWeights, branchGenerateWeights + 5, nodes[id].branchGenerateWeights);
                std::copy(typeLikelihoodWeights, typeLikelihoodWeights + 16, nodes[id].roomTypeLikelihoodWeights);

                nodes[id].customData = customData;
                uint32_t nonZeroCount = 0, determinedType;
                for(int i = 0; i < 16; i++)
                {
                    if(typeLikelihoodWeights[i] != 0)
                    {
                        nonZeroCount++;
                        determinedType = i;
                    }
                }
                if(nonZeroCount == 0)throw std::runtime_error("At least one type likelihood weight should be non zero");
                if(nonZeroCount == 1)nodes[id].determinedType = static_cast<StructurePieceType>(determinedType);
                else nodes[id].determinedType = UNKNOWN;

                if(parent == -1)return id; 
                nodes[parent].ch[whichChild] = id;
                return id;
            }
            uint32_t totalNodes = 0;
        }tree;

        static constexpr uint32_t optionalChildIndexStart(StructurePieceType type)
        {
            switch(type)
            {
                case SMALL_CORRIDOR: case LIBRARY: case PORTAL_ROOM: case NONE: return 0;
                case ROOM_CROSSING: return 3;
                default: return 1;
            }
        }
        static constexpr uint32_t nonexistantChildIndexStart(StructurePieceType type)
        {
            switch(type)
            {
                case SMALL_CORRIDOR: case LIBRARY: case PORTAL_ROOM: case NONE: return 0;
                case BRANCHABLE_CORRIDOR: case ROOM_CROSSING: return 3;
                case FIVE_WAY_CROSSING: return 5;
                default: return 1;
            }
        }
        void dfsBuild(uint32_t p = 0, uint64_t hash = 42)
        {
            branchHashToIndexMap.delayedInsert(hash, p);

            StructurePieceType type = tree.nodes[p].determinedType;
            if(type == UNKNOWN)
            {
                std::fill(tree.nodes[p].ch, tree.nodes[p].ch + 5, unsureIfBranchGenerateWithoutChildObservation);
                return;
            }

            for(uint32_t i = 0; i < optionalChildIndexStart(type); i++) // these branches must exist
            {
                if(tree.nodes[p].ch[i] < 0)
                    tree.nodes[p].ch[i] = unsureIfBranchGenerateWithoutChildObservation;
                else 
                    dfsBuild(tree.nodes[p].ch[i], updateHash(hash, i));
            }
            for(uint32_t i = optionalChildIndexStart(type); i < nonexistantChildIndexStart(type); i++)
            {
                if(tree.nodes[p].ch[i] < 0)continue;
                dfsBuild(tree.nodes[p].ch[i], updateHash(hash, i));
            }
        }

        bool build(Xoshiro256pp& rng);
    };

    inline StrongholdObservation generateObservation(uint64_t seed, uint32_t nodeCount, bool excludePortalRoom = false);
}
