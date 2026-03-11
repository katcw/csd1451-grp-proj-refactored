/*===========================================================================
@filename   customer.hpp
@brief      Generic customer AI system. A customer walks to a target position
            then idles and displays an order bubble above its head.

    Single-customer API (used by Tutorial):
        CustomerSystem_Load(state)
        CustomerSystem_Init(state, spawnPos, targetPos, order [, orderMod])
        CustomerSystem_Spawn(state)
        CustomerSystem_Update(state, dt)
        CustomerSystem_Draw(state, fontId)
        CustomerSystem_DrawUI(state, fontId)
        CustomerSystem_Free(state)
        CustomerSystem_Unload(state)
        CustomerSystem_Serve(state)

    Multi-customer pool API (used by Level 1):
        CustomerPool_Load(poolState)
        CustomerPool_Init(poolState, spawnPos, targets, targetCount)
        CustomerPool_Update(poolState, dt)
        CustomerPool_Draw(poolState, fontId)
        CustomerPool_DrawUI(poolState, fontId)
        CustomerPool_Free(poolState)
        CustomerPool_Unload(poolState)
        CustomerPool_TryServe(poolState, flowerType, flowerModifier, playerPos)
            → returns gold earned, 0 on mismatch, -1 if no customer in range

    Spawn and target positions are set via Init and may be tuned freely by the
    caller. Both APIs are self-contained and reusable across game states.
============================================================================*/
#pragma once

#include "AEEngine.h"
#include "entity.hpp"
#include "sprite.hpp"
#include "item_types.hpp"

// Customer patience, change value to modify customer's patience level
namespace CustomerSystem { constexpr float PATIENCE_MAX = 30.f; }

enum class CustomerState { ARRIVING, WAITING, LEAVING };

enum class ReceiveOrder { NO_ORDER, CORRECT_ORDER, WRONG_ORDER };

class Customer : public Entity::EntityBase
{
public:
    CustomerState          state              = CustomerState::ARRIVING;
    FlowerType             order              = FlowerType::NONE;
    FlowerModifier         orderModifier      = FlowerModifier::NONE;  // required modifier (NONE = plain)
	ReceiveOrder           receiveOrderResult = ReceiveOrder::NO_ORDER;  // result of player's attempt to serve this customer
    float                  stateTimer         = 0.f;
    Sprite::AnimationState animState          = {};
    float                  patience           = CustomerSystem::PATIENCE_MAX;
    bool                   patienceRanOut     = false;
    bool                   servedSuccessfully = false;
};

namespace CustomerSystem
{
    constexpr float SPEED       = 120.f;
    constexpr float ARRIVE_EPS  = 2.f;     // distance threshold to snap to target

    // Vertical offset of order bubble in world coordinates
    constexpr float BUBBLE_OFFSET_Y = 80.f;

    //========================================================================
    // Single-customer State (Tutorial)
    //========================================================================
    struct State
    {
        Customer           customer;
        AEVec2             spawnPos   = {};
        AEVec2             targetPos  = {};
        bool               active     = false;

        Sprite::SpriteData sprite;
        AEGfxVertexList*   spriteMesh = nullptr;
        AEGfxVertexList*   bubbleMesh = nullptr;
    };

    // Load — allocates GPU resources (textures, meshes). Call once in Load().
    void CustomerSystem_Load(State& s);

    // Init — configures spawn/target positions and desired flower order.
    //        Does NOT spawn the customer yet; call Spawn() when ready.
    void CustomerSystem_Init(State& s, AEVec2 spawnPos, AEVec2 targetPos,
                             FlowerType order,
                             FlowerModifier orderMod = FlowerModifier::NONE);

    // Spawn — places the customer at spawnPos and begins the ARRIVING walk.
    //         May be called repeatedly (e.g. for a new customer each round).
    void CustomerSystem_Spawn(State& s);

    // Update — moves the customer and advances animation. Pass frame dt.
    void CustomerSystem_Update(State& s, float dt);

    // Draw — draws the customer sprite only (no patience bar or order bubble).
    //        Call before PlayerSystem::Draw() so the player appears in front.
    void CustomerSystem_Draw(const State& s, s8 fontId);

    // DrawUI — draws the patience bar and order bubble only (no sprite).
    //          Call after PlayerSystem::Draw() so tooltips appear above the player.
    //          flowerIcons: optional texture array indexed by FlowerType (1–6).
    void CustomerSystem_DrawUI(const State& s, s8 fontId,
                               AEGfxTexture** flowerIcons = nullptr);

    // Free — releases mesh memory. Call in the state's Free().
    void CustomerSystem_Free(State& s);

    // Unload — releases texture memory. Call in the state's Unload().
    void CustomerSystem_Unload(State& s);

    // Serve — marks the customer as successfully served and begins their LEAVING walk.
    void CustomerSystem_Serve(State& s);

    // Returns true once the customer has reached the target and is WAITING.
    inline bool IsWaiting(const State& s)
    {
        return s.active && s.customer.state == CustomerState::WAITING;
    }

    //========================================================================
    // Multi-customer Pool (Level 1)
    //========================================================================

    constexpr int   POOL_MAX           = 5;     // [TUNE] max concurrent customers
    constexpr float SPAWN_INTERVAL_MIN = 8.f;   // [TUNE] min seconds between spawns
    constexpr float SPAWN_INTERVAL_MAX = 15.f;  // [TUNE] max seconds between spawns
    constexpr int   GOLD_REWARD_PLAIN    = 40;  // [TUNE] gold for correct plain flower
    constexpr int   GOLD_REWARD_MODIFIED = 65;  // [TUNE] gold for correct modified flower

    // Manages POOL_MAX customers sharing one loaded sprite sheet.
    struct PoolState
    {
        // Shared GPU resources — loaded once for all slots
        Sprite::SpriteData sprite;
        AEGfxVertexList*   spriteMesh = nullptr;
        AEGfxVertexList*   bubbleMesh = nullptr;

        // Per-customer instance data (no GPU resources)
        struct Slot
        {
            Customer customer;
            AEVec2   spawnPos    = {};
            AEVec2   targetPos   = {};
            bool     active      = false;
            int      targetIndex = -1;  // which entry in targetPositions[] is occupied
        };
        Slot slots[POOL_MAX];

        // Available target positions — pool tracks occupancy per-index
        static constexpr int MAX_TARGETS = POOL_MAX * 2;
        AEVec2 targetPositions[MAX_TARGETS];
        bool   targetOccupied[MAX_TARGETS] = {};
        int    targetCount = 0;

        AEVec2 spawnPoint  = {};
        float  spawnTimer  = 0.f;   // counts down; ≤ 0 → may spawn next customer
    };

    // Load — allocates shared GPU resources. Call once in the state's Load().
    void CustomerPool_Load(PoolState& p);

    // Init — sets spawn point, copies target positions, resets all slots.
    //        spawnTimer starts at 0 so the first customer spawns on the first Update.
    void CustomerPool_Init(PoolState& p, AEVec2 spawnPos,
                           const AEVec2* targets, int targetCount);

    // Update — ticks spawn timer, spawns new customers, and updates all active slots.
    void CustomerPool_Update(PoolState& p, float dt);

    // Draw — draws all active customer sprites (call before PlayerSystem::Draw).
    void CustomerPool_Draw(const PoolState& p, s8 fontId);

    // DrawUI — draws patience bars and order bubbles (call after PlayerSystem::Draw).
    //          flowerIcons: optional texture array indexed by FlowerType (1–6).
    void CustomerPool_DrawUI(const PoolState& p, s8 fontId,
                             AEGfxTexture** flowerIcons = nullptr);

    // Free — releases mesh memory. Call in the state's Free().
    void CustomerPool_Free(PoolState& p);

    // Unload — releases texture memory. Call in the state's Unload().
    void CustomerPool_Unload(PoolState& p);

    // TryServe — find nearest WAITING customer within INTERACT_RADIUS.
    //   Correct match  → serve, return GOLD_REWARD_PLAIN or GOLD_REWARD_MODIFIED.
    //   In-range wrong → return 0 (customer stays, no serve).
    //   None in range  → return -1.
    int CustomerPool_TryServe(PoolState& p, FlowerType type, FlowerModifier mod,
                              AEVec2 playerPos);
}
