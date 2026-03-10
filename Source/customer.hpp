/*===========================================================================
@filename   customer.hpp
@brief      Generic customer AI system. A customer walks to a target position
            then idles and displays an order bubble above its head.

    CustomerSystem_Load(state)
    CustomerSystem_Init(state, spawnPos, targetPos, order)
    CustomerSystem_Spawn(state)
    CustomerSystem_Update(state, dt)
    CustomerSystem_Draw(state, fontId)
    CustomerSystem_Free(state)
    CustomerSystem_Unload(state)

    Spawn and target positions are set via Init and may be tuned freely by the
    caller. The system is self-contained and reusable across any game state.
============================================================================*/
#pragma once

#include "AEEngine.h"
#include "entity.hpp"
#include "sprite.hpp"
#include "item_types.hpp"

// Customer patience, change value to modify customer's patience level
namespace CustomerSystem { constexpr float PATIENCE_MAX = 30.f; } 

enum class CustomerState { ARRIVING, WAITING, LEAVING };

class Customer : public Entity::EntityBase
{
public:
    CustomerState          state                = CustomerState::ARRIVING;
    FlowerType             order                = FlowerType::NONE;
    float                  stateTimer           = 0.f;
    Sprite::AnimationState animState            = {};
    float                  patience             = CustomerSystem::PATIENCE_MAX;
    bool                   patienceRanOut       = false;
    bool                   servedSuccessfully   = false;
};

namespace CustomerSystem
{
    constexpr float SPEED           = 120.f;   
    constexpr float ARRIVE_EPS      = 2.f;     // Distance threshold to snap to target

    // Vertical offset of order bubble in world coordinates 
    constexpr float BUBBLE_OFFSET_Y  = 80.f;

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
    void CustomerSystem_Init(State& s, AEVec2 spawnPos, AEVec2 targetPos, FlowerType order);

    // Spawn — places the customer at spawnPos and begins the ARRIVING walk.
    //         May be called repeatedly (e.g. for a new customer each round).
    void CustomerSystem_Spawn(State& s);

    // Update — moves the customer and advances animation. Pass frame dt.
    void CustomerSystem_Update(State& s, float dt);

    // Draw — draws the customer sprite and order bubble.
    void CustomerSystem_Draw(const State& s, s8 fontId);

    // Free — releases mesh memory. Call in the state's Free().
    void CustomerSystem_Free(State& s);

    // Unload — releases texture memory. Call in the state's Unload().
    void CustomerSystem_Unload(State& s);

    // Serve — marks the customer as successfully served and begins their LEAVING walk.
    //         Call from tutorial when the player delivers the correct flower.
    void CustomerSystem_Serve(State& s);

    // Returns true once the customer has reached the target and is WAITING.
    inline bool IsWaiting(const State& s)
    {
        return s.active && s.customer.state == CustomerState::WAITING;
    }
}
