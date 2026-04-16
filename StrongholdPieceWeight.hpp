#pragma once

#include<iostream>
#include<vector>
#include<string>
#include<cstring>
#include<memory>
#include<cmath>
#include<functional>
#include"MutableBoundingBox.hpp"
#include"Directions.hpp"
#include"StructurePieces.hpp"

namespace StrongholdPieceWeight
{
    using namespace StructurePieces;
    struct PieceWeights
    {
        static constexpr StructurePieceType attemptOrder[11] = {
            BRANCHABLE_CORRIDOR, PRISON_CELL, LEFT_TURN, RIGHT_TURN,
            ROOM_CROSSING, STRAIGHT_STAIRS, SPIRAL_STAIRS, 
            FIVE_WAY_CROSSING, CHEST_CORRIDOR, LIBRARY, PORTAL_ROOM
        };

        static constexpr uint32_t inf = 0, undefined = 0;
        static constexpr uint32_t weights[16] = {
            5, undefined, 5, 20,
            20, 10, 10, 20,
            5, 5, 5, 40, 
            undefined, undefined, undefined, undefined
        };

        static constexpr uint32_t instancesLimits[16] = {
            4, undefined, 4, inf, 
            inf, 6, 2, 1,
            5, 5, 5, inf, 
            undefined, undefined, undefined, undefined
        };

        static constexpr uint32_t minDepth[16] = {
            undefined, undefined, undefined, undefined,
            undefined, undefined, 5, 6,
            undefined, undefined, undefined, undefined,
            undefined, undefined, undefined, undefined
        };

        uint32_t instancesSpawned[16];
        inline void clear() { memset(instancesSpawned, 0, sizeof(instancesSpawned)); }
        PieceWeights() { clear(); }

        inline bool instanceLimitNotReached(StructurePieceType type) const { 
            return instancesLimits[type] == 0 || instancesSpawned[type] < instancesLimits[type];
        }
        inline bool roomDepthSatisfied(StructurePieceType type, uint32_t depth) const {
            return minDepth[type] <= depth; 
        }
    };
}
