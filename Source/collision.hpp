/*===========================================================================
@filename   collision.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      AABB collision tests (static and dynamic) and a binary tile-map
            structure with grid-based solid queries.
============================================================================*/
#pragma once

#include <vector>
#include "AEEngine.h"

namespace Collision
{
	// Axis aligned bounding box (AABB)
	struct AABB
	{
		AEVec2 min; // Bottom left corner
		AEVec2 max; // Top right corner
	};

	// Binary tile map loaded from a space separated text file 
	struct Map
	{
		int		width		= 0;
		int		height		= 0;
		float	tileSize	= 64.0f; // Size of each tile in world coordinates
		AEVec2	origin		= { -800.f, -448.f }; // Bottom left corner position of grid in world coordinates
		bool	ready		= false; // Guard against uninitialised maps

		std::vector<unsigned char>	solid; // Array of binary values to represent walls and passable objects
		std::vector<int>			data;  // Array of raw tile IDs, used for extracting prop positions
	};

	/******************************************************************************/
	/*
	@brief  Checks if two rectangular hitboxes are currently touching or
	        overlapping each other, using static AABB test as per CSD1130.

	@param  aabb1   The first rectangle.
	@param  aabb2   The second rectangle.

	@return true if the rectangles overlap, false if they are apart.
	*/
	/******************************************************************************/
	bool IsSolid_Static(const AABB& aabb1, const AABB& aabb2);

	/******************************************************************************/
	/*
	@brief  Checks if two dynamic objects will collide at any point during
	        this frame, using dynamic AABB test as per CSD1130.

	@param  aabb1                 The first rectangle.
	@param  vel1                  How far the first rectangle moves this frame (x, y).
	@param  aabb2                 The second rectangle.
	@param  vel2                  How far the second rectangle moves this frame (x, y).
	@param  firstTimeOfCollision  Filled with how far into the frame the collision
	                              first occurs, as a fraction between 0 and the
	                              frame duration. Only written to when true is returned.

	@return true if the rectangles will collide this frame, false otherwise.
	*/
	/******************************************************************************/
	bool IsSolid_Dynamic(const AABB& aabb1, const AEVec2& vel1,
	                     const AABB& aabb2, const AEVec2& vel2,
	                     float& firstTimeOfCollision);

	/******************************************************************************/
	/*
	@brief  Loads a tile map from a text file and prepares it for collision checks.
	        The file must contain rows of space-separated integers. Only tiles
	        with the value 1 are treated as solid walls; all other values are
	        passable. Each tile's integer value is also stored separately so that
	        spawn points and props can be looked up later with Map_GetCentres.

	        [!] When firstRowIsTop is true (default), the first row in the file
	            is treated as the visual top of the map. The rows are flipped
	            internally so that row 0 in memory always represents the bottom
	            of the game world.

	@param  filepath      Path to the tile map text file.
	@param  map           The Map struct to populate.
	@param  tileSize      Width and height of a single tile in world units.
	@param  origin        World position of the bottom-left corner of the map.
	@param  firstRowIsTop Whether the first line of the file represents the top
	                      of the map (default: true).
	*/
	/******************************************************************************/
	void Map_Load(const char* filepath, Map& map, float tileSize, AEVec2 origin,
	              bool firstRowIsTop = true);

	/******************************************************************************/
	/*
	@brief  Clears all tile data from a Map and resets it to its default empty
	        state. Call this when leaving a level to free memory.

	@param  map   The Map struct to clear.
	*/
	/******************************************************************************/
	void Map_Unload(Map& map);

	/******************************************************************************/
	/*
	@brief  Checks whether a rectangle overlaps any solid tile on the map.
	        The rectangle is described by its center point and half-dimensions.
	        All four corners of the rectangle are tested individually.
	        Any corner that lands outside the map boundary also counts as solid.

	@param  map    The tile map to test against.
	@param  cx     X position of the rectangle's center.
	@param  cy     Y position of the rectangle's center.
	@param  halfW  Half the width of the rectangle.
	@param  halfH  Half the height of the rectangle.

	@return true if any corner of the rectangle is inside a solid tile or
	        outside the map, false if all corners are in open space.
	*/
	/******************************************************************************/
	bool Map_IsSolidAABB(const Map& map, float cx, float cy, float halfW, float halfH);

	/******************************************************************************/
	/*
	@brief  Finds every tile on the map with the given ID and writes the
	        center position of each matching tile into the output array.
	        Use this at load time to locate spawn points or props placed in
	        the map file, avoiding the need to hardcode world positions.

	@param  map     The tile map to search.
	@param  id      The tile ID to search for.
	@param  out     Array to write the center positions into.
	@param  maxOut  Maximum number of positions to write (size of out[]).

	@return The number of matching tiles found (never exceeds maxOut).
	*/
	/******************************************************************************/
	int Map_GetCentres(const Map& map, int id, AEVec2* out, int maxOut);

}

