#include <iostream>
#include <cstdio>
#include <cmath>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "level2.hpp"
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
    AEGfxTexture* uiCardTexture = nullptr;
    AEGfxVertexList* squareMesh = nullptr;
    s8 fontId = -1;

    enum Level2Stage { STAGE_TEXT_SEQUENCE, STAGE_GAMEPLAY };
    Level2Stage currentStage = STAGE_TEXT_SEQUENCE;

    enum Level2Phase { FADE_IN, HOLD, FADE_OUT, BG_TRANSITION, COMPLETE };
    Level2Phase currentPhase = FADE_IN;

    float phaseTimer = 0.f;
    float alpha = 0.f;
    int currentTextIndex = 0;

    const float FADE_IN_DURATION = 1.5f;
    const float HOLD_DURATION = 2.5f;
    const float FADE_OUT_DURATION = 1.0f;
    const float BG_TRANSITION_DURATION = 1.5f;

    float red = 0.f, green = 0.f, blue = 0.f;
    float backgroundTimer = 0.f;

    const float targetRed = 0.85f;
    const float targetGreen = 0.84f;
    const float targetBlue = 0.80f;

    struct TextEntry { const char* text; float x, y, scale, r, g, b; };

    const TextEntry textSequence[] =
    {
        { "day two", 0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
        { "business is picking up", 0.f, 0.f, 1.f, 1.f, 1.f, 1.f },
    };
    const int TEXT_COUNT = static_cast<int>(sizeof(textSequence) / sizeof(textSequence[0]));

    AEGfxTexture* floorOneTex = nullptr;
    AEGfxTexture* floorTwoTex = nullptr;
    AEGfxVertexList* tileMesh = nullptr;
    Collision::Map collisionMap;

    static const SeedType s_chestSeeds[] = {
        SeedType::ROSE, SeedType::TULIP, SeedType::SUNFLOWER,
        SeedType::DAISY, SeedType::LILY, SeedType::ORCHID
    };

    constexpr int MAX_MAP_CHESTS = 8;

    AEVec2 potPositions[plantSystem::MAX_PLANTS];
    int potCount = 0;
    plantSystem::ChestData chests[MAX_MAP_CHESTS];
    int chestCount = 0;

    float chestTooltipT[MAX_MAP_CHESTS]{};
    float heldTooltipT = 0.f;
    float customerTooltipT[CustomerSystem::POOL_MAX]{};

    plantSystem::State plantState;
    ParticleSystem::State particleState;

    AEVec2 runeTablePositions[TableSystem::MAX_TABLES];
    int runeTableCount = 0;
    AEVec2 poisonTablePositions[TableSystem::MAX_TABLES];
    int poisonTableCount = 0;
    TableSystem::State tableState;

    CustomerSystem::PoolState customerPool;

    static const AEVec2 CUSTOMER_SPAWN_POS = { 0.0f, -225.f };

    static const AEVec2 CUSTOMER_TARGETS[] = {
        { -384.f,  -96.f }, { -256.f,  -96.f }, { -128.f,  -96.f },
        {   64.f,  -96.f }, {  320.f,  -96.f },
        { -350.f, -200.f }, {  -80.f, -200.f }, {  350.f, -200.f },
    };
    constexpr int CUSTOMER_TARGET_COUNT =
        static_cast<int>(sizeof(CUSTOMER_TARGETS) / sizeof(CUSTOMER_TARGETS[0]));

    bool cameraReady = false;
    float camPanTimer = 0.f;
    const float CAM_PAN_DURATION = 2.0f;
    const float CAM_START_Y = -750.f;
    const float CAM_END_Y = 0.f;
    float currentCamY = CAM_START_Y;

    AEGfxTexture* infoPanelTex = nullptr;
    AEGfxTexture* goldIconTex = nullptr;
    AEGfxTexture* clockIconTex = nullptr;

    const AEVec2 PANEL_POS = { 640.f, 400.f };
    const float PANEL_W = 300.f;
    const float PANEL_H = 80.f;
    const float ICON_SIZE = 30.f;

    const AEVec2 GOLD_ICON_OFF = { -100.f, 0.f };
    const AEVec2 GOLD_TEXT_OFF = { -65.f, 0.f };
    const AEVec2 CLOCK_ICON_OFF = { 10.f, 0.f };
    const AEVec2 CLOCK_TEXT_OFF = { 75.f, 0.f };

    AEGfxTexture* flowerIconTex[7] = {};
    AEGfxTexture* seedPackTex[7] = {};

    enum class Level2End { PLAYING, WIN, LOSE };
    Level2End endState = Level2End::PLAYING;
    float fadeAlpha = 0.f;

    constexpr int GOLD_WIN_TARGET = 60;

    Merchant merchant;
    bool merchantStarted = false;

    // Consumed at the start of this level from gRunBonuses.nextLevelCoinMultiplier
    float levelCoinMultiplier = 1.0f;

    constexpr float PLAYER_HW = PlayerSystem::HALF_W;
    constexpr float PLAYER_HH = PlayerSystem::HALF_H;
    constexpr float PROP_COLL_HW = 32.f;
    constexpr float PROP_COLL_HH = 32.f;

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
}

