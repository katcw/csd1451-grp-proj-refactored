/*===========================================================================
@filename   tables.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief      See tables.hpp. Handles rune tables (map ID 5) and poison
            tables (map ID 6). Each table type loads its texture and draws
            a 128×64 world-unit sprite (2× the 64×32 source pixels) at its
            world position. AABB half-extents match the drawn size exactly.

            Flower infusion mechanic:
              - E near table while holding FLOWER  → places flower (starts 3s timer)
              - E near table with ready slot        → picks up flower + FIRE/POISON modifier
            On-table indicator: coloured square + progress bar drawn above the table.
============================================================================*/
#include "tables.hpp"
#include "utilities.hpp"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include <cmath>

namespace TableSystem
{
    //========================================================================
    // INTERNAL HELPERS
    //========================================================================

    // Per-flower colour for the on-table indicator (matches customer bubble colours).
    static void GetFlowerColor(FlowerType ft, float& r, float& g, float& b)
    {
        switch (ft)
        {
        case FlowerType::ROSE:      r=1.0f; g=0.2f;  b=0.3f;  break;
        case FlowerType::TULIP:     r=1.0f; g=0.4f;  b=0.8f;  break;
        case FlowerType::SUNFLOWER: r=1.0f; g=0.85f; b=0.0f;  break;
        case FlowerType::DAISY:     r=1.0f; g=1.0f;  b=0.8f;  break;
        case FlowerType::LILY:      r=0.8f; g=0.5f;  b=1.0f;  break;
        case FlowerType::ORCHID:    r=0.6f; g=0.0f;  b=0.8f;  break;
        default:                    r=1.0f; g=1.0f;  b=1.0f;  break;
        }
    }

    //========================================================================
    // LIFECYCLE
    //========================================================================

    // Load — create mesh and load table textures.
    void TableSystem_Load(State& s)
    {
        s.mesh      = BasicUtilities::createSquareMesh();
        s.runeTex   = BasicUtilities::loadTexture("Assets/rune_table.png");
        s.poisonTex = BasicUtilities::loadTexture("Assets/poision_table.png"); // [NOTE] asset filename has typo
    }

    // Init — populate s.tables[] from two world-position arrays.
    // Rune entries are stored first (indices 0..runeCount-1),
    // then poison entries (indices runeCount..runeCount+poisonCount-1).
    void TableSystem_Init(State& s,
                          const AEVec2* runePosArr,   int runeCount,
                          const AEVec2* poisonPosArr, int poisonCount)
    {
        s.count = 0;
        if (runeCount   > MAX_TABLES) runeCount   = MAX_TABLES;
        if (poisonCount > MAX_TABLES) poisonCount = MAX_TABLES;

        for (int i = 0; i < runeCount;   ++i)
        {
            s.tables[s.count] = { runePosArr[i],   TableType::RUNE   };
            s.slots[s.count]  = TableSlot{};
            ++s.count;
        }
        for (int i = 0; i < poisonCount; ++i)
        {
            s.tables[s.count] = { poisonPosArr[i], TableType::POISON };
            s.slots[s.count]  = TableSlot{};
            ++s.count;
        }
    }

    //========================================================================
    // UPDATE
    //========================================================================

    // Update — tick infusion timers and handle E-key placement / pickup.
    void TableSystem_Update(State& s, float dt, AEVec2 playerPos, HeldState& held)
    {
        // Tick all active infusions
        for (int i = 0; i < s.count; ++i)
        {
            TableSlot& slot = s.slots[i];
            if (slot.occupied && !slot.ready)
            {
                slot.timer -= dt;
                if (slot.timer <= 0.f) { slot.timer = 0.f; slot.ready = true; }
            }
        }

        if (!AEInputCheckTriggered(AEVK_E)) return;

        // Find the nearest table within interact radius and handle interaction
        for (int i = 0; i < s.count; ++i)
        {
            const Table& t    = s.tables[i];
            TableSlot&   slot = s.slots[i];

            float dx = t.pos.x - playerPos.x;
            float dy = t.pos.y - playerPos.y;
            if (std::sqrt(dx*dx + dy*dy) >= TABLE_INTERACT_RADIUS) continue;

            if (!slot.occupied && held.type == HeldItem::FLOWER)
            {
                // Place flower → begin infusion
                slot.occupied = true;
                slot.flower   = held.flower;
                slot.timer    = INFUSE_TIME;
                slot.ready    = false;
                held          = HeldState{};
            }
            else if (slot.ready && held.type == HeldItem::NONE)
            {
                // Pick up infused flower — apply FIRE or POISON modifier
                held.type     = HeldItem::FLOWER;
                held.flower   = slot.flower;
                held.modifier = (t.type == TableType::RUNE)
                                ? FlowerModifier::FIRE
                                : FlowerModifier::POISON;
                slot          = TableSlot{};
            }
            break;  // only interact with one table per E press
        }
    }

    //========================================================================
    // DRAW
    //========================================================================

