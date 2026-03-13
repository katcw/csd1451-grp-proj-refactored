/*===========================================================================
@filename   player.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Implements the PlayerSystem: tile-based WASD movement with a
            hold threshold, click-to-path A* navigation, Shift sprint, and
            MC sprite animation driven by facing direction and movement state.

            Movement contract:
              - Update() interpolates position toward nextCoordinates each frame.
              - Entity::HandleCollisions() validates the intended next tile.
              - Entity::UpdateTransformations() finalises index and facing.
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

static const bool debug = true;

// =============================================================================
//                      Constants
// =============================================================================

using json = nlohmann::json;
namespace Sprite = BasicUtilities::Sprite;

static constexpr float SPRINT_MULTIPLIER = 1.5f;
static constexpr float WALK_MULTIPLIER = 1.0f;

float movementMultiplier = 1.0f;

// =============================================================================
//                           Debug meshes
// =============================================================================

AEGfxVertexList* pathLineMesh = nullptr;

// =============================================================================
//                  Anonymous namespace: helper declarations
// =============================================================================
namespace
{
    bool Is_Moving();
    bool Is_Sprinting();
    Entity::FaceDirection GetWASDDirection();
    void UpdateMoveState();
    void drawDebugLine(AEGfxVertexList* mesh, const AEVec2& from, const AEVec2& to, float thickness);

    float TileProgress();

    /// [TEMPORARY] Computes velocity and displacement for free-form WASD movement.
    void CalculateMovement(Entity::FaceDirection direction, float speed, float multiplier,
        float dt, float& moveX, float& moveY);
}

//============================================================================
// DATA
//============================================================================
namespace
{
    Sprite::SpriteData mcSprite;
    Sprite::AnimationState animState;
    AEGfxVertexList* spriteMesh = nullptr;

    unsigned int currentTexSlot = 0;
    unsigned int currentTagIndex = 1;

    unsigned int prevTexSlot = 0;
    unsigned int prevTagIndex = 1;
}

namespace PlayerSystem
{
    Player* p1 = nullptr;

    float playerScaleX = 100.0f,
          playerScaleY = 100.f;

    void Load()
    {
        mcSprite.count = 0;
        Sprite::LoadEntry4("MC", mcSprite);

        if (debug) std::cout << "[DEBUG] playerSpriteData.count = " << mcSprite.count << "\n";

        if (debug)
        {
            pathLineMesh = BasicUtilities::createSquareMesh(1.0f, 1.0f, 0x88FFFF00);
        }
    }

    //==========================================================================
    // Init
    //==========================================================================
    void Init(AEVec2 startPos)
    {
        p1 = new Player(
            0u,
            Entity::EntityType::PLAYER,
            startPos,
            startPos,
            Entity::Index{ 0, 0 },
            Entity::Index{ 0, 0 },
            200.0f,
            Entity::FaceDirection::DOWN,
            Entity::MoveState::DEFAULT,
            true,
            false,
            false,
            HeldState{}
        );

        Entity::AddEntityAt(p1, 0);

        p1->UpdateIndex();
        p1->SetNextCoordinates(startPos);

        spriteMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);

        prevTexSlot = 0;
        prevTagIndex = 1;
        animState = Sprite::AnimationState{};
    }

    //==========================================================================
    // Update
    //==========================================================================
    void Update(float dt)
    {
        if (!p1) return;

        UpdateMoveState();

        Entity::FaceDirection wasd = GetWASDDirection();

        // =================================================================
        // [TEMPORARY] Free-form WASD movement — DELETE when tile movement
        //             is re-enabled and prop collisions are fixed.
        // =================================================================
        if (wasd != Entity::FaceDirection::LAST)
        {
            if (!p1->IsPathEmpty())
            {
                p1->ClearPath();
                if (debug) std::cout << "[Path] Cancelled by keyboard input\n";
            }

            p1->SetFaceDirection(wasd);

            float moveX = 0.f, moveY = 0.f;
            CalculateMovement(wasd, p1->GetSpeed(), movementMultiplier, dt, moveX, moveY);

            AEVec2 curPos = p1->GetCoordinates();
            AEVec2 tentative = { curPos.x + moveX, curPos.y + moveY };

            // Set next coordinates — HandleCollisions will validate per-axis
            p1->SetNextCoordinates(tentative);
            p1->SetCoordinates(tentative);
        }
        // =================================================================
        // END [TEMPORARY]
        // =================================================================

        /*
        // =================================================================
        // TILE-BASED WASD MOVEMENT — uncomment when prop collisions are fixed
        // =================================================================
        bool isStationary = p1->IsCoordsSame();

        if (wasd != Entity::FaceDirection::LAST)
        {
            if (!p1->IsPathEmpty())
            {
                p1->ClearPath();
                if (debug) std::cout << "[Path] Cancelled by keyboard input\n";
            }

            p1->SetFaceDirection(wasd);

            if (isStationary)
            {
                if (wasd == p1->inputDirection)
                {
                    p1->inputHoldTimer += dt;
                }
                else
                {
                    p1->inputDirection = wasd;
                    p1->inputHoldTimer = dt;
                }

                if (p1->inputHoldTimer >= INPUT_HOLD_THRESHOLD)
                {
                    Entity::Index targetIdx = Entity::AdjacentIndex(p1->GetCurrentIndex(), wasd);

                    if (Entity::IsTilePassable(targetIdx))
                    {
                        p1->SetNextIndex(targetIdx);
                        p1->SetNextCoordinates(Entity::IndexToWorld(targetIdx));
                        p1->SetMoveState(Entity::MoveState::WALKING);
                    }

                    p1->inputHoldTimer = 0.f;
                }
            }
            else
            {
                if (wasd == p1->inputDirection && TileProgress() >= 0.5f)
                {
                    p1->inputHoldTimer = INPUT_HOLD_THRESHOLD;
                }
                else
                {
                    p1->inputDirection = wasd;
                    p1->inputHoldTimer = 0.f;
                }
            }
        }
        else
        {
            p1->inputHoldTimer = 0.f;
            p1->inputDirection = Entity::FaceDirection::LAST;
        }

        // Click-to-path
        if (AEInputCheckTriggered(AEVK_LBUTTON) && wasd == Entity::FaceDirection::LAST)
        {
            int mouseX = 0, mouseY = 0;
            AEInputGetCursorPosition(&mouseX, &mouseY);

            float mouseWorldX = 0.f, mouseWorldY = 0.f;
            BasicUtilities::screenCoordsToWorldCoords(mouseX, mouseY, mouseWorldX, mouseWorldY);

            Entity::Index clickedIdx;
            clickedIdx.x = static_cast<int>(std::floor((mouseWorldX - (-800.f)) / 64.f));
            clickedIdx.y = static_cast<int>(std::floor((mouseWorldY - (-448.f)) / 64.f));

            if (!p1->IsCoordsSame())
            {
                float progr = TileProgress();
                if (progr >= 0.5f)
                {
                    AEVec2 snapTo = p1->GetNextCoordinates();
                    p1->SetCoordinates(snapTo);
                    p1->SetNextCoordinates(snapTo);
                    p1->UpdateIndex();
                }
                else
                {
                    AEVec2 curCentre = Entity::IndexToWorld(p1->GetCurrentIndex());
                    p1->SetCoordinates(curCentre);
                    p1->SetNextCoordinates(curCentre);
                    p1->UpdateIndex();
                }
            }

            p1->UpdateIndex();

            if (!(clickedIdx.x == p1->GetCurrentIndex().x && clickedIdx.y == p1->GetCurrentIndex().y))
            {
                p1->SetNextIndex(clickedIdx);
                p1->SetNextCoordinates(Entity::IndexToWorld(clickedIdx));

                bool pathOk = p1->SetPath();
                if (debug)
                {
                    const char* result = pathOk ? "[Path] Path" : "[Path] No path";
                    std::cout << result << " from (" << p1->GetCurrentIndex().x << ','
                              << p1->GetCurrentIndex().y << ") to ("
                              << clickedIdx.x << ',' << clickedIdx.y << ")\n";
                }
            }
        }

        // Follow A* path
        if (!p1->IsPathEmpty())
        {
            p1->FollowPath(dt);
        }

        // Smooth interpolation for WASD tile movement
        if (!p1->IsCoordsSame() && p1->IsPathEmpty())
        {
            AEVec2 cur = p1->GetCoordinates();
            AEVec2 target = p1->GetNextCoordinates();
            AEVec2 toTarget = { target.x - cur.x, target.y - cur.y };
            float dist = std::sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

            float effectiveSpeed = p1->GetSpeed() * movementMultiplier;

            if (dist <= 1e-3f || effectiveSpeed * dt >= dist)
            {
                p1->SetCoordinates(target);
                p1->SetNextCoordinates(target);
                p1->UpdateIndex();
                p1->SetVelocity({ 0.f, 0.f });
            }
            else
            {
                AEVec2 dir = { toTarget.x / dist, toTarget.y / dist };
                AEVec2 newPos = { cur.x + dir.x * effectiveSpeed * dt,
                                  cur.y + dir.y * effectiveSpeed * dt };
                p1->SetCoordinates(newPos);
                p1->SetVelocity({ dir.x * effectiveSpeed, dir.y * effectiveSpeed });
            }
        }
        // =================================================================
        // END TILE-BASED WASD MOVEMENT
        // =================================================================
        */

        // ── ANIMATION ─────────────────────────────────────────────────────
        switch (p1->GetLastDirection())
        {
        case Entity::FaceDirection::LEFT:  currentTagIndex = 0; break;
        case Entity::FaceDirection::RIGHT: currentTagIndex = 1; break;
        case Entity::FaceDirection::UP:    currentTagIndex = 2; break;
        case Entity::FaceDirection::DOWN:  currentTagIndex = 3; break;
        default: break;
        }

        currentTexSlot = (p1->GetMoveState() == Entity::MoveState::WALKING ||
            p1->GetMoveState() == Entity::MoveState::RUNNING) ? 1 : 0;
        if (p1->held.type != HeldItem::NONE) currentTexSlot += 2;

        if (currentTexSlot != prevTexSlot || currentTagIndex != prevTagIndex)
        {
            animState = Sprite::AnimationState{};
            prevTexSlot = currentTexSlot;
            prevTagIndex = currentTagIndex;
        }

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

        if (debug && pathLineMesh && !p1->IsPathEmpty())
        {
            const auto& path = p1->GetPath();
            AEVec2 prev = p1->GetCoordinates();

            for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i)
            {
                const Entity::Node& n = path[static_cast<size_t>(i)];
                AEVec2 center = Entity::IndexToWorld(n.index);
                drawDebugLine(pathLineMesh, prev, center, 6.0f);
                prev = center;
            }
        }
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

        if (debug && pathLineMesh)
        {
            AEGfxMeshFree(pathLineMesh);
            pathLineMesh = nullptr;
        }

        currentTexSlot  = 0;
        currentTagIndex = 1;
        prevTexSlot = 0;
        prevTagIndex = 1;
        animState = Sprite::AnimationState{};
    }

    void Unload()
    {
        p1 = nullptr;
        mcSprite.Free();
    }

    // =========================================================================
    //                          Player class definitions
    // =========================================================================

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
            id_, type, coord, nextCoord, currentIdx, nextIdx,
            spd, dir, st, act, AI, hold)
    {
        this->held = holding;
    }

    Player::~Player() {}

} // namespace PlayerSystem


