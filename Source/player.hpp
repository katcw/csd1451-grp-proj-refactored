/*===========================================================================
@filename   player.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Player class and PlayerSystem lifecycle functions.
            The player is an EntityBase with MC sprite animation and
            WASD movement with per-axis collision against the tile map.
============================================================================*/
#pragma once

#include "collision.hpp"
#include "utilities.hpp"
#include "item_types.hpp"

namespace Sprite = BasicUtilities::Sprite;

//============================================================================
// PLAYER SYSTEM
//============================================================================
namespace PlayerSystem
{
	//============================================================================
	// PLAYER CLASS
	//============================================================================

	// Player class inherits from EntityBase class
	// Player class has public and protected data members of EntityBase class
	class Player : public Entity::EntityBase
	{
	public:
		HeldState held; // 

		/**
	 * @brief Parameterized constructor using full EntityBase initialization.
	 *
	 * @details Allows explicit initialization of all inherited members
	 *          via the full EntityBase constructor.
	 *
	 * @param[in] id       Unique id (0 = unassigned, will be set by AddEntity if used)
	 * @param[in] coord    Initial world coordinates
	 * @param[in] nextCoord Initial next/target coordinates
	 * @param[in] currentIdx Initial grid index
	 * @param[in] nextIdx Next grid index
	 * @param[in] spd      Movement speed in pixels per second
	 * @param[in] dir      Initial facing direction
	 * @param[in] st       Initial movement state
	 * @param[in] hold     Initial holding flag
	 */
		Player(ID id,
			Entity::EntityType type,
			AEVec2 coord,
			AEVec2 nextCoord,
			Entity::Index currentIdx,
			Entity::Index nextIdx,
			float spd,
			Entity::FaceDirection dir,
			Entity::MoveState st,
			bool act = true,
			bool AI = false,
			bool hold = false,
			HeldState holding = {});

		~Player();
	};

	extern Player* p1;

	// [!] MAKE SURE to change these if player scale changes, if not collision
	//	   will be slightly off
	constexpr float HALF_W = 25.0f; // Half width of player scale
	constexpr float HALF_H = 50.0f; // Half height of player scale

	void Load();

	/******************************************************************************/
	/*
		Allocates player:
			[1] Starting pos
			[2] Loads MC sprite animations (Idle, Walking, IdleCarry, WalkingCarry)
			[3] Creates sprite mesh
	*/
	/******************************************************************************/
	void Init(AEVec2 startPos);

	/******************************************************************************/
	/*
		Reads WASD input, moves the player with per-axis collision against the
		tile map, and updates the active animation clip
	*/
	/******************************************************************************/
	void Update(const Collision::Map& map, float dt);

	/******************************************************************************/
	/*
		It just draws
	*/
	/******************************************************************************/
	void Draw();

	/******************************************************************************/
	/*
		Frees the sprite textures, sprite mesh, and deletes player object
	*/
	/******************************************************************************/
	void Free();

	void Unload();
} 

