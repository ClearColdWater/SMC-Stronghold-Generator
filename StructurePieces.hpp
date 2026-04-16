#pragma once

#include<iostream>
#include<vector>
#include<string>
#include<memory>
#include<cmath>
#include<functional>
#include"MutableBoundingBox.hpp"
#include"Directions.hpp"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#endif

namespace StructurePieces
{
    using namespace utils;
    enum StructurePieceType {
        CHEST_CORRIDOR = 0, SMALL_CORRIDOR = 1, FIVE_WAY_CROSSING = 2, LEFT_TURN = 3, 
        RIGHT_TURN = 4, ROOM_CROSSING = 5, LIBRARY = 6, PORTAL_ROOM = 7, 
        PRISON_CELL = 8, SPIRAL_STAIRS = 9, STRAIGHT_STAIRS = 10, BRANCHABLE_CORRIDOR = 11, 
        STARTER_STAIRS = 12, NONE = 13, UNKNOWN = 14
    };
    constexpr const char* StructurePieceTypeNames[] = {"CHEST_CORRIDOR", "SMALL_CORRIDOR", "FIVE_WAY_CROSSING", "LEFT_TURN", "RIGHT_TURN", "ROOM_CROSSING", "LIBRARY", "PORTAL_ROOM", "PRISON_CELL", "SPIRAL_STAIRS", "STRAIGHT_STAIRS", "BRANCHABLE_CORRIDOR", "STARTER_STAIRS", "NONE", "UNKNOWN"};

    struct GenerationContext 
    {
        uint32_t nextId;
	    inline uint32_t getNextId() { return nextId++; }
	    
	    StructurePieceType forcedNextPieceType = NONE;
	    StructurePieceType lastPlaced = NONE;
	    
	    std::vector<uint32_t> pendingChildren; // index to existingPieces
	    MutableBoundingBox starterPieceBox;
	    
	    struct SoABoundingBox
	    {
	        std::vector<int> minX, minY, minZ;
	        std::vector<int> maxX, maxY, maxZ;
	        uint32_t realSize = 0;
	        
            SoABoundingBox() = default;
	        SoABoundingBox(uint32_t size){ reserve(size); }

            void reserve(uint32_t size)
            {
                minX.reserve(size); minY.reserve(size); minZ.reserve(size);
	            maxX.reserve(size); maxY.reserve(size); maxZ.reserve(size);
            }
            
            void clear()
            {
                minX.clear(); minY.clear(); minZ.clear(); 
                maxX.clear(); maxY.clear(); maxZ.clear();
                realSize = 0;
            }
	        void push_back(MutableBoundingBox box)
	        {
	            if(realSize < minX.size())
                {
                    minX[realSize] = box.minX; minY[realSize] = box.minY; minZ[realSize] = box.minZ;
                    maxX[realSize] = box.maxX; maxY[realSize] = box.maxY; maxZ[realSize] = box.maxZ;
                } 
                else 
                {
                    minX.push_back(box.minX); minY.push_back(box.minY); minZ.push_back(box.minZ);
                    maxX.push_back(box.maxX); maxY.push_back(box.maxY); maxZ.push_back(box.maxZ);
                    
                    while (minX.size() % 8 != 0) 
                    {
                        minX.push_back(INT_MAX); minY.push_back(INT_MAX); minZ.push_back(INT_MAX);
                        maxX.push_back(INT_MIN); maxY.push_back(INT_MIN); maxZ.push_back(INT_MIN);
                    }
                }
                realSize++;
            }
            void remove(uint32_t index)
            {
                minX[index] = minY[index] = minZ[index] = INT_MAX;
                maxX[index] = maxY[index] = maxZ[index] = INT_MIN;
            }
        }existingBoxes{512}, observedBoxes;
        
        virtual ~GenerationContext() = default;
        GenerationContext() = default;
        GenerationContext(const GenerationContext&) = default;
        GenerationContext(GenerationContext&&) = default;
        GenerationContext& operator=(const GenerationContext&) = default;
        GenerationContext& operator=(GenerationContext&&) = default;
    };
    struct PieceList 
    { 
        virtual ~PieceList() = default; 
        PieceList() = default;
        PieceList(const PieceList&) = default;
        PieceList(PieceList&&) = default;
        PieceList& operator=(const PieceList&) = default;
        PieceList& operator=(PieceList&&) = default;
    };
    struct StructurePiece
    {
    protected:
        uint32_t id;
        StructurePieceType pieceType;
        uint32_t roomDepth;
        MutableBoundingBox boundingBox;
        Direction facingDirection; // = coordBaseMode
    public:
        uint32_t expansionOrder; // NOT generationOrder!

        StructurePiece(uint32_t Id, StructurePieceType type, uint32_t depth, MutableBoundingBox box, Direction direction) : id(Id), pieceType(type), roomDepth(depth), boundingBox(box), facingDirection(direction) {}

        inline const MutableBoundingBox& getBoundingBox() const { return boundingBox; }
        inline Direction getFacingDirection() const { return facingDirection; }
        inline void setFacingDirection(Direction direction) { facingDirection = direction; }
        inline uint32_t getRoomDepth() const { return roomDepth; }
        inline StructurePieceType getPieceType() const noexcept { return pieceType; }
        
        // virtual void buildComponent([[maybe_unused]] GenerationContext& context, [[maybe_unused]] PieceList& existingPieces, Xoshiro256pp& rng) = 0;
        // virtual ~StructurePiece() = default;

        inline std::string getPieceNameInfo() { return StructurePieceTypeNames[pieceType]; }
    };
    
    inline int find_intersecting_scalar(const GenerationContext::SoABoundingBox& existingBoxes, const MutableBoundingBox& box)
    {
        const int n = static_cast<int>(existingBoxes.minX.size());
        for(int i = 0; i < n; ++i)
        {
            if(existingBoxes.maxX[i] >= box.minX &&
               box.maxX >= existingBoxes.minX[i] &&
               existingBoxes.maxY[i] >= box.minY &&
               box.maxY >= existingBoxes.minY[i] &&
               existingBoxes.maxZ[i] >= box.minZ &&
               box.maxZ >= existingBoxes.minZ[i]
            ){
                return i;
            }
        }
        return -1;
    }

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    enum class FindIntersectingSIMDLevel
    {
        Scalar,
        AVX2,
        AVX512
    };
    inline FindIntersectingSIMDLevel get_find_intersecting_best_simd_level()
    {
    #if defined(__GNUC__) || defined(__clang__)
        __builtin_cpu_init();
        if(__builtin_cpu_supports("avx512f"))
            return FindIntersectingSIMDLevel::AVX512;
        if(__builtin_cpu_supports("avx2"))
            return FindIntersectingSIMDLevel::AVX2;
        return FindIntersectingSIMDLevel::Scalar;
    #else
        return FindIntersectingSIMDLevel::Scalar;
    #endif
    }

    #if defined(__GNUC__) || defined(__clang__)
    __attribute__((target("avx2")))
    #endif
    inline int find_intersecting_AVX2_impl(const GenerationContext::SoABoundingBox& existingBoxes, const MutableBoundingBox& box)
    {
        const int n = static_cast<int>(existingBoxes.minX.size());
        if(n == 0)return -1;
    
        const int32_t* p_minX = existingBoxes.minX.data();
        const int32_t* p_minY = existingBoxes.minY.data();
        const int32_t* p_minZ = existingBoxes.minZ.data();
        const int32_t* p_maxX = existingBoxes.maxX.data();
        const int32_t* p_maxY = existingBoxes.maxY.data();
        const int32_t* p_maxZ = existingBoxes.maxZ.data();
    
        __m256i q_minX_sub1 = _mm256_set1_epi32(box.minX - 1);
        __m256i q_maxX_add1 = _mm256_set1_epi32(box.maxX + 1);
        __m256i q_minY_sub1 = _mm256_set1_epi32(box.minY - 1);
        __m256i q_maxY_add1 = _mm256_set1_epi32(box.maxY + 1);
        __m256i q_minZ_sub1 = _mm256_set1_epi32(box.minZ - 1);
        __m256i q_maxZ_add1 = _mm256_set1_epi32(box.maxZ + 1);
    
        int i = 0;
        for (; i <= n - 8; i += 8) 
        {
            __m256i minX = _mm256_loadu_si256((__m256i*)&p_minX[i]);
            __m256i maxX = _mm256_loadu_si256((__m256i*)&p_maxX[i]);
            __m256i minY = _mm256_loadu_si256((__m256i*)&p_minY[i]);
            __m256i maxY = _mm256_loadu_si256((__m256i*)&p_maxY[i]);
            __m256i minZ = _mm256_loadu_si256((__m256i*)&p_minZ[i]);
            __m256i maxZ = _mm256_loadu_si256((__m256i*)&p_maxZ[i]);
    
            __m256i cx1 = _mm256_cmpgt_epi32(maxX, q_minX_sub1);
            __m256i cx2 = _mm256_cmpgt_epi32(q_maxX_add1, minX);
            __m256i cy1 = _mm256_cmpgt_epi32(maxY, q_minY_sub1);
            __m256i cy2 = _mm256_cmpgt_epi32(q_maxY_add1, minY);
            __m256i cz1 = _mm256_cmpgt_epi32(maxZ, q_minZ_sub1);
            __m256i cz2 = _mm256_cmpgt_epi32(q_maxZ_add1, minZ);
    
            __m256i hit_all = _mm256_and_si256(_mm256_and_si256(cx1, cx2), 
                              _mm256_and_si256(_mm256_and_si256(cy1, cy2), 
                                               _mm256_and_si256(cz1, cz2)));
    
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(hit_all));
            
            if(mask)
                return i + __builtin_ctz((unsigned int)mask);
        }

        /*for (; i < n; ++i) {
            if (p_maxX[i] >= box.minX && p_minX[i] <= box.maxX &&
                p_maxY[i] >= box.minY && p_minY[i] <= box.maxY &&
                p_maxZ[i] >= box.minZ && p_minZ[i] <= box.maxZ) {
                return i; 
            }
        }*/
    
        return -1;
    }

    #if defined(__GNUC__) || defined(__clang__)
    __attribute__((target("avx512f")))
    #endif
    inline int find_intersecting_AVX512_impl(const GenerationContext::SoABoundingBox& existingBoxes, const MutableBoundingBox& box)
    {
        const int n = static_cast<int>(existingBoxes.minX.size());
        if(n == 0)return -1;

        const int32_t* p_minX = existingBoxes.minX.data();
        const int32_t* p_minY = existingBoxes.minY.data();
        const int32_t* p_minZ = existingBoxes.minZ.data();
        const int32_t* p_maxX = existingBoxes.maxX.data();
        const int32_t* p_maxY = existingBoxes.maxY.data();
        const int32_t* p_maxZ = existingBoxes.maxZ.data();

        const __m512i q_minX_minus1 = _mm512_set1_epi32(box.minX - 1);
        const __m512i q_maxX_plus1  = _mm512_set1_epi32(box.maxX + 1);
        const __m512i q_minY_minus1 = _mm512_set1_epi32(box.minY - 1);
        const __m512i q_maxY_plus1  = _mm512_set1_epi32(box.maxY + 1);
        const __m512i q_minZ_minus1 = _mm512_set1_epi32(box.minZ - 1);
        const __m512i q_maxZ_plus1  = _mm512_set1_epi32(box.maxZ + 1);

        int i = 0;
        
        for(; i + 16 <= n; i += 16)
        {
            const __m512i minX = _mm512_loadu_si512(reinterpret_cast<const void*>(p_minX + i));
            const __m512i minY = _mm512_loadu_si512(reinterpret_cast<const void*>(p_minY + i));
            const __m512i minZ = _mm512_loadu_si512(reinterpret_cast<const void*>(p_minZ + i));
            const __m512i maxX = _mm512_loadu_si512(reinterpret_cast<const void*>(p_maxX + i));
            const __m512i maxY = _mm512_loadu_si512(reinterpret_cast<const void*>(p_maxY + i));
            const __m512i maxZ = _mm512_loadu_si512(reinterpret_cast<const void*>(p_maxZ + i));

            const __mmask16 cmpX1 = _mm512_cmpgt_epi32_mask(maxX, q_minX_minus1);
            const __mmask16 cmpX2 = _mm512_cmpgt_epi32_mask(q_maxX_plus1, minX);
            const __mmask16 cmpY1 = _mm512_cmpgt_epi32_mask(maxY, q_minY_minus1);
            const __mmask16 cmpY2 = _mm512_cmpgt_epi32_mask(q_maxY_plus1, minY);
            const __mmask16 cmpZ1 = _mm512_cmpgt_epi32_mask(maxZ, q_minZ_minus1);
            const __mmask16 cmpZ2 = _mm512_cmpgt_epi32_mask(q_maxZ_plus1, minZ);
            const __mmask16 hit = cmpX1 & cmpX2 & cmpY1 & cmpY2 & cmpZ1 & cmpZ2;
            if(hit)
                return i + __builtin_ctz(static_cast<unsigned>(hit));
        }
        
        // padding ensures multiple of 8, use AVX2 for tailing
        for(; i + 8 <= n; i += 8)
        {
            const __m256i minX = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_minX + i));
            const __m256i minY = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_minY + i));
            const __m256i minZ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_minZ + i));
            const __m256i maxX = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_maxX + i));
            const __m256i maxY = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_maxY + i));
            const __m256i maxZ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p_maxZ + i));

            const __m256i q_minX_minus1_256 = _mm256_set1_epi32(box.minX - 1);
            const __m256i q_maxX_plus1_256  = _mm256_set1_epi32(box.maxX + 1);
            const __m256i q_minY_minus1_256 = _mm256_set1_epi32(box.minY - 1);
            const __m256i q_maxY_plus1_256  = _mm256_set1_epi32(box.maxY + 1);
            const __m256i q_minZ_minus1_256 = _mm256_set1_epi32(box.minZ - 1);
            const __m256i q_maxZ_plus1_256  = _mm256_set1_epi32(box.maxZ + 1);
            const __m256i cmpX1 = _mm256_cmpgt_epi32(maxX, q_minX_minus1_256);
            const __m256i cmpX2 = _mm256_cmpgt_epi32(q_maxX_plus1_256, minX);
            const __m256i cmpY1 = _mm256_cmpgt_epi32(maxY, q_minY_minus1_256);
            const __m256i cmpY2 = _mm256_cmpgt_epi32(q_maxY_plus1_256, minY);
            const __m256i cmpZ1 = _mm256_cmpgt_epi32(maxZ, q_minZ_minus1_256);
            const __m256i cmpZ2 = _mm256_cmpgt_epi32(q_maxZ_plus1_256, minZ);

            const __m256i hit256 =
                _mm256_and_si256(
                    _mm256_and_si256(cmpX1, cmpX2),
                    _mm256_and_si256(
                        _mm256_and_si256(cmpY1, cmpY2),
                        _mm256_and_si256(cmpZ1, cmpZ2)
                    )
                );
            
            const int mask = _mm256_movemask_ps(_mm256_castsi256_ps(hit256));
            if(mask)
                return i + __builtin_ctz(static_cast<unsigned>(mask));
        }
        return -1;
    }

    inline int find_intersecting(const GenerationContext::SoABoundingBox& existingBoxes, const MutableBoundingBox& box)
    {
        static const FindIntersectingSIMDLevel level = get_find_intersecting_best_simd_level();
        switch(level)
        {
            case FindIntersectingSIMDLevel::AVX512:
                return find_intersecting_AVX512_impl(existingBoxes, box);
            case FindIntersectingSIMDLevel::AVX2:
                return find_intersecting_AVX2_impl(existingBoxes, box);
            default:
                return find_intersecting_scalar(existingBoxes, box);
        }
    }

#else
    inline int find_intersecting(const GenerationContext::SoABoundingBox& existingBoxes, const MutableBoundingBox& box)
    {
        return find_intersecting_scalar(existingBoxes, box);
    }
#endif

}
