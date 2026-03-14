/*===========================================================================
@filename   player.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Player class and PlayerSystem lifecycle functions.
            The player is an EntityBase with MC sprite animation and
            tile-based WASD movement with hold-threshold input.
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

	class Player : public Entity::EntityBase
	{
	public:
		HeldState held;

		/// Accumulated time the current WASD key has been held (seconds).
		/// Movement commits only after this exceeds INPUT_HOLD_THRESHOLD.
		float inputHoldTimer = 0.f;

		/// Direction currently being held (LAST = none).
		Entity::FaceDirection inputDirection = Entity::FaceDirection::LAST;

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
	constexpr float HALF_H = 30.0f; // Half height of player scale

	/// How long WASD must be held before the player commits to moving one tile (seconds).
	constexpr float INPUT_HOLD_THRESHOLD = 0.12f;

	void Load();
	void Init(AEVec2 startPos);
	void Update(float dt);
	void Draw();
	void Free();
	void Unload();
} 

