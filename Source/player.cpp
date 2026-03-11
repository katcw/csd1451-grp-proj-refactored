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

// =============================================================================
//                               DEBUG FLAG
// =============================================================================

static const bool debug = false;  ///< Enable/disable debug visualization and output

// =============================================================================
//                      Global constants and variables
// =============================================================================

using json = nlohmann::json;
namespace Sprite = BasicUtilities::Sprite;

static constexpr float DEGREES_TO_RADIANS = PI / 180.0f;  ///< Conversion factor from degrees to radians
static constexpr float SPRINT_MULTIPLIER = 1.5f;          ///< Speed multiplier when sprinting
static constexpr float WALK_MULTIPLIER = 1.0f;            ///< Speed multiplier when walking
static constexpr float TILE_SIZE = 50.0f;                 ///< Size of a tile in pixels

float movementMuliplier = 1.0f;							  ///< Current movement speed multiplier

// =============================================================================
//                           Player meshes
// =============================================================================

AEGfxVertexList* playerMesh = nullptr;    ///< Player sprite mesh
AEGfxVertexList* playerMesh_D = nullptr;  ///< Debug collision box mesh
AEGfxVertexList* arrowMesh_D = nullptr;   ///< Debug direction arrow mesh

// =============================================================================
//                  Anonymous namespace: helper function declarations
// =============================================================================
namespace
{

	// -------------------------------------------------------------------------
	//                         Input detection
	// -------------------------------------------------------------------------

	/**
	 * @brief Checks if any movement keys are pressed.
	 * @return True if W, A, S, or D is currently pressed
	 */
	bool Is_Moving();

	/**
	 * @brief Checks if player is idle.
	 * @return True if no movement input for a certain time
	 * @todo Implement idle detection based on time without input
	 */
	bool Is_Idle();

	/**
	 * @brief Checks if player is performing an action.
	 * @return True if action key is pressed
	 * @todo Implement action key detection
	 */
	bool Is_Action();

	/**
	 * @brief Checks if sprint keys are pressed.
	 * @return True if left or right shift is currently pressed
	 */
	bool Is_Sprinting();

	// -------------------------------------------------------------------------
	//                         State and movement
	// -------------------------------------------------------------------------

	/**
	 * @brief Determines movement direction based on input keys.
	 * @details Checks WASD keys and updates the player's lastDirection.
	 *          Priority order: W > S > A > D
	 * @return FaceDirection enum value, or FACE_LAST if no key pressed
	 */
	Entity::FaceDirection Last_Direction();

	/**
	 * @brief Updates player state based on current input.
	 * @details Sets player state to running, walking, action, idle, or default
	 *          based on key input. Also updates the movement multiplier.
	 */
	void Last_State();

	/**
	 * @brief Calculates movement delta based on direction.
	 * @details Computes X and Y movement components using speed, multiplier,
	 *          and delta time. Output is written to moveX and moveY parameters.
	 * @param[in] direction Direction enum value
	 * @param[in] speed Base movement speed (pixels per second)
	 * @param[in] multiplier Speed multiplier (e.g., sprint = 1.5)
	 * @param[in] dt Delta time in seconds
	 * @param[out] moveX Calculated X movement component
	 * @param[out] moveY Calculated Y movement component
	 */
	void CalculateMovement(Entity::FaceDirection direction, float speed, float multiplier,
		float dt, float& moveX, float& moveY);

	// -------------------------------------------------------------------------
	//                    DEBUG: Mesh creation and drawing
	// -------------------------------------------------------------------------

	/**
	 * @brief Creates an arrow mesh for direction indicator.
	 * @param[in] color Mesh color in ARGB format (0xAARRGGBB)
	 * @return Pointer to created vertex list
	 */
	AEGfxVertexList* arrowmesh(unsigned int color);

