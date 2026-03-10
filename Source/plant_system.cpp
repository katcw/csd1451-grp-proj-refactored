/*===========================================================================
@filename   plant_system.cpp
@brief      See plant_system.hpp. Logic ported from the old repo's
            plant_system.cpp; inventory_system dependency removed, interaction
            and growth merged into a single Update pass.
============================================================================*/
#include "plant_system.hpp"
#include "utilities.hpp"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include <cmath>
#include <cstdio>

namespace plantSystem
{
    //========================================================================
    // INTERNAL HELPERS
    //========================================================================

    static const float s_seedRates[(int)SeedType::COUNT] =
    {
        1.0f, // NONE
        1.0f, // ROSE
        1.1f, // TULIP
        0.8f, // SUNFLOWER  (slower; big flower)
        1.3f, // DAISY      (fastest; simple flower)
        0.7f, // LILY
        0.6f, // ORCHID     (slowest; exotic)
    };

    static PlantStage StageFromGrowth(float growth)
    {
        if (growth >= 1.f)    return STAGE_FULLY_GROWN;
        if (growth >= 0.66f)  return STAGE_GROWING;
        if (growth >= 0.33f)  return STAGE_SPROUT;
        return STAGE_SEED;
    }

    // Rectangular growth bar drawn below the pot.
    //   mesh    — unit square mesh shared with the plant system
    //   cx, cy  — world-space centre of the pot
    //   growth  — [0, 1]
    static void DrawGrowthBar(AEGfxVertexList* mesh, float cx, float cy, float growth)
    {
        const float BAR_W = 48.f;
        const float BAR_H =  8.f;
        const float OY    = -46.f;

        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

        float fr = growth < 0.5f ? growth * 2.f : 1.f;
        float fg = growth < 0.5f ? 1.f           : (1.f - growth) * 2.f;

        AEMtx33 sclMtx, trsMtx, mtx;

        // Background (dark)
        AEGfxSetColorToMultiply(0.2f, 0.2f, 0.2f, 1.f);
        AEMtx33Scale(&sclMtx, BAR_W, BAR_H);
        AEMtx33Trans(&trsMtx, cx, cy + OY);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);

        if (growth <= 0.f) return;

