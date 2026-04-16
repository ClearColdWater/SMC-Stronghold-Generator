#pragma once

#include"StrongholdStructure.hpp"
#include"StrongholdObservations.hpp"
#include<unordered_set>
#include<cmath>

namespace StrongholdVerifier
{
    using namespace StrongholdPieces;
    using namespace StrongholdObservations;
    using namespace StrongholdStrucures;
    constexpr int verifySuccess = 1;
    constexpr int noRoot = 0xc0000001, duplicateHash = 0xc0000002, extraNode = 0xc0000003, typeMismatch = 0xc0000004, customDataMismatch = 0xc0000005, missingNode = 0xc0000006, selfCollide = 0xc0000007;
    constexpr int instanceLimitExceeded = 0xf0000001;

    inline bool isChildBranchForcedToGenerate(const StrongholdObservation::ObservedStrongholdTree::Node& node, int branchIndex)
    {
        if(node.determinedType == UNKNOWN || node.determinedType == NONE)return false;
        if(branchIndex < static_cast<int>(StrongholdObservation::optionalChildIndexStart(node.determinedType)))return true;
        return std::isinf(node.branchGenerateWeights[branchIndex]);
    }

    template<nextPieceSelectionMethod method>
    int dfsCheckHelper(const Stronghold<method>& stronghold, const StrongholdObservation& observation, SortedVectorMap<uint64_t, uint32_t>& hashToListIndexMap, uint32_t listIndex)
    {
        const AbstractStrongholdPiece* piece = getBase(stronghold.builtPieces.list[listIndex]);
        uint64_t pieceHash = piece->ancestryHash;
        int observationIndex = observation.branchHashToIndexMap.find(piece->ancestryHash);
        if(observationIndex == -1)return extraNode;

        const StrongholdObservation::ObservedStrongholdTree::Node& observedNode = observation.tree.nodes[observationIndex];
        StructurePieceType determinedType = observedNode.determinedType;
        if(determinedType != UNKNOWN && determinedType != piece->getPieceType())return typeMismatch;
        if(determinedType == LIBRARY && observedNode.customData != (piece->getBoundingBox().getYSize() == 6))return customDataMismatch;

        if(determinedType == UNKNOWN)return verifySuccess;
        for(uint32_t i = 0; i < StrongholdObservation::nonexistantChildIndexStart(determinedType); i++)
        {
            int childListIndex = hashToListIndexMap.find(updateHash(pieceHash, i));
            if(observedNode.ch[i] == branchNotGenerate)
            {
                if(childListIndex != -1)
                    return extraNode;      
            }
            else if(observedNode.ch[i] == unsureIfBranchGenerateWithoutChildObservation)continue;
            else
            {
                const auto& childNode = observation.tree.nodes[observedNode.ch[i]];
                if(childNode.determinedType == NONE)
                {
                    if(childListIndex != -1)
                        return extraNode;
                    continue;
                }

                if(childListIndex == -1)
                {
                    if(isChildBranchForcedToGenerate(observedNode, i) && childNode.roomTypeLikelihoodWeights[NONE] == 0)
                        return missingNode;
                    continue;
                }

                if(childNode.determinedType == UNKNOWN)
                    continue;
                
                int result = dfsCheckHelper(stronghold, observation, hashToListIndexMap, childListIndex);
                if(result != verifySuccess)return result;
            }
        }
        return verifySuccess;
    }

    template<nextPieceSelectionMethod method>
    int verifyStronghold(const Stronghold<method>& stronghold, const StrongholdObservation& observation)
    {
        // check if root exists
        // build hash->index map for stronghold
        // check if each hash from observation is found in stronghold 
        // (use dfs to check if ones decided not to exist was generated too)
        // check if limit for each type was not exceeded
        // check if no pieces collide with each other (except small corridors are allowed to, they can in vanilla)

        uint32_t listSize = stronghold.builtPieces.list.size();
        if(listSize == 0)return noRoot;
        SortedVectorMap<uint64_t, uint32_t> hashToListIndexMap;
        for(uint32_t i = 0; i < listSize; i++)
            hashToListIndexMap.delayedInsert(getBase(stronghold.builtPieces.list[i])->ancestryHash, i);
        hashToListIndexMap.build();

        std::unordered_set<uint64_t> seen;
        for(uint32_t i = 0; i < listSize; i++)
        {
            uint64_t hash = getBase(stronghold.builtPieces.list[i])->ancestryHash;
            if(!seen.insert(hash).second) return duplicateHash;
        }

        int result = dfsCheckHelper(stronghold, observation, hashToListIndexMap, 0);
        if(result != verifySuccess)return result;

        uint32_t instancesSpawned[16] = {0};
        for(uint32_t i = 2; i < listSize; i++) // first 2 pieces don't count towards instances spawned
            instancesSpawned[getBase(stronghold.builtPieces.list[i])->getPieceType()]++;

        for(uint32_t i = 0; i < 16; i++)
            if(PieceWeights::instancesLimits[i] != 0 && instancesSpawned[i] > PieceWeights::instancesLimits[i])
                return instanceLimitExceeded;

        GenerationContext::SoABoundingBox boxes{listSize};
        for(uint32_t i = 0; i < listSize; i++)
        {
            const AbstractStrongholdPiece* piece = getBase(stronghold.builtPieces.list[i]);
            if(piece->getPieceType()== SMALL_CORRIDOR)continue;
            if(find_intersecting(boxes, piece->getBoundingBox()) != -1)return selfCollide;
            boxes.push_back(piece->getBoundingBox());
        }
        
        return verifySuccess;
    }
}