	/**
	 * @brief Draws a mesh at given position and scale.
	 * @details Builds a transformation matrix and draws the mesh.
	 * @param[in] mesh Pointer to vertex list to draw
	 * @param[in] posX X position in world space
	 * @param[in] posY X position in world space
	 * @param[in] w Width scaling factor
	 * @param[in] h Height scaling factor
	 */
	void drawMesh(AEGfxVertexList* mesh, float posX, float posY, float w, float h);

	/**
	 * @brief Draws an arrow mesh with rotation.
	 * @details Builds a transformation matrix with scale, rotation, and translation,
	 *          then draws the mesh. Used for direction indicator.
	 * @param[in] mesh Pointer to vertex list to draw
	 * @param[in] posX X position in world space
	 * @param[in] posY Y position in world space
	 * @param[in] w Width scaling factor
	 * @param[in] h Height scaling factor
	 * @param[in] angle Rotation angle in degrees
	 */
	void drawArrowMesh(AEGfxVertexList* mesh, float posX, float posY,
		float w, float h, float angle);
}


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

	void Load()
	{
		// Reset count and attempt to load each entry, counting successes
		mcSprite.count = 0;

		Sprite::LoadEntry4("MC", mcSprite);

		if (debug) std::cout << "[DEBUG] playerSpriteData.count = " << mcSprite.count << "\n";

		// Create debug meshes
		if (debug)
		{
			playerMesh_D = BasicUtilities::createSquareMesh(0x44FF0000);
			arrowMesh_D = arrowmesh(0x88FFFF00);
		}
	}

	//==========================================================================
	// Init
	//==========================================================================
	void Init(AEVec2 startPos)
	{
		p1 = new Player(
			/*id_*/        0u,
			/*type*/       Entity::EntityType::PLAYER,
			/*coord*/      startPos,
			/*nextCoord*/  startPos,
			/*currentIdx*/ Entity::Index{ 0, 0 },
			/*nextIdx*/    Entity::Index{ 0, 0 },
			/*spd*/        200.0f,
			/*dir*/        Entity::FaceDirection::DOWN,
			/*st*/         Entity::MoveState::DEFAULT,
			/*act*/        true,
			/*AI*/         false,
			/*hold*/       false,
			/*held*/	   HeldState{}
		);

		// MC spritesheet: 128x128 total, 32x32 per frame → UV per frame = 32/128 = 0.25
		spriteMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);
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
		float currentSpeed = p1->GetSpeed();
		if (AEInputCheckCurr(AEVK_LSHIFT))
			currentSpeed *= 1.5f;

		p1->SetVelocity(AEVec2{ dx * currentSpeed, dy * currentSpeed });

		// ── Per-axis collision ─────────────────────────────────────────────────
		float newX = p1->GetCoordinates().x + p1->GetVelocity().x * dt;
		if (!Collision::Map_IsSolidAABB(map, newX, p1->GetCoordinates().y, HALF_W, HALF_H))
			p1->RefX() = newX;

		float newY = p1->GetCoordinates().y + p1->GetVelocity().y * dt;
		if (!Collision::Map_IsSolidAABB(map, p1->GetCoordinates().x, newY, HALF_W, HALF_H))
			p1->RefY() = newY;

		// ── Facing direction and move state ────────────────────────────────────
		if (dx != 0.f || dy != 0.f)
		{
			p1->SetMoveState(Entity::MoveState::WALKING);

			// Horizontal takes priority over vertical for facing
			if		(dx < 0.f) p1->SetFaceDirection(Entity::FaceDirection::LEFT);
			else if (dx > 0.f) p1->SetFaceDirection(Entity::FaceDirection::RIGHT);
			else if (dy > 0.f) p1->SetFaceDirection(Entity::FaceDirection::UP);
			else               p1->SetFaceDirection(Entity::FaceDirection::DOWN);
		}
		else
		{
			p1->SetMoveState(Entity::MoveState::IDLE);
		}

		// ── Select animation ───────────────────────────────────────────────────
		// Tag index: 0=Left, 1=Right, 2=Up, 3=Back (down/toward camera)
		switch (p1->GetLastDirection())
		{
		case Entity::FaceDirection::LEFT:  currentTagIndex = 0; break;
		case Entity::FaceDirection::RIGHT: currentTagIndex = 1; break;
		case Entity::FaceDirection::UP:    currentTagIndex = 2; break;
		case Entity::FaceDirection::DOWN:  currentTagIndex = 3; break;
		}

		// Texture slot: 0=Idle, 1=Walking, 2=IdleCarry, 3=WalkCarry
		currentTexSlot = (p1->GetMoveState() == Entity::MoveState::WALKING) ? 1 : 0;
		if (p1->held.type != HeldItem::NONE) currentTexSlot += 2;

		Sprite::UpdateAnimation(animState, mcSprite.jsonList[currentTexSlot], currentTagIndex);
	}

	//==========================================================================
	// Draw
	//==========================================================================
	void Draw()
	{
		if (!p1 || !spriteMesh) return;
		if (!mcSprite.textureList[currentTexSlot]) return;

		Sprite::DrawAnimation(spriteMesh,
		                      mcSprite.textureList[currentTexSlot],
		                      animState,
		                      p1->GetCoordinates(),
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

	void Unload()
	{
		// This function is reserved for unloading player-specific assets if needed.
		// Currently, all assets are freed in Free(), so this is empty.
	}

	// =========================================================================
    //                          Player class definitions
    // =========================================================================

    // -------------------------------------------------------------------------
    //                         Constructors & Destructor
    // -------------------------------------------------------------------------

	Player::Player(ID id_,
		Entity::EntityType type,
		AEVec2 coord,
		AEVec2 nextCoord,
		Entity::Index currentIdx,
		Entity::Index nextIdx,
		float spd,
		Entity::FaceDirection dir,
		Entity::MoveState st,
		bool act,
		bool AI,
		bool hold,
		HeldState holding)
		: Entity::EntityBase(
			id_,
			type,
			coord,
			nextCoord,
			currentIdx,
			nextIdx,
			spd,
			dir,
			st,
			act,
			AI,
			hold
			)
	{
		this->held = holding;
	}

	Player::~Player() 
	{
		
	}
} // namespace PlayerSystem


// =============================================================================
//              Anonymous namespace: helper function implementations
// =============================================================================
namespace
{
	// -------------------------------------------------------------------------
	//                         Input detection
	// -------------------------------------------------------------------------

	bool Is_Moving()
	{
		return (AEInputCheckCurr(AEVK_W) || AEInputCheckCurr(AEVK_S) ||
			AEInputCheckCurr(AEVK_A) || AEInputCheckCurr(AEVK_D));
	}

	bool Is_Idle()
	{
		return false;
	}

	bool Is_Action()
	{
		return false;
	}

	bool Is_Sprinting()
	{
		return (AEInputCheckCurr(AEVK_LSHIFT) || AEInputCheckCurr(AEVK_RSHIFT));
	}

	// -------------------------------------------------------------------------
	//                         State and movement
	// -------------------------------------------------------------------------

	Entity::FaceDirection Last_Direction()
	{
		if (AEInputCheckCurr(AEVK_W))
		{
			PlayerSystem::p1->SetFaceDirection(Entity::FaceDirection::UP);
			return Entity::FaceDirection::UP;
		}
		else if (AEInputCheckCurr(AEVK_S))
		{
			PlayerSystem::p1->SetFaceDirection(Entity::FaceDirection::DOWN);
			return Entity::FaceDirection::DOWN;
		}
		else if (AEInputCheckCurr(AEVK_A))
		{
			PlayerSystem::p1->SetFaceDirection(Entity::FaceDirection::LEFT);
			return Entity::FaceDirection::LEFT;
		}
		else if (AEInputCheckCurr(AEVK_D))
		{
			PlayerSystem::p1->SetFaceDirection(Entity::FaceDirection::RIGHT);
			return Entity::FaceDirection::RIGHT;
		}
		else
		{
			return Entity::FaceDirection::LAST;
		}
	}

	void Last_State()
	{
		bool moving = Is_Moving();
		bool sprinting = Is_Sprinting() && moving;

		if (sprinting)
		{
			PlayerSystem::p1->SetMoveState(Entity::MoveState::RUNNING);
			movementMuliplier = SPRINT_MULTIPLIER; 
		}										   
		else if (moving)						   
		{										   
			PlayerSystem::p1->SetMoveState(Entity::MoveState::WALKING);
			movementMuliplier = WALK_MULTIPLIER;   
		}										   
		else if (Is_Idle())						   
		{										   
			PlayerSystem::p1->SetMoveState(Entity::MoveState::IDLE);
		}										   
		else									   
		{										   
			PlayerSystem::p1->SetMoveState(Entity::MoveState::DEFAULT);
		}
	}

	void CalculateMovement(Entity::FaceDirection direction, float speed, float multiplier,
		float dt, float& moveX, float& moveY)
	{
		moveX = 0.0f;
		moveY = 0.0f;

		float movement = (speed * dt) * multiplier;

		switch (direction)
		{
		case Entity::FaceDirection::UP:    moveY = movement;  break;
		case Entity::FaceDirection::DOWN:  moveY = -movement; break;
		case Entity::FaceDirection::LEFT:  moveX = -movement; break;
		case Entity::FaceDirection::RIGHT: moveX = movement;  break;
		default:                                    break;
		}
	}

	// -------------------------------------------------------------------------
	//                    DEBUG: Mesh creation and drawing
	// -------------------------------------------------------------------------

	AEGfxVertexList* arrowmesh(unsigned int color)
	{
		AEGfxMeshStart();
		AEGfxTriAdd(
			0.0f, 0.5f, color, 0.5f, 0.0f,      // Top vertex
			-0.5f, -0.5f, color, 0.0f, 1.0f,    // Bottom-left vertex
			0.5f, -0.5f, color, 1.0f, 1.0f     // Bottom-right vertex
		);
		return AEGfxMeshEnd();
	}

	void drawMesh(AEGfxVertexList* mesh, float posX, float posY, float w, float h)
	{
		if (!mesh) return;

		AEMtx33 s = { 0 };  // Scale matrix
		AEMtx33 t = { 0 };  // Translation matrix
		AEMtx33 m = { 0 };  // Final transformation matrix

		AEMtx33Scale(&s, w, h);
		AEMtx33Trans(&t, posX, posY);
		AEMtx33Concat(&m, &t, &s);
		AEGfxSetTransform(m.m);

		AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
	}

	void drawArrowMesh(AEGfxVertexList* mesh, float posX, float posY,
		float w, float h, float angle)
	{
		if (!mesh) return;

		AEMtx33 s = { 0 };  // Scale matrix
		AEMtx33 r = { 0 };  // Rotation matrix
		AEMtx33 t = { 0 };  // Translation matrix
		AEMtx33 m = { 0 };  // Final transformation matrix

		AEMtx33Scale(&s, w, h);
		AEMtx33Trans(&t, posX, posY);

		// Build rotation matrix manually
		float rad = angle * DEGREES_TO_RADIANS;
		r.m[0][0] = cosf(rad); r.m[0][1] = -sinf(rad); r.m[0][2] = 0.0f;
		r.m[1][0] = sinf(rad); r.m[1][1] = cosf(rad);  r.m[1][2] = 0.0f;
		r.m[2][0] = 0.0f;      r.m[2][1] = 0.0f;       r.m[2][2] = 1.0f;

		// Concatenate: transform = translate * (rotate * scale)
		AEMtx33Concat(&m, &r, &s);
		AEMtx33Concat(&m, &t, &m);
		AEGfxSetTransform(m.m);

		AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
	}
}