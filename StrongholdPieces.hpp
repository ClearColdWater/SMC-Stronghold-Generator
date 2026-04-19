#pragma once

#include<iostream>
#include<vector>
#include<string>
#include<memory>
#include<cmath>
#include<functional>
#include<optional>
#include<variant>
#include<array>
#include<cassert>
#include<utility>
#include"MutableBoundingBox.hpp"
#include"Directions.hpp"
#include"AbstractStrongholdPieces.hpp"
#include"StrongholdPieceWeight.hpp"
#include"StrongholdObservations.hpp"
#include"StrongholdPieceInfo.hpp"
#include"StrongholdAuxiliaryInfo.hpp"
#include"PendingBitset.hpp"

namespace StrongholdPieces
{
    using namespace StrongholdPieceWeight;
    using namespace StructurePieces;
	using namespace StrongholdObservations;
	using namespace StrongholdAuxiliary;
    using namespace utils;

    struct StrongholdGenerationContext : public GenerationContext
	{
	    PieceWeights availablePieceList;
		uint32_t priorInstancesSpawned[16];
		uint32_t totalWeight;
		bool portalRoomGenerated;
		bool lastCanAddStructurePiecesResult;

		const StrongholdObservation* observation;
		double logImportanceWeight;
		double pendingTypePriorityAuxCarry;

		const StrongholdAuxiliaryInfo* guidingInfo;
		StrongholdAuxiliaryInfo* nextGenInfo; // shared but thread_local

		struct PendingSets
		{
			std::vector<PendingSet> sets; // by observation index
			uint64_t winnerCount[1024];

			inline void clear()
			{
				sets.clear();
				memset(winnerCount, 0, sizeof(winnerCount));
			}

			inline void reserve(uint32_t size){ sets.reserve(size); }
		}pendingSets;

		double omega;
		double remainingLogImportanceDebt;
		
		void prepareStructurePieces()
		{
			nextId = 0;
			availablePieceList.clear();
			pendingSets.clear();
			
			totalWeight = 0;
			forcedNextPieceType = NONE;
			lastPlaced = NONE;
			
			pendingChildren.clear();
			existingBoxes.clear();
			
			portalRoomGenerated = false;
			lastCanAddStructurePiecesResult = true;

			observation = nullptr;
			logImportanceWeight = 0;
			pendingTypePriorityAuxCarry = 0;

			guidingInfo = nullptr;
			nextGenInfo = nullptr;
			omega = 1;
			remainingLogImportanceDebt = 0;

			starterPieceBox = MutableBoundingBox();
		}
		
		inline bool canAddStructurePieces() 
		{
        	bool flag = false;
        	totalWeight = 0;
            for(uint32_t i = 0; i < 11; i++) 
			{
                StructurePieceType type = availablePieceList.attemptOrder[i];
				if(availablePieceList.instanceLimitNotReached(type))
                {
                    if(availablePieceList.instancesLimits[type] > 0)flag = true;
                    totalWeight += availablePieceList.weights[type];
                }
            }
        	return flag;
    	}
    };
    const StructurePiece* findIntersecting(GenerationContext& generationContext, PieceList& existingPieces, MutableBoundingBox box);
    
	
	struct ConcreteStrongholdPieces
	{
		struct ChestCorridor;
		struct SmallCorridor;
		struct FiveWayCrossing;
		struct LeftTurn;
		struct RightTurn;
		struct Library;
		struct PortalRoom;
		struct PrisonCell;
		struct RoomCrossing;
		struct BranchableCorridor;
		struct StraightStairs;
		struct SpiralStairs;
		struct StarterStairs;
		
		static StrongholdPieceInfo getNextPieceInfoVanilla(
			GenerationContext& generationContext, 
			PieceList& existingPieces,
            Xoshiro256pp& rng, int x, int y, int z,
            Direction direction, uint32_t roomDepth
		);

		static StrongholdPieceInfo getNextPieceInfoGuided(
			GenerationContext& generationContext, 
			PieceList& existingPieces,
            Xoshiro256pp& rng, int x, int y, int z,
            Direction direction, uint32_t roomDepth,
			uint64_t hash
		);

		static StrongholdPieceInfo getNextPieceInfoRestricted(
			GenerationContext& generationContext, 
			PieceList& existingPieces,
            Xoshiro256pp& rng, int x, int y, int z,
            Direction direction, uint32_t roomDepth,
			uint64_t hash
		);

		template<nextPieceSelectionMethod method = vanilla>
		static uint32_t generatePieceFromSmallDoor(
			GenerationContext& generationContext, 
			PieceList& existingPieces,
            Xoshiro256pp& rng, int x, int y, int z,
            Direction direction, uint32_t roomDepth,
			uint64_t hash
		);
        
		template<nextPieceSelectionMethod method = vanilla>
		static uint32_t generateAndAddPiece(
			GenerationContext& generationContext, 
			PieceList& existingPieces,
            Xoshiro256pp& rng, int x, int y, int z,
            Direction direction, uint32_t roomDepth,
			uint64_t hash
		);
	};
	
	struct ConcreteStrongholdPieces::SpiralStairs : public AbstractStrongholdPiece
	{
		bool isSource;
		SpiralStairs(uint32_t Id, uint32_t depth, Direction direction, int x, int z) :
			AbstractStrongholdPiece(AbstractStrongholdPiece::Door::OPENING, Id, StructurePieceType::STARTER_STAIRS, depth, MutableBoundingBox(x, 64, z, x + 5 - 1, 74, z + 5 - 1), direction) , isSource(true) {}
		
		SpiralStairs(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::SPIRAL_STAIRS, depth, box, direction) , isSource(false) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		);
		