    // Draw — render all tables as 128×64 world-space sprites.
    //        When a slot is occupied, also draw a flower-colour indicator and
    //        a small infuse-progress bar above the table.
    void TableSystem_Draw(const State& s, AEGfxTexture** flowerIcons)
    {
        for (int i = 0; i < s.count; ++i)
        {
            const Table&     t    = s.tables[i];
            const TableSlot& slot = s.slots[i];

            // ── Table sprite ───────────────────────────────────────────────
            AEGfxTexture* tex = (t.type == TableType::RUNE)
                                ? s.runeTex : s.poisonTex;
            BasicUtilities::drawUICard(s.mesh, tex,
                t.pos.x, t.pos.y, TABLE_DRAW_W, TABLE_DRAW_H);

            if (!slot.occupied) continue;

            // ── On-table flower indicator ──────────────────────────────────
            constexpr float ICON_SIZE  = 28.f;
            constexpr float ICON_OFF_Y = TABLE_DRAW_H * 0.5f + 14.f;  // above table top edge

            // Dim while infusing; full alpha when ready
            const float iconAlpha = slot.ready ? 1.f : 0.5f;

            const int flowerIdx = static_cast<int>(slot.flower);
            AEGfxTexture* iconTex = (flowerIcons && flowerIdx >= 1 && flowerIdx <= 6)
                                    ? flowerIcons[flowerIdx] : nullptr;

            AEMtx33 sclMtx{}, trsMtx{}, mtx{};

            if (iconTex)
            {
                BasicUtilities::drawUICard(s.mesh, iconTex,
                                           t.pos.x, t.pos.y + ICON_OFF_Y,
                                           ICON_SIZE, ICON_SIZE, iconAlpha);
                // Restore colour mode for the progress bar draws below
                AEGfxSetRenderMode(AE_GFX_RM_COLOR);
                AEGfxSetBlendMode(AE_GFX_BM_BLEND);
                AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
            }
            else
            {
                float ir, ig, ib;
                GetFlowerColor(slot.flower, ir, ig, ib);
                AEGfxSetRenderMode(AE_GFX_RM_COLOR);
                AEGfxSetBlendMode(AE_GFX_BM_BLEND);
                AEGfxSetTransparency(1.f);
                AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
                AEGfxSetColorToMultiply(ir * iconAlpha, ig * iconAlpha, ib * iconAlpha, 1.f);
                AEMtx33Scale(&sclMtx, ICON_SIZE, ICON_SIZE);
                AEMtx33Trans(&trsMtx, t.pos.x, t.pos.y + ICON_OFF_Y);
                AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                AEGfxSetTransform(mtx.m);
                AEGfxMeshDraw(s.mesh, AE_GFX_MDM_TRIANGLES);
            }

            // ── Infuse progress bar (drains as timer counts down) ──────────
            if (!slot.ready)
            {
                constexpr float BAR_W  = 40.f;
                constexpr float BAR_H  =  5.f;
                constexpr float BAR_OY = ICON_OFF_Y + ICON_SIZE * 0.5f + 5.f;

                const float progress = slot.timer / INFUSE_TIME;  // 1 → 0

                // Background
                AEGfxSetColorToMultiply(0.2f, 0.2f, 0.2f, 1.f);
                AEMtx33Scale(&sclMtx, BAR_W, BAR_H);
                AEMtx33Trans(&trsMtx, t.pos.x, t.pos.y + BAR_OY);
                AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                AEGfxSetTransform(mtx.m);
                AEGfxMeshDraw(s.mesh, AE_GFX_MDM_TRIANGLES);

                // Fill (orange for RUNE/FIRE, green for POISON)
                float fr = (t.type == TableType::RUNE) ? 1.0f : 0.2f;
                float fg = (t.type == TableType::RUNE) ? 0.5f : 1.0f;
                float fb = 0.f;
                AEGfxSetColorToMultiply(fr, fg, fb, 1.f);
                AEMtx33Scale(&sclMtx, BAR_W * progress, BAR_H);
                AEMtx33Trans(&trsMtx,
                    t.pos.x - BAR_W * 0.5f + BAR_W * progress * 0.5f,
                    t.pos.y + BAR_OY);
                AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                AEGfxSetTransform(mtx.m);
                AEGfxMeshDraw(s.mesh, AE_GFX_MDM_TRIANGLES);
            }
        }
    }

    //========================================================================
    // FREE / UNLOAD
    //========================================================================

    // Free — release mesh memory. Call in the state's Free().
    void TableSystem_Free(State& s)
    {
        if (s.mesh) { AEGfxMeshFree(s.mesh); s.mesh = nullptr; }
    }

    // Unload — release texture memory and reset counters. Call in the state's Unload().
    void TableSystem_Unload(State& s)
    {
        AEGfxTextureUnload(s.runeTex);
        AEGfxTextureUnload(s.poisonTex);
        s.runeTex = s.poisonTex = nullptr;
        s.count = 0;
    }
}
