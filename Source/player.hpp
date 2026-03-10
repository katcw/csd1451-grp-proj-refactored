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

#include "entity.hpp"
#include "collision.hpp"
#include "sprite.hpp"
#include "item_types.hpp"

//============================================================================
// PLAYER CLASS
//============================================================================

// Player class inherits from EntityBase class
// [?] Player class has public and protected data members of EntityBase class
class Player : public Entity::EntityBase
{
public:
    HeldState held; // 
};

//============================================================================
// PLAYER SYSTEM
//============================================================================
namespace PlayerSystem
{
	extern Player* p1;

	// [!] MAKE SURE to change these if player scale changes, if not collision
	//	   will be slightly off
	constexpr float HALF_W = 25.0f; // Half width of player scale
	constexpr float HALF_H = 50.0f; // Half height of player scale

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

} 

