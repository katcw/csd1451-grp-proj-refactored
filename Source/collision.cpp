/*===========================================================================
@filename   collision.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Implements AABB collision checks (static and dynamic) and
            binary tile-map loading/querying
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include "AEEngine.h"
#include "AEFrameRateController.h"
#include "collision.hpp"

namespace Collision
{
	//============================================================================
	// STATIC AABB
	//============================================================================
	bool IsSolid_Static(const AABB& aabb1, const AABB& aabb2)
	{
		// No overlap when separated on either axis
		if (aabb1.max.x < aabb2.min.x || aabb1.min.x > aabb2.max.x ||
		    aabb1.max.y < aabb2.min.y || aabb1.min.y > aabb2.max.y)
			return false;

		return true;
	}

	//============================================================================
	// DYNAMIC AABB
	//============================================================================
	bool IsSolid_Dynamic(const AABB& aabb1, const AEVec2& vel1,
	                     const AABB& aabb2, const AEVec2& vel2,
	                     float& firstTimeOfCollision)
	{
		float tFirst = 0.0f;
		float tLast  = static_cast<float>(AEFrameRateControllerGetFrameTime());

		// Step 2: Calculate relative velocity of aabb2 with respect to aabb1.
		// We treat aabb1 as stationary; aabb2 moves with relative velocity Vb.
		float vRelX = vel2.x - vel1.x;
		float vRelY = vel2.y - vel1.y;

		// ── X-AXIS ──────────────────────────────────────────────────────────
		if (vRelX < 0.f)
		{
			// Case 1: [ <- B  A ] — B is already past A's left side, moving further left
			if (aabb2.max.x < aabb1.min.x)
				return false;

			// Case 4: [ A  <- B ] — B approaching from the right
			if (aabb1.max.x < aabb2.min.x)
			{
				float tEnter = (aabb1.max.x - aabb2.min.x) / vRelX;
				float tExit  = (aabb1.min.x - aabb2.max.x) / vRelX;
				if (tFirst < tEnter) tFirst = tEnter;
				if (tLast  > tExit)  tLast  = tExit;
			}
		}
		else if (vRelX > 0.f)
		{
			// Case 2: [ B ->  A ] — B approaching from the left
			if (aabb1.min.x > aabb2.max.x)
			{
				float tEnter = (aabb1.min.x - aabb2.max.x) / vRelX;
				float tExit  = (aabb1.max.x - aabb2.min.x) / vRelX;
				if (tFirst < tEnter) tFirst = tEnter;
				if (tLast  > tExit)  tLast  = tExit;
			}

			// Case 3: [ A  B -> ] — B already past A's right side, moving further right
			if (aabb1.max.x < aabb2.min.x)
				return false;
		}
		else // vRelX == 0
		{
			// Case 5: [ A  B ] — no relative X movement, check overlap
			if (aabb1.max.x < aabb2.min.x) return false;
			if (aabb1.min.x > aabb2.max.x) return false;
		}

		// Case 6: time interval already degenerate
		if (tFirst > tLast) return false;

		// ── Y-AXIS ──────────────────────────────────────────────────────────
		if (vRelY < 0.f)
		{
			// Case 1: [ <- B  A ]
			if (aabb2.max.y < aabb1.min.y)
				return false;

			// Case 4: [ A  <- B ]
			if (aabb1.max.y < aabb2.min.y)
			{
				float tEnter = (aabb1.max.y - aabb2.min.y) / vRelY;
				float tExit  = (aabb1.min.y - aabb2.max.y) / vRelY;
				if (tFirst < tEnter) tFirst = tEnter;
				if (tLast  > tExit)  tLast  = tExit;
			}
		}
		else if (vRelY > 0.f)
		{
			// Case 2: [ B ->  A ]
			if (aabb1.min.y > aabb2.max.y)
			{
				float tEnter = (aabb1.min.y - aabb2.max.y) / vRelY;
				float tExit  = (aabb1.max.y - aabb2.min.y) / vRelY;
				if (tFirst < tEnter) tFirst = tEnter;
				if (tLast  > tExit)  tLast  = tExit;
			}

			// Case 3: [ A  B -> ]
			if (aabb1.max.y < aabb2.min.y)
				return false;
		}
		else // vRelY == 0
		{
			// Case 5
			if (aabb1.max.y < aabb2.min.y) return false;
			if (aabb1.min.y > aabb2.max.y) return false;
		}

		// Case 6
		if (tFirst > tLast) return false;

		firstTimeOfCollision = tFirst;
		return true;
	}

	//============================================================================
	// LOAD MAP 
	//============================================================================
	void Map_Load(const char* filepath, Map& map, float tileSize, AEVec2 origin,
	              bool firstRowIsTop)
	{
		std::ifstream file(filepath);
		if (!file.is_open())
		{
			std::cerr << "ERROR: Cannot open map file: " << filepath << '\n';
			return;
		}

		map.tileSize = tileSize;
		map.origin   = origin;
		map.ready    = false;

		// Read all rows into two parallel buffers:
		//   solidRows — binary 0/1 (only ID 1 = wall is solid; ID 2/3/4 are passable in the tile map)
		//   rawRows   — original integer tile IDs for prop/spawn position extraction
		std::vector<std::vector<unsigned char>> solidRows;
		std::vector<std::vector<int>>           rawRows;
		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty()) continue;
			std::istringstream ss(line);
			std::vector<unsigned char> solidRow;
			std::vector<int>           rawRow;
			int val;
			while (ss >> val)
			{
				solidRow.push_back(static_cast<unsigned char>(val == 1 ? 1 : 0));
				rawRow.push_back(val);
			}
			if (!solidRow.empty())
			{
				solidRows.push_back(solidRow);
				rawRows.push_back(rawRow);
			}
		}

		if (solidRows.empty())
		{
			std::cerr << "[Collision] ERROR: Map file is empty: " << filepath << '\n';
			return;
		}

		map.height = static_cast<int>(solidRows.size());
		map.width  = static_cast<int>(solidRows[0].size());

		// Flip rows if file's first row represents the top of the map,
		// so that row 0 in memory corresponds to the AE-world bottom.
		if (firstRowIsTop)
		{
			std::reverse(solidRows.begin(), solidRows.end());
			std::reverse(rawRows.begin(),   rawRows.end());
		}

		// Flatten into map.solid and map.data (both row-major)
		map.solid.clear();
		map.solid.reserve(static_cast<size_t>(map.width) * map.height);
		for (const auto& row : solidRows)
			for (unsigned char cell : row)
				map.solid.push_back(cell);

		map.data.clear();
		map.data.reserve(static_cast<size_t>(map.width) * map.height);
		for (const auto& row : rawRows)
			for (int cell : row)
				map.data.push_back(cell);

		map.ready = true;
		std::cout << "[Collision] Map loaded: " << filepath
		          << " (" << map.width << 'x' << map.height << ")\n";
	}

	//============================================================================
	// UNLOAD MAP
	//============================================================================
	void Map_Unload(Map& map)
	{
		map.solid.clear();
		map.solid.shrink_to_fit();
		map.data.clear();
		map.data.shrink_to_fit();
		map.width  = 0;
		map.height = 0;
		map.ready  = false;
	}

	//============================================================================
	// SOLID QUERY
	//============================================================================
	bool Map_IsSolidAABB(const Map& map, float cx, float cy, float halfW, float halfH)
	{
		if (!map.ready) return false;

		// Test all four corners of the bounding box
		const float cornersX[2] = { cx - halfW, cx + halfW };
		const float cornersY[2] = { cy - halfH, cy + halfH };

		for (float cornerX : cornersX)
		{
			for (float cornerY : cornersY)
			{
				int col = static_cast<int>(std::floor((cornerX - map.origin.x) / map.tileSize));
				int row = static_cast<int>(std::floor((cornerY - map.origin.y) / map.tileSize));

				// Out-of-bounds is treated as solid
				if (col < 0 || col >= map.width || row < 0 || row >= map.height)
					return true;

				if (map.solid[static_cast<size_t>(row) * map.width + col])
					return true;
			}
		}

		return false;
	}

	//============================================================================
	// POSITION EXTRACTION
	//============================================================================
	int Map_GetCentres(const Map& map, int id, AEVec2* out, int maxOut)
	{
		if (!map.ready || !out || maxOut <= 0) return 0;

		int n = 0;
		for (int row = 0; row < map.height && n < maxOut; ++row)
		{
			for (int col = 0; col < map.width && n < maxOut; ++col)
			{
				if (map.data[static_cast<size_t>(row) * map.width + col] != id) continue;
				out[n].x = map.origin.x + col * map.tileSize + map.tileSize * 0.5f;
				out[n].y = map.origin.y + row * map.tileSize + map.tileSize * 0.5f;
				++n;
			}
		}
		return n;
	}

}
