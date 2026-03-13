#include "merchant.hpp"

#include "AEGraphics.h"
#include "AEInput.h"
#include "AEMtx33.h"
#include "gold.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>

namespace Sprite = BasicUtilities::Sprite;

namespace
{
    Sprite::SpriteData     gMerchantSprite;
    Sprite::AnimationState gMerchantAnimState;

    AEGfxVertexList* gQuadMesh = nullptr;
    AEGfxVertexList* gMerchantMesh = nullptr;

    constexpr float MERCHANT_SCALE = 180.0f;
    constexpr float ARRIVE_Y = -40.0f;

    // ------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------
    constexpr int   SHOP_SLOTS = 3;
    constexpr float CARD_W = 210.0f;
    constexpr float CARD_H = 165.0f;
    constexpr float CARD_HALF_W = CARD_W * 0.5f;
    constexpr float CARD_HALF_H = CARD_H * 0.5f;

    constexpr float CARD_Y = -185.0f;
    constexpr float CARD_X[SHOP_SLOTS] = { -250.0f, 0.0f, 250.0f };

    constexpr float SPEECH_X = 165.0f;
    constexpr float SPEECH_Y = 105.0f;
    constexpr float SPEECH_W = 430.0f;
    constexpr float SPEECH_H = 185.0f;

    constexpr float MERCHANT_UI_X = 520.0f;
    constexpr float MERCHANT_UI_Y = -210.0f;

    constexpr float BACKDROP_W = 1100.0f;
    constexpr float BACKDROP_H = 700.0f;

    constexpr float WALL_X = 0.0f;
    constexpr float WALL_Y = 40.0f;
    constexpr float WALL_W = 840.0f;
    constexpr float WALL_H = 470.0f;

