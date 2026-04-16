#pragma once

#include"Xoshiro256PlusPlus.hpp"
namespace utils
{
	constexpr uint32_t DIRECTIONS_COUNT = 4;
	enum Direction {NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3};
	constexpr const char* DirectionNames[] = {"NORTH", "EAST", "SOUTH", "WEST"};
	inline constexpr Direction HORIZONTALS[] = {NORTH, EAST, SOUTH, WEST};
	inline Direction getRandomDirection(Xoshiro256pp& rng){
		return HORIZONTALS[rng.nextInt(DIRECTIONS_COUNT)];
	}
}
