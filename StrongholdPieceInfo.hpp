#pragma once

#include"AbstractStrongholdPieces.hpp"

namespace StrongholdPieces
{
    struct StrongholdPieceInfo
    {
        bool ch[4] = {false, false, false, false};
        MutableBoundingBox boundingBox;
        StructurePieceType pieceType = NONE;
        AbstractStrongholdPiece::Door door = AbstractStrongholdPiece::OPENING;
        uint32_t customData = 0; // for RoomCrossing.roomType, Library.isSmall
        StrongholdPieceInfo(uint32_t failReason) : customData(failReason) {}
        StrongholdPieceInfo(MutableBoundingBox box, StructurePieceType type, 
            AbstractStrongholdPiece::Door entranceDoor, uint32_t data = 0
        ) : boundingBox(box), pieceType(type), door(entranceDoor), customData(data) {}
        StrongholdPieceInfo(MutableBoundingBox box, StructurePieceType type, 
            AbstractStrongholdPiece::Door entranceDoor, bool b1, bool b2
        ) : ch{b1, b2}, boundingBox(box), pieceType(type), door(entranceDoor) {}
        StrongholdPieceInfo(MutableBoundingBox box, StructurePieceType type, 
            AbstractStrongholdPiece::Door entranceDoor, bool b1, bool b2, bool b3, bool b4
        ) : ch{b1, b2, b3, b4}, boundingBox(box), pieceType(type), door(entranceDoor) {}
    };

    struct BoundingBoxData
    {
        MutableBoundingBox boundingBox;
        uint32_t customData = 0;

        BoundingBoxData() = default;
        BoundingBoxData(MutableBoundingBox box) : boundingBox(box) {}
        BoundingBoxData(MutableBoundingBox box, uint32_t data) : boundingBox(box), customData(data) {}
    };
}