void Level2_Load()
{
    uiCardTexture = BasicUtilities::loadTexture("Assets/ui_card.png");
    fontId = AEGfxCreateFont("Assets/PixelifySans-Regular.ttf", 36);
    if (fontId == -1)
        std::cout << "ERROR! COULD NOT CREATE FONT FOR LEVEL 2\n";

    floorOneTex = BasicUtilities::loadTexture("Assets/floor_one.png");
    floorTwoTex = BasicUtilities::loadTexture("Assets/floor_two.png");

    AEVec2 origin = { -800.f, -448.f };
    Collision::Map_Load("Assets/level1_map_data.txt", collisionMap, 64.f, origin);
    // Change to level2_map_data.txt later if you have one

    plantSystem::PlantSystem_Load(plantState);
    ParticleSystem::Load(particleState);
    TableSystem::TableSystem_Load(tableState);
    MerchantSystem::Load();

    Entity::Load(&collisionMap);
    CustomerSystem::CustomerPool_Load(customerPool);

    infoPanelTex = BasicUtilities::loadTexture("Assets/info_panel.png");
    goldIconTex = BasicUtilities::loadTexture("Assets/gold_icon.png");
    clockIconTex = BasicUtilities::loadTexture("Assets/clock_icon.png");

    for (int i = 1; i <= 6; ++i)
    {
        char path[64];
        sprintf_s(path, sizeof(path), "Assets/flower-assets/flower_icon_%d.png", i);
        flowerIconTex[i] = BasicUtilities::loadTexture(path);
        sprintf_s(path, sizeof(path), "Assets/flower-assets/seed_pack_%d.png", i);
        seedPackTex[i] = BasicUtilities::loadTexture(path);
    }
}

