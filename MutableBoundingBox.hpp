#pragma once

#include<string>
#include<sstream>
#include"Directions.hpp"
namespace utils
{
	struct MutableBoundingBox
	{
		int minX, minY, minZ;
		int maxX, maxY, maxZ;
		MutableBoundingBox() : minX(0), minY(0), minZ(0), maxX(0), maxY(0), maxZ(0) {}
		MutableBoundingBox(int x1, int y1, int z1, int x2, int y2, int z2)
		{
			minX = std::min(x1, x2);
            minY = std::min(y1, y2);
            minZ = std::min(z1, z2);
            maxX = std::max(x1, x2);
            maxY = std::max(y1, y2);
            maxZ = std::max(z1, z2);
		}
		inline static MutableBoundingBox getNewBoundingBox() {return MutableBoundingBox{INT_MAX, INT_MAX, INT_MAX, INT_MIN, INT_MIN, INT_MIN}; }
		
		/*static MutableBoundingBox getComponentToAddBoundingBox(
			int x, int y, int z,
            int offsetX, int offsetY, int offsetZ,
            int sizeX, int sizeY, int sizeZ,
            Direction direction
		){
            switch (direction) {
                case NORTH: return MutableBoundingBox(x + offsetX, y + offsetY, z - sizeZ + 1 + offsetZ,
                        x + sizeX - 1 + offsetX, y + sizeY - 1 + offsetY, z + offsetZ);
                case SOUTH: return MutableBoundingBox(x + offsetX, y + offsetY, z + offsetZ,
                        x + sizeX - 1 + offsetX, y + sizeY - 1 + offsetY, z + sizeZ - 1 + offsetZ);
                case WEST:  return MutableBoundingBox(x - sizeZ + 1 + offsetZ, y + offsetY, z + offsetX,
                        x + offsetZ, y + sizeY - 1 + offsetY, z + sizeX - 1 + offsetX);
                default:
                case EAST:  return MutableBoundingBox(x + offsetZ, y + offsetY, z + offsetX,
                        x + sizeZ - 1 + offsetZ, y + sizeY - 1 + offsetY, z + sizeX - 1 + offsetX);
            };
        }*/
        static MutableBoundingBox getComponentToAddBoundingBox(
            int x, int y, int z,
            int offsetX, int offsetY, int offsetZ,
            int sizeX, int sizeY, int sizeZ,
            Direction direction
        ){
            int minXs[4] = { x + offsetX, x + offsetZ, x + offsetX, x - sizeZ + 1 + offsetZ};
            int minZs[4] = { z - sizeZ + 1 + offsetZ, z + offsetX, z + offsetZ, z + offsetX};
            int maxXs[4] = { x + sizeX - 1 + offsetX, x + sizeZ - 1 + offsetZ, x + sizeX - 1 + offsetX, x + offsetZ};
            int maxZs[4] = { z + offsetZ, z + sizeX - 1 + offsetX, z + sizeZ - 1 + offsetZ, z + sizeX - 1 + offsetX};
            return MutableBoundingBox(
                minXs[direction], y + offsetY, minZs[direction],
                maxXs[direction], y + sizeY - 1 + offsetY, maxZs[direction]
            );
        }
        
        inline int getXSize() const { return maxX - minX + 1; }
        inline int getYSize() const { return maxY - minY + 1; }
        inline int getZSize() const { return maxZ - minZ + 1; }
        
        inline bool intersectWith(const MutableBoundingBox& other) const
        {
        	return 
				this->maxX >= other.minX && this->minX <= other.maxX &&
                this->maxZ >= other.minZ && this->minZ <= other.maxZ &&
                this->maxY >= other.minY && this->minY <= other.maxY
			;	
		}
		
		void expandTo(const MutableBoundingBox& box)
		{
			minX = std::min(this->minX, box.minX);
            minY = std::min(this->minY, box.minY);
            minZ = std::min(this->minZ, box.minZ);
            maxX = std::max(this->maxX, box.maxX);
            maxY = std::max(this->maxY, box.maxY);
            maxZ = std::max(this->maxZ, box.maxZ);
		}
		
		std::string getBoxInfo()
		{
			std::ostringstream oss;
			oss << "Box[" << minX << ',' << minY << ',' << minZ <<
				   " -> " << maxX << ',' << maxY << ',' << maxZ;
			return oss.str();	
		}
	};
}
