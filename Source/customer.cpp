/*===========================================================================
@filename   customer.cpp
@brief      See customer.hpp.

            Animation mirrors player.cpp exactly:
                slot 0 = Idle        tag 0=Left 1=Right 2=Up 3=Down
                slot 1 = Walking     (same tag layout)
            Carry slots (2, 3) reserved for future use.

            Movement logic ported from old repo ai_customer.cpp:
            normalise dx/dy toward target, step by SPEED*dt, snap within EPS.
============================================================================*/

#include "customer.hpp"
#include "utilities.hpp"
#include <cmath>
#include <cstdio>

//============================================================================
// HELPERS (file-scoped)
//============================================================================

namespace
{
    //------------------------------------------------------------------------
    // Order bubble layout constants (implementation detail — not in header).
    //------------------------------------------------------------------------
    constexpr float BUBBLE_H         = 40.f;   // Fixed height of the tooltip box
    constexpr float BUBBLE_ICON_SIZE = 30.f;   // Coloured square width & height
    constexpr float BUBBLE_ICON_GAP  = 6.f;    // Gap between icon right edge and text
    constexpr float BUBBLE_SIDE_PAD  = 10.f;   // Padding on each horizontal side of the box

    //------------------------------------------------------------------------
    // Patience bar geometry constants (tunable).
    //------------------------------------------------------------------------
    constexpr float PBAR_W        = 60.f;   // [TUNE] full-bar pixel width in world units
    constexpr float PBAR_H        =  8.f;   // [TUNE] bar height in world units
    constexpr float PBAR_OFFSET_Y = 55.f;   // [TUNE] world units above customer centre

    //------------------------------------------------------------------------
    // Returns the display name for a FlowerType (used in the order bubble).
    //------------------------------------------------------------------------
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

    //------------------------------------------------------------------------
    // Per-flower colour for the icon square in the order bubble.
    // Colours ported from old repo's GetSeedColor().
    //------------------------------------------------------------------------
    void GetFlowerColor(FlowerType ft, float& r, float& g, float& b)
    {
        switch (ft)
        {
        case FlowerType::ROSE:      r=1.0f; g=0.2f;  b=0.3f; break;
        case FlowerType::TULIP:     r=1.0f; g=0.4f;  b=0.8f; break;
        case FlowerType::SUNFLOWER: r=1.0f; g=0.85f; b=0.0f; break;
        case FlowerType::DAISY:     r=1.0f; g=1.0f;  b=0.8f; break;
        case FlowerType::LILY:      r=0.8f; g=0.5f;  b=1.0f; break;
        case FlowerType::ORCHID:    r=0.6f; g=0.0f;  b=0.8f; break;
        default:                    r=1.0f; g=1.0f;  b=1.0f; break;
        }
    }

    //------------------------------------------------------------------------
    // Resolves (facing, moveState) → (texSlot, tagIndex) identically to
    // player.cpp so the customer uses the same animation conventions.
    //
    //   texSlot  : 0=Idle  1=Walking
    //   tagIndex : 0=Left  1=Right  2=Up  3=Down
    //------------------------------------------------------------------------
    void ResolveAnim(const Customer& c, unsigned int& texSlot, unsigned int& tagIndex)
    {
        // Use carry slots (2=idle-carry, 3=walk-carry) once the customer has been served.
        const bool carrying = (c.state == CustomerState::LEAVING && c.servedSuccessfully);
        if (carrying)
            texSlot = (c.moveState == Entity::MoveState::WALKING) ? 3u : 2u;
        else
            texSlot = (c.moveState == Entity::MoveState::WALKING) ? 1u : 0u;

        switch (c.facing)
        {
        case Entity::FaceDirection::LEFT:  tagIndex = 0; break;
        case Entity::FaceDirection::RIGHT: tagIndex = 1; break;
        case Entity::FaceDirection::UP:    tagIndex = 2; break;
        case Entity::FaceDirection::DOWN:  default: tagIndex = 3; break;
        }
    }
}

