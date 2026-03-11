#include "merchant.hpp"

#include "AEGraphics.h"
#include "AEMtx33.h"
#include "gold.hpp"
#include "sprite.hpp"
#include "utilities.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>

namespace
{
    Sprite::SpriteData     gMerchantSprite;
    Sprite::AnimationState gMerchantAnimState;

    AEGfxVertexList* gQuadMesh = nullptr;
    AEGfxVertexList* gMerchantMesh = nullptr;

    constexpr float MERCHANT_SCALE = 100.0f;
    constexpr float ARRIVE_Y = -50.0f;

    // Shop panel layout (screen-space, centred around 0,0)
    constexpr float PANEL_W = 700.0f;
    constexpr float PANEL_H = 500.0f;
    constexpr float HEADER_H = 70.0f;

    constexpr float SLOT_W = 150.0f;
    constexpr float SLOT_H = 120.0f;
    constexpr float SLOT_HALF_W = SLOT_W * 0.5f;
    constexpr float SLOT_HALF_H = SLOT_H * 0.5f;

    constexpr float START_X = -200.0f;
    constexpr float START_Y = 90.0f;
    constexpr float SPACING_X = 200.0f;
    constexpr float SPACING_Y = 150.0f;

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
        const int row = i / 3;
        const int col = i % 3;
        x = START_X + col * SPACING_X;
        y = START_Y - row * SPACING_Y;
    }

    int GetHoveredSlotIndex(float worldX, float worldY)
    {
        for (int i = 0; i < 9; ++i)
        {
            float x = 0.f, y = 0.f;
            GetSlotPosition(i, x, y);

            const bool inside =
                worldX >= x - SLOT_HALF_W && worldX <= x + SLOT_HALF_W &&
                worldY >= y - SLOT_HALF_H && worldY <= y + SLOT_HALF_H;

            if (inside) return i;
        }
        return -1;
    }

    const char* SeedName(SeedType st)
    {
        switch (st)
        {
        case SeedType::ROSE:      return "Rose Seed";
        case SeedType::TULIP:     return "Tulip Seed";
        case SeedType::SUNFLOWER: return "Sunflower Seed";
        case SeedType::DAISY:     return "Daisy Seed";
        case SeedType::LILY:      return "Lily Seed";
        case SeedType::ORCHID:    return "Orchid Seed";
        default:                  return "Unknown Seed";
        }
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

        m.position = { 0.f, -400.f };
        m.speed = 250.0f;
        m.facing = Entity::FaceDirection::UP;
        m.moveState = Entity::MoveState::IDLE;

        m.items[0] = { ShopItemType::SEED,          SeedType::ROSE,      10, "Rose Seed" };
        m.items[1] = { ShopItemType::SEED,          SeedType::TULIP,     15, "Tulip Seed" };
        m.items[2] = { ShopItemType::BONUS_GOLD,    SeedType::NONE,      20, "Gold Boost" };

        m.items[3] = { ShopItemType::EXTRA_STORAGE, SeedType::NONE,      25, "Storage +" };
        m.items[4] = { ShopItemType::SEED,          SeedType::SUNFLOWER,  8, "Sunflower Seed" };
        m.items[5] = { ShopItemType::SEED,          SeedType::DAISY,     18, "Daisy Seed" };

        m.items[6] = { ShopItemType::SEED,          SeedType::LILY,      12, "Lily Seed" };
        m.items[7] = { ShopItemType::BONUS_GOLD,    SeedType::NONE,      30, "Big Boost" };
        m.items[8] = { ShopItemType::SEED,          SeedType::ORCHID,    22, "Orchid Seed" };

        for (int i = 0; i < 9; ++i)
            GetSlotPosition(i, m.slotX[i], m.slotY[i]);
    }

    void Start(Merchant& m)
    {
        m.active = true;
        m.arrived = false;
        m.shopOpen = false;
        m.position = { 0.f, -400.f };
        m.velocity = { 0.f, 0.f };
        m.facing = Entity::FaceDirection::UP;
        m.moveState = Entity::MoveState::WALKING;
        m.hoveredIndex = -1;
        m.uiMessage[0] = '\0';
        m.uiMessageTimer = 0.f;
        gMerchantAnimState = Sprite::AnimationState{};
    }

    void Update(Merchant& m, float dt)
    {
        if (!m.active) return;

        if (!m.arrived)
        {
            m.velocity = { 0.f, m.speed };
            m.position.y += m.velocity.y * dt;
            m.facing = Entity::FaceDirection::UP;
            m.moveState = Entity::MoveState::WALKING;

            if (m.position.y >= ARRIVE_Y)
            {
                m.position.y = ARRIVE_Y;
                m.velocity = { 0.f, 0.f };
                m.arrived = true;
                m.shopOpen = true;
                m.moveState = Entity::MoveState::IDLE;
            }
        }
        else
        {
            m.velocity = { 0.f, 0.f };
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

            float worldX = 0.f, worldY = 0.f;
            BasicUtilities::screenCoordsToWorldCoords(mx, my, worldX, worldY);

            m.hoveredIndex = GetHoveredSlotIndex(worldX, worldY);
        }

        const unsigned int texSlot = GetAnimTexSlot(m.moveState);
        const unsigned int tagIndex = GetAnimTagIndex(m.facing);

        if (texSlot < gMerchantSprite.count)
            Sprite::UpdateAnimation(gMerchantAnimState, gMerchantSprite.metas[texSlot], tagIndex);
    }

    void Draw(Merchant& m, s8 fontId)
    {
        if (!m.active) return;

        // Draw merchant in world-space first
        const unsigned int texSlot = GetAnimTexSlot(m.moveState);
        if (texSlot < gMerchantSprite.count && gMerchantSprite.textures[texSlot] && gMerchantMesh)
        {
            Sprite::DrawAnimation(gMerchantMesh,
                gMerchantSprite.textures[texSlot],
                gMerchantAnimState,
                m.position,
                { MERCHANT_SCALE, MERCHANT_SCALE });
        }

        if (!m.shopOpen) return;

        // Draw shop UI in screen-space
        float oldCamX = 0.f, oldCamY = 0.f;
        AEGfxGetCamPosition(&oldCamX, &oldCamY);
        AEGfxSetCamPosition(0.f, 0.f);

        DrawQuad(0.f, -32.f, PANEL_W, PANEL_H, 0.05f, 0.05f, 0.05f, 0.90f);
        DrawQuad(0.f, 180.f, PANEL_W, HEADER_H, 0.12f, 0.12f, 0.12f, 0.95f);

        BasicUtilities::drawText(fontId, "MERCHANT SHOP",
            0.f, 180.f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f);

        for (int i = 0; i < 9; ++i)
        {
            const ShopItem& it = m.items[i];
            const bool affordable = Gold::CanAfford(it.price);

            const float x = m.slotX[i];
            const float y = m.slotY[i];

            if (affordable)
                DrawQuad(x, y, SLOT_W, SLOT_H, 0.15f, 0.15f, 0.15f, 1.0f);
            else
                DrawQuad(x, y, SLOT_W, SLOT_H, 0.08f, 0.08f, 0.08f, 1.0f);

            if (m.hoveredIndex == i)
                DrawQuad(x, y, SLOT_W + 10.f, SLOT_H + 10.f, 1.0f, 1.0f, 0.2f, 0.25f);

            // Simple icon block
            if (it.type == ShopItemType::SEED)
                DrawQuad(x, y + 15.f, 60.f, 60.f, 0.2f, 0.6f, 0.2f, 1.0f);
            else if (it.type == ShopItemType::BONUS_GOLD)
                DrawQuad(x, y + 15.f, 60.f, 60.f, 0.9f, 0.7f, 0.0f, 1.0f);
            else
                DrawQuad(x, y + 15.f, 60.f, 60.f, 0.3f, 0.4f, 0.9f, 1.0f);

            BasicUtilities::drawText(fontId, it.name,
                x, y - 35.0f, 0.55f,
                1.0f, 1.0f, 1.0f, 1.0f);

            char priceText[32];
            sprintf_s(priceText, sizeof(priceText), "%d G", it.price);

            const float pr = 1.0f;
            const float pg = affordable ? 1.0f : 0.2f;
            const float pb = 0.0f;

            BasicUtilities::drawText(fontId, priceText,
                x, y - 58.0f, 0.65f,
                pr, pg, pb, 1.0f);
        }

        if (m.uiMessageTimer > 0.f && m.uiMessage[0] != '\0')
        {
            BasicUtilities::drawText(fontId, m.uiMessage,
                0.f, -315.f, 0.75f,
                1.f, 1.f, 1.f, 1.f);
        }

        BasicUtilities::drawText(fontId, "Click item to buy. Press N to skip.",
            0.f, -370.f, 0.6f,
            0.8f, 0.8f, 0.8f, 1.0f);

        AEGfxSetCamPosition(oldCamX, oldCamY);
    }

    void HandleMousePurchase(Merchant& m, HeldState& held)
    {
        if (!m.shopOpen) return;

        if (AEInputCheckTriggered(AEVK_N))
        {
            m.shopOpen = false;
            m.active = false;
            SetMessage(m, "Maybe next time.", 1.0f);
            return;
        }

        if (!AEInputCheckTriggered(AEVK_LBUTTON)) return;

        s32 mx = 0, my = 0;
        AEInputGetCursorPosition(&mx, &my);

        float worldX = 0.f, worldY = 0.f;
        BasicUtilities::screenCoordsToWorldCoords(mx, my, worldX, worldY);

        const int index = GetHoveredSlotIndex(worldX, worldY);
        if (index < 0 || index >= 9) return;

        ShopItem& it = m.items[index];

        if (!Gold::CanAfford(it.price))
        {
            SetMessage(m, "Not enough gold!");
            return;
        }

        if (it.type == ShopItemType::SEED)
        {
            if (held.type != HeldItem::NONE)
            {
                SetMessage(m, "Your hands are full!");
                return;
            }

            if (!Gold::Spend(it.price))
            {
                SetMessage(m, "Not enough gold!");
                return;
            }

            held = HeldState{};
            held.type = HeldItem::SEED;
            held.seed = it.seed;

            SetMessage(m, SeedName(it.seed));
            return;
        }

        if (it.type == ShopItemType::BONUS_GOLD)
        {
            if (!Gold::Spend(it.price))
            {
                SetMessage(m, "Not enough gold!");
                return;
            }

            Gold::Earn(10);
            SetMessage(m, "Purchased!");
            return;
        }

        if (it.type == ShopItemType::EXTRA_STORAGE)
        {
            // Not implemented in the current repo architecture.
            SetMessage(m, "Storage upgrade not implemented.");
            return;
        }

        SetMessage(m, "Purchased!");
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