// =============================================================================
//              Anonymous namespace: helper implementations
// =============================================================================
namespace
{
    bool Is_Moving()
    {
        return (AEInputCheckCurr(AEVK_W) || AEInputCheckCurr(AEVK_S) ||
            AEInputCheckCurr(AEVK_A) || AEInputCheckCurr(AEVK_D));
    }

    bool Is_Sprinting()
    {
        return (AEInputCheckCurr(AEVK_LSHIFT) || AEInputCheckCurr(AEVK_RSHIFT));
    }

    Entity::FaceDirection GetWASDDirection()
    {
        if (AEInputCheckCurr(AEVK_W))      return Entity::FaceDirection::UP;
        else if (AEInputCheckCurr(AEVK_S))  return Entity::FaceDirection::DOWN;
        else if (AEInputCheckCurr(AEVK_A))  return Entity::FaceDirection::LEFT;
        else if (AEInputCheckCurr(AEVK_D))  return Entity::FaceDirection::RIGHT;
        return Entity::FaceDirection::LAST;
    }

    void UpdateMoveState()
    {
        if (!PlayerSystem::p1) return;

        bool moving = Is_Moving();
        bool sprinting = Is_Sprinting() && moving;
        bool midTile = !PlayerSystem::p1->IsCoordsSame();

        if (sprinting)
        {
            PlayerSystem::p1->SetMoveState(Entity::MoveState::RUNNING);
            movementMultiplier = SPRINT_MULTIPLIER;
        }
        else if (moving || midTile || !PlayerSystem::p1->IsPathEmpty())
        {
            PlayerSystem::p1->SetMoveState(Entity::MoveState::WALKING);
            movementMultiplier = WALK_MULTIPLIER;
        }
        else
        {
            PlayerSystem::p1->SetMoveState(Entity::MoveState::DEFAULT);
        }
    }

    float TileProgress()
    {
        if (!PlayerSystem::p1) return 0.f;

        AEVec2 cur     = PlayerSystem::p1->GetCoordinates();
        AEVec2 target  = PlayerSystem::p1->GetNextCoordinates();
        AEVec2 origin  = Entity::IndexToWorld(PlayerSystem::p1->GetCurrentIndex());

        float totalDx = target.x - origin.x;
        float totalDy = target.y - origin.y;
        float totalDist = std::sqrtf(totalDx * totalDx + totalDy * totalDy);

        if (totalDist < 1e-4f) return 1.f;

        float coveredDx = cur.x - origin.x;
        float coveredDy = cur.y - origin.y;
        float coveredDist = std::sqrtf(coveredDx * coveredDx + coveredDy * coveredDy);

        return coveredDist / totalDist;
    }

    /// [TEMPORARY] Free-form WASD movement calculation — delete when tile movement returns.
    void CalculateMovement(Entity::FaceDirection direction, float speed, float multiplier,
        float dt, float& moveX, float& moveY)
    {
        moveX = 0.0f;
        moveY = 0.0f;

        const float effectiveSpeed = speed * multiplier;
        float velX = 0.0f;
        float velY = 0.0f;

        switch (direction)
        {
        case Entity::FaceDirection::UP:    velY =  effectiveSpeed; break;
        case Entity::FaceDirection::DOWN:  velY = -effectiveSpeed; break;
        case Entity::FaceDirection::LEFT:  velX = -effectiveSpeed; break;
        case Entity::FaceDirection::RIGHT: velX =  effectiveSpeed; break;
        default: break;
        }

        if (PlayerSystem::p1)
            PlayerSystem::p1->SetVelocity(AEVec2{ velX, velY });

        moveX = velX * dt;
        moveY = velY * dt;
    }

    void drawDebugLine(AEGfxVertexList* mesh, const AEVec2& from, const AEVec2& to, float thickness)
    {
        if (!mesh) return;

        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float length = sqrtf(dx * dx + dy * dy);
        if (length < 1e-4f) return;

        float angle = atan2f(dy, dx);
        float midX = 0.5f * (from.x + to.x);
        float midY = 0.5f * (from.y + to.y);

        AEMtx33 s = { 0 }, r = { 0 }, t = { 0 }, tmp = { 0 }, m = { 0 };
        AEMtx33Scale(&s, length, thickness);
        AEMtx33Rot(&r, angle);
        AEMtx33Trans(&t, midX, midY);
        AEMtx33Concat(&tmp, &r, &s);
        AEMtx33Concat(&m, &t, &tmp);
        AEGfxSetTransform(m.m);

        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetColorToMultiply(1.0f, 1.0f, 0.0f, 1.0f);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }
}