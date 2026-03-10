/*===========================================================================
@filename   plant_system.hpp
@brief      Potting and watering system. Player picks a seed from a chest,
            plants it in an empty pot (E key), and the plant grows over time.
            Holding Q near a pot boosts growth (watering). Generic system:

    PlantSystem_Load(state)
    PlantSystem_Init(state, potPositions, potCount)
    PlantSystem_Update(state, dt, playerPos, held, potPositions, potCount, chests, chestCount)
    PlantSystem_Draw(state, chests, chestCount)
    PlantSystem_Free(state)
    PlantSystem_Unload(state)
============================================================================*/
#pragma once

#include "AEEngine.h"
#include "item_types.hpp"

namespace plantSystem
{
    // Constants
    constexpr int   MAX_PLANTS       = 6;
    constexpr float INTERACT_RADIUS  = 100.f;  // [TUNE] world-unit reach for E/Q interactions
    constexpr float BASE_GROWTH_RATE = 0.04f;  // Progress per second
    constexpr float WATER_MULTIPLIER = 3.0f;   // growth multiplier while watered
    constexpr float WATER_MAX        = 10.f;   // seconds of water in the can
    constexpr float WATER_DRAIN_RATE = 1.f;    // units/sec while Q is held

    //========================================================================
    // ENUMS & STRUCTS
    //========================================================================
    enum PlantStage { STAGE_SEED, STAGE_SPROUT, STAGE_GROWING, STAGE_FULLY_GROWN };

    struct Plant
    {
        bool       active      = false;
        AEVec2     pos         = {};
        SeedType   seedType    = SeedType::NONE;
        float      growth      = 0.f;     // [0.0, 1.0]
        PlantStage stage       = STAGE_SEED;
        bool       isWatered   = false;
        float      waterTimer  = 0.f;     // decays when Q released; 0 → not watered
    };

    struct WateringCan
    {
        float water    = WATER_MAX;
        float maxWater = WATER_MAX;
        float drainRate = WATER_DRAIN_RATE;
    };

    // Defined in the game state; tells the system where each chest sits
    struct ChestData
    {
        AEVec2   pos;
        SeedType seed;
    };

    struct State
    {
        Plant           plants[MAX_PLANTS];
        WateringCan     can;
        int             potCount  = 0;

        // GPU resources owned by State
        AEGfxTexture*    potTex        = nullptr;  // empty pot
        AEGfxTexture*    potPlantedTex = nullptr;  // pot with seed planted
        AEGfxTexture*    chestTex      = nullptr;
        AEGfxTexture*    stageTex[4] = {};          // zero-initialised to nullptr; set in PlantSystem_Load
        AEGfxVertexList* mesh          = nullptr;
    };

    //========================================================================
    // LIFECYCLE
    //========================================================================
    void PlantSystem_Load  (State& s);
    void PlantSystem_Init  (State& s, const AEVec2* potPositions, int potCount);
    void PlantSystem_Update(State& s, float dt,
                            AEVec2 playerPos, HeldState& held,
                            const AEVec2* potPositions, int potCount,
                            const ChestData* chests, int chestCount);
    void PlantSystem_Draw  (State const& s,
                            const ChestData* chests, int chestCount);
    void PlantSystem_Free  (State& s);
    void PlantSystem_Unload(State& s);

    //========================================================================
    // HELPERS (exposed for the future harvest system)
    //========================================================================
    // Returns the index of the nearest active plant within INTERACT_RADIUS,
    // or -1 if none qualify.
    int PlantSystem_NearestPlant(State const& s, AEVec2 playerPos);
}