void Level2_Initialise()
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

    cameraReady = false;
    camPanTimer = 0.f;
    currentCamY = CAM_START_Y;
    AEGfxSetCamPosition(0.f, CAM_START_Y);

    AEVec2 spawnBuf[1];
    if (Collision::Map_GetCentres(collisionMap, 2, spawnBuf, 1) > 0)
        PlayerSystem::Init(spawnBuf[0]);
    else
        PlayerSystem::Init({ 0.f, 0.f });

    Entity::Init();

    potCount = Collision::Map_GetCentres(collisionMap, 3, potPositions, plantSystem::MAX_PLANTS);

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

    runeTableCount = Collision::Map_GetCentres(collisionMap, 5, runeTablePositions, TableSystem::MAX_TABLES);
    poisonTableCount = Collision::Map_GetCentres(collisionMap, 6, poisonTablePositions, TableSystem::MAX_TABLES);

    TimeOfDay::Init();

    // Consume next-level-only bonuses at the start of Level 2
    levelCoinMultiplier = gRunBonuses.nextLevelCoinMultiplier;
    gRunBonuses.nextLevelCoinMultiplier = 1.0f;

    Gold::Init(gRunBonuses.nextLevelStartGoldBonus);
    gRunBonuses.nextLevelStartGoldBonus = 0;

    if (!plantState.mesh)         plantSystem::PlantSystem_Load(plantState);
    if (!tableState.mesh)         TableSystem::TableSystem_Load(tableState);
    if (!customerPool.spriteMesh) CustomerSystem::CustomerPool_Load(customerPool);
    if (!particleState.mesh)      ParticleSystem::Load(particleState);

    plantSystem::PlantSystem_Init(plantState, potPositions, potCount);
    TableSystem::TableSystem_Init(
        tableState,
        runeTablePositions, runeTableCount,
        poisonTablePositions, poisonTableCount);

    CustomerSystem::CustomerPool_Init(
        customerPool,
        CUSTOMER_SPAWN_POS,
        CUSTOMER_TARGETS,
        CUSTOMER_TARGET_COUNT);

    endState = Level2End::PLAYING;
    fadeAlpha = 0.f;

    MerchantSystem::Init(merchant);
    merchantStarted = false;

    // Apply permanent boosts from Level 1 merchant onward but got error so needa fix this 
    //if (PlayerSystem::p1)
    //    PlayerSystem::p1->speed += gRunBonuses.speedLevel * 50.f;

    //plantState.can.capacity += gRunBonuses.waterLevel * 2;
}

void Level2_Update()
{
    float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());

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
                red = targetRed;
                green = targetGreen;
                blue = targetBlue;
                currentPhase = COMPLETE;
            }
            break;
        }

        case COMPLETE:
            currentStage = STAGE_GAMEPLAY;
            break;
        }

        return;
    }

    TimeOfDay::Update(dt);

    if (!cameraReady)
    {
        camPanTimer += dt;
        float t = camPanTimer / CAM_PAN_DURATION;
        if (t > 1.f) t = 1.f;
        float tEased = 1.f - (1.f - t) * (1.f - t);

        currentCamY = CAM_START_Y + (CAM_END_Y - CAM_START_Y) * tEased;
        AEGfxSetCamPosition(0.f, currentCamY);

        if (camPanTimer >= CAM_PAN_DURATION)
        {
            currentCamY = CAM_END_Y;
            AEGfxSetCamPosition(0.f, CAM_END_Y);
            cameraReady = true;
        }

        return;
    }

    if (AEInputCheckTriggered(AEVK_LBRACKET))
        Debug::enabled = !Debug::enabled;

    if (endState == Level2End::PLAYING && TimeOfDay::HasEnded())
    {
        endState = (Gold::GetTotal() >= GOLD_WIN_TARGET)
            ? Level2End::WIN : Level2End::LOSE;
    }

    if (endState != Level2End::PLAYING)
    {
        fadeAlpha = std::fminf(fadeAlpha + dt * 0.5f, 1.f);

        if (endState == Level2End::WIN)
        {
            if (!merchantStarted && AEInputCheckTriggered(AEVK_R))
            {
                nextState = GS_RESTART;
                return;
            }

            if (!merchantStarted && AEInputCheckTriggered(AEVK_SPACE))
            {
                MerchantSystem::Start(merchant, gRunBonuses);
                gRunBonuses.nextMerchantDiscount = 0.0f;
                merchantStarted = true;
            }

            if (merchantStarted)
            {
                MerchantSystem::Update(merchant, dt);
                MerchantSystem::HandleMousePurchase(merchant, gRunBonuses);

                if (!merchant.active)
                {
                    // Change this later if you add GS_LEVEL3
                    nextState = GS_RESTART;
                }
            }

            return;
        }

        if (fadeAlpha >= 1.f && AEInputCheckTriggered(AEVK_R))
            nextState = GS_RESTART;

        return;
    }

    constexpr float ANIM_SPEED = 6.0f;

    for (int i = 0; i < chestCount; ++i)
    {
        bool inRange =
            std::fabsf(chests[i].pos.x - PlayerSystem::p1->GetCoordinates().x) < PLAYER_HW + PROP_COLL_HW &&
            std::fabsf(chests[i].pos.y - PlayerSystem::p1->GetCoordinates().y) < PLAYER_HH + PROP_COLL_HH;
        BasicUtilities::tickEase(chestTooltipT[i], inRange, dt, ANIM_SPEED);
    }

    BasicUtilities::tickEase(heldTooltipT,
        PlayerSystem::p1->held.type != HeldItem::NONE, dt, ANIM_SPEED);

    for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
    {
        const auto& sl = customerPool.slots[ci];
        BasicUtilities::tickEase(customerTooltipT[ci],
            sl.active && sl.customer->state == CustomerState::WAITING, dt, ANIM_SPEED);
    }

    AEVec2 prevPos = PlayerSystem::p1->GetCoordinates();
    Entity::Update();

    {
        constexpr float PW = PLAYER_HW, PH = PLAYER_HH;
        constexpr float PROP_HW = PROP_COLL_HW, PROP_HH = PROP_COLL_HH;
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

                for (int i = 0; i < tableState.count; ++i)
                    if (fabsf(cx - tableState.tables[i].pos.x) < PW + TableSystem::TABLE_COLL_HW &&
                        fabsf(cy - tableState.tables[i].pos.y) < PH + TableSystem::TABLE_COLL_HH &&
                        (!topOnly || cy >= tableState.tables[i].pos.y - PROP_BOTTOM_ALLOW))
                        return true;

                for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
                {
                    const auto& sl = customerPool.slots[ci];
                    if (!sl.active || sl.customer->state != CustomerState::WAITING) continue;
                    if (fabsf(cx - sl.customer->GetCoordinates().x) < PW + PROP_HW &&
                        fabsf(cy - sl.customer->GetCoordinates().y) < PH + PROP_HH &&
                        (!topOnly || cy >= sl.customer->GetCoordinates().y - PROP_BOTTOM_ALLOW))
                        return true;
                }

                return false;
            };

        if (isPropBlocked(PlayerSystem::p1->GetCoordinates().x, prevPos.y, true))
            PlayerSystem::p1->RefX() = prevPos.x;
        if (isPropBlocked(PlayerSystem::p1->GetCoordinates().x, PlayerSystem::p1->GetCoordinates().y, true))
            PlayerSystem::p1->RefY() = prevPos.y;
    }

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
            int finalGold = static_cast<int>(gold * levelCoinMultiplier);
            Gold::Earn(finalGold);
            PlayerSystem::p1->held = HeldState{};
            ePressHandled = true;
        }
    }

    if (!ePressHandled)
        TableSystem::TableSystem_Update(tableState, dt,
            PlayerSystem::p1->GetCoordinates(), PlayerSystem::p1->held);

    plantSystem::PlantSystem_Update(plantState, dt,
        PlayerSystem::p1->GetCoordinates(), PlayerSystem::p1->held,
        potPositions, potCount, chests, chestCount);

    CustomerSystem::CustomerPool_Update(customerPool, dt);

    particleState.emitTimer -= dt;
    if (AEInputCheckCurr(AEVK_Q) && particleState.emitTimer <= 0.f)
    {
        int waterIdx = plantSystem::PlantSystem_NearestPlant(
            plantState, PlayerSystem::p1->GetCoordinates());
        if (waterIdx >= 0 && plantState.can.water > 0.f)
        {
            ParticleSystem::EmitWater(particleState, plantState.plants[waterIdx].pos);
            particleState.emitTimer = 0.05f;
        }
    }
    ParticleSystem::Update(particleState, dt);
}