//============================================================================
// CUSTOMER SYSTEM IMPLEMENTATION
//============================================================================

namespace CustomerSystem
{
    //========================================================================
    // Load — allocates GPU resources
    //========================================================================
    void CustomerSystem_Load(State& s)
    {
        // AI_Customer spritesheet: same 128x128 layout as MC → UV 0.25 per frame
        s.spriteMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);
        s.bubbleMesh = BasicUtilities::createSquareMesh();

        Sprite::LoadEntry(s.sprite, 0,
            "Assets/character-assets/AI_Customer_Idle.png",
            "Assets/character-assets/AI_Customer_Idle.json");

        Sprite::LoadEntry(s.sprite, 1,
            "Assets/character-assets/AI_Customer_Walk.png",
            "Assets/character-assets/AI_Customer_Walk.json");

        Sprite::LoadEntry(s.sprite, 2,
            "Assets/character-assets/AI_Customer_Idle_Carry.png",
            "Assets/character-assets/AI_Customer_Idle_Carry.json");

        Sprite::LoadEntry(s.sprite, 3,
            "Assets/character-assets/AI_Customer_Walk_Carry.png",
            "Assets/character-assets/AI_Customer_Walk_Carry.json");
    }

    //========================================================================
    // Init — configure positions and desired flower; does NOT spawn yet
    //========================================================================
    void CustomerSystem_Init(State& s, AEVec2 spawnPos, AEVec2 targetPos, FlowerType order)
    {
        s.spawnPos  = spawnPos;
        s.targetPos = targetPos;
        s.active    = false;

        s.customer.order = order;

        // Reset animation state so no stale UV offsets carry over
        s.customer.animState = Sprite::AnimationState{};
    }

    //========================================================================
    // Spawn — place customer and begin walking
    //========================================================================
    void CustomerSystem_Spawn(State& s)
    {
        s.customer.position   = s.spawnPos;
        s.customer.velocity   = {};
        s.customer.speed      = SPEED;
        s.customer.facing              = Entity::FaceDirection::RIGHT; // default; updated each tick
        s.customer.moveState           = Entity::MoveState::WALKING;
        s.customer.state               = CustomerState::ARRIVING;
        s.customer.stateTimer          = 0.f;
        s.customer.animState           = Sprite::AnimationState{};
        s.customer.patience            = CustomerSystem::PATIENCE_MAX;
        s.customer.patienceRanOut      = false;
        s.customer.servedSuccessfully  = false;
        s.active                       = true;
    }

    //========================================================================
    // Update — movement + animation
    //========================================================================
    void CustomerSystem_Update(State& s, float dt)
    {
        if (!s.active) return;

        Customer& c = s.customer;
        c.stateTimer += dt;

        if (c.state == CustomerState::ARRIVING)
        {
            // Move toward targetPos
            float dx   = s.targetPos.x - c.position.x;
            float dy   = s.targetPos.y - c.position.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= ARRIVE_EPS)
            {
                // Snap to target and transition to WAITING
                c.position  = s.targetPos;
                c.velocity  = {};
                c.moveState = Entity::MoveState::IDLE;
                c.state     = CustomerState::WAITING;
                c.stateTimer = 0.f;
            }
            else
            {
                // Normalise and step
                float step = SPEED * dt;
                if (step > dist) step = dist;

                float nx = dx / dist;
                float ny = dy / dist;

                c.velocity   = { nx * SPEED, ny * SPEED };
                c.position.x += nx * step;
                c.position.y += ny * step;
                c.moveState   = Entity::MoveState::WALKING;

                // Facing: horizontal takes priority (matches player.cpp convention)
                if      (std::fabsf(dx) >= std::fabsf(dy))
                    c.facing = (dx < 0.f) ? Entity::FaceDirection::LEFT
                                           : Entity::FaceDirection::RIGHT;
                else
                    c.facing = (dy > 0.f) ? Entity::FaceDirection::UP
                                           : Entity::FaceDirection::DOWN;
            }
        }
        else if (c.state == CustomerState::WAITING)
        {
            // Patience countdown — tutorial.cpp responds when patienceRanOut becomes true
            c.patience -= dt;
            if (c.patience <= 0.f)
            {
                c.patience       = 0.f;
                c.patienceRanOut = true;
                c.moveState      = Entity::MoveState::WALKING;
                c.state          = CustomerState::LEAVING;
                c.stateTimer     = 0.f;
            }
        }
        else if (c.state == CustomerState::LEAVING)
        {
            // Mirror of ARRIVING: same movement logic toward spawnPos instead of targetPos
            float dx   = s.spawnPos.x - c.position.x;
            float dy   = s.spawnPos.y - c.position.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= ARRIVE_EPS)
            {
                c.position  = s.spawnPos;
                c.velocity  = {};
                c.moveState = Entity::MoveState::IDLE;
                s.active    = false;   // despawn
            }
            else
            {
                float step = SPEED * dt;
                if (step > dist) step = dist;

                float nx = dx / dist;
                float ny = dy / dist;

                c.velocity   = { nx * SPEED, ny * SPEED };
                c.position.x += nx * step;
                c.position.y += ny * step;
                c.moveState   = Entity::MoveState::WALKING;

                if      (std::fabsf(dx) >= std::fabsf(dy))
                    c.facing = (dx < 0.f) ? Entity::FaceDirection::LEFT
                                           : Entity::FaceDirection::RIGHT;
                else
                    c.facing = (dy > 0.f) ? Entity::FaceDirection::UP
                                           : Entity::FaceDirection::DOWN;
            }
        }

        // Advance animation
        unsigned int texSlot = 0, tagIndex = 1;
        ResolveAnim(c, texSlot, tagIndex);
        if (texSlot < s.sprite.count && s.sprite.textures[texSlot])
            Sprite::UpdateAnimation(c.animState, s.sprite.metas[texSlot], tagIndex);
    }

    //========================================================================
    // Draw — sprite + order bubble
    //========================================================================
    void CustomerSystem_Draw(const State& s, s8 fontId)
    {
        if (!s.active) return;

        const Customer& c = s.customer;

        // ── Sprite ───────────────────────────────────────────────────────────
        unsigned int texSlot = 0, tagIndex = 1;
        ResolveAnim(c, texSlot, tagIndex);

        if (texSlot < s.sprite.count && s.sprite.textures[texSlot])
        {
            Sprite::DrawAnimation(s.spriteMesh,
                                  s.sprite.textures[texSlot],
                                  c.animState,
                                  c.position,
                                  { 100.f, 100.f });
        }

        // ── Patience bar — draw only while WAITING ───────────────────────────
        if (c.state == CustomerState::WAITING)
        {
            const float fill  = c.patience / CustomerSystem::PATIENCE_MAX;
            const float barCx = c.position.x;
            const float barCy = c.position.y + PBAR_OFFSET_Y;

            // Colour: green (fill=1) → yellow (fill=0.5) → red (fill=0)
            const float pr = (fill < 0.5f) ? 1.f        : (1.f - fill) * 2.f;
            const float pg = (fill < 0.5f) ? fill * 2.f : 1.f;

            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetTransparency(1.f);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
            AEMtx33 sclMtx{}, trsMtx{}, mtx{};

            // Background (dark)
            AEGfxSetColorToMultiply(0.2f, 0.2f, 0.2f, 1.f);
            AEMtx33Scale(&sclMtx, PBAR_W, PBAR_H);
            AEMtx33Trans(&trsMtx, barCx, barCy);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(s.bubbleMesh, AE_GFX_MDM_TRIANGLES);

            // Filled portion — left-anchored
            if (fill > 0.f)
            {
                AEGfxSetColorToMultiply(pr, pg, 0.f, 1.f);
                AEMtx33Scale(&sclMtx, PBAR_W * fill, PBAR_H);
                AEMtx33Trans(&trsMtx, barCx - PBAR_W * 0.5f + PBAR_W * fill * 0.5f, barCy);
                AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                AEGfxSetTransform(mtx.m);
                AEGfxMeshDraw(s.bubbleMesh, AE_GFX_MDM_TRIANGLES);
            }
        }

        // ── Order bubble — draw only while WAITING ───────────────────────────
        if (c.state != CustomerState::WAITING) return;
        if (c.order == FlowerType::NONE)       return;

        // ── Measure text width in world space ────────────────────────────────
        constexpr float TEXT_SCALE = 0.6f;
        float normW = 0.f, normH = 0.f;
        AEGfxGetPrintSize(fontId, FlowerName(c.order), TEXT_SCALE, &normW, &normH);
        const float textWorldW = normW * (AEGfxGetWindowWidth() / 2.f);

        // ── Compute dynamic tooltip width ─────────────────────────────────────
        // [ SIDE_PAD | ICON | ICON_GAP | text | SIDE_PAD ]
        const float tooltipW = BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE
                             + BUBBLE_ICON_GAP + textWorldW + BUBBLE_SIDE_PAD;

        // ── Anchor positions ──────────────────────────────────────────────────
        const float bubbleCx  = c.position.x;
        const float bubbleCy  = c.position.y + BUBBLE_OFFSET_Y;
        const float leftEdge  = bubbleCx - tooltipW * 0.5f;
        const float iconCx    = leftEdge + BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE * 0.5f;
        const float textCx    = leftEdge + BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE
                              + BUBBLE_ICON_GAP + textWorldW * 0.5f;

        // ── Shared render state ───────────────────────────────────────────────
        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        AEMtx33 sclMtx{}, trsMtx{}, mtx{};

        // ── Background ────────────────────────────────────────────────────────
        AEGfxSetTransparency(0.75f);
        AEGfxSetColorToMultiply(0.1f, 0.1f, 0.1f, 1.f);
        AEMtx33Scale(&sclMtx, tooltipW, BUBBLE_H);
        AEMtx33Trans(&trsMtx, bubbleCx, bubbleCy);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(s.bubbleMesh, AE_GFX_MDM_TRIANGLES);

        // ── Coloured icon square (placeholder for future flower texture) ──────
        float ir, ig, ib;
        GetFlowerColor(c.order, ir, ig, ib);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToMultiply(ir, ig, ib, 1.f);
        AEMtx33Scale(&sclMtx, BUBBLE_ICON_SIZE, BUBBLE_ICON_SIZE);
        AEMtx33Trans(&trsMtx, iconCx, bubbleCy);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(s.bubbleMesh, AE_GFX_MDM_TRIANGLES);

        // ── Flower name text ──────────────────────────────────────────────────
        BasicUtilities::drawText(fontId, FlowerName(c.order),
                                 textCx, bubbleCy,
                                 TEXT_SCALE, 1.f, 1.f, 1.f, 1.f);
    }

    //========================================================================
    // Free — release mesh memory (called in state's Free())
    //========================================================================
    void CustomerSystem_Free(State& s)
    {
        if (s.spriteMesh) { AEGfxMeshFree(s.spriteMesh); s.spriteMesh = nullptr; }
        if (s.bubbleMesh) { AEGfxMeshFree(s.bubbleMesh); s.bubbleMesh = nullptr; }
        s.active = false;
    }

    //========================================================================
    // Unload — release texture memory (called in state's Unload())
    //========================================================================
    void CustomerSystem_Unload(State& s)
    {
        s.sprite.Free();
    }

    //========================================================================
    // Serve — mark customer as successfully served and begin LEAVING walk
    //========================================================================
    void CustomerSystem_Serve(State& s)
    {
        if (!s.active) return;
        s.customer.servedSuccessfully = true;
        s.customer.moveState          = Entity::MoveState::WALKING;
        s.customer.state              = CustomerState::LEAVING;
        s.customer.stateTimer         = 0.f;
    }

} // namespace CustomerSystem
