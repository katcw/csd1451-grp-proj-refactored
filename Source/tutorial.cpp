/*===========================================================================
@filename   tutorial.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg
@course     CSD1451
@section    Section B

@brief      Tutorial game state. Begins with a narrative text sequence
            (black screen, fade-in/hold/fade-out per text entry) followed
            by a smooth background transition to beige, then enters the
            tutorial gameplay stage.
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
#include "tutorial.hpp"
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
#include "customer.hpp"
#include "debug.hpp"

namespace
{
    // Assets 
    AEGfxTexture*       uiCardTexture   = nullptr;
    AEGfxVertexList*    squareMesh      = nullptr;
    s8                  fontId          = -1;

    // Tutorial state will be split into two stages
    // [1] STAGE_TEXT_SEQUENCE will display introductory storyline
    // [2] STAGE_GAMEPLAY will begin after introductory storyline, main gameplay stage
    enum TutorialStage { STAGE_TEXT_SEQUENCE, STAGE_GAMEPLAY };
    TutorialStage currentStage = STAGE_TEXT_SEQUENCE;

    // Text sequence phases for fade in and out effect 
    enum TutorialPhase { FADE_IN, HOLD, FADE_OUT, BG_TRANSITION, COMPLETE };
    TutorialPhase currentPhase = FADE_IN;

    float phaseTimer = 0.0f;
    float alpha = 0.0f;
    int   currentTextIndex = 0;

    const float FADE_IN_DURATION = 1.5f;
    const float HOLD_DURATION = 2.5f;
    const float FADE_OUT_DURATION = 1.0f;
    const float BG_TRANSITION_DURATION = 1.5f;

    // Background colour during transition to STAGE_GAMEPLAY
    float red = 0.f, green = 0.f, blue = 0.f;
    float backgroundTimer = 0.f;

    const float targetRed = 0.85f;
    const float targetGreen = 0.84f;
    const float targetBlue = 0.80f;

    // Text sequence data
    struct TextEntry
    {
        const char* text;
        float       x, y;
        float       scale;
        float       r, g, b;
    };

    // Array of text to print
    const TextEntry textSequence[] =
    {
        { "once upon a time, there was a man named prassana",          0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
        { "and he said that our 200 minds combined can't beat his one", 0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
        { "i guess he's right...",                                      0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
    };
    const int TEXT_COUNT = static_cast<int>(sizeof(textSequence) / sizeof(textSequence[0]));

    // STAGE_GAMEPLAY assets
    AEGfxTexture*       floorOneTex     = nullptr;
    AEGfxTexture*       floorTwoTex     = nullptr;
    AEGfxVertexList*    tileMesh        = nullptr;
    Collision::Map      collisionMap;

    //========================================================================
    // WORLD OBJECT LAYOUT — derived from map_data.txt at runtime
    //
    // map_data.txt ID scheme:
    //   0 = floor (passable)
    //   1 = background / wall (solid in tile map)
    //   2 = player spawn (passable marker)
    //   3 = pot  (passable in tile map; blocked via per-axis AABB collision)
    //   4 = chest (passable in tile map; blocked via per-axis AABB collision)
    //
    // Seed types for chests found in map_data (scan order: bottom row first,
    // then left to right). Add more entries here to assign seeds to chests
    // placed in additional columns.
    //========================================================================
    static const SeedType   s_chestSeeds[]  = { SeedType::ROSE };
    constexpr int           MAX_MAP_CHESTS  = 8;

    AEVec2                  potPositions[plantSystem::MAX_PLANTS];
    int                     potCount        = 0;
    plantSystem::ChestData  chests[MAX_MAP_CHESTS];
    int                     chestCount      = 0;

    plantSystem::State      plantState;
    ParticleSystem::State   particleState;
    CustomerSystem::State   customerState;

    // [TUNE] Customer entry — bottom-centre of house, walks upward into interior
    const AEVec2 CUSTOMER_SPAWN_POS  = {   0.0f, -225.0f };  // off-screen below bottom wall
    const AEVec2 CUSTOMER_TARGET_POS = {   0.0f, 0.0f };  // 2 tiles into interior floor

    constexpr int GOLD_REWARD_FLOWER_SALE = 25;   // [TUNE] gold earned on successful sale
    constexpr int GOLD_PENALTY_IMPATIENT  = 10;   // [TUNE] gold lost when customer leaves

    // Camera pan
    bool        cameraReady         = false;
    float       camPanTimer         = 0.f;
    const float CAM_PAN_DURATION    = 2.0f;
    const float CAM_START_Y         = -750.f;
    const float CAM_END_Y           = 0.f;
    float       currentCamY         = CAM_START_Y; 

    // Info panel assets
    AEGfxTexture* infoPanelTex      = nullptr;
    AEGfxTexture* goldIconTex       = nullptr;
    AEGfxTexture* clockIconTex      = nullptr;

    const AEVec2 PANEL_POS          = { 640.f, 400.f }; // World coordinates
    const float  PANEL_W            = 300.f;
    const float  PANEL_H            = 80.f;
    const float  ICON_SIZE          = 30.f;

    // Info panel elements
    // Offsetes from PANEL_POS, adjust element positions here
    const AEVec2 GOLD_ICON_OFF      = { -100.f, 0.f };  // Gold icon centre 
    const AEVec2 GOLD_TEXT_OFF      = { -65.f, 0.f };  // Gold text 
    const AEVec2 CLOCK_ICON_OFF     = { 10.f,   0.f };  // Clock icon centre
    const AEVec2 CLOCK_TEXT_OFF     = { 75.f, 0.f };  // Clock text

    // Collision data 
    constexpr float PLAYER_HW       = PlayerSystem::HALF_W;  
    constexpr float PLAYER_HH       = PlayerSystem::HALF_H;
    constexpr float PROP_COLL_HW    = 32.f;  // Half of sprite's 64px
    constexpr float PROP_COLL_HH    = 32.f;

    // PanelState drives the info panel slide animation at the screen bottom.
    
    // Tutorial task system
    // [!] To add a new task: add GameplayTask value, TASK_DATA entry, completion check in Update,
    //     and (if needed) pointer target branch in drawTaskPointer().
    enum GameplayTask { TASK_INTRO,        
                        TASK_PICK_SEED,     
                        TASK_PLANT_SEED,    
                        TASK_WATER_PLANT,   
                        TASK_WAITING,      
                        TASK_HARVEST,     
                        TASK_SELLING,      
                        TASK_RETRY,       
                        TASK_ALL_DONE,      
                      };
    GameplayTask currentTask = TASK_INTRO;

    // Tutorial panel animation states
    enum PanelState { PANEL_HIDDEN, PANEL_SLIDING_IN, PANEL_VISIBLE, PANEL_SLIDING_OUT };
    PanelState panelState = PANEL_HIDDEN;

    // Task instruction textures
    AEGfxTexture* taskPanelTex = nullptr;   // info_panel_light.png
    AEGfxTexture* pointerTex   = nullptr;   // tutorial_pointer.png

    // Panel slide
    float       panelSlideTimer = 0.f;
    const float PANEL_SLIDE_DUR = 0.4f;

    // Panel geometry — screen-pinned (camera fixed at Y=0 after pan)
    constexpr float TASK_PANEL_W = 800.f;
    const float TASK_PANEL_H     = 150.f;
    const float TASK_PANEL_Y_ON  = -450.f + TASK_PANEL_H * 0.5f + 20.f;  // visible target
    const float TASK_PANEL_Y_OFF = -450.f - TASK_PANEL_H * 0.5f;          // off-screen

    // Pointer (bouncing arrow above the target prop)
    float       pointerBounceTimer        = 0.f;
    const float POINTER_SIZE              = 48.f;
    const float POINTER_OFFSET_Y          = 80.f;    // world units above prop centre (pots, chests)
    // Customer pointer must clear the order bubble:
    // BUBBLE_OFFSET_Y(80) + BUBBLE_H/2(20) + POINTER_SIZE/2(24) + margin(8) = 132
    const float CUSTOMER_POINTER_OFFSET_Y = 132.f;  // [TUNE] world units above customer centre
    const float POINTER_BOUNCE_AMP        = 8.f;
    const float POINTER_BOUNCE_FREQ       = 1.5f;    // Hz

    // Task data (text + optional prompt), indexed by GameplayTask
    struct TaskData
    {
        const char* text;               // main instruction (word-wrapped within panel)
        const char* prompt = nullptr;   // secondary line drawn lighter below; omit if none
    };

    // One entry per GameplayTask value (0–7). TASK_ALL_DONE (8) is never indexed.
    // The existing guard `currentTask < TASK_ALL_DONE` enforces this bound.
    const TaskData TASK_DATA[] = {
        { "Welcome to your first day! Let's start your first day proper",
          "Press LMB to continue" },                                            // TASK_INTRO
        { "Seeds are located in chests, try picking one up!",
          "Press [E] on a chest to pick seed" },                                // TASK_PICK_SEED
        { "Now we need to plant them in a pot, so go over to one",
          "Press [E] on a pot to plant the seed" },                             // TASK_PLANT_SEED
        { "Your uncle was kind enough to leave you a magical watering can "
          "that makes the plant grow faster",
          "Hold [Q] on a planted pot to speed up growth" },                     // TASK_WATER_PLANT
        { "Now we shall wait..." },                                              // TASK_WAITING
        { "Harvest the flower!",
          "Press [E] near the pot" },                                            // TASK_HARVEST
        { "Sell the flower to the customer",
          "Press [E] near the customer" },                                       // TASK_SELLING
        { "The customer left! You must serve them before patience runs out.",
          "Press LMB to try again" },                                            // TASK_RETRY
    };

    // Draws the bouncing pointer above the prop relevant to the current task.
    // No-op for tasks with no pointer target (e.g. TASK_INTRO).
    // To add a pointer for a new task, add a branch here.
    void drawTaskPointer()
    {
        const AEVec2* target = nullptr;
        if (currentTask == TASK_PICK_SEED && chestCount > 0)
            target = &chests[0].pos;
        else if (currentTask == TASK_PLANT_SEED && potCount > 0)
            target = &potPositions[0];
        else if (currentTask == TASK_WATER_PLANT || currentTask == TASK_HARVEST)
        {
            // Point to the active (planted / fully-grown) pot, whichever one it is
            for (int i = 0; i < plantState.potCount; ++i)
            {
                if (plantState.plants[i].active)
                {
                    target = &plantState.plants[i].pos;
                    break;
                }
            }
        }
        else if (currentTask == TASK_SELLING && customerState.active)
            target = &customerState.customer.position;   // tracks the customer as they move
        // TASK_RETRY intentionally has no pointer

        if (!target) return;

        constexpr float PI_F = 3.14159265f;
        float bounceY = sinf(pointerBounceTimer * 2.f * PI_F * POINTER_BOUNCE_FREQ)
                        * POINTER_BOUNCE_AMP;
        const float offsetY = (currentTask == TASK_SELLING) ? CUSTOMER_POINTER_OFFSET_Y
                                                             : POINTER_OFFSET_Y;
        BasicUtilities::drawUICard(squareMesh, pointerTex,
            target->x, target->y + offsetY + bounceY,
            POINTER_SIZE, POINTER_SIZE);
    }

}

void Tutorial_Load()
{
    uiCardTexture = BasicUtilities::loadTexture("Assets/ui_card.png");
    fontId = AEGfxCreateFont("Assets/PixelifySans-Regular.ttf", 36);
    if (fontId == -1)
        std::cout << "ERROR! COULD NOT CREATE FONT FOR TUTORIAL\n";

    // Gameplay stage resources
    floorOneTex = BasicUtilities::loadTexture("Assets/floor_one.png");
    floorTwoTex = BasicUtilities::loadTexture("Assets/floor_two.png");
    AEVec2 origin = { -800.f, -448.f };
    Collision::Map_Load("Assets/map_data.txt", collisionMap, 64.f, origin);

    plantSystem::PlantSystem_Load(plantState);
    ParticleSystem::Load(particleState);
    CustomerSystem::CustomerSystem_Load(customerState);

    // Info panel textures
    infoPanelTex = BasicUtilities::loadTexture("Assets/info_panel.png");
    goldIconTex  = BasicUtilities::loadTexture("Assets/gold_icon.png");
    clockIconTex = BasicUtilities::loadTexture("Assets/clock_icon.png");

    // Tutorial task textures
    taskPanelTex = BasicUtilities::loadTexture("Assets/info_panel_light.png");
    pointerTex   = BasicUtilities::loadTexture("Assets/tutorial_pointer.png");
}

void Tutorial_Initialise()
{
    squareMesh         = BasicUtilities::createSquareMesh();
    currentStage       = STAGE_TEXT_SEQUENCE;
    currentPhase       = FADE_IN;
    currentTextIndex   = 0;
    phaseTimer         = 0.0f;
    alpha              = 0.0f;
    red = green = blue = 0.0f;
    backgroundTimer    = 0.0f;

    // Gameplay stage
    tileMesh = BasicUtilities::createSquareMesh();

    // Camera pan
    cameraReady = false;
    camPanTimer = 0.f;
    currentCamY = CAM_START_Y;
    AEGfxSetCamPosition(0.f, CAM_START_Y);

    // Derive world coordinate positions from map text file
    AEVec2 spawnBuf[1]; // Player ID 1
    if (Collision::Map_GetCentres(collisionMap, 2, spawnBuf, 1) > 0)
        PlayerSystem::Init(spawnBuf[0]);
    else
        PlayerSystem::Init({ 0.f, 0.f }); // Fallback if no player ID present in map

    // Pots ID 3
    potCount = Collision::Map_GetCentres(collisionMap, 3,
                   potPositions, plantSystem::MAX_PLANTS);

    // Chests ID 4
    AEVec2 chestPosBuf[MAX_MAP_CHESTS];
    chestCount = Collision::Map_GetCentres(collisionMap, 4, chestPosBuf, MAX_MAP_CHESTS);
    for (int i = 0; i < chestCount; ++i)
    {
        chests[i].pos  = chestPosBuf[i];
        chests[i].seed = (i < (int)(sizeof(s_chestSeeds) / sizeof(*s_chestSeeds)))
                             ? s_chestSeeds[i] : SeedType::ROSE;
    }

    TimeOfDay::Init(); // Restart time of day
    Gold::Init(0); // Reset gold back to 0
    plantSystem::PlantSystem_Init(plantState, potPositions, potCount);
    CustomerSystem::CustomerSystem_Init(customerState,
        CUSTOMER_SPAWN_POS, CUSTOMER_TARGET_POS, FlowerType::ROSE);

    // Tutorial task system
    currentTask        = TASK_INTRO;
    panelState         = PANEL_HIDDEN;
    panelSlideTimer    = 0.f;
    pointerBounceTimer = 0.f;
}

void Tutorial_Update()
{
    if (AEInputCheckTriggered(AEVK_ESCAPE)) nextState = GS_MAINMENU;

    float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());

    // [1] STAGE_TEXT_SEQUENCE
    if (currentStage == STAGE_TEXT_SEQUENCE)
    {
        if (AEInputCheckTriggered(AEVK_SPACE) &&
            currentPhase != BG_TRANSITION && currentPhase != COMPLETE)
        {
            currentPhase    = BG_TRANSITION;
            backgroundTimer = 0.0f;
            alpha           = 0.0f;
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
            alpha = 1.0f;
            if (phaseTimer >= HOLD_DURATION) { currentPhase = FADE_OUT; phaseTimer = 0.f; }
            break;

        case FADE_OUT:
            alpha = 1.0f - (phaseTimer / FADE_OUT_DURATION);
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
            red   = t * targetRed;
            green = t * targetGreen;
            blue  = t * targetBlue;
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
        TimeOfDay::Update(dt); // Advances time during camera pan

        // Camera panning
        if (!cameraReady)
        {
            camPanTimer += dt;
            float t      = camPanTimer / CAM_PAN_DURATION;
            if (t > 1.f) t = 1.f;
            float tEased = 1.f - (1.f - t) * (1.f - t); // quad ease-out

            currentCamY = CAM_START_Y + (CAM_END_Y - CAM_START_Y) * tEased;
            AEGfxSetCamPosition(0.f, currentCamY);

            if (camPanTimer >= CAM_PAN_DURATION)
            {
                currentCamY     = CAM_END_Y;
                AEGfxSetCamPosition(0.f, CAM_END_Y);
                cameraReady     = true; // After camera is done panning, switch cameraReady to true
                panelState      = PANEL_SLIDING_IN;
                panelSlideTimer = 0.f;
            }
        }
        // Executes when cameraReady is set to true after camera panning
        else
        {
            // Debug mode toggle
            if (AEInputCheckTriggered(AEVK_LBRACKET))
                Debug::enabled = !Debug::enabled;

            //------------------------------------------------------------------
            // Player movement — tile map blocks walls (ID=1).
            // Pots and chests are handled by per-axis AABB below:
            //   X-axis : block from all sides
            //   Y-axis : block from top only; allow overlap from below
            //------------------------------------------------------------------
            AEVec2 prevPos = PlayerSystem::p1->position;
            PlayerSystem::Update(collisionMap, dt);

            // Directional prop AABB (PW/PH = player half-extents, PROP_HW/HH = prop half-extents)
            {
                constexpr float PW = PLAYER_HW,    PH = PLAYER_HH;
                constexpr float PROP_HW = PROP_COLL_HW, PROP_HH = PROP_COLL_HH;

                // ── Tunable: bottom-overlap allowance ────────────────────────────────
                // Controls how far the player may overlap a prop from below before the
                // Y-axis collision fires. Measured in world units from the prop's Y centre.
                //
                //   0.f     → Y block starts at the prop's midline. The player's centre
                //             may sit anywhere below that midline, allowing the player's
                //             upper body to visually overlap the prop's lower half.
                //   PROP_HH → Y block starts at the prop's bottom edge. Almost no overlap
                //             is permitted; the collision becomes near-symmetric.
                //
                // Rule of thumb: higher = tighter (less overlap); lower = looser (more).
                // ─────────────────────────────────────────────────────────────────────
                constexpr float PROP_BOTTOM_ALLOW = 0.f;

                auto isPropBlocked = [&](float cx, float cy, bool topOnly) -> bool
                {
                    for (int i = 0; i < potCount; ++i)
                        if (fabsf(cx - potPositions[i].x) < PW + PROP_HW &&
                            fabsf(cy - potPositions[i].y) < PH + PROP_HH &&
                            (!topOnly || cy >= potPositions[i].y - PROP_BOTTOM_ALLOW))
                            return true;
                    for (int i = 0; i < chestCount; ++i)
                        if (fabsf(cx - chests[i].pos.x) < PW + PROP_HW &&
                            fabsf(cy - chests[i].pos.y) < PH + PROP_HH &&
                            (!topOnly || cy >= chests[i].pos.y - PROP_BOTTOM_ALLOW))
                            return true;
                    if (customerState.active)
                        if (fabsf(cx - customerState.customer.position.x) < PW + PROP_HW &&
                            fabsf(cy - customerState.customer.position.y) < PH + PROP_HH &&
                            (!topOnly || cy >= customerState.customer.position.y - PROP_BOTTOM_ALLOW))
                            return true;
                    return false;
                };

                if (isPropBlocked(PlayerSystem::p1->position.x, prevPos.y, true))
                    PlayerSystem::p1->position.x = prevPos.x;
                if (isPropBlocked(PlayerSystem::p1->position.x, PlayerSystem::p1->position.y, true))
                    PlayerSystem::p1->position.y = prevPos.y;
            }

            // Plant system
            if (currentTask != TASK_INTRO)
                plantSystem::PlantSystem_Update(plantState, dt,
                    PlayerSystem::p1->position, PlayerSystem::p1->held,
                    potPositions, potCount, chests, chestCount);

            // ── TUTORIAL TASK SYSTEM ─────────────────────────────────────────────
            //  All tasks and its conditions are listed below
            // ─────────────────────────────────────────────────────────────────────
            // Panel slide animation (timer only advances during transitions)
            if (panelState == PANEL_SLIDING_IN || panelState == PANEL_SLIDING_OUT)
            {
                panelSlideTimer += dt;
                if (panelState == PANEL_SLIDING_IN && panelSlideTimer >= PANEL_SLIDE_DUR)
                    { panelState = PANEL_VISIBLE;  panelSlideTimer = 0.f; }
                else if (panelState == PANEL_SLIDING_OUT && panelSlideTimer >= PANEL_SLIDE_DUR)
                {
                    panelSlideTimer = 0.f;

                    if (currentTask == TASK_RETRY)
                    {
                        // Non-linear: restart at harvest or sell depending on player state
                        currentTask = (PlayerSystem::p1->held.type == HeldItem::FLOWER)
                                      ? TASK_SELLING : TASK_HARVEST;
                        CustomerSystem::CustomerSystem_Spawn(customerState);
                        panelState = PANEL_SLIDING_IN;
                    }
                    else if (currentTask == TASK_SELLING)
                    {
                        // Sale complete — skip TASK_RETRY and go directly to end
                        currentTask = TASK_ALL_DONE;
                        panelState  = PANEL_HIDDEN;
                    }
                    else if (currentTask == TASK_WATER_PLANT)
                    {
                        // If plant fully grew on its own, skip TASK_WAITING → TASK_HARVEST
                        bool alreadyGrown = false;
                        for (int i = 0; i < plantState.potCount; ++i)
                            if (plantState.plants[i].active &&
                                plantState.plants[i].stage == plantSystem::STAGE_FULLY_GROWN)
                                { alreadyGrown = true; break; }

                        currentTask = alreadyGrown ? TASK_HARVEST : TASK_WAITING;
                        CustomerSystem::CustomerSystem_Spawn(customerState); // spawn in both paths
                        panelState  = PANEL_SLIDING_IN;
                    }
                    else
                    {
                        // Normal linear advance
                        currentTask = static_cast<GameplayTask>(currentTask + 1);
                        panelState  = (currentTask < TASK_ALL_DONE) ? PANEL_SLIDING_IN
                                                                      : PANEL_HIDDEN;
                        // Spawn customer as soon as TASK_WAITING begins
                        if (currentTask == TASK_WAITING)
                            CustomerSystem::CustomerSystem_Spawn(customerState);
                    }
                }
            }

            if (panelState != PANEL_HIDDEN)
                pointerBounceTimer += dt;

            // TASK_INTRO: Advance on LMB
            if (currentTask == TASK_INTRO && panelState == PANEL_VISIBLE &&
                AEInputCheckTriggered(AEVK_LBUTTON))
            {
                panelState      = PANEL_SLIDING_OUT;
                panelSlideTimer = 0.f;
            }

            // TASK_PICK_SEED: Player to pick up seed from chest
            if (currentTask == TASK_PICK_SEED && panelState == PANEL_VISIBLE &&
                PlayerSystem::p1->held.type == HeldItem::SEED)
            {
                panelState      = PANEL_SLIDING_OUT;
                panelSlideTimer = 0.f;
            }

            // TASK_PLANT_SEED: player planted a seed in a pot
            if (currentTask == TASK_PLANT_SEED && panelState == PANEL_VISIBLE)
            {
                for (int i = 0; i < plantState.potCount; ++i)
                {
                    if (plantState.plants[i].active)
                    {
                        panelState      = PANEL_SLIDING_OUT;
                        panelSlideTimer = 0.f;
                        break;
                    }
                }
            }

            // TASK_WAITING: wait for the planted seed to fully grow (no player input needed)
            if (currentTask == TASK_WAITING && panelState == PANEL_VISIBLE)
            {
                for (int i = 0; i < plantState.potCount; ++i)
                {
                    if (plantState.plants[i].active &&
                        plantState.plants[i].stage == plantSystem::STAGE_FULLY_GROWN)
                    {
                        panelState      = PANEL_SLIDING_OUT;
                        panelSlideTimer = 0.f;
                        break;
                    }
                }
            }

            // TASK_WATER_PLANT: advance when player waters OR plant already fully grown
            if (currentTask == TASK_WATER_PLANT && panelState == PANEL_VISIBLE)
            {
                for (int i = 0; i < plantState.potCount; ++i)
                {
                    if (plantState.plants[i].isWatered ||
                        plantState.plants[i].stage == plantSystem::STAGE_FULLY_GROWN)
                    {
                        panelState      = PANEL_SLIDING_OUT;
                        panelSlideTimer = 0.f;
                        break;
                    }
                }
            }

            // TASK_HARVEST: Player harvested the fully-grown flower (E near pot)
            if (currentTask == TASK_HARVEST && panelState == PANEL_VISIBLE &&
                PlayerSystem::p1->held.type == HeldItem::FLOWER)
            {
                panelState      = PANEL_SLIDING_OUT;
                panelSlideTimer = 0.f;
            }

            // TASK_SELLING: Player presses E near customer while holding flower
            if (currentTask == TASK_SELLING && panelState == PANEL_VISIBLE &&
                CustomerSystem::IsWaiting(customerState) &&
                AEInputCheckTriggered(AEVK_E) &&
                PlayerSystem::p1->held.type == HeldItem::FLOWER)
            {
                float dx = customerState.customer.position.x - PlayerSystem::p1->position.x;
                float dy = customerState.customer.position.y - PlayerSystem::p1->position.y;
                if (std::sqrt(dx * dx + dy * dy) < plantSystem::INTERACT_RADIUS)
                {
                    CustomerSystem::CustomerSystem_Serve(customerState);
                    Gold::Earn(GOLD_REWARD_FLOWER_SALE);
                    PlayerSystem::p1->held = HeldState{};   // clear flower; MC returns to non-carry anim
                    panelState      = PANEL_SLIDING_OUT;
                    panelSlideTimer = 0.f;
                }
            }

            // TASK_RETRY: Player acknowledges the failure and requests a retry (LMB)
            if (currentTask == TASK_RETRY && panelState == PANEL_VISIBLE &&
                AEInputCheckTriggered(AEVK_LBUTTON))
            {
                panelState      = PANEL_SLIDING_OUT;
                panelSlideTimer = 0.f;
            }

            //------------------------------------------------------------------
            // Water particles — radial spray from pot centre
            // (rate-limited; only fires when a nearby active plant is being watered)
            //------------------------------------------------------------------
            particleState.emitTimer -= dt;
            if (AEInputCheckCurr(AEVK_Q) && particleState.emitTimer <= 0.f)
            {
                int waterIdx = plantSystem::PlantSystem_NearestPlant(
                    plantState, PlayerSystem::p1->position);
                if (waterIdx >= 0 && plantState.can.water > 0.f)
                {
                    ParticleSystem::EmitWater(particleState,
                        plantState.plants[waterIdx].pos);
                    particleState.emitTimer = 0.05f; // 20 bursts/sec
                }
            }
            ParticleSystem::Update(particleState, dt);
            CustomerSystem::CustomerSystem_Update(customerState, dt);

            // Patience-ran-out detection (one-shot: guard becomes false after task changes)
            if (customerState.customer.patienceRanOut &&
                (currentTask == TASK_HARVEST || currentTask == TASK_SELLING))
            {
                Gold::ApplyPenalty(GOLD_PENALTY_IMPATIENT);
                currentTask     = TASK_RETRY;
                panelState      = PANEL_VISIBLE;   // panel already on-screen; text updates instantly
                panelSlideTimer = 0.f;
            }
        }
    }
}

void Tutorial_Draw()
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

        // ── Floor tiles ─────────────────────────────────────────────────────
        for (int row = 0; row < collisionMap.height; ++row)
        {
            for (int col = 0; col < collisionMap.width; ++col)
            {
                if (collisionMap.solid[static_cast<size_t>(row) * collisionMap.width + col]) continue;

                float cx  = collisionMap.origin.x + col * collisionMap.tileSize + collisionMap.tileSize * 0.5f;
                float cy  = collisionMap.origin.y + row * collisionMap.tileSize + collisionMap.tileSize * 0.5f;
                AEGfxTexture* tex = ((col + row) % 2 == 0) ? floorOneTex : floorTwoTex;
                BasicUtilities::drawUICard(tileMesh, tex, cx, cy,
                                           collisionMap.tileSize, collisionMap.tileSize);
            }
        }

        // ── World objects: pots, chests, plants (below player) ───────────────
        plantSystem::PlantSystem_Draw(plantState, chests, chestCount);

        // ── Customer sprite (always below player) ────────────────────────────
        CustomerSystem::CustomerSystem_Draw(customerState, fontId);

        // ── Player + particles — draw order based on perspective ─────────────
        // Facing UP  → player's back is to the camera (player is "in front of"
        //              the pot), so player is drawn on top of the particles.
        // All other  → player faces the camera (player is "behind" the pot),
        //              so particles appear on top of the player.
        if (PlayerSystem::p1->facing == Entity::FaceDirection::UP)
        {
            ParticleSystem::Draw(particleState);
            PlayerSystem::Draw();
        }
        else
        {
            PlayerSystem::Draw();
            ParticleSystem::Draw(particleState);
        }

        // ── Customer UI (patience bar + order bubble) — always above player ──
        CustomerSystem::CustomerSystem_DrawUI(customerState, fontId);

        // ── Info panel — world-space; camera pans up to reveal it ────────────
        // Panel background + icons use drawUICard (camera-aware ✓)
        BasicUtilities::drawUICard(squareMesh, infoPanelTex,
            PANEL_POS.x, PANEL_POS.y, PANEL_W, PANEL_H);

        BasicUtilities::drawUICard(squareMesh, goldIconTex,
            PANEL_POS.x + GOLD_ICON_OFF.x, PANEL_POS.y + GOLD_ICON_OFF.y,
            ICON_SIZE, ICON_SIZE);

        BasicUtilities::drawUICard(squareMesh, clockIconTex,
            PANEL_POS.x + CLOCK_ICON_OFF.x, PANEL_POS.y + CLOCK_ICON_OFF.y,
            ICON_SIZE, ICON_SIZE);

        // Text: camera-compensated (world Y − camera Y = screen Y)
        // Camera X is always 0 in this state, so world X = screen X.
        char goldBuf[16], timeBuf[16];
        sprintf_s(goldBuf, sizeof(goldBuf), "%d", Gold::GetTotal());
        TimeOfDay::GetClockString(timeBuf, sizeof(timeBuf));

        BasicUtilities::drawText(fontId, goldBuf,
            PANEL_POS.x + GOLD_TEXT_OFF.x,
            (PANEL_POS.y + GOLD_TEXT_OFF.y) - currentCamY,
            0.8f, 1.f, 0.85f, 0.f);        // gold colour

        BasicUtilities::drawText(fontId, timeBuf,
            PANEL_POS.x + CLOCK_TEXT_OFF.x,
            (PANEL_POS.y + CLOCK_TEXT_OFF.y) - currentCamY,
            0.8f, 1.f, 1.f, 1.f);           // white

        // ── Tutorial task panel + pointer ────────────────────────────────────
        if (panelState != PANEL_HIDDEN)
        {
            // Compute eased panel Y
            float py;
            {
                float t = panelSlideTimer / PANEL_SLIDE_DUR;
                if (t > 1.f) t = 1.f;
                if (panelState == PANEL_SLIDING_IN)
                {
                    float te = 1.f - (1.f - t) * (1.f - t); // quad ease-out
                    py = TASK_PANEL_Y_OFF + (TASK_PANEL_Y_ON - TASK_PANEL_Y_OFF) * te;
                }
                else if (panelState == PANEL_SLIDING_OUT)
                {
                    float te = t * t; // quad ease-in
                    py = TASK_PANEL_Y_ON + (TASK_PANEL_Y_OFF - TASK_PANEL_Y_ON) * te;
                }
                else
                {
                    py = TASK_PANEL_Y_ON;
                }
            }

            BasicUtilities::drawUICard(squareMesh, taskPanelTex,
                0.f, py, TASK_PANEL_W, TASK_PANEL_H);

            if (currentTask >= TASK_INTRO && currentTask < TASK_ALL_DONE)
            {
                constexpr float TEXT_SCALE   = 0.8f;
                constexpr float PROMPT_SCALE = 0.7f;
                constexpr float TEXT_MAX_W   = TASK_PANEL_W - 60.f;  // 30 px padding each side

                const TaskData& td = TASK_DATA[currentTask];

                // With prompt: start main text above centre to leave room below
                // Without prompt: centre main text on panel Y
                const float mainStartY = td.prompt ? py + 25.f : py;

                float belowMain = BasicUtilities::drawTextWrapped(
                    fontId, td.text, 0.f, mainStartY,
                    TEXT_SCALE, 0.2f, 0.1f, 0.05f, TEXT_MAX_W);

                if (td.prompt)
                    BasicUtilities::drawText(fontId, td.prompt,
                        0.f, belowMain - 4.f,
                        PROMPT_SCALE, 0.55f, 0.55f, 0.55f);
            }

            drawTaskPointer();
        }

        //========================================================================
        // DEBUG MODE
        //========================================================================
        if (Debug::enabled)
        {
            // Setup: colour-only translucent fill (colour set per entity type below)
            AEMtx33 sclMtx{}, trsMtx{}, mtx{};
            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

            // Lambda: draw one AABB with a given colour (tunable via debug.hpp)
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
                drawAABB(PlayerSystem::p1->position.x, PlayerSystem::p1->position.y,
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
            // Customer AABB
            if (customerState.active)
                drawAABB(customerState.customer.position.x, customerState.customer.position.y,
                         PROP_COLL_HW, PROP_COLL_HH,
                         Debug::CUSTOMER_R, Debug::CUSTOMER_G, Debug::CUSTOMER_B);

            // Debug status text
            if (PlayerSystem::p1)
            {
                const auto& dbgP = *PlayerSystem::p1;

                const char* faceStr =
                    dbgP.facing == Entity::FaceDirection::UP    ? "UP"    :
                    dbgP.facing == Entity::FaceDirection::DOWN  ? "DOWN"  :
                    dbgP.facing == Entity::FaceDirection::LEFT  ? "LEFT"  : "RIGHT";

                const char* heldStr =
                    dbgP.held.type == HeldItem::SEED   ? "SEED"   :
                    dbgP.held.type == HeldItem::FLOWER ? "FLOWER" : "NONE";

                char dbgBuf[64];

                BasicUtilities::drawText(fontId, "[DEBUG]",
                    -790.f, -380.f, 0.7f, 1.f, 1.f, 0.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "pos  %.1f  %.1f",
                    dbgP.position.x, dbgP.position.y);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -410.f, 0.6f, 1.f, 1.f, 1.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "face %s", faceStr);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -430.f, 0.6f, 1.f, 1.f, 1.f, 1.f);

                sprintf_s(dbgBuf, sizeof(dbgBuf), "held %s", heldStr);
                BasicUtilities::drawText(fontId, dbgBuf,
                    -790.f, -450.f, 0.6f, 1.f, 1.f, 1.f, 1.f);
            }
        }
    }
}

void Tutorial_Free()
{
    AEGfxMeshFree(squareMesh);

    // Gameplay stage
    AEGfxMeshFree(tileMesh);
    tileMesh = nullptr;
    PlayerSystem::Free();
    plantSystem::PlantSystem_Free(plantState);
    ParticleSystem::Free(particleState);
    CustomerSystem::CustomerSystem_Free(customerState);

    // [!] So it doesn't affect other states, cause fuck me it affected
    AEGfxSetCamPosition(0.f, 0.f);
}

void Tutorial_Unload()
{
    AEGfxTextureUnload(uiCardTexture);
    AEGfxDestroyFont(fontId);

    // Gameplay stage
    AEGfxTextureUnload(floorOneTex);
    AEGfxTextureUnload(floorTwoTex);
    floorOneTex = floorTwoTex = nullptr;
    Collision::Map_Unload(collisionMap);
    plantSystem::PlantSystem_Unload(plantState);
    CustomerSystem::CustomerSystem_Unload(customerState);

    // Info panel
    AEGfxTextureUnload(infoPanelTex);
    AEGfxTextureUnload(goldIconTex);
    AEGfxTextureUnload(clockIconTex);
    infoPanelTex = goldIconTex = clockIconTex = nullptr;

    // Tutorial task textures
    AEGfxTextureUnload(taskPanelTex);
    AEGfxTextureUnload(pointerTex);
    taskPanelTex = pointerTex = nullptr;
}