void Level2_Draw()
{
    if (currentStage == STAGE_TEXT_SEQUENCE)
    {
        AEGfxSetBackgroundColor(red, green, blue);

        if (currentPhase == FADE_IN || currentPhase == HOLD || currentPhase == FADE_OUT)
        {
            if (currentTextIndex < TEXT_COUNT)
            {
                const TextEntry& e = textSequence[currentTextIndex];
                BasicUtilities::drawText(fontId, e.text, e.x, e.y, e.scale, e.r, e.g, e.b, alpha);
            }
        }

        return;
    }

    float r, g, b;
    TimeOfDay::GetBackgroundColor(r, g, b);
    AEGfxSetBackgroundColor(r, g, b);

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

    plantSystem::PlantSystem_Draw(plantState, chests, chestCount);
    TableSystem::TableSystem_Draw(tableState, flowerIconTex);
    CustomerSystem::CustomerPool_Draw(customerPool, fontId);
    Entity::Draw();

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

    {
        float smoothedCustomerT[CustomerSystem::POOL_MAX]{};
        for (int ci = 0; ci < CustomerSystem::POOL_MAX; ++ci)
            smoothedCustomerT[ci] = BasicUtilities::smoothstep(customerTooltipT[ci]);
        CustomerSystem::CustomerPool_DrawUI(customerPool, fontId, flowerIconTex, smoothedCustomerT);
    }

    BasicUtilities::drawUICard(squareMesh, infoPanelTex, PANEL_POS.x, PANEL_POS.y, PANEL_W, PANEL_H);
    BasicUtilities::drawUICard(squareMesh, goldIconTex,
        PANEL_POS.x + GOLD_ICON_OFF.x, PANEL_POS.y + GOLD_ICON_OFF.y, ICON_SIZE, ICON_SIZE);
    BasicUtilities::drawUICard(squareMesh, clockIconTex,
        PANEL_POS.x + CLOCK_ICON_OFF.x, PANEL_POS.y + CLOCK_ICON_OFF.y, ICON_SIZE, ICON_SIZE);

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

    if (endState != Level2End::PLAYING)
    {
        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);

        float overlayAlpha = fadeAlpha;
        if (endState == Level2End::WIN)
            overlayAlpha = 0.65f;

        AEGfxSetTransparency(overlayAlpha);
        AEGfxSetColorToMultiply(0.10f, 0.06f, 0.03f, 1.f);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

        AEMtx33 sclMtx{}, trsMtx{}, mtx{};
        AEMtx33Scale(&sclMtx, 1600.f, 900.f);
        AEMtx33Trans(&trsMtx, 0.f, currentCamY);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);

        if (fadeAlpha >= 1.f)
        {
            const bool win = (endState == Level2End::WIN);
            const char* msg = win ? "YOU WIN!" : "YOU LOSE";
            const float mr = 1.f;
            const float mg = win ? 0.85f : 0.2f;
            const float mb = win ? 0.0f : 0.2f;

            BasicUtilities::drawText(fontId, msg, 0.f, 50.f, 2.0f, mr, mg, mb, 1.f);

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

        if (endState == Level2End::WIN && merchantStarted)
            MerchantSystem::Draw(merchant, fontId);
    }
}

void Level2_Free()
{
    AEGfxMeshFree(squareMesh); squareMesh = nullptr;
    AEGfxMeshFree(tileMesh);   tileMesh = nullptr;
    Entity::Free();
    plantSystem::PlantSystem_Free(plantState);
    ParticleSystem::Free(particleState);
    TableSystem::TableSystem_Free(tableState);
    CustomerSystem::CustomerPool_Free(customerPool);
    AEGfxSetCamPosition(0.f, 0.f);
    MerchantSystem::Free();
}

void Level2_Unload()
{
    if (uiCardTexture)
    {
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

    for (int i = 1; i <= 6; ++i)
    {
        if (flowerIconTex[i]) { AEGfxTextureUnload(flowerIconTex[i]); flowerIconTex[i] = nullptr; }
        if (seedPackTex[i]) { AEGfxTextureUnload(seedPackTex[i]);   seedPackTex[i] = nullptr; }
    }
}