    // ------------------------------------------------------------
    // Random
    // ------------------------------------------------------------
    std::mt19937& RNG()
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng;
    }

    // ------------------------------------------------------------
    // Utility drawing
    // ------------------------------------------------------------
    void DrawQuad(float x, float y, float w, float h,
        float r, float g, float b, float a)
    {
        if (!gQuadMesh) return;

        AEMtx33 scl{}, trs{}, mtx{};
        AEMtx33Scale(&scl, w, h);
        AEMtx33Trans(&trs, x, y);
        AEMtx33Concat(&mtx, &trs, &scl);

        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(a);
        AEGfxSetColorToMultiply(r, g, b, 1.0f);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(gQuadMesh, AE_GFX_MDM_TRIANGLES);
    }

    void SetMessage(Merchant& m, const char* msg, float timeSec = 2.0f)
    {
        if (!msg) return;
        strcpy_s(m.uiMessage, sizeof(m.uiMessage), msg);
        m.uiMessageTimer = timeSec;
    }

    void GetSlotPosition(int i, float& x, float& y)
    {
        if (i < 0 || i >= SHOP_SLOTS)
        {
            x = 0.0f;
            y = 0.0f;
            return;
        }

        x = CARD_X[i];
        y = CARD_Y;
    }

    int GetHoveredSlotIndex(float worldX, float worldY)
    {
        for (int i = 0; i < SHOP_SLOTS; ++i)
        {
            float x = 0.f, y = 0.f;
            GetSlotPosition(i, x, y);

            const bool inside =
                worldX >= x - CARD_HALF_W && worldX <= x + CARD_HALF_W &&
                worldY >= y - CARD_HALF_H && worldY <= y + CARD_HALF_H;

            if (inside)
                return i;
        }

        return -1;
    }

    unsigned int GetAnimTagIndex(Entity::FaceDirection facing)
    {
        switch (facing)
        {
        case Entity::FaceDirection::LEFT:  return 0u;
        case Entity::FaceDirection::RIGHT: return 1u;
        case Entity::FaceDirection::UP:    return 2u;
        case Entity::FaceDirection::DOWN:  return 3u;
        default:                           return 3u;
        }
    }

    unsigned int GetAnimTexSlot(Entity::MoveState moveState)
    {
        return (moveState == Entity::MoveState::WALKING) ? 1u : 0u;
    }

    // ------------------------------------------------------------
    // Shop item helpers
    // ------------------------------------------------------------
    int ApplyDiscountToPrice(int basePrice, const RunBonuses& bonuses)
    {
        float multiplier = 1.0f - bonuses.nextMerchantDiscount;
        if (multiplier < 0.1f)
            multiplier = 0.1f;

        int discounted = static_cast<int>(basePrice * multiplier + 0.5f);
        return (discounted < 1) ? 1 : discounted;
    }

    ShopItem MakeItem(
        ShopItemType type,
        int price,
        const char* name,
        const char* desc,
        bool nextLevelOnly = false,
        bool nextMerchantOnly = false)
    {
        ShopItem item{};
        item.type = type;
        item.price = price;
        item.nextLevelOnly = nextLevelOnly;
        item.nextMerchantOnly = nextMerchantOnly;

        if (name)
            strcpy_s(item.name, sizeof(item.name), name);

        if (desc)
            strcpy_s(item.desc, sizeof(item.desc), desc);

        return item;
    }

    std::array<ShopItem, 7> BuildOfferPool(const RunBonuses& bonuses)
    {
        return
        {
            MakeItem(
                ShopItemType::SPEED_BOOST,
                ApplyDiscountToPrice(100, bonuses),
                "better shoes",
                "Move faster for\nthe whole run"),

            MakeItem(
                ShopItemType::WATER_UPGRADE,
                ApplyDiscountToPrice(100, bonuses),
                "more water!",
                "Upgrade watering\nfor the whole run"),

            MakeItem(
                ShopItemType::COIN_MULTIPLIER,
                ApplyDiscountToPrice(120, bonuses),
                "coin boost",
                "Earn more coins\nnext level only",
                true, false),

            MakeItem(
                ShopItemType::START_GOLD,
                ApplyDiscountToPrice(90, bonuses),
                "head start",
                "Start next level\nwith extra gold",
                true, false),

            MakeItem(
                ShopItemType::MERCHANT_FAVOR,
                ApplyDiscountToPrice(80, bonuses),
                "merchant favor",
                "One free reroll\nnext shop",
                false, true),

            MakeItem(
                ShopItemType::DISCOUNT_COUPON,
                ApplyDiscountToPrice(85, bonuses),
                "discount coupon",
                "Next merchant shop\nis cheaper",
                false, true),

            MakeItem(
                ShopItemType::MERCHANT_GAMBLE,
                ApplyDiscountToPrice(70, bonuses),
                "merchant's gamble",
                "Random reward...\nor unlucky draw")
        };
    }

    void RandomizeShopOffers(Merchant& m, const RunBonuses& bonuses)
    {
        auto pool = BuildOfferPool(bonuses);

        std::array<int, 7> indices{ 0,1,2,3,4,5,6 };
        std::shuffle(indices.begin(), indices.end(), RNG());

        for (int i = 0; i < SHOP_SLOTS; ++i)
            m.offers[i] = pool[indices[i]];
    }

    // ------------------------------------------------------------
    // Merchant speech
    // ------------------------------------------------------------
    const char* GetRandomIntroLine()
    {
        static constexpr const char* lines[] =
        {
            "Fresh stock!",
            "Best deals in town.",
            "No refunds.",
            "Care to gamble?",
            "I've got what you need.",
            "Three offers.\nChoose wisely."
        };

        std::uniform_int_distribution<int> dist(0, static_cast<int>(std::size(lines)) - 1);
        return lines[dist(RNG())];
    }

    const char* GetPurchaseLine()
    {
        static constexpr const char* lines[] =
        {
            "A fine choice.",
            "Excellent pick.",
            "That'll serve you well.",
            "Pleasure doing business.",
            "A wise investment.",
            "You won't regret that."
        };

        std::uniform_int_distribution<int> dist(0, static_cast<int>(std::size(lines)) - 1);
        return lines[dist(RNG())];
    }

    // ------------------------------------------------------------
    // Gamble result
    // ------------------------------------------------------------
    void ResolveMerchantGamble(Merchant& m, RunBonuses& bonuses)
    {
        std::uniform_int_distribution<int> roll(1, 100);
        const int r = roll(RNG());

        if (r <= 10)
        {
            Gold::Spend(10);
            SetMessage(m, "Unlucky draw!", 2.5f);
            return;
        }

        if (r <= 35)
        {
            bonuses.speedLevel += 1;
            SetMessage(m, "Lucky!\nSpeed up!", 2.5f);
            return;
        }

        if (r <= 60)
        {
            bonuses.waterLevel += 1;
            SetMessage(m, "Lucky!\nMore water!", 2.5f);
            return;
        }

        if (r <= 80)
        {
            bonuses.nextLevelCoinMultiplier += 0.5f;
            SetMessage(m, "Lucky!\nCoin boost!", 2.5f);
            return;
        }

        if (r <= 92)
        {
            bonuses.nextLevelStartGoldBonus += 15;
            SetMessage(m, "Lucky!\nExtra start gold!", 2.5f);
            return;
        }

        bonuses.freeRerollAvailable = true;
        SetMessage(m, "Very lucky!\nFree reroll next shop!", 2.5f);
    }

    // ------------------------------------------------------------
    // Background decorations
    // ------------------------------------------------------------
    void DrawChest(float x, float y)
    {
        DrawQuad(x, y, 44.0f, 28.0f, 0.35f, 0.20f, 0.08f, 1.0f);
        DrawQuad(x, y + 7.0f, 44.0f, 10.0f, 0.48f, 0.28f, 0.10f, 1.0f);
        DrawQuad(x - 18.0f, y, 5.0f, 28.0f, 0.65f, 0.65f, 0.70f, 1.0f);
        DrawQuad(x + 18.0f, y, 5.0f, 28.0f, 0.65f, 0.65f, 0.70f, 1.0f);
        DrawQuad(x, y - 2.0f, 8.0f, 8.0f, 0.15f, 0.15f, 0.15f, 1.0f);
    }

    void DrawSpeechBubble(float x, float y)
    {
        DrawQuad(x, y, SPEECH_W, SPEECH_H, 1.0f, 1.0f, 1.0f, 1.0f);

        // Bubble tail
        DrawQuad(x + 135.0f, y - 85.0f, 34.0f, 34.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        DrawQuad(x + 150.0f, y - 98.0f, 24.0f, 24.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    void DrawItemIcon(const ShopItem& it, float x, float y)
    {
        switch (it.type)
        {
        case ShopItemType::SPEED_BOOST:
            DrawQuad(x, y, 58.0f, 58.0f, 0.35f, 0.45f, 0.95f, 1.0f);
            DrawQuad(x, y, 28.0f, 28.0f, 0.95f, 0.85f, 0.10f, 1.0f);
            break;

        case ShopItemType::WATER_UPGRADE:
            DrawQuad(x, y, 58.0f, 58.0f, 0.90f, 0.55f, 0.10f, 1.0f);
            DrawQuad(x, y, 22.0f, 30.0f, 0.20f, 0.50f, 0.95f, 1.0f);
            break;

        case ShopItemType::COIN_MULTIPLIER:
            DrawQuad(x, y, 58.0f, 58.0f, 0.25f, 0.25f, 0.80f, 1.0f);
            DrawQuad(x, y, 30.0f, 30.0f, 0.95f, 0.75f, 0.05f, 1.0f);
            break;

        case ShopItemType::START_GOLD:
            DrawQuad(x, y, 58.0f, 58.0f, 0.20f, 0.65f, 0.20f, 1.0f);
            DrawQuad(x, y, 30.0f, 30.0f, 0.95f, 0.75f, 0.05f, 1.0f);
            break;

        case ShopItemType::MERCHANT_FAVOR:
            DrawQuad(x, y, 58.0f, 58.0f, 0.50f, 0.20f, 0.70f, 1.0f);
            DrawQuad(x, y, 26.0f, 26.0f, 0.95f, 0.95f, 0.95f, 1.0f);
            break;

        case ShopItemType::DISCOUNT_COUPON:
            DrawQuad(x, y, 58.0f, 58.0f, 0.85f, 0.20f, 0.30f, 1.0f);
            DrawQuad(x, y, 32.0f, 20.0f, 1.0f, 0.95f, 0.70f, 1.0f);
            break;

        case ShopItemType::MERCHANT_GAMBLE:
            DrawQuad(x, y, 58.0f, 58.0f, 0.55f, 0.35f, 0.12f, 1.0f);
            DrawQuad(x, y, 16.0f, 16.0f, 0.15f, 0.15f, 0.15f, 1.0f);
            break;

        default:
            DrawQuad(x, y, 58.0f, 58.0f, 0.45f, 0.45f, 0.45f, 1.0f);
            break;
        }
    }

    const char* TypeHint(const ShopItem& it)
    {
        if (it.type == ShopItemType::NONE)
            return "";

        if (it.nextLevelOnly)
            return "next lvl";

        if (it.nextMerchantOnly)
            return "merchant";

        return "perm";
    }

    void RerollShop(Merchant& m, const RunBonuses& bonuses)
    {
        RandomizeShopOffers(m, bonuses);
        m.hoveredIndex = -1;
        SetMessage(m, "New stock!", 1.5f);
    }
}

namespace MerchantSystem
{
    void Load()
    {
        gMerchantSprite = Sprite::SpriteData{};
        gMerchantAnimState = Sprite::AnimationState{};

        if (!gQuadMesh)
            gQuadMesh = BasicUtilities::createSquareMesh();

        if (!gMerchantMesh)
            gMerchantMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);

        if (!Sprite::LoadEntry(gMerchantSprite, 0,
            "Assets/Character_sprites/Merchant_Idle.png",
            "Assets/Character_sprites/Merchant_Idle.json"))
        {
            std::cerr << "[Merchant] Failed to load idle sprite.\n";
        }

        if (!Sprite::LoadEntry(gMerchantSprite, 1,
            "Assets/Character_sprites/Merchant_Walk.png",
            "Assets/Character_sprites/Merchant_Walk.json"))
        {
            std::cerr << "[Merchant] Failed to load walk sprite.\n";
        }
    }

    void Init(Merchant& m)
    {
        m = Merchant{};

        m.position = { 0.0f, -400.0f };
        m.speed = 250.0f;
        m.facing = Entity::FaceDirection::UP;
        m.moveState = Entity::MoveState::IDLE;

        for (int i = 0; i < SHOP_SLOTS; ++i)
            GetSlotPosition(i, m.slotX[i], m.slotY[i]);
    }

    void Start(Merchant& m, const RunBonuses& bonuses)
    {
        m.active = true;
        m.arrived = false;
        m.shopOpen = false;
        m.position = { 0.0f, -400.0f };
        m.velocity = { 0.0f, 0.0f };
        m.facing = Entity::FaceDirection::UP;
        m.moveState = Entity::MoveState::WALKING;
        m.hoveredIndex = -1;
        m.uiMessage[0] = '\0';
        m.uiMessageTimer = 0.0f;
        m.rerolledThisVisit = false;
        m.usedFreeRerollThisVisit = false;

        RandomizeShopOffers(m, bonuses);
        SetMessage(m, GetRandomIntroLine(), 2.5f);

        gMerchantAnimState = Sprite::AnimationState{};
    }

    void Update(Merchant& m, float dt)
    {
        if (!m.active)
            return;

        if (!m.arrived)
        {
            m.velocity = { 0.0f, m.speed };
            m.position.y += m.velocity.y * dt;
            m.facing = Entity::FaceDirection::UP;
            m.moveState = Entity::MoveState::WALKING;

            if (m.position.y >= ARRIVE_Y)
            {
                m.position.y = ARRIVE_Y;
                m.velocity = { 0.0f, 0.0f };
                m.arrived = true;
                m.shopOpen = true;
                m.moveState = Entity::MoveState::IDLE;
            }
        }
        else
        {
            m.velocity = { 0.0f, 0.0f };
            m.moveState = Entity::MoveState::IDLE;
        }

        if (m.uiMessageTimer > 0.0f)
        {
            m.uiMessageTimer -= dt;
            if (m.uiMessageTimer <= 0.0f)
            {
                m.uiMessageTimer = 0.0f;
                m.uiMessage[0] = '\0';
            }
        }

        m.hoveredIndex = -1;

        if (m.shopOpen)
        {
            s32 mx = 0, my = 0;
            AEInputGetCursorPosition(&mx, &my);

            float worldX = 0.0f, worldY = 0.0f;
            BasicUtilities::screenCoordsToWorldCoords(mx, my, worldX, worldY);

            m.hoveredIndex = GetHoveredSlotIndex(worldX, worldY);
        }

        const unsigned int texSlot = GetAnimTexSlot(m.moveState);
        const unsigned int tagIndex = GetAnimTagIndex(m.facing);

        if (texSlot < gMerchantSprite.count)
            Sprite::UpdateAnimation(gMerchantAnimState, gMerchantSprite.jsonList[texSlot], tagIndex);
    }

    void Draw(Merchant& m, s8 fontId)
    {
        if (!m.active)
            return;

        const unsigned int texSlot = GetAnimTexSlot(m.moveState);
        if (texSlot < gMerchantSprite.count && gMerchantSprite.textureList[texSlot] && gMerchantMesh)
        {
            Sprite::DrawAnimation(
                gMerchantMesh,
                gMerchantSprite.textureList[texSlot],
                gMerchantAnimState,
                m.position,
                { MERCHANT_SCALE, MERCHANT_SCALE });
        }

        if (!m.shopOpen)
            return;

        float oldCamX = 0.0f, oldCamY = 0.0f;
        AEGfxGetCamPosition(&oldCamX, &oldCamY);
        AEGfxSetCamPosition(0.0f, 0.0f);

        // Background
        DrawQuad(0.0f, 0.0f, BACKDROP_W, BACKDROP_H, 0.45f, 0.45f, 0.45f, 1.0f);
        DrawQuad(WALL_X, WALL_Y, WALL_W, WALL_H, 0.18f, 0.10f, 0.05f, 1.0f);

        // Decorative chests
        DrawChest(-300.0f, 140.0f);
        DrawChest(-190.0f, 140.0f);
        DrawChest(-80.0f, 140.0f);

        DrawChest(-300.0f, 25.0f);
        DrawChest(-190.0f, 25.0f);
        DrawChest(-80.0f, 25.0f);

        // Speech bubble
        DrawSpeechBubble(SPEECH_X, SPEECH_Y);

        const char* bubbleText = "Take your pick.";

        if (m.uiMessageTimer > 0.0f && m.uiMessage[0] != '\0')
        {
            bubbleText = m.uiMessage;
        }
        else if (m.hoveredIndex >= 0 && m.hoveredIndex < SHOP_SLOTS)
        {
            bubbleText = m.offers[m.hoveredIndex].desc;
        }

        BasicUtilities::drawText(
            fontId,
            bubbleText,
            SPEECH_X,
            SPEECH_Y + 5.0f,
            0.62f,
            0.0f, 0.0f, 0.0f, 1.0f);

        // Cards
        for (int i = 0; i < SHOP_SLOTS; ++i)
        {
            const ShopItem& it = m.offers[i];
            const bool soldOut = (it.type == ShopItemType::NONE);
            const bool affordable = Gold::CanAfford(it.price);
            const bool hovered = (m.hoveredIndex == i);

            const float x = m.slotX[i];
            const float y = m.slotY[i] + (hovered ? 14.0f : 4.0f);

            if (hovered)
                DrawQuad(x, y, CARD_W + 12.0f, CARD_H + 12.0f, 0.98f, 0.92f, 0.60f, 0.28f);

            if (soldOut)
            {
                DrawQuad(x, y, CARD_W, CARD_H, 0.35f, 0.30f, 0.24f, 1.0f);
            }
            else
            {
                DrawQuad(
                    x, y, CARD_W, CARD_H,
                    affordable ? 0.69f : 0.48f,
                    affordable ? 0.54f : 0.38f,
                    affordable ? 0.34f : 0.28f,
                    1.0f);
            }

            DrawItemIcon(it, x, y + 36.0f);

            BasicUtilities::drawText(
                fontId,
                it.name,
                x, y - 8.0f,
                0.56f,
                soldOut ? 0.80f : 1.0f,
                soldOut ? 0.80f : 1.0f,
                soldOut ? 0.80f : 1.0f,
                1.0f);

            if (!soldOut)
            {
                char priceText[32];
                sprintf_s(priceText, sizeof(priceText), "%dG", it.price);

                BasicUtilities::drawText(
                    fontId,
                    priceText,
                    x, y - 42.0f,
                    0.64f,
                    1.0f, 0.87f, 0.05f, 1.0f);

                BasicUtilities::drawText(
                    fontId,
                    TypeHint(it),
                    x, y - 72.0f,
                    0.44f,
                    0.92f, 0.92f, 0.92f, 1.0f);
            }

            if (soldOut)
            {
                DrawQuad(x, y, CARD_W - 20.0f, 28.0f, 0.15f, 0.15f, 0.15f, 0.55f);
                BasicUtilities::drawText(
                    fontId,
                    "sold out",
                    x, y - 2.0f,
                    0.48f,
                    1.0f, 0.9f, 0.9f, 1.0f);
            }
        }

        // Placeholder region on right for merchant feel
        DrawQuad(MERCHANT_UI_X, MERCHANT_UI_Y, 180.0f, 220.0f, 0.35f, 0.18f, 0.15f, 0.0f);

        BasicUtilities::drawText(
            fontId,
            "Click to buy",
            0.0f, -335.0f,
            0.54f,
            0.95f, 0.95f, 0.95f, 1.0f);

        BasicUtilities::drawText(
            fontId,
            "Press N to skip",
            -150.0f, -370.0f,
            0.48f,
            0.82f, 0.82f, 0.82f, 1.0f);

        BasicUtilities::drawText(
            fontId,
            "Press R to reroll (10G)",
            150.0f, -370.0f,
            0.48f,
            0.82f, 0.82f, 0.82f, 1.0f);

        AEGfxSetCamPosition(oldCamX, oldCamY);
    }

    void HandleMousePurchase(Merchant& m, RunBonuses& bonuses)
    {
        if (!m.shopOpen)
            return;

        // Skip merchant
        if (AEInputCheckTriggered(AEVK_N))
        {
            m.shopOpen = false;
            m.active = false;
            SetMessage(m, "Maybe next time.", 1.0f);
            return;
        }

        // ------------------------------------------------------------
        // REROLL SHOP
        // ------------------------------------------------------------
        if (AEInputCheckTriggered(AEVK_R))
        {
            constexpr int REROLL_COST = 10;

            if (m.rerolledThisVisit)
            {
                SetMessage(m, "Already rerolled.", 1.5f);
                return;
            }

            // Free reroll from Merchant Favor
            if (bonuses.freeRerollAvailable)
            {
                bonuses.freeRerollAvailable = false;
                m.usedFreeRerollThisVisit = true;
                m.rerolledThisVisit = true;

                RerollShop(m, bonuses);
                SetMessage(m, "A favor for you.", 1.8f);
                return;
            }

            if (!Gold::CanAfford(REROLL_COST))
            {
                SetMessage(m, "Need 10G to reroll.", 1.8f);
                return;
            }

            if (!Gold::Spend(REROLL_COST))
            {
                SetMessage(m, "Need 10G to reroll.", 1.8f);
                return;
            }

            m.rerolledThisVisit = true;

            RerollShop(m, bonuses);
            SetMessage(m, "Let's see...", 1.8f);
            return;
        }

        // ------------------------------------------------------------
        // BUY ITEM
        // ------------------------------------------------------------
        if (!AEInputCheckTriggered(AEVK_LBUTTON))
            return;

        s32 mx = 0, my = 0;
        AEInputGetCursorPosition(&mx, &my);

        float worldX = 0.0f, worldY = 0.0f;
        BasicUtilities::screenCoordsToWorldCoords(mx, my, worldX, worldY);

        const int index = GetHoveredSlotIndex(worldX, worldY);
        if (index < 0 || index >= SHOP_SLOTS)
            return;

        ShopItem& it = m.offers[index];

        if (it.type == ShopItemType::NONE)
        {
            SetMessage(m, "That one's gone.", 1.5f);
            return;
        }

        if (!Gold::CanAfford(it.price))
        {
            SetMessage(m, "Not enough gold!", 1.8f);
            return;
        }

        if (!Gold::Spend(it.price))
        {
            SetMessage(m, "Not enough gold!", 1.8f);
            return;
        }

        switch (it.type)
        {
        case ShopItemType::SPEED_BOOST:
            bonuses.speedLevel += 1;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::WATER_UPGRADE:
            bonuses.waterLevel += 1;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::COIN_MULTIPLIER:
            bonuses.nextLevelCoinMultiplier += 0.5f;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::START_GOLD:
            bonuses.nextLevelStartGoldBonus += 15;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::MERCHANT_FAVOR:
            bonuses.freeRerollAvailable = true;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::DISCOUNT_COUPON:
            bonuses.nextMerchantDiscount = 0.25f;
            SetMessage(m, GetPurchaseLine(), 2.0f);
            break;

        case ShopItemType::MERCHANT_GAMBLE:
            ResolveMerchantGamble(m, bonuses);
            break;

        default:
            SetMessage(m, "Nothing happened?", 1.5f);
            break;
        }

        // Mark slot sold
        m.offers[index] = MakeItem(
            ShopItemType::NONE,
            0,
            "sold out",
            "Already purchased");
    }

    void Free()
    {
        if (gQuadMesh)
        {
            AEGfxMeshFree(gQuadMesh);
            gQuadMesh = nullptr;
        }

        if (gMerchantMesh)
        {
            AEGfxMeshFree(gMerchantMesh);
            gMerchantMesh = nullptr;
        }

        gMerchantAnimState = Sprite::AnimationState{};
    }

    void Unload()
    {
        gMerchantSprite.Free();
    }
}