        // Fill (green → yellow → orange-red)
        AEGfxSetColorToMultiply(fr, fg, 0.f, 1.f);
        AEMtx33Scale(&sclMtx, BAR_W * growth, BAR_H);
        AEMtx33Trans(&trsMtx, cx - BAR_W * 0.5f + BAR_W * growth * 0.5f, cy + OY);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }

    //========================================================================
    // LIFECYCLE
    //========================================================================

    void PlantSystem_Load(State& s)
    {
        s.mesh          = BasicUtilities::createSquareMesh();
        s.potTex        = BasicUtilities::loadTexture("Assets/pot_empty.png");
        s.potPlantedTex = BasicUtilities::loadTexture("Assets/pot_planted.png");
        s.chestTex      = BasicUtilities::loadTexture("Assets/chest.png");

        // Placeholder textures until real plant-stage art is ready
        s.stageTex[STAGE_SEED]        = BasicUtilities::loadTexture("Assets/prototype_exit_button.png");
        s.stageTex[STAGE_SPROUT]      = BasicUtilities::loadTexture("Assets/prototype_exit_button.png");
        s.stageTex[STAGE_GROWING]     = BasicUtilities::loadTexture("Assets/prototype_play_button.png");
        s.stageTex[STAGE_FULLY_GROWN] = BasicUtilities::loadTexture("Assets/prototype_credits_button.png");
    }

    void PlantSystem_Init(State& s, const AEVec2* potPositions, int potCount)
    {
        s.potCount = (potCount > MAX_PLANTS) ? MAX_PLANTS : potCount;

        for (int i = 0; i < s.potCount; ++i)
        {
            s.plants[i]     = Plant{};
            s.plants[i].pos = potPositions[i];
        }

        s.can.water    = s.can.maxWater;
        s.can.drainRate = WATER_DRAIN_RATE;
    }

    //========================================================================
    // UPDATE  (single merged pass: positions → chest → plant → water → growth)
    //========================================================================
    void PlantSystem_Update(State& s, float dt,
                            AEVec2 playerPos, HeldState& held,
                            const AEVec2* potPositions, int potCount,
                            const ChestData* chests, int chestCount)
    {
        const int count = (potCount > MAX_PLANTS) ? MAX_PLANTS : potCount;

        //--------------------------------------------------------------------
        // Step 0: Sync pot world positions (allows easy repositioning)
        //--------------------------------------------------------------------
        for (int i = 0; i < count; ++i)
            s.plants[i].pos = potPositions[i];

        //--------------------------------------------------------------------
        // Step 1: E pressed + empty hands
        //   1a. Near a FULLY_GROWN active plant → harvest flower (priority)
        //   1b. Near a chest, if no harvest occurred  → pick up seed
        //--------------------------------------------------------------------
        if (AEInputCheckTriggered(AEVK_E) && held.type == HeldItem::NONE)
        {
            bool harvested = false;

            // 1a — harvest fully-grown plant
            for (int i = 0; i < count && !harvested; ++i)
            {
                if (!s.plants[i].active || s.plants[i].stage != STAGE_FULLY_GROWN) continue;
                float dx = s.plants[i].pos.x - playerPos.x;
                float dy = s.plants[i].pos.y - playerPos.y;
                if (std::sqrt(dx * dx + dy * dy) < INTERACT_RADIUS)
                {
                    held.flower     = static_cast<FlowerType>(s.plants[i].seedType);
                    held.type       = HeldItem::FLOWER;
                    AEVec2 savedPos = potPositions[i];
                    s.plants[i]     = Plant{};        // reset (clears active flag)
                    s.plants[i].pos = savedPos;       // restore world position
                    harvested       = true;
                }
            }

            // 1b — pick seed from chest only if no harvest occurred
            if (!harvested)
            {
                for (int c = 0; c < chestCount; ++c)
                {
                    float dx = chests[c].pos.x - playerPos.x;
                    float dy = chests[c].pos.y - playerPos.y;
                    if (std::sqrt(dx * dx + dy * dy) < INTERACT_RADIUS)
                    {
                        held.type = HeldItem::SEED;
                        held.seed = chests[c].seed;
                        break;
                    }
                }
            }
        }
        //--------------------------------------------------------------------
        // Step 2: E pressed + holding seed → plant in nearest empty pot
        //--------------------------------------------------------------------
        else if (AEInputCheckTriggered(AEVK_E) && held.type == HeldItem::SEED)
        {
            for (int i = 0; i < count; ++i)
            {
                if (s.plants[i].active) continue;

                float dx   = s.plants[i].pos.x - playerPos.x;
                float dy   = s.plants[i].pos.y - playerPos.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < INTERACT_RADIUS)
                {
                    s.plants[i].active     = true;
                    s.plants[i].seedType   = held.seed;
                    s.plants[i].growth     = 0.f;
                    s.plants[i].stage      = STAGE_SEED;
                    s.plants[i].isWatered  = false;
                    s.plants[i].waterTimer = 0.f;
                    held = HeldState{};
                    break;
                }
            }
        }

        //--------------------------------------------------------------------
        // Step 3: Q held + water remaining → water nearest active plant
        //--------------------------------------------------------------------
        if (AEInputCheckCurr(AEVK_Q) && s.can.water > 0.f)
        {
            int idx = PlantSystem_NearestPlant(s, playerPos);
            if (idx >= 0)
            {
                s.can.water -= s.can.drainRate * dt;
                if (s.can.water < 0.f) s.can.water = 0.f;

                s.plants[idx].isWatered  = true;
                s.plants[idx].waterTimer = 0.6f; // refresh; decays when Q released
            }
        }

        //--------------------------------------------------------------------
        // Step 4: R pressed → refill watering can
        //--------------------------------------------------------------------
        if (AEInputCheckTriggered(AEVK_R))
            s.can.water = s.can.maxWater;

        //--------------------------------------------------------------------
        // Step 5: Per-plant growth tick
        //--------------------------------------------------------------------
        for (int i = 0; i < count; ++i)
        {
            if (!s.plants[i].active) continue;
            Plant& p = s.plants[i];

            // Decay water timer
            if (p.waterTimer > 0.f)
            {
                p.waterTimer -= dt;
                if (p.waterTimer <= 0.f)
                {
                    p.waterTimer = 0.f;
                    p.isWatered  = false;
                }
            }

            if (p.growth >= 1.f) continue;

            float rate = BASE_GROWTH_RATE * s_seedRates[(int)p.seedType];
            if (p.isWatered) rate *= WATER_MULTIPLIER;

            p.growth += rate * dt;
            if (p.growth > 1.f) p.growth = 1.f;
            p.stage = StageFromGrowth(p.growth);
        }
    }

    //========================================================================
    // DRAW
    //========================================================================
    void PlantSystem_Draw(State const& s,
                          const ChestData* chests, int chestCount)
    {
        // ── Chests ──────────────────────────────────────────────────────────
        for (int c = 0; c < chestCount; ++c)
            BasicUtilities::drawUICard(s.mesh, s.chestTex,
                chests[c].pos.x, chests[c].pos.y, 64.f, 64.f);

        // ── Pots + plants ────────────────────────────────────────────────────
        for (int i = 0; i < s.potCount; ++i)
        {
            const Plant& p = s.plants[i];
            float cx = p.pos.x, cy = p.pos.y;

            // Pot base: planted texture when a seed is in, empty otherwise
            AEGfxTexture* potToDraw = p.active ? s.potPlantedTex : s.potTex;
            BasicUtilities::drawUICard(s.mesh, potToDraw, cx, cy, 64.f, 64.f);

            if (!p.active) continue;

            // Rectangular growth bar below the pot
            DrawGrowthBar(s.mesh, cx, cy, p.growth);
        }
    }

    //========================================================================
    // FREE / UNLOAD
    //========================================================================
    void PlantSystem_Free(State& s)
    {
        if (s.mesh)          { AEGfxMeshFree(s.mesh);               s.mesh          = nullptr; }
        if (s.potTex)        { AEGfxTextureUnload(s.potTex);        s.potTex        = nullptr; }
        if (s.potPlantedTex) { AEGfxTextureUnload(s.potPlantedTex); s.potPlantedTex = nullptr; }
        if (s.chestTex)      { AEGfxTextureUnload(s.chestTex);      s.chestTex      = nullptr; }

        // STAGE_SEED and STAGE_SPROUT share the same pointer — unload once
        for (int i = 0; i < 4; ++i)
        {
            if (i == (int)STAGE_SPROUT &&
                s.stageTex[STAGE_SPROUT] == s.stageTex[STAGE_SEED])
            {
                s.stageTex[STAGE_SPROUT] = nullptr;
                continue;
            }
            if (s.stageTex[i]) { AEGfxTextureUnload(s.stageTex[i]); s.stageTex[i] = nullptr; }
        }
    }

    void PlantSystem_Unload(State& s)
    {
        PlantSystem_Free(s);
        s.potCount = 0;
        for (int i = 0; i < MAX_PLANTS; ++i) s.plants[i] = Plant{};
        s.can = WateringCan{};
    }

    //========================================================================
    // HELPERS
    //========================================================================
    int PlantSystem_NearestPlant(State const& s, AEVec2 playerPos)
    {
        int   bestIdx  = -1;
        float bestDist = INTERACT_RADIUS;

        for (int i = 0; i < s.potCount; ++i)
        {
            if (!s.plants[i].active) continue;
            float dx   = s.plants[i].pos.x - playerPos.x;
            float dy   = s.plants[i].pos.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
        return bestIdx;
    }

} // namespace plantSystem
