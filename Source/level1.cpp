/*===========================================================================
@filename   level1.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief      Level 1 game state. Begins with a narrative text sequence
            (black screen, fade-in/hold/fade-out per entry) followed by a
            smooth background transition to beige, then a camera pan-up into
            the playable floor world.

            Gameplay loop:
              - Grow flowers in pots, apply modifiers at rune/poison tables.
              - Serve arriving customers their ordered (and optionally
                modified) flowers before their patience expires.
              - Earn GOLD_WIN_TARGET gold before the day ends (18:00) to win.
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <cstdio>
#include <cmath>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "level1.hpp"
#include "game_state_manager.hpp"
#include "game_state_list.hpp"
#include "utilities.hpp"
#include "collision.hpp"
#include "player.hpp"
#include "time_of_day.hpp"
#include "gold.hpp"
#include "item_types.hpp"
#include "plant_system.hpp"
#include "particle_system.hpp"
#include "tables.hpp"
#include "customer.hpp"
#include "debug.hpp"
#include "entity.hpp"
#include "merchant.hpp"

namespace
{
    //========================================================================
    // Assets
    //========================================================================
    AEGfxTexture* uiCardTexture = nullptr;
    AEGfxVertexList* squareMesh = nullptr;
    s8               fontId = -1;

    //========================================================================
    // Level stage: narrative text intro → gameplay
    //========================================================================
    enum Level1Stage { STAGE_TEXT_SEQUENCE, STAGE_GAMEPLAY };
    Level1Stage currentStage = STAGE_TEXT_SEQUENCE;

    // Text sequence phases
    enum Level1Phase { FADE_IN, HOLD, FADE_OUT, BG_TRANSITION, COMPLETE };
    Level1Phase currentPhase = FADE_IN;

    float phaseTimer = 0.f;
    float alpha = 0.f;
    int   currentTextIndex = 0;

    const float FADE_IN_DURATION = 1.5f;
    const float HOLD_DURATION = 2.5f;
    const float FADE_OUT_DURATION = 1.0f;
    const float BG_TRANSITION_DURATION = 1.5f;

    float red = 0.f, green = 0.f, blue = 0.f;
    float backgroundTimer = 0.f;

    const float targetRed = 0.85f;
    const float targetGreen = 0.84f;
    const float targetBlue = 0.80f;

    // Text sequence data
    struct TextEntry { const char* text; float x, y, scale, r, g, b; };

    // [TUNE] Level 1 intro text — edit strings here
    const TextEntry textSequence[] =
    {
        { "day one", 0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
        { "your shop opens today, make prasanna proud",  0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
    };
    const int TEXT_COUNT = static_cast<int>(sizeof(textSequence) / sizeof(textSequence[0]));

    //========================================================================
    // Gameplay map / floor assets
    //========================================================================
    AEGfxTexture* floorOneTex = nullptr;
    AEGfxTexture* floorTwoTex = nullptr;
    AEGfxVertexList* tileMesh = nullptr;
    Collision::Map   collisionMap;

    //========================================================================
    // Prop state — loaded from level1_map_data.txt
    //   ID 2 = player spawn
    //   ID 3 = pot
    //   ID 4 = chest
    //   ID 5 = rune table
    //   ID 6 = poison table
    //========================================================================

    // Chest seeds — left-to-right reading order of the 6 chests in the map
    static const SeedType s_chestSeeds[] = {
        SeedType::ROSE, SeedType::TULIP, SeedType::SUNFLOWER,
        SeedType::DAISY, SeedType::LILY, SeedType::ORCHID
    };

    constexpr int MAX_MAP_CHESTS = 8;   // [TUNE] must be ≥ chest count in map

    AEVec2                  potPositions[plantSystem::MAX_PLANTS];
    int                     potCount = 0;
    plantSystem::ChestData  chests[MAX_MAP_CHESTS];
    int                     chestCount = 0;

    float chestTooltipT[MAX_MAP_CHESTS]{};           // 0-1 ease-in/out per chest
    float heldTooltipT = 0.f;                  // 0-1 ease-in/out for held-item bar
    float customerTooltipT[CustomerSystem::POOL_MAX]{}; // 0-1 ease-in/out per customer slot

    plantSystem::State      plantState;
    ParticleSystem::State   particleState;

    // Table positions (loaded from map IDs 5 and 6)
    AEVec2                  runeTablePositions[TableSystem::MAX_TABLES];
    int                     runeTableCount = 0;
    AEVec2                  poisonTablePositions[TableSystem::MAX_TABLES];
    int                     poisonTableCount = 0;
    TableSystem::State      tableState;

    //========================================================================
    // Customer pool
    //========================================================================
    CustomerSystem::PoolState customerPool;

    // Spawn point
    static const AEVec2 CUSTOMER_SPAWN_POS = { 0.0f, -225.f };

    // Target positions (y ≤ 0; well below map centre to stay clear of props)
    static const AEVec2 CUSTOMER_TARGETS[] = {
        { -384.f,  -96.f }, { -256.f,  -96.f }, { -128.f,  -96.f },
        {   64.f,  -96.f }, {  320.f,  -96.f },
        { -350.f, -200.f }, {  -80.f, -200.f }, {  350.f, -200.f },
    };
    constexpr int CUSTOMER_TARGET_COUNT =
        static_cast<int>(sizeof(CUSTOMER_TARGETS) / sizeof(CUSTOMER_TARGETS[0]));

    //========================================================================
    // Camera pan
    //========================================================================
    bool        cameraReady = false;
    float       camPanTimer = 0.f;
    const float CAM_PAN_DURATION = 2.0f;
    const float CAM_START_Y = -750.f;
    const float CAM_END_Y = 0.f;
    float       currentCamY = CAM_START_Y;

    //========================================================================
    // Info panel assets
    //========================================================================
    AEGfxTexture* infoPanelTex = nullptr;
    AEGfxTexture* goldIconTex = nullptr;
    AEGfxTexture* clockIconTex = nullptr;

    const AEVec2 PANEL_POS = { 640.f, 400.f };
    const float  PANEL_W = 300.f;
    const float  PANEL_H = 80.f;
    const float  ICON_SIZE = 30.f;

    const AEVec2 GOLD_ICON_OFF = { -100.f, 0.f };
    const AEVec2 GOLD_TEXT_OFF = { -65.f, 0.f };
    const AEVec2 CLOCK_ICON_OFF = { 10.f, 0.f };
    const AEVec2 CLOCK_TEXT_OFF = { 75.f, 0.f };

    //========================================================================
    // HUD icon textures — flower and seed pack, indexed by enum cast to int
    //========================================================================
    AEGfxTexture* flowerIconTex[7] = {};  // [FlowerType 0-6]: 0=unused, 1-6=icons
    AEGfxTexture* seedPackTex[7] = {};  // [SeedType   0-6]: 0=unused, 1-6=icons

    //========================================================================
    // Win / lose state
    //========================================================================
    enum class Level1End { PLAYING, WIN, LOSE };
    Level1End endState = Level1End::PLAYING;
    float     fadeAlpha = 0.f;

    constexpr int GOLD_WIN_TARGET = 40;  // [TUNE] gold required to win

    //========================================================================
    // Merchant post-win state
    //========================================================================
    Merchant merchant;
    bool merchantStarted = false;

    //========================================================================
    // Collision extents
    //========================================================================
    constexpr float PLAYER_HW = PlayerSystem::HALF_W;
    constexpr float PLAYER_HH = PlayerSystem::HALF_H;
    constexpr float PROP_COLL_HW = 32.f;   // Half of sprite's 64 px
    constexpr float PROP_COLL_HH = 32.f;

    //========================================================================
    // Local helpers — display names for seed and flower types
    //========================================================================
    const char* SeedName(SeedType st)
    {
        switch (st)
        {
        case SeedType::ROSE:      return "ROSE";
        case SeedType::TULIP:     return "TULIP";
        case SeedType::SUNFLOWER: return "SUNFLOWER";
        case SeedType::DAISY:     return "DAISY";
        case SeedType::LILY:      return "LILY";
        case SeedType::ORCHID:    return "ORCHID";
        default:                  return "???";
        }
    }

    const char* FlowerName(FlowerType ft)
    {
        switch (ft)
        {
        case FlowerType::ROSE:      return "ROSE";
        case FlowerType::TULIP:     return "TULIP";
        case FlowerType::SUNFLOWER: return "SUNFLOWER";
        case FlowerType::DAISY:     return "DAISY";
        case FlowerType::LILY:      return "LILY";
        case FlowerType::ORCHID:    return "ORCHID";
        default:                    return "???";
        }
    }

} // anonymous namespace

//============================================================================
// LIFECYCLE
//============================================================================

void Level1_Load()
{
    uiCardTexture = BasicUtilities::loadTexture("Assets/ui_card.png");
    fontId = AEGfxCreateFont("Assets/PixelifySans-Regular.ttf", 36);
    if (fontId == -1)
        std::cout << "ERROR! COULD NOT CREATE FONT FOR LEVEL 1\n";

    floorOneTex = BasicUtilities::loadTexture("Assets/floor_one.png");
    floorTwoTex = BasicUtilities::loadTexture("Assets/floor_two.png");

    AEVec2 origin = { -800.f, -448.f };
    Collision::Map_Load("Assets/level1_map_data.txt", collisionMap, 64.f, origin);

    plantSystem::PlantSystem_Load(plantState);
    ParticleSystem::Load(particleState);
    TableSystem::TableSystem_Load(tableState);
    MerchantSystem::Load();

    // Entity load
    Entity::Load();

    // Customer pool — shared GPU resources for all concurrent customers
    CustomerSystem::CustomerPool_Load(customerPool);

    infoPanelTex = BasicUtilities::loadTexture("Assets/info_panel.png");
    goldIconTex = BasicUtilities::loadTexture("Assets/gold_icon.png");
    clockIconTex = BasicUtilities::loadTexture("Assets/clock_icon.png");

    // HUD icon textures — flower icon and seed pack icon for each flower type
    for (int i = 1; i <= 6; ++i)
    {
        char path[64];
        sprintf_s(path, sizeof(path), "Assets/flower-assets/flower_icon_%d.png", i);
        flowerIconTex[i] = BasicUtilities::loadTexture(path);
        sprintf_s(path, sizeof(path), "Assets/flower-assets/seed_pack_%d.png", i);
        seedPackTex[i] = BasicUtilities::loadTexture(path);
    }
}

void Level1_Initialise()
{
    squareMesh = BasicUtilities::createSquareMesh();
    tileMesh = BasicUtilities::createSquareMesh();
    currentStage = STAGE_TEXT_SEQUENCE;
    currentPhase = FADE_IN;
    currentTextIndex = 0;
    phaseTimer = 0.f;
    alpha = 0.f;
    red = green = blue = 0.f;
    backgroundTimer = 0.f;

    // Camera pan
    cameraReady = false;
    camPanTimer = 0.f;
    currentCamY = CAM_START_Y;
    AEGfxSetCamPosition(0.f, CAM_START_Y);

    // Player spawn from map (ID 2)
    AEVec2 spawnBuf[1];
    if (Collision::Map_GetCentres(collisionMap, 2, spawnBuf, 1) > 0)
        PlayerSystem::Init(spawnBuf[0]);
    else
        PlayerSystem::Init({ 0.f, 0.f });

	// Entity system init
	Entity::Init();

    // Pots (ID 3)
    potCount = Collision::Map_GetCentres(collisionMap, 3,
        potPositions, plantSystem::MAX_PLANTS);

    // Chests (ID 4) + seed assignment (left-to-right reading order)
    AEVec2 chestPosBuf[MAX_MAP_CHESTS];
    chestCount = Collision::Map_GetCentres(collisionMap, 4, chestPosBuf, MAX_MAP_CHESTS);
    for (int i = 0; i < chestCount; ++i)
    {
        chests[i].pos = chestPosBuf[i];
        chests[i].seed = (i < (int)(sizeof(s_chestSeeds) / sizeof(*s_chestSeeds)))
            ? s_chestSeeds[i] : SeedType::ROSE;
    }

    for (int i = 0; i < MAX_MAP_CHESTS; ++i)
        chestTooltipT[i] = 0.f;
    heldTooltipT = 0.f;
    for (int i = 0; i < CustomerSystem::POOL_MAX; ++i)
        customerTooltipT[i] = 0.f;

    // Tables (IDs 5 and 6)
    runeTableCount = Collision::Map_GetCentres(collisionMap, 5,
        runeTablePositions, TableSystem::MAX_TABLES);
    poisonTableCount = Collision::Map_GetCentres(collisionMap, 6,
        poisonTablePositions, TableSystem::MAX_TABLES);

    TimeOfDay::Init();
    Gold::Init(0);

    // GS_RESTART skips fpLoad — re-create GPU resources freed by Level1_Free if needed
    if (!plantState.mesh)         plantSystem::PlantSystem_Load(plantState);
    if (!tableState.mesh)         TableSystem::TableSystem_Load(tableState);
    if (!customerPool.spriteMesh) CustomerSystem::CustomerPool_Load(customerPool);
    if (!particleState.mesh)      ParticleSystem::Load(particleState);

    plantSystem::PlantSystem_Init(plantState, potPositions, potCount);
    TableSystem::TableSystem_Init(tableState,
        runeTablePositions, runeTableCount,
        poisonTablePositions, poisonTableCount);

    // Customer pool — first spawn timer is 0 so a customer arrives quickly
    CustomerSystem::CustomerPool_Init(customerPool, CUSTOMER_SPAWN_POS,
        CUSTOMER_TARGETS, CUSTOMER_TARGET_COUNT);

    // Win/lose state reset
    endState = Level1End::PLAYING;
    fadeAlpha = 0.f;

    // Merchant reset
    MerchantSystem::Init(merchant);
    merchantStarted = false;

}

void Level1_Update()
{
    float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());

    // [1] STAGE_TEXT_SEQUENCE
    if (currentStage == STAGE_TEXT_SEQUENCE)
    {
        if (AEInputCheckTriggered(AEVK_SPACE) &&
            currentPhase != BG_TRANSITION && currentPhase != COMPLETE)
        {
            currentPhase = BG_TRANSITION;
            backgroundTimer = 0.f;
            alpha = 0.f;
            return;
        }

        phaseTimer += dt;

        switch (currentPhase)
        {
        case FADE_IN:
            alpha = phaseTimer / FADE_IN_DURATION;
            if (phaseTimer >= FADE_IN_DURATION) { alpha = 1.f; currentPhase = HOLD; phaseTimer = 0.f; }
            break;

        case HOLD:
            alpha = 1.f;
            if (phaseTimer >= HOLD_DURATION) { currentPhase = FADE_OUT; phaseTimer = 0.f; }
            break;

        case FADE_OUT:
            alpha = 1.f - (phaseTimer / FADE_OUT_DURATION);
            if (phaseTimer >= FADE_OUT_DURATION)
            {
                alpha = 0.f;
                ++currentTextIndex;
                if (currentTextIndex < TEXT_COUNT) { currentPhase = FADE_IN; phaseTimer = 0.f; }
                else { currentPhase = BG_TRANSITION; backgroundTimer = 0.f; phaseTimer = 0.f; }
            }
            break;

        case BG_TRANSITION:
        {
            backgroundTimer += dt;
            float t = backgroundTimer / BG_TRANSITION_DURATION;
            if (t > 1.f) t = 1.f;
            red = t * targetRed;
            green = t * targetGreen;
            blue = t * targetBlue;
            if (backgroundTimer >= BG_TRANSITION_DURATION)
            {
                red = targetRed; green = targetGreen; blue = targetBlue;
                currentPhase = COMPLETE;
            }
            break;
        }

        case COMPLETE:
            currentStage = STAGE_GAMEPLAY;
            break;
        }
    }

    // [2] STAGE_GAMEPLAY
    else if (currentStage == STAGE_GAMEPLAY)
    {
        TimeOfDay::Update(dt);

        // Camera panning
        if (!cameraReady)
        {
            camPanTimer += dt;
            float t = camPanTimer / CAM_PAN_DURATION;
            if (t > 1.f) t = 1.f;
            float tEased = 1.f - (1.f - t) * (1.f - t); // quad ease-out

            currentCamY = CAM_START_Y + (CAM_END_Y - CAM_START_Y) * tEased;
            AEGfxSetCamPosition(0.f, currentCamY);

            if (camPanTimer >= CAM_PAN_DURATION)
            {
                currentCamY = CAM_END_Y;
                AEGfxSetCamPosition(0.f, CAM_END_Y);
                cameraReady = true;
            }
        }
        else
        {
            if (AEInputCheckTriggered(AEVK_LBRACKET))
                Debug::enabled = !Debug::enabled;

            //------------------------------------------------------------------
            // Check for game end: time ran out
            //------------------------------------------------------------------
            //------------------------------------------------------------------
// Check for game end: time ran out
//------------------------------------------------------------------
            if (endState == Level1End::PLAYING && TimeOfDay::HasEnded())
            {
                endState = (Gold::GetTotal() >= GOLD_WIN_TARGET)
                    ? Level1End::WIN : Level1End::LOSE;
            }

            // Win/lose overlay / merchant sequence: freeze normal gameplay
            if (endState != Level1End::PLAYING)
            {
                fadeAlpha = std::fminf(fadeAlpha + dt * 0.5f, 1.f);

                if (endState == Level1End::WIN)
                {
                    // Start merchant only when SPACE is pressed
                    if (!merchantStarted && AEInputCheckTriggered(AEVK_SPACE))
                    {
                        MerchantSystem::Start(merchant);
                        merchantStarted = true;
                    }

                    // Once started, merchant walks in and shop can be used
                    if (merchantStarted)
                    {
                        MerchantSystem::Update(merchant, dt);
                        MerchantSystem::HandleMousePurchase(merchant, PlayerSystem::p1->held);

                        if (!merchant.active)
                        {
                            nextState = GS_RESTART; // change later if needed
                        }
                    }

                    return;
                }

                // LOSE flow
                if (fadeAlpha >= 1.f)
                {
                    if (AEInputCheckTriggered(AEVK_R))
                        nextState = GS_RESTART;
                }

                return;
            }
            //------------------------------------------------------------------
            // Player movement — tile map blocks walls (ID=1).
            // Pots, chests, tables, and WAITING customers use per-axis AABB.
            //------------------------------------------------------------------
            // Tick all ease-in/out timers
            constexpr float ANIM_SPEED = 6.0f;

            for (int i = 0; i < chestCount; ++i)
            {
                bool inRange = std::fabsf(chests[i].pos.x - PlayerSystem::p1->GetCoordinates().x) < PLAYER_HW + PROP_COLL_HW &&
                    std::fabsf(chests[i].pos.y - PlayerSystem::p1->GetCoordinates().y) < PLAYER_HH + PROP_COLL_HH;
                BasicUtilities::tickEase(chestTooltipT[i], inRange, dt, ANIM_SPEED);
            }

            BasicUtilities::tickEase(heldTooltipT,
                PlayerSystem::p1->held.type != HeldItem::NONE, dt, ANIM_SPEED);

            for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
            {
                const auto& sl = customerPool.slots[ci];
                BasicUtilities::tickEase(customerTooltipT[ci],
                    sl.active && sl.customer.state == CustomerState::WAITING, dt, ANIM_SPEED);
            }

            AEVec2 prevPos = PlayerSystem::p1->GetCoordinates();
            PlayerSystem::Update(collisionMap, dt);

            Entity::Update();

            {
                constexpr float PW = PLAYER_HW, PH = PLAYER_HH;
                constexpr float PROP_HW = PROP_COLL_HW, PROP_HH = PROP_COLL_HH;
                constexpr float PROP_BOTTOM_ALLOW = 0.f;

                auto isPropBlocked = [&](float cx, float cy, bool topOnly) -> bool
                    {
                        // Pots
                        for (int i = 0; i < potCount; ++i)
                            if (fabsf(cx - potPositions[i].x) < PW + PROP_HW &&
                                fabsf(cy - potPositions[i].y) < PH + PROP_HH &&
                                (!topOnly || cy >= potPositions[i].y - PROP_BOTTOM_ALLOW))
                                return true;

                        // Chests
                        for (int i = 0; i < chestCount; ++i)
                            if (fabsf(cx - chests[i].pos.x) < PW + PROP_HW &&
                                fabsf(cy - chests[i].pos.y) < PH + PROP_HH &&
                                (!topOnly || cy >= chests[i].pos.y - PROP_BOTTOM_ALLOW))
                                return true;

                        // Tables (wider than standard props)
                        for (int i = 0; i < tableState.count; ++i)
                            if (fabsf(cx - tableState.tables[i].pos.x) < PW + TableSystem::TABLE_COLL_HW &&
                                fabsf(cy - tableState.tables[i].pos.y) < PH + TableSystem::TABLE_COLL_HH &&
                                (!topOnly || cy >= tableState.tables[i].pos.y - PROP_BOTTOM_ALLOW))
                                return true;

                    // WAITING customers block the player (ARRIVING/LEAVING are moving; don't block)
                    for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
                    {
                        const auto& sl = customerPool.slots[ci];
                        if (!sl.active || sl.customer.state != CustomerState::WAITING) continue;
                        if (fabsf(cx - sl.customer.GetCoordinates().x) < PW + PROP_HW &&
                            fabsf(cy - sl.customer.GetCoordinates().y) < PH + PROP_HH &&
                            (!topOnly || cy >= sl.customer.GetCoordinates().y - PROP_BOTTOM_ALLOW))
                            return true;
                    }
                    return false;
                };

                if (isPropBlocked(PlayerSystem::p1->GetCoordinates().x, prevPos.y, true))
                    PlayerSystem::p1->RefX() = prevPos.x;
                if (isPropBlocked(PlayerSystem::p1->GetCoordinates().x, PlayerSystem::p1->GetCoordinates().y, true))
                    PlayerSystem::p1->RefY() = prevPos.y;
            }

            //------------------------------------------------------------------
            // Customer serve — priority E-key handler (only when holding a flower).
            // Runs before table and plant updates so the correct interaction fires.
            //------------------------------------------------------------------
            bool ePressHandled = false;
            if (AEInputCheckTriggered(AEVK_E) &&
                PlayerSystem::p1->held.type == HeldItem::FLOWER)
            {
                int gold = CustomerSystem::CustomerPool_TryServe(
                    customerPool,
                    PlayerSystem::p1->held.flower,
                    PlayerSystem::p1->held.modifier,
                    PlayerSystem::p1->GetCoordinates());

                if (gold > 0)
                {
                    Gold::Earn(gold);
                    PlayerSystem::p1->held = HeldState{};
                    ePressHandled = true;
                }
            }

            // Table interaction (E key: place held flower → start infusion,
            //                    or pick up infused flower → apply modifier).
            // Skipped if the E press already served a customer.
            if (!ePressHandled)
                TableSystem::TableSystem_Update(tableState, dt,
                    PlayerSystem::p1->GetCoordinates(), PlayerSystem::p1->held);

            // Plant system (E = pick seed / plant / harvest; Q = water)
            plantSystem::PlantSystem_Update(plantState, dt,
                PlayerSystem::p1->GetCoordinates(), PlayerSystem::p1->held,
                potPositions, potCount, chests, chestCount);

            // Customer pool — spawn new customers and advance all active ones
            CustomerSystem::CustomerPool_Update(customerPool, dt);

            // Water particles — radial spray from pot centre while Q is held
            particleState.emitTimer -= dt;
            if (AEInputCheckCurr(AEVK_Q) && particleState.emitTimer <= 0.f)
            {
                int waterIdx = plantSystem::PlantSystem_NearestPlant(
                    plantState, PlayerSystem::p1->GetCoordinates());
                if (waterIdx >= 0 && plantState.can.water > 0.f)
                {
                    ParticleSystem::EmitWater(particleState,
                        plantState.plants[waterIdx].pos);
                    particleState.emitTimer = 0.05f; // 20 bursts/sec
                }
            }
            ParticleSystem::Update(particleState, dt);
        }
    }
}

void Level1_Draw()
{
    // STAGE_TEXT_SEQUENCE
    if (currentStage == STAGE_TEXT_SEQUENCE)
    {
        AEGfxSetBackgroundColor(red, green, blue);

        if (currentPhase == FADE_IN || currentPhase == HOLD || currentPhase == FADE_OUT)
        {
            if (currentTextIndex < TEXT_COUNT)
            {
                const TextEntry& e = textSequence[currentTextIndex];
                BasicUtilities::drawText(fontId, e.text, e.x, e.y, e.scale,
                    e.r, e.g, e.b, alpha);
            }
        }
    }

    // STAGE_GAMEPLAY
    else if (currentStage == STAGE_GAMEPLAY)
    {
        float r, g, b;
        TimeOfDay::GetBackgroundColor(r, g, b);
        AEGfxSetBackgroundColor(r, g, b);

        // ── Floor tiles ──────────────────────────────────────────────────────
        for (int row = 0; row < collisionMap.height; ++row)
        {
            for (int col = 0; col < collisionMap.width; ++col)
            {
                if (collisionMap.solid[static_cast<size_t>(row) * collisionMap.width + col]) continue;

                float cx = collisionMap.origin.x + col * collisionMap.tileSize + collisionMap.tileSize * 0.5f;
                float cy = collisionMap.origin.y + row * collisionMap.tileSize + collisionMap.tileSize * 0.5f;
                AEGfxTexture* tex = ((col + row) % 2 == 0) ? floorOneTex : floorTwoTex;
                BasicUtilities::drawUICard(tileMesh, tex, cx, cy,
                    collisionMap.tileSize, collisionMap.tileSize);
            }
        }

        // ── Props: pots and chests ───────────────────────────────────────────
        plantSystem::PlantSystem_Draw(plantState, chests, chestCount);

        // ── Tables ──────────────────────────────────────────────────────────
        TableSystem::TableSystem_Draw(tableState, flowerIconTex);

        // ── Customers — sprites before player so player appears in front ─────
        CustomerSystem::CustomerPool_Draw(customerPool, fontId);

        // Draw entities
        Entity::Draw();

        // ── Player + particles — draw order based on perspective ─────────────
        // Facing UP  → player's back is to the camera; particles appear below.
        // All other  → player faces the camera; particles appear above.
        if (PlayerSystem::p1->GetLastDirection() == Entity::FaceDirection::UP)
        {
            ParticleSystem::Draw(particleState);
            PlayerSystem::Draw();
        }
        else
        {
            PlayerSystem::Draw();
            ParticleSystem::Draw(particleState);
        }

        // ── Customer patience bars and order bubbles (above player) ──────────
        {
            float smoothedCustomerT[CustomerSystem::POOL_MAX]{};
            for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
                smoothedCustomerT[ci] = BasicUtilities::smoothstep(customerTooltipT[ci]);
            CustomerSystem::CustomerPool_DrawUI(customerPool, fontId, flowerIconTex, smoothedCustomerT);
        }

        // ── Info panel ───────────────────────────────────────────────────────
        BasicUtilities::drawUICard(squareMesh, infoPanelTex,
            PANEL_POS.x, PANEL_POS.y, PANEL_W, PANEL_H);

        BasicUtilities::drawUICard(squareMesh, goldIconTex,
            PANEL_POS.x + GOLD_ICON_OFF.x, PANEL_POS.y + GOLD_ICON_OFF.y,
            ICON_SIZE, ICON_SIZE);

        BasicUtilities::drawUICard(squareMesh, clockIconTex,
            PANEL_POS.x + CLOCK_ICON_OFF.x, PANEL_POS.y + CLOCK_ICON_OFF.y,
            ICON_SIZE, ICON_SIZE);

        char goldBuf[16], timeBuf[16];
        sprintf_s(goldBuf, sizeof(goldBuf), "%d", Gold::GetTotal());
        TimeOfDay::GetClockString(timeBuf, sizeof(timeBuf));

        BasicUtilities::drawText(fontId, goldBuf,
            PANEL_POS.x + GOLD_TEXT_OFF.x,
            (PANEL_POS.y + GOLD_TEXT_OFF.y) - currentCamY,
            0.8f, 1.f, 0.85f, 0.f);

        BasicUtilities::drawText(fontId, timeBuf,
            PANEL_POS.x + CLOCK_TEXT_OFF.x,
            (PANEL_POS.y + CLOCK_TEXT_OFF.y) - currentCamY,
            0.8f, 1.f, 1.f, 1.f);

        // ── Chest tooltips — dark bar with seed pack icon + name above chest ──
        if (cameraReady && PlayerSystem::p1)
        {
            for (int i = 0; i < chestCount; ++i)
            {
                float dx = chests[i].pos.x - PlayerSystem::p1->GetCoordinates().x;
                float dy = chests[i].pos.y - PlayerSystem::p1->GetCoordinates().y;
                if (std::fabsf(dx) >= PLAYER_HW + PROP_COLL_HW ||
                    std::fabsf(dy) >= PLAYER_HH + PROP_COLL_HH) continue;
                if (chestTooltipT[i] <= 0.f) continue;

                int idx = static_cast<int>(chests[i].seed);
                if (idx < 1 || idx > 6) continue;

                // Smoothstep the raw timer before passing to drawTooltip
                float t = BasicUtilities::smoothstep(chestTooltipT[i]);

                BasicUtilities::drawTooltip(
                    squareMesh,
                    seedPackTex[idx],
                    1.f, 1.f, 1.f,
                    SeedName(chests[i].seed),
                    chests[i].pos.x, chests[i].pos.y + 65.f,
                    fontId,
                    0.55f, 0.85f,
                    t);
            }
        }

        // ── Held-item tooltip — dark bar at screen bottom (camera-independent) ─
        if (cameraReady && PlayerSystem::p1 && heldTooltipT > 0.f)
        {
            constexpr float TX = 0.f;    // screen-space X (centred)
            constexpr float TY = -380.f; // screen-space Y (near bottom)
            const HeldState& held = PlayerSystem::p1->held;

            float hT = BasicUtilities::smoothstep(heldTooltipT);

            if (held.type == HeldItem::SEED)
            {
                int idx = static_cast<int>(held.seed);
                BasicUtilities::drawTooltip(
                    squareMesh,
                    (idx >= 1 && idx <= 6) ? seedPackTex[idx] : nullptr,
                    1.f, 1.f, 0.6f,
                    SeedName(held.seed),
                    TX, TY, fontId,
                    0.55f, 0.85f, hT);
            }
            else if (held.type == HeldItem::FLOWER)
            {
                int idx = static_cast<int>(held.flower);
                char labelBuf[32];
                if (held.modifier == FlowerModifier::FIRE)
                    sprintf_s(labelBuf, sizeof(labelBuf), "%s [FIRE]", FlowerName(held.flower));
                else if (held.modifier == FlowerModifier::POISON)
                    sprintf_s(labelBuf, sizeof(labelBuf), "%s [POISON]", FlowerName(held.flower));
                else
                    sprintf_s(labelBuf, sizeof(labelBuf), "%s", FlowerName(held.flower));

                BasicUtilities::drawTooltip(
                    squareMesh,
                    (idx >= 1 && idx <= 6) ? flowerIconTex[idx] : nullptr,
                    1.f, 0.85f, 0.3f,
                    labelBuf,
                    TX, TY, fontId,
                    0.55f, 0.85f, hT);
            }
        }

        // ── Win/lose overlay — fade to black then show result + restart hint ─
        if (endState != Level1End::PLAYING)
{
    AEGfxSetRenderMode(AE_GFX_RM_COLOR);
    AEGfxSetBlendMode(AE_GFX_BM_BLEND);

    float overlayAlpha = fadeAlpha;

    // Keep the level visible when the merchant arrives after a win
    if (endState == Level1End::WIN)
        overlayAlpha = 0.35f;

    AEGfxSetTransparency(overlayAlpha);
    AEGfxSetColorToMultiply(0.f, 0.f, 0.f, 1.f);
    AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

            // Full-screen black quad centred on the camera position
            AEMtx33 sclMtx{}, trsMtx{}, mtx{};
            AEMtx33Scale(&sclMtx, 1600.f, 900.f);
            AEMtx33Trans(&trsMtx, 0.f, currentCamY);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);

            // Show result text only once fully faded
            if (fadeAlpha >= 1.f)
            {
                const bool  win = (endState == Level1End::WIN);
                const char* msg = win ? "YOU WIN!" : "YOU LOSE";
                const float mr = 1.f;
                const float mg = win ? 0.85f : 0.2f;
                const float mb = win ? 0.0f : 0.2f;

                BasicUtilities::drawText(fontId, msg,
                    0.f, 50.f, 2.0f, mr, mg, mb, 1.f);

                if (win)
                {
                    if (!merchantStarted)
                    {
                        BasicUtilities::drawText(fontId, "press SPACE to call the merchant",
                            0.f, -20.f, 0.8f, 1.f, 1.f, 1.f, 1.f);

                        BasicUtilities::drawText(fontId, "press R to replay level",
                            0.f, -60.f, 0.8f, 1.f, 1.f, 1.f, 1.f);
                    }
                    else
                    {
                        BasicUtilities::drawText(fontId, "click items to buy, press N to skip",
                            0.f, -40.f, 0.7f, 1.f, 1.f, 1.f, 1.f);
                    }
                }
                else
                {
                    BasicUtilities::drawText(fontId, "press R to restart",
                        0.f, -40.f, 0.8f, 1.f, 1.f, 1.f, 1.f);
                }
            }
            if (endState == Level1End::WIN && merchantStarted)
            {
                MerchantSystem::Draw(merchant, fontId);
            }
        }

        //======================================================================
        // DEBUG MODE
        //======================================================================
        if (Debug::enabled)
        {
            AEMtx33 sclMtx{}, trsMtx{}, mtx{};
            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

            auto drawAABB = [&](float cx, float cy, float hw, float hh,
                float r, float g, float b)
                {
                    AEGfxSetTransparency(Debug::HITBOX_ALPHA);
                    AEGfxSetColorToMultiply(r, g, b, 1.f);
                    AEMtx33Scale(&sclMtx, hw * 2.f, hh * 2.f);
                    AEMtx33Trans(&trsMtx, cx, cy);
                    AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                    AEGfxSetTransform(mtx.m);
                    AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);
                };

            // Player AABB
            if (PlayerSystem::p1)
                drawAABB(PlayerSystem::p1->GetCoordinates().x, PlayerSystem::p1->GetCoordinates().y,
                    PLAYER_HW, PLAYER_HH,
                    Debug::PLAYER_R, Debug::PLAYER_G, Debug::PLAYER_B);
            // Pot AABBs
            for (int i = 0; i < potCount; ++i)
                drawAABB(potPositions[i].x, potPositions[i].y, PROP_COLL_HW, PROP_COLL_HH,
                    Debug::POT_R, Debug::POT_G, Debug::POT_B);
            // Chest AABBs
            for (int i = 0; i < chestCount; ++i)
                drawAABB(chests[i].pos.x, chests[i].pos.y, PROP_COLL_HW, PROP_COLL_HH,
                    Debug::CHEST_R, Debug::CHEST_G, Debug::CHEST_B);
            // Table AABBs (rune = purple, poison = lime)
            for (int i = 0; i < tableState.count; ++i)
            {
                bool isRune = (tableState.tables[i].type == TableSystem::TableType::RUNE);
                drawAABB(tableState.tables[i].pos.x, tableState.tables[i].pos.y,
                    TableSystem::TABLE_COLL_HW, TableSystem::TABLE_COLL_HH,
                    isRune ? Debug::RUNE_TABLE_R : Debug::POISON_TABLE_R,
                    isRune ? Debug::RUNE_TABLE_G : Debug::POISON_TABLE_G,
                    isRune ? Debug::RUNE_TABLE_B : Debug::POISON_TABLE_B);
            }

            // Debug status text
            if (PlayerSystem::p1)
            {
                const auto& dbgP = *PlayerSystem::p1;

                const char* faceStr =
                    dbgP.GetLastDirection() == Entity::FaceDirection::UP ? "UP" :
                    dbgP.GetLastDirection() == Entity::FaceDirection::DOWN ? "DOWN" :
                    dbgP.GetLastDirection() == Entity::FaceDirection::LEFT ? "LEFT" : "RIGHT";

                const char* heldStr =
                    dbgP.held.type == HeldItem::SEED ? "SEED" :
                    dbgP.held.type == HeldItem::FLOWER ? "FLOWER" : "NONE";

                const char* modStr =
                    dbgP.held.modifier == FlowerModifier::FIRE ? "+FIRE" :
                    dbgP.held.modifier == FlowerModifier::POISON ? "+POISON" : "";

                char dbgBuf[64];

                BasicUtilities::drawText(fontId, "[DEBUG]",
                    -790.f, -380.f, 0.7f, 1.f, 1.f, 0.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "pos  %.1f  %.1f",
                    dbgP.GetCoordinates().x, dbgP.GetCoordinates().y);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -410.f, 0.6f, 1.f, 1.f, 1.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "face %s", faceStr);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -430.f, 0.6f, 1.f, 1.f, 1.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "held %s%s", heldStr, modStr);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -450.f, 0.6f, 1.f, 1.f, 1.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "gold %d", Gold::GetTotal());
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -470.f, 0.6f, 1.f, 1.f, 1.f, 1.f);
            }
        }
    }
}
void Level1_Free()
{
    AEGfxMeshFree(squareMesh); squareMesh = nullptr;
    AEGfxMeshFree(tileMesh);   tileMesh   = nullptr;
	Entity::Free();
    plantSystem::PlantSystem_Free(plantState);
    ParticleSystem::Free(particleState);
    TableSystem::TableSystem_Free(tableState);
    CustomerSystem::CustomerPool_Free(customerPool);
    AEGfxSetCamPosition(0.f, 0.f);
    MerchantSystem::Free();
}

void Level1_Unload()
{
    if (uiCardTexture) {
          AEGfxTextureUnload(uiCardTexture);
        uiCardTexture = nullptr;
    }
    AEGfxDestroyFont(fontId);

    AEGfxTextureUnload(floorOneTex);
    AEGfxTextureUnload(floorTwoTex);
    floorOneTex = floorTwoTex = nullptr;
    Collision::Map_Unload(collisionMap);

    plantSystem::PlantSystem_Unload(plantState);
    TableSystem::TableSystem_Unload(tableState);
    CustomerSystem::CustomerPool_Unload(customerPool);
	Entity::Unload();
    MerchantSystem::Unload();

    AEGfxTextureUnload(infoPanelTex);
    AEGfxTextureUnload(goldIconTex);
    AEGfxTextureUnload(clockIconTex);
    infoPanelTex = goldIconTex = clockIconTex = nullptr;

    // HUD icon textures
    for (int i = 1; i <= 6; ++i)
    {
        if (flowerIconTex[i]) { AEGfxTextureUnload(flowerIconTex[i]); flowerIconTex[i] = nullptr; }
        if (seedPackTex[i]) { AEGfxTextureUnload(seedPackTex[i]);   seedPackTex[i] = nullptr; }
    }
}