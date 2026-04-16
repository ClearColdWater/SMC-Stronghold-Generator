#pragma once

#include<iostream>
#include<vector>
#include<string>
#include<memory>
#include<cmath>
#include<functional>
#include"MutableBoundingBox.hpp"
#include"Directions.hpp"
#include"StructurePieces.hpp"

namespace StrongholdPieces
{
    using namespace StructurePieces;
	enum nextPieceSelectionMethod { vanilla = 0, observationGuided = 1, constraintRestricted = 2};
    struct AbstractStrongholdPiece : public StructurePiece
	{
		enum Door {OPENING = 0, WOOD_DOOR = 1, GRATES = 2, IRON_DOOR = 3};
		static constexpr const char* DoorNames[] = {"OPENING", "WOOD_DOOR", "GRATES", "IRON_DOOR"};
		// static const StructurePiece* findIntersecting(PieceList& existingPieces, MutableBoundingBox box);
		// virtual ~AbstractStrongholdPiece() = default;
		uint64_t ancestryHash;
		
	protected:
		AbstractStrongholdPiece(Door door, uint32_t Id, StructurePieceType type, uint32_t depth, MutableBoundingBox box, Direction direction) : 
			StructurePiece(Id, type, depth, box, direction), entryDoor(door) {}
			
		Door entryDoor = Door::OPENING;
		
		template<nextPieceSelectionMethod method = vanilla>
		uint32_t getNextComponentNormal(
			GenerationContext& context, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng, int offsetX, int offsetY,
			uint32_t whichChild
		);

		template<nextPieceSelectionMethod method = vanilla>
		uint32_t getNextComponentX(
			GenerationContext& context, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng, int offsetY, int offsetZ,
			uint32_t whichChild
		);

		template<nextPieceSelectionMethod method = vanilla>
		uint32_t getNextComponentZ(
			GenerationContext& context, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng, int offsetY, int offsetX,
			uint32_t whichChild
		);
	public:
	    static inline Door getRandomDoor(Xoshiro256pp& rng)
		{
			int i = rng.nextInt(5);
			switch(i)
			{
				case 0:
				case 1:
				default:
					return Door::OPENING;
				case 2:
                   	return Door::WOOD_DOOR;
	            case 3:
	                return Door::GRATES;
	            case 4:
	                return Door::IRON_DOOR;
			}
		}
		static bool canStrongholdGoDeeper(MutableBoundingBox box) {
        	return box.minY > 10;
    	}
	};
}
