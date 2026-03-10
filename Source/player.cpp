/*===========================================================================
@filename   player.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Implements the PlayerSystem: WASD movement with per-axis tile-map
            collision, Shift sprint (1.5x), and MC sprite animation driven by
            the player's facing direction and movement state
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <cmath>
#include "AEEngine.h"
#include "player.hpp"
#include "utilities.hpp"

//============================================================================
// DATA
//============================================================================
namespace
{
	// 4 animation slots: Idle, Walking, IdleCarry, WalkCarry
	Sprite::SpriteData mcSprite;
	// Current frame state
	Sprite::AnimationState animState;
	// UV sized to one frame (0.25 x 0.25 for 32/128)
	AEGfxVertexList* spriteMesh = nullptr; 

	// Tracks which texture slot is active for Draw()
	unsigned int currentTexSlot = 0;  
	// Tracks tag index (direction) for Draw()
	unsigned int currentTagIndex = 1;  
}

namespace PlayerSystem
{
	// MC - pointer to Player struct 
	Player* p1 = nullptr;

	float playerScaleX = 100.0f,
		  playerScaleY = 100.f;

	//==========================================================================
	// Init
	//==========================================================================
	void Init(AEVec2 startPos)
	{
		p1           = new Player();
		p1->position = startPos;
		p1->speed    = 200.f;

		// MC spritesheet: 128x128 total, 32x32 per frame → UV per frame = 32/128 = 0.25
		spriteMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);

		Sprite::LoadEntry(mcSprite, 0,
			"Assets/character-assets/MC_Idle.png",
			"Assets/character-assets/MC_Idle.json");

		Sprite::LoadEntry(mcSprite, 1,
			"Assets/character-assets/MC_walking.png",
			"Assets/character-assets/MC_walking.json");

		Sprite::LoadEntry(mcSprite, 2,
			"Assets/character-assets/MC_idle_carry.png",
			"Assets/character-assets/MC_idle_carry.json");

		Sprite::LoadEntry(mcSprite, 3,
			"Assets/character-assets/MC_walking_carry.png",
			"Assets/character-assets/MC_walking_carry.json");
	}

	//==========================================================================
	// Update
	//==========================================================================
	void Update(const Collision::Map& map, float dt)
	{
		if (!p1) return;

		// ── Input ─────────────────────────────────────────────────────────────
		float dx = 0.f, dy = 0.f;

		if (AEInputCheckCurr(AEVK_W) || AEInputCheckCurr(AEVK_UP))    dy += 1.f;
		if (AEInputCheckCurr(AEVK_S) || AEInputCheckCurr(AEVK_DOWN))  dy -= 1.f;
		if (AEInputCheckCurr(AEVK_A) || AEInputCheckCurr(AEVK_LEFT))  dx -= 1.f;
		if (AEInputCheckCurr(AEVK_D) || AEInputCheckCurr(AEVK_RIGHT)) dx += 1.f;

		// Normalise diagonal movement
		float len = std::sqrt(dx * dx + dy * dy);
		if (len > 0.f)
		{
			dx /= len;
			dy /= len;
		}

		// Sprint (Shift held → 1.5×)
		float currentSpeed = p1->speed;
		if (AEInputCheckCurr(AEVK_LSHIFT))
			currentSpeed *= 1.5f;

		p1->velocity = { dx * currentSpeed, dy * currentSpeed };

		// ── Per-axis collision ─────────────────────────────────────────────────
		float newX = p1->position.x + p1->velocity.x * dt;
		if (!Collision::Map_IsSolidAABB(map, newX, p1->position.y, HALF_W, HALF_H))
			p1->position.x = newX;

		float newY = p1->position.y + p1->velocity.y * dt;
		if (!Collision::Map_IsSolidAABB(map, p1->position.x, newY, HALF_W, HALF_H))
			p1->position.y = newY;

		// ── Facing direction and move state ────────────────────────────────────
		if (dx != 0.f || dy != 0.f)
		{
			p1->moveState = Entity::MoveState::WALKING;

			// Horizontal takes priority over vertical for facing
			if      (dx < 0.f) p1->facing = Entity::FaceDirection::LEFT;
			else if (dx > 0.f) p1->facing = Entity::FaceDirection::RIGHT;
			else if (dy > 0.f) p1->facing = Entity::FaceDirection::UP;
			else               p1->facing = Entity::FaceDirection::DOWN;
		}
		else
		{
			p1->moveState = Entity::MoveState::IDLE;
		}

		// ── Select animation ───────────────────────────────────────────────────
		// Tag index: 0=Left, 1=Right, 2=Up, 3=Back (down/toward camera)
		switch (p1->facing)
		{
		case Entity::FaceDirection::LEFT:  currentTagIndex = 0; break;
		case Entity::FaceDirection::RIGHT: currentTagIndex = 1; break;
		case Entity::FaceDirection::UP:    currentTagIndex = 2; break;
		case Entity::FaceDirection::DOWN:  currentTagIndex = 3; break;
		}

		// Texture slot: 0=Idle, 1=Walking, 2=IdleCarry, 3=WalkCarry
		currentTexSlot = (p1->moveState == Entity::MoveState::WALKING) ? 1 : 0;
		if (p1->held.type != HeldItem::NONE) currentTexSlot += 2;

		Sprite::UpdateAnimation(animState, mcSprite.metas[currentTexSlot], currentTagIndex);
	}

	//==========================================================================
	// Draw
	//==========================================================================
	void Draw()
	{
		if (!p1 || !spriteMesh) return;
		if (!mcSprite.textures[currentTexSlot]) return;

		Sprite::DrawAnimation(spriteMesh,
		                      mcSprite.textures[currentTexSlot],
		                      animState,
		                      p1->position,
		                      { playerScaleX, playerScaleX });
	}

	//==========================================================================
	// Free
	//==========================================================================
	void Free()
	{
		mcSprite.Free();

		if (spriteMesh)
		{
			AEGfxMeshFree(spriteMesh);
			spriteMesh = nullptr;
		}

		delete p1;
		p1 = nullptr;

		currentTexSlot  = 0;
		currentTagIndex = 1;
		animState = Sprite::AnimationState{};
	}

} // namespace PlayerSystem