		static std::optional<SpiralStairs> createPiece(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng, int x, int y, int z, 
			Direction direction, uint32_t roomDepth
		);
	};
	
	struct ConcreteStrongholdPieces::StarterStairs : public ConcreteStrongholdPieces::SpiralStairs
	{
		StarterStairs(uint32_t Id, Direction direction, int x, int z) :
			SpiralStairs(Id, 0, direction, x, z) {}
	};
	
	template<nextPieceSelectionMethod method>
	void ConcreteStrongholdPieces::SpiralStairs::buildComponent(
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng
	){
		if(this->isSource)
        {
            generationContext.starterPieceBox = this->boundingBox;
            generationContext.forcedNextPieceType = FIVE_WAY_CROSSING;
        }
		this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 1, 1, 0);
	}
	
	std::optional<ConcreteStrongholdPieces::SpiralStairs> ConcreteStrongholdPieces::SpiralStairs::createPiece(
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng, int x, int y, int z, 
		Direction direction, uint32_t roomDepth
	){
		MutableBoundingBox box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -7, 0, 5, 11, 5, direction);
		return canStrongholdGoDeeper(box) && findIntersecting(generationContext, existingPieces, box) == nullptr ? std::make_optional(
			ConcreteStrongholdPieces::SpiralStairs(
				AbstractStrongholdPiece::getRandomDoor(rng), generationContext.getNextId(), roomDepth, box, direction)
			) : std::nullopt;
	}
	
	struct ConcreteStrongholdPieces::ChestCorridor : public AbstractStrongholdPiece
	{
		ChestCorridor(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::CHEST_CORRIDOR, depth, box, direction) {}
		
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 1, 1, 0);
		}
	};
	
	struct ConcreteStrongholdPieces::StraightStairs : public AbstractStrongholdPiece
	{
		StraightStairs(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::STRAIGHT_STAIRS, depth, box, direction) {}
		
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 1, 1, 0);
		}
	};
	
	struct ConcreteStrongholdPieces::PrisonCell : public AbstractStrongholdPiece
	{
		PrisonCell(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::PRISON_CELL, depth, box, direction) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 1, 1, 0);
		}
	};
	
	struct ConcreteStrongholdPieces::LeftTurn : public AbstractStrongholdPiece
	{
		LeftTurn(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::LEFT_TURN, depth, box, direction) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			Direction direction = this->getFacingDirection();
			if(direction != NORTH && direction != EAST)
				this->getNextComponentZ<method>(generationContext, existingPieces, rng, 1, 1, 0);
			else
				this->getNextComponentX<method>(generationContext, existingPieces, rng, 1, 1, 0);
		}
	};
	
	struct ConcreteStrongholdPieces::RightTurn : public AbstractStrongholdPiece
	{
		RightTurn(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::RIGHT_TURN, depth, box, direction) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			Direction direction = this->getFacingDirection();
			if(direction != NORTH && direction != EAST)
				this->getNextComponentX<method>(generationContext, existingPieces, rng, 1, 1, 0);
			else
				this->getNextComponentZ<method>(generationContext, existingPieces, rng, 1, 1, 0);
		}
	};
	
	struct ConcreteStrongholdPieces::PortalRoom : public AbstractStrongholdPiece
	{
		PortalRoom(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::PORTAL_ROOM, depth, box, direction) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			[[maybe_unused]] GenerationContext& generationContext, 
			[[maybe_unused]] PieceList& existingPieces, 
			[[maybe_unused]] Xoshiro256pp& rng
		){
			static_cast<StrongholdGenerationContext&>(generationContext).portalRoomGenerated = true;
		}
	};
	
	struct ConcreteStrongholdPieces::Library : public AbstractStrongholdPiece
	{
		Library(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::LIBRARY, depth, box, direction) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			[[maybe_unused]] GenerationContext& generationContext, 
			[[maybe_unused]] PieceList& existingPieces, 
			[[maybe_unused]] Xoshiro256pp& rng
		){}
	};
	
	struct ConcreteStrongholdPieces::SmallCorridor : public AbstractStrongholdPiece
	{
		uint32_t steps;
		SmallCorridor(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::SMALL_CORRIDOR, depth, box, direction), 
			steps((direction != NORTH && direction != SOUTH) ? box.getXSize() : box.getZSize()) {}
			
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			[[maybe_unused]] GenerationContext& generationContext, 
			[[maybe_unused]] PieceList& existingPieces, 
			[[maybe_unused]]Xoshiro256pp& rng
		){}
		
		static std::optional<MutableBoundingBox> findPieceBox(
		    GenerationContext& generationContext, 
			PieceList& existingPieces, 
			int x, int y, int z, Direction direction
		){
			MutableBoundingBox box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0, 5, 5, 4, direction);
			
			const StructurePiece* piece = findIntersecting(generationContext, existingPieces, box);
			if(piece == nullptr || piece->getBoundingBox().minY != box.minY)return std::nullopt;
			
			for(int j = 3; j >= 1; j--)
			{
				box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0, 5, 5, j - 1, direction);
				if(!piece->getBoundingBox().intersectWith(box))
					return MutableBoundingBox(MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0, 5, 5, j, direction));
			}
			return std::nullopt;
		}
	};
	
	struct ConcreteStrongholdPieces::RoomCrossing : public AbstractStrongholdPiece
	{
		uint32_t roomType;
		RoomCrossing(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction, uint32_t type) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::ROOM_CROSSING, depth, box, direction), roomType(type) {}
		
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 4, 1, 0);
			this->getNextComponentX<method>(generationContext, existingPieces, rng, 1, 4, 1);
			this->getNextComponentZ<method>(generationContext, existingPieces, rng, 1, 4, 2);
		}
	};
	
	struct ConcreteStrongholdPieces::BranchableCorridor : public AbstractStrongholdPiece
	{
		bool expandsX, expandsZ;
		BranchableCorridor(AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction, bool expandX, bool expandZ) :
			AbstractStrongholdPiece(door, Id, StructurePieceType::BRANCHABLE_CORRIDOR, depth, box, direction), expandsX(expandX), expandsZ(expandZ) {}
		
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 1, 1, 0);
			if(this->expandsX)this->getNextComponentX<method>(generationContext, existingPieces, rng, 1, 2, 1);
			if(this->expandsZ)this->getNextComponentZ<method>(generationContext, existingPieces, rng, 1, 2, 2);
		}
	};
	
	struct ConcreteStrongholdPieces::FiveWayCrossing : public AbstractStrongholdPiece
	{
		bool confusedLeftLow, confusedLeftHigh, confusedRightLow, confusedRightHigh; // not at all the branches their names suggest
		FiveWayCrossing(
			AbstractStrongholdPiece::Door door, uint32_t Id, uint32_t depth, MutableBoundingBox box, Direction direction, 
			bool leftLow, bool leftHigh, bool rightLow, bool rightHigh
		) : 
			AbstractStrongholdPiece(door, Id, StructurePieceType::FIVE_WAY_CROSSING, depth, box, direction), 
			confusedLeftLow(leftLow), confusedLeftHigh(leftHigh), confusedRightLow(rightLow), confusedRightHigh(rightHigh) {}
		
		template<nextPieceSelectionMethod method = vanilla>
		void buildComponent(
			GenerationContext& generationContext, 
			PieceList& existingPieces, 
			Xoshiro256pp& rng
		){
			int i = 3, j = 5;
			Direction direction = this->getFacingDirection();
			
			if(direction == WEST || direction == NORTH)
			{
				i = 8 - i;
				j = 8 - j;
			}
			
			this->getNextComponentNormal<method>(generationContext, existingPieces, rng, 5, 1, 0);
			if(this->confusedLeftLow)
				this->getNextComponentX<method>(generationContext, existingPieces, rng, i, 1, 1);
			if(this->confusedLeftHigh)
				this->getNextComponentX<method>(generationContext, existingPieces, rng, j, 7, 2);
			if(this->confusedRightLow)
				this->getNextComponentZ<method>(generationContext, existingPieces, rng, i, 1, 3);
			if(this->confusedRightHigh)
				this->getNextComponentZ<method>(generationContext, existingPieces, rng, j, 7, 4);
		}
	};
	
	using StrongholdPieceVariant = std::variant<
		ConcreteStrongholdPieces::ChestCorridor,
		ConcreteStrongholdPieces::SmallCorridor,
		ConcreteStrongholdPieces::FiveWayCrossing,
		ConcreteStrongholdPieces::LeftTurn,
		ConcreteStrongholdPieces::RightTurn,
		ConcreteStrongholdPieces::Library,
		ConcreteStrongholdPieces::PortalRoom,
		ConcreteStrongholdPieces::PrisonCell,
		ConcreteStrongholdPieces::RoomCrossing,
		ConcreteStrongholdPieces::BranchableCorridor,
		ConcreteStrongholdPieces::StraightStairs,
		ConcreteStrongholdPieces::SpiralStairs,
		ConcreteStrongholdPieces::StarterStairs
	>;
	
	struct StrongholdPieceList : public PieceList
	{
	    std::vector<StrongholdPieceVariant> list;
	    StrongholdPieceList() { list.reserve(512); }
	    
	    template<typename T>
        uint32_t add(T&& piece) 
        {
            list.emplace_back(std::forward<T>(piece));
            return static_cast<uint32_t>(list.size() - 1);
        }
    };
	inline AbstractStrongholdPiece* getBase(StrongholdPieceVariant& piece) noexcept
	{
		switch(piece.index())
		{
			case 0: return &std::get<0>(piece);
			case 1: return &std::get<1>(piece);
			case 2: return &std::get<2>(piece);
			case 3: return &std::get<3>(piece);
			case 4: return &std::get<4>(piece);
			case 5: return &std::get<5>(piece);
			case 6: return &std::get<6>(piece);
			case 7: return &std::get<7>(piece);
			case 8: return &std::get<8>(piece);
			case 9: return &std::get<9>(piece);
			case 10:return &std::get<10>(piece);
			case 11:return &std::get<11>(piece);
			case 12:return &std::get<12>(piece);
			default:std::unreachable();
		}
	}

	inline const AbstractStrongholdPiece* getBase(const StrongholdPieceVariant& piece) noexcept
	{
		switch(piece.index())
		{
			case 0: return &std::get<0>(piece);
			case 1: return &std::get<1>(piece);
			case 2: return &std::get<2>(piece);
			case 3: return &std::get<3>(piece);
			case 4: return &std::get<4>(piece);
			case 5: return &std::get<5>(piece);
			case 6: return &std::get<6>(piece);
			case 7: return &std::get<7>(piece);
			case 8: return &std::get<8>(piece);
			case 9: return &std::get<9>(piece);
			case 10:return &std::get<10>(piece);
			case 11:return &std::get<11>(piece);
			case 12:return &std::get<12>(piece);
			default:std::unreachable();
		}
	}
	
	inline const StructurePiece* findIntersecting(GenerationContext& generationContext, PieceList& existingPieces, MutableBoundingBox box)
	{
	    int index = find_intersecting(generationContext.existingBoxes, box);
	    if(index == -1)return nullptr;
	    
	    auto& existingConcreteStrongholdPieces = static_cast<StrongholdPieceList&>(existingPieces);
	    return getBase(existingConcreteStrongholdPieces.list[index]);
	}
	
	
	template<nextPieceSelectionMethod method = vanilla>
	inline void dispatchBuildComponent(
        StrongholdPieceVariant& variantPiece,
        GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng
    ){
        std::visit([&](auto& piece) {
            piece.template buildComponent<method>(generationContext, existingPieces, rng);
        }, variantPiece);
    }

	constexpr int getPrecheckIndex(StructurePieceType type) 
	{
		switch(type) 
		{
			case LEFT_TURN: case RIGHT_TURN: return 0;
			case CHEST_CORRIDOR: case BRANCHABLE_CORRIDOR: case FIVE_WAY_CROSSING: case PRISON_CELL: case LIBRARY: return 1;
			case SPIRAL_STAIRS: case STRAIGHT_STAIRS: return 2;
			case ROOM_CROSSING: case PORTAL_ROOM: return 3;
			default: return -1;
		}
	}

	constexpr bool requiresFurtherCheck(StructurePieceType type)
	{
		switch(type) 
		{
			case FIVE_WAY_CROSSING: case PORTAL_ROOM: case PRISON_CELL: case STRAIGHT_STAIRS: case LIBRARY: return true;
			default: return false;
		}
	}
	// leaves the final check to the caller
	static inline BoundingBoxData getBoundingBoxFromType(
		StructurePieceType type,
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		int x, int y, int z, Direction direction
	){
		switch(type)
		{
			case CHEST_CORRIDOR:  	  return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  7, direction);
			case STARTER_STAIRS:
			case SPIRAL_STAIRS:       return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -7, 0,  5, 11,  5, direction);
			case STRAIGHT_STAIRS: 	  return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -7, 0,  5, 11,  8, direction);
			case PRISON_CELL:     	  return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  9,  5, 11, direction);
			case LEFT_TURN:       	  return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  5, direction);
			case RIGHT_TURN:      	  return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  5, direction);
			case PORTAL_ROOM:         return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 11,  8, 16, direction);
			case ROOM_CROSSING:       return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 11,  7, 11, direction);
			case BRANCHABLE_CORRIDOR: return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  7, direction);
			case FIVE_WAY_CROSSING:   return MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -3, 0, 10,  9, 11, direction);
			case LIBRARY:
			{
				bool isSmallLibrary = false;
				MutableBoundingBox box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 14, 11, 15, direction);
				if(!AbstractStrongholdPiece::canStrongholdGoDeeper(box) || findIntersecting(generationContext, existingPieces, box) != nullptr)
				{
					isSmallLibrary = true;
					box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 14, 6, 15, direction);
				}
				return BoundingBoxData(box, isSmallLibrary);
			}
			default: break;
		}
		return BoundingBoxData();
	}

	template<bool restricted = false>
	static inline bool decideIfBranchGenerate(int ch, double prior, double likelihoodMass, Xoshiro256pp& rng)
	{
		if(ch == -1)return false;
		if(std::isinf(likelihoodMass))return true;
		if constexpr(restricted)return false;
		double posteriorMass = prior * likelihoodMass, totalMass = posteriorMass + (1 - prior) /* * 1 (likelihood of no branch is defined as 1)*/;
		return rng.nextDouble() * totalMass < posteriorMass;
	}

	template<bool restricted = false>
	static inline StrongholdPieceInfo generatePieceInfoData(
		StructurePieceType type, MutableBoundingBox box, 
		Xoshiro256pp& rng, int observationIndex, 
		const StrongholdObservation* obs, uint32_t customData = 0
	){
		if(type == PORTAL_ROOM)
			return StrongholdPieceInfo(box, type, AbstractStrongholdPiece::Door::OPENING);
		auto door = AbstractStrongholdPiece::getRandomDoor(rng);
		switch(type) 
		{
			case BRANCHABLE_CORRIDOR: 
			{
				bool b1, b2;
				if(observationIndex != -1) 
				{
					b1 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[1], 1.0 / 2.0, obs->tree.nodes[observationIndex].branchGenerateWeights[1], rng);
					b2 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[2], 1.0 / 2.0, obs->tree.nodes[observationIndex].branchGenerateWeights[2], rng);
				}
				else 
				{
					b1 = rng.nextBool();
					b2 = rng.nextBool();
				}
				return StrongholdPieceInfo(box, type, door, b1, b2);
			}
			case FIVE_WAY_CROSSING: 
			{
				bool b1, b2, b3, b4;
				if(observationIndex != -1) 
				{
					b1 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[1], 1.0 / 2.0, obs->tree.nodes[observationIndex].branchGenerateWeights[1], rng);
					b2 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[2], 1.0 / 2.0, obs->tree.nodes[observationIndex].branchGenerateWeights[2], rng);
					b3 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[3], 1.0 / 2.0, obs->tree.nodes[observationIndex].branchGenerateWeights[3], rng);
					b4 = decideIfBranchGenerate<restricted>(obs->tree.nodes[observationIndex].ch[4], 2.0 / 3.0, obs->tree.nodes[observationIndex].branchGenerateWeights[4], rng);
				} 
				else 
				{
					b1 = rng.nextBool();
					b2 = rng.nextBool();
					b3 = rng.nextBool();
					b4 = rng.nextInt(3) > 0;
				}
				return StrongholdPieceInfo(box, type, door, b1, b2, b3, b4);
			}
			case ROOM_CROSSING:
				return StrongholdPieceInfo(box, type, door, rng.nextInt(5));
			case LIBRARY:
				return StrongholdPieceInfo(box, type, door, customData);
			default:
				return StrongholdPieceInfo(box, type, door);
		}
	}

	StrongholdPieceInfo ConcreteStrongholdPieces::getNextPieceInfoVanilla(
		GenerationContext& generationContext, 
		PieceList& existingPieces,
		Xoshiro256pp& rng, int x, int y, int z,
		Direction direction, uint32_t roomDepth
	){
		auto& context = static_cast<StrongholdGenerationContext&>(generationContext);
		if(!context.canAddStructurePieces()) return 0xffffffff;
		if(generationContext.forcedNextPieceType != NONE)
    	{
			StructurePieceType type = generationContext.forcedNextPieceType;
			generationContext.forcedNextPieceType = NONE;
			BoundingBoxData data = getBoundingBoxFromType(
				type, 
				generationContext, 
				existingPieces, 
				x, y, z, direction
			);
			MutableBoundingBox& box = data.boundingBox;
			if(!AbstractStrongholdPiece::canStrongholdGoDeeper(box) || 
				findIntersecting(generationContext, existingPieces, box) != nullptr)
				    return 0xffffffff;
			
			return generatePieceInfoData(type, box, rng, -1, nullptr, data.customData);
        }
        
		auto& list = context.availablePieceList;
        for(int attempts = 0; attempts < 5; attempts++)
        {
            int r = rng.nextInt(context.totalWeight);
            for(uint32_t i = 0; i < 11; i++)
            {
				StructurePieceType currentType = list.attemptOrder[i];
				if(!list.instanceLimitNotReached(currentType))continue;

                r -= list.weights[currentType];
                if(r < 0)
                {
                    if(!list.roomDepthSatisfied(currentType, roomDepth) || currentType == context.lastPlaced) break;
                    
					BoundingBoxData data = getBoundingBoxFromType(
						currentType, 
						generationContext, 
						existingPieces, 
						x, y, z, direction
					);
					MutableBoundingBox& box = data.boundingBox;

					// yes continue vanilla doesn't break here
					if(!AbstractStrongholdPiece::canStrongholdGoDeeper(box) || 
						findIntersecting(generationContext, existingPieces, box) != nullptr)continue; 

					list.instancesSpawned[currentType]++;
                    context.lastPlaced = currentType;

					return generatePieceInfoData(currentType, box, rng, -1, nullptr, data.customData);
                }
            }
        }
        
        auto boxOpt = SmallCorridor::findPieceBox(generationContext, existingPieces, x, y, z, direction);
        if(boxOpt && boxOpt->minY > 1)
            return StrongholdPieceInfo(*boxOpt, SMALL_CORRIDOR, AbstractStrongholdPiece::Door::OPENING);

    	return 0xffffffff;
	}

	StrongholdPieceInfo ConcreteStrongholdPieces::getNextPieceInfoGuided(
		GenerationContext& generationContext, 
		PieceList& existingPieces,
		Xoshiro256pp& rng, int x, int y, int z,
		Direction direction, uint32_t roomDepth,
		uint64_t hash
	){
		auto& context = static_cast<StrongholdGenerationContext&>(generationContext);
		int observationIndex = context.observation != nullptr ? 
			context.observation->branchHashToIndexMap.find(hash) : -1;

		if(observationIndex != -1)
		{
			uint32_t observedBoxIndex = context.observation->nodeIndexToBoxIndex[observationIndex];
			if(observedBoxIndex != 0xffffffff) // UNKNOWN & NONE
				context.observedBoxes.remove(observedBoxIndex);
		}

    	if(generationContext.forcedNextPieceType != NONE)
    	{
			StructurePieceType type = generationContext.forcedNextPieceType;
			context.forcedNextPieceType = NONE;
			BoundingBoxData data = getBoundingBoxFromType(
				type, 
				generationContext, 
				existingPieces, 
				x, y, z, direction
			);
			MutableBoundingBox& box = data.boundingBox;
			if(!AbstractStrongholdPiece::canStrongholdGoDeeper(box) || 
				findIntersecting(generationContext, existingPieces, box) != nullptr)
					return 0xffffffff;

			return generatePieceInfoData(type, box, rng, observationIndex, context.observation, data.customData);
        }

		if(observationIndex != -1)
			context.priorInstancesSpawned[context.observation->tree.nodes[observationIndex].determinedType]--; // doesn't matter if it is UNKNOWN or NONE

		// -----------------------------------------------
		// check piece validity & calculate bounding boxes
		// -----------------------------------------------
        
        MutableBoundingBox preCheckBoxes[4] = {
            MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  5, direction), // = Left/Right Turn, nothing is smaller
            MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -1, 0,  5,  5,  7, direction), // = Chest/Branchable Corridor
            MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -1, -7, 0,  5, 11,  5, direction), // = SpiralStairs, Smaller than StraightStairs
            MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 11,  7, 11, direction)  // = RoomCrossing, Smaller than Portal Room
        };
        
        bool preCheckResult[4];
        preCheckResult[0] = !AbstractStrongholdPiece::canStrongholdGoDeeper(preCheckBoxes[0]) || (findIntersecting(generationContext, existingPieces, preCheckBoxes[0]) != nullptr);
        preCheckResult[1] = preCheckResult[0] || (findIntersecting(generationContext, existingPieces, preCheckBoxes[1]) != nullptr);
        preCheckResult[2] = preCheckResult[0] || !AbstractStrongholdPiece::canStrongholdGoDeeper(preCheckBoxes[2]) || (findIntersecting(generationContext, existingPieces, preCheckBoxes[2]) != nullptr);
        preCheckResult[3] = preCheckResult[1] || (findIntersecting(generationContext, existingPieces, preCheckBoxes[3]) != nullptr);
        
        bool isContextValid[16] = {false};
        bool isBoxInvalid[16] = {false};
		uint32_t isFutureCollisionValid[16] = {0};
        MutableBoundingBox expectedBoxSize[16];
		uint32_t customData[16]; // actually only for libraries
        
		auto& list = context.availablePieceList;
		bool canAddStructurePieces = context.canAddStructurePieces();
		context.lastCanAddStructurePiecesResult = canAddStructurePieces;

		if(canAddStructurePieces)
		{
			for(uint32_t i = 0; i < 11; i++)
			{
				StructurePieceType currentType = list.attemptOrder[i];
				if(!list.instanceLimitNotReached(currentType))continue;

				isContextValid[currentType] = list.roomDepthSatisfied(currentType, roomDepth) && currentType != context.lastPlaced;
				if(!isContextValid[currentType]) continue; 
				
				int precheckIndex = getPrecheckIndex(currentType);
				if(preCheckResult[precheckIndex])
				{
					isBoxInvalid[currentType] = true;
					continue;
				}

				BoundingBoxData data = getBoundingBoxFromType(
					currentType, 
					generationContext, 
					existingPieces, 
					x, y, z, direction
				);
				expectedBoxSize[currentType] = data.boundingBox;
				customData[currentType] = data.customData;

				if(requiresFurtherCheck(currentType))
					isBoxInvalid[currentType] = 
						!AbstractStrongholdPiece::canStrongholdGoDeeper(expectedBoxSize[currentType]) || 
							findIntersecting(generationContext, existingPieces, expectedBoxSize[currentType]) != nullptr;
				else
					isBoxInvalid[currentType] = false;

				isFutureCollisionValid[currentType] = find_intersecting(generationContext.observedBoxes, expectedBoxSize[currentType]) == -1;
				isFutureCollisionValid[currentType] *= list.instancesLimits[currentType] == 0 ? 1 :
					list.instancesLimits[currentType] >= list.instancesSpawned[currentType] + context.priorInstancesSpawned[currentType] + 1;
				if(observationIndex != -1 && context.observation->tree.nodes[observationIndex].determinedType == currentType && currentType == LIBRARY)
					isFutureCollisionValid[currentType] *= (data.customData == context.observation->tree.nodes[observationIndex].customData);
			}
		}
        
		// ---------------------------------
		// calculate posterior probabilities
		// ---------------------------------

        int nextLandingType = -1;
        uint32_t landingWeight[16] = {0}, failWeight = 0;
        
		if(canAddStructurePieces)
		{
			for(int i = 10; i >= 0; i--)
			{
				StructurePieceType currentType = list.attemptOrder[i];
				if(!list.instanceLimitNotReached(currentType))continue;

				if(isContextValid[currentType])
				{
					if(isBoxInvalid[currentType])
					{
						if(nextLandingType != -1)
							landingWeight[nextLandingType] += list.weights[currentType];
						else 
							failWeight += list.weights[currentType];
					}
					else landingWeight[nextLandingType = currentType] += list.weights[currentType];
				}
				else 
				{
					nextLandingType = -1;
					failWeight += list.weights[currentType];
				}
			}
		}
        
        double pChosen[16] = {0}, pFailOnce = static_cast<double>(failWeight) / context.totalWeight;
        double pFail = pFailOnce * pFailOnce * pFailOnce * pFailOnce * pFailOnce;
        double fac = 1.0 + pFailOnce * (1.0 + pFailOnce * (1.0 + pFailOnce * (1.0 + pFailOnce)));
        
		if(canAddStructurePieces)
		{
			for(uint32_t currentType = 0; currentType < 16; currentType++) // for SIMD optimization
				pChosen[currentType] = fac * static_cast<double>(landingWeight[currentType]) / context.totalWeight;
		}
		
        auto smallCorridorBox = SmallCorridor::findPieceBox(generationContext, existingPieces, x, y, z, direction);
		isFutureCollisionValid[NONE] = 1;
		isFutureCollisionValid[SMALL_CORRIDOR] = 1; 

		// need to implement biased proposal for small corridor
		if(!canAddStructurePieces)pChosen[NONE] = 1;
        else if(smallCorridorBox && smallCorridorBox->minY > 1)
		{
			// rejection of incompatible small corridor length results in drastic drop in N
			uint32_t smallCorridorLength = (direction != NORTH && direction != SOUTH) ? smallCorridorBox->getXSize() : smallCorridorBox->getZSize();
			if(observationIndex != -1 && context.observation->tree.nodes[observationIndex].determinedType == SMALL_CORRIDOR)
				isFutureCollisionValid[SMALL_CORRIDOR] = (smallCorridorLength == context.observation->tree.nodes[observationIndex].customData);
			/**/
			
			pChosen[SMALL_CORRIDOR] = pFail;
			pChosen[NONE] = 0; // for importance factor calculations
			// isFutureCollisionValid[SMALL_CORRIDOR] = find_intersecting(generationContext.observedBoxes, expectedBoxSize[SMALL_CORRIDOR]) == -1; 
			// (the small corridor can in fact collide with future pieces and still be valid due to mojang's buggy code, this should never be enabled)
		}
		else
		{
			pChosen[SMALL_CORRIDOR] = 0;
			pChosen[NONE] = pFail;
		}

		double importanceWeightFactor = 0;
		for(uint32_t currentType = 0; currentType < 16; currentType++)
		{
			pChosen[currentType] *= isFutureCollisionValid[currentType];
			if(observationIndex != -1)
				pChosen[currentType] *= context.observation->tree.nodes[observationIndex].roomTypeLikelihoodWeights[currentType];
			importanceWeightFactor += pChosen[currentType];
		}
		double logWeightFactor = std::log(importanceWeightFactor);
		context.logImportanceWeight += logWeightFactor;

		if(observationIndex != -1)
		{	
			if(context.guidingInfo != nullptr)
				context.remainingLogImportanceDebt -= 
					context.guidingInfo->nodeLogMeanImportanceWeightDelta[observationIndex];
			
			if(context.nextGenInfo != nullptr)
			{
				double logOmega = std::log(context.omega);
				context.nextGenInfo->nodeLogMeanImportanceWeightDelta[observationIndex] = 
					StrongholdAuxiliaryInfo::LogSumExp(context.nextGenInfo->nodeLogMeanImportanceWeightDelta[observationIndex], logOmega + logWeightFactor);
				context.nextGenInfo->nodeLogMeanImportanceWeightDeltaSq[observationIndex] =
					StrongholdAuxiliaryInfo::LogSumExp(context.nextGenInfo->nodeLogMeanImportanceWeightDeltaSq[observationIndex], logOmega + 2.0 * logWeightFactor);
				context.nextGenInfo->nodeTotalOmega[observationIndex] += context.omega;
			}
		}
			
		// need to implement forward rejection for collision later (鈭?
		// also forward rejection for lastPlaced (鈭?(kinda...))
		// placed after observationIndex != -1 because they need to be updated even if importanceWeightFactor == 0
		if(importanceWeightFactor == 0)
		{
			context.logImportanceWeight = -1e9;
			return 0xffffffff;
		}

		for(uint32_t currentType = 0; currentType < 16; currentType++)
			pChosen[currentType] /= importanceWeightFactor;
        
		// ------------------
		// generate the piece
		// ------------------

        double r = rng.nextDouble();
		for(uint32_t i = 0; i < 11; ++i) 
		{
			StructurePieceType currentType = context.availablePieceList.attemptOrder[i];
			r -= pChosen[currentType]; // constraints contained in pChosen
			if(r <= 0) 
			{
				context.availablePieceList.instancesSpawned[currentType]++;
				context.lastPlaced = currentType;
				return generatePieceInfoData(currentType, expectedBoxSize[currentType], rng, observationIndex, context.observation, customData[currentType]);
			}
		}
		
		if(pChosen[SMALL_CORRIDOR] > 0)
			return StrongholdPieceInfo(*smallCorridorBox, SMALL_CORRIDOR, AbstractStrongholdPiece::Door::OPENING);
		
		return 0xffffffff;
	}

	StrongholdPieceInfo ConcreteStrongholdPieces::getNextPieceInfoRestricted(
		GenerationContext& generationContext, 
		PieceList& existingPieces,
		Xoshiro256pp& rng, int x, int y, int z,
		Direction direction, [[maybe_unused]] uint32_t roomDepth,
		uint64_t hash
	){
		auto& context = static_cast<StrongholdGenerationContext&>(generationContext);
		int observationIndex = context.observation != nullptr ? 
			context.observation->branchHashToIndexMap.find(hash) : -1;
		// doesn't need to check for canAddStructurePieces

		if(observationIndex == -1)return 0xffffffff;
		StructurePieceType determinedType = context.observation->tree.nodes[observationIndex].determinedType;

		MutableBoundingBox box;
		switch(determinedType)
		{
			case UNKNOWN: return 0xffffffff;
			case NONE: return 0xffffffff;
			// small corridors are always in the middle of two pieces
			// and can never cause collision check failures by itself.
			case SMALL_CORRIDOR: return 0xffffffff; 
			case LIBRARY:
			{
				if(!context.observation->tree.nodes[observationIndex].customData)
					box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 14, 11, 15, direction);
				else
					box = MutableBoundingBox::getComponentToAddBoundingBox(x, y, z, -4, -1, 0, 14, 6, 15, direction);
				break;
			}
			default:
			{
				BoundingBoxData data = getBoundingBoxFromType(
					determinedType, 
					generationContext, 
					existingPieces, 
					x, y, z, direction
				);
				box = data.boundingBox;
			}
		}

		// precheck for invalid observations to collision and only collision
		if(!AbstractStrongholdPiece::canStrongholdGoDeeper(box) || 
			findIntersecting(generationContext, existingPieces, box) != nullptr
		){
			context.logImportanceWeight = -1e9;
			return 0xffffffff;
		}
		
		// customData is useless here i think
		return generatePieceInfoData<true>(determinedType, box, rng, observationIndex, context.observation); 
	}

	template<nextPieceSelectionMethod method>
	uint32_t ConcreteStrongholdPieces::generatePieceFromSmallDoor(
		GenerationContext& generationContext, 
		PieceList& existingPieces,
        Xoshiro256pp& rng, int x, int y, int z,
        Direction direction, uint32_t roomDepth,
		uint64_t hash
	){
	    auto& context = static_cast<StrongholdGenerationContext&>(generationContext);
    	//if(!context.canAddStructurePieces()) return 0xffffffff;

		StrongholdPieceInfo info(0);
		if constexpr(method == vanilla)info = getNextPieceInfoVanilla(
			generationContext, existingPieces, rng, x, y, z, direction, roomDepth
		);
		else if constexpr(method == observationGuided)info = getNextPieceInfoGuided(
			generationContext, existingPieces, rng, x, y, z, direction, roomDepth, hash
		);
		else info = getNextPieceInfoRestricted(
			generationContext, existingPieces, rng, x, y, z, direction, roomDepth, hash
		);
		
		if(info.customData != 0xffffffff)
		{
			auto& list = static_cast<StrongholdPieceList&>(existingPieces);
            uint32_t id = context.getNextId();
			switch(info.pieceType)
			{
				case CHEST_CORRIDOR:      return list.add(ConcreteStrongholdPieces::     ChestCorridor(info.door, id, roomDepth, info.boundingBox, direction));
                case SMALL_CORRIDOR:      return list.add(ConcreteStrongholdPieces::     SmallCorridor(info.door, id, roomDepth, info.boundingBox, direction));
                case FIVE_WAY_CROSSING:   return list.add(ConcreteStrongholdPieces::   FiveWayCrossing(info.door, id, roomDepth, info.boundingBox, direction, info.ch[0], info.ch[1], info.ch[2], info.ch[3]));
                case LEFT_TURN:           return list.add(ConcreteStrongholdPieces::          LeftTurn(info.door, id, roomDepth, info.boundingBox, direction));
                case RIGHT_TURN:          return list.add(ConcreteStrongholdPieces::         RightTurn(info.door, id, roomDepth, info.boundingBox, direction));
                case LIBRARY:             return list.add(ConcreteStrongholdPieces::           Library(info.door, id, roomDepth, info.boundingBox, direction));
                case PORTAL_ROOM:         return list.add(ConcreteStrongholdPieces::        PortalRoom(info.door, id, roomDepth, info.boundingBox, direction));
                case PRISON_CELL:         return list.add(ConcreteStrongholdPieces::        PrisonCell(info.door, id, roomDepth, info.boundingBox, direction));
                case ROOM_CROSSING:       return list.add(ConcreteStrongholdPieces::      RoomCrossing(info.door, id, roomDepth, info.boundingBox, direction, info.customData));
                case BRANCHABLE_CORRIDOR: return list.add(ConcreteStrongholdPieces::BranchableCorridor(info.door, id, roomDepth, info.boundingBox, direction, info.ch[0], info.ch[1]));
                case STRAIGHT_STAIRS:     return list.add(ConcreteStrongholdPieces::    StraightStairs(info.door, id, roomDepth, info.boundingBox, direction));
                case SPIRAL_STAIRS:       return list.add(ConcreteStrongholdPieces::      SpiralStairs(info.door, id, roomDepth, info.boundingBox, direction));
                default: return 0xffffffff;
			}
		}
		return 0xffffffff;
	}
	
	template<nextPieceSelectionMethod method>
	uint32_t ConcreteStrongholdPieces::generateAndAddPiece(
		GenerationContext& generationContext, 
		PieceList& existingPieces,
        Xoshiro256pp& rng, int x, int y, int z,
        Direction direction, uint32_t roomDepth,
		uint64_t hash
	){
    	if(roomDepth > 50)
    		return 0xffffffff;
    	
    	if(std::abs(x - generationContext.starterPieceBox.minX) > 112 || 
		   std::abs(z - generationContext.starterPieceBox.minZ) > 112)
		   return 0xffffffff;
		   
		uint32_t pieceId = generatePieceFromSmallDoor<method>(generationContext, existingPieces, rng, x, y, z, direction, roomDepth + 1, hash);
		if(pieceId == 0xffffffff)return 0xffffffff;
		
		generationContext.pendingChildren.push_back(pieceId);
		generationContext.existingBoxes.push_back(getBase(static_cast<StrongholdPieceList&>(existingPieces).list[pieceId])->getBoundingBox());
		getBase(static_cast<StrongholdPieceList&>(existingPieces).list[pieceId])->ancestryHash = hash;
		return pieceId;
	}
	
	template<nextPieceSelectionMethod method>
	inline uint32_t AbstractStrongholdPiece::getNextComponentNormal(
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng, int offsetX, int offsetY,
		uint32_t whichChild
	){
        Direction direction = this->getFacingDirection();
        uint32_t newPiece = 0xffffffff;
		uint64_t nextHash = updateHash(this->ancestryHash, whichChild);

        switch (direction) {
            case NORTH:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces, 
						rng, this->boundingBox.minX + offsetX, this->boundingBox.minY + offsetY, this->boundingBox.minZ - 1, 
						direction, this->getRoomDepth(), nextHash);
                break;
            case SOUTH:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces,
                        rng, this->boundingBox.minX + offsetX, this->boundingBox.minY + offsetY, this->boundingBox.maxZ + 1, 
						direction, this->getRoomDepth(), nextHash);
                break;
            case WEST:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces,
                        rng, this->boundingBox.minX - 1, this->boundingBox.minY + offsetY, this->boundingBox.minZ + offsetX, 
						direction, this->getRoomDepth(), nextHash);
                break;
            case EAST:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces,
                        rng, this->boundingBox.maxX + 1, this->boundingBox.minY + offsetY, this->boundingBox.minZ + offsetX, 
						direction, this->getRoomDepth(), nextHash);
                break;
        }

        return newPiece;
    }
    
	template<nextPieceSelectionMethod method>
    inline uint32_t AbstractStrongholdPiece::getNextComponentX(
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng, int offsetY, int offsetZ,
		uint32_t whichChild
	){
        Direction direction = this->getFacingDirection();
        uint32_t newPiece = 0xffffffff;
		uint64_t nextHash = updateHash(this->ancestryHash, whichChild);

        switch (direction) 
		{
            case NORTH:
            case SOUTH:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces, 
						rng, this->boundingBox.minX - 1, this->boundingBox.minY + offsetY, this->boundingBox.minZ + offsetZ, 
						WEST, this->getRoomDepth(), nextHash);
                break;
            case WEST:
            case EAST:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces, 
                        rng, this->boundingBox.minX + offsetZ, this->boundingBox.minY + offsetY, this->boundingBox.minZ - 1, 
						NORTH, this->getRoomDepth(), nextHash);
                break;
        }

        return newPiece;
    }
    
	template<nextPieceSelectionMethod method>
    inline uint32_t AbstractStrongholdPiece::getNextComponentZ(
		GenerationContext& generationContext, 
		PieceList& existingPieces, 
		Xoshiro256pp& rng, int offsetY, int offsetX,
		uint32_t whichChild
	){
		Direction direction = this->getFacingDirection();
        uint32_t newPiece = 0xffffffff;
		uint64_t nextHash = updateHash(this->ancestryHash, whichChild);
        
        switch (direction) 
		{
            case NORTH:
            case SOUTH:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces, 
                        rng, this->boundingBox.maxX + 1, this->boundingBox.minY + offsetY, this->boundingBox.minZ + offsetX, 
						EAST, this->getRoomDepth(), nextHash);
                break;
            case WEST:
            case EAST:
                newPiece = ConcreteStrongholdPieces::generateAndAddPiece<method>(generationContext, existingPieces, 
                        rng, this->boundingBox.minX + offsetX, this->boundingBox.minY + offsetY, this->boundingBox.maxZ + 1, 
						SOUTH, this->getRoomDepth(), nextHash);
                break;
        }
        
        return newPiece;
	}
}
