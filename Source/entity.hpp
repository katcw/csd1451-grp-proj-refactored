/*===========================================================================
@filename   entity.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Shared base class for all movable characters (player, NPCs, etc.),
            along with enums for facing direction and movement state.
            Header-only — other character classes inherit from EntityBase
            to get these common fields for free.
============================================================================*/
#pragma once

#include "AEEngine.h"

namespace Entity
{
	enum class FaceDirection { LEFT, RIGHT, UP, DOWN };
	enum class MoveState     { IDLE, WALKING };

	/******************************************************************************/
	/*
	@brief  Base class shared by all movable characters (player, NPCs, etc.).
	        Any character class that inherits from this automatically receives
	        all of the data members listed below. Only add members here when
	        they are needed by every type of character — keep character-specific
	        data in the character's own class instead.

	@member position   Where the character currently is in the game world (x, y).
	                   (0, 0) by default.

	@member velocity   How fast the character is moving and in which direction
	                   this frame (x, y). Applied each update to move position.
	                   (0, 0) by default.

	@member speed      The character's maximum movement speed. Used to scale
	                   velocity when movement input is applied. 0 by default.

	@member facing     The direction the character is currently looking.
	                   Used by the animation and draw systems to pick the
	                   correct sprite. Defaults to RIGHT.

	@member moveState  Whether the character is standing still or walking.
	                   Used by the animation system to switch between idle
	                   and walk animations. Defaults to IDLE.
	*/
	/******************************************************************************/
	class EntityBase
	{
	public:
		AEVec2        position  = { 0.f, 0.f };
		AEVec2        velocity  = { 0.f, 0.f };
		float         speed     = 0.f;
		FaceDirection facing    = FaceDirection::RIGHT;
		MoveState     moveState = MoveState::IDLE;
	};

} 

