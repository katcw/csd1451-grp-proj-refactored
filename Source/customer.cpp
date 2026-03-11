/*===========================================================================
@filename   customer.cpp
@brief      See customer.hpp.

            Single-customer API: see customer.hpp.

            Pool API: all POOL_MAX slots share one Sprite::SpriteData and one
            pair of meshes (spriteMesh, bubbleMesh).  Private static helpers
            UpdateSlot / DrawCustomerSprite / DrawCustomerUI operate on a
            single customer so the logic is not duplicated between the
            single-customer and pool code paths.

            Animation mirrors player.cpp exactly:
                slot 0 = Idle         tag 0=Left 1=Right 2=Up 3=Down
                slot 1 = Walking      (same tag layout)
                slot 2 = Idle-Carry   (used when leaving after being served)
                slot 3 = Walk-Carry   (same tag layout)
============================================================================*/

#include "customer.hpp"
#include "utilities.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>   // rand, RAND_MAX

//============================================================================
// FILE-SCOPED HELPERS
//============================================================================

namespace
{
    //------------------------------------------------------------------------
    // Order bubble layout constants.
    //------------------------------------------------------------------------
    constexpr float BUBBLE_H         = 40.f;  // Tooltip box height
    constexpr float BUBBLE_ICON_SIZE = 30.f;  // Flower colour square size
    constexpr float BUBBLE_ICON_GAP  =  6.f;  // Gap between icon and text
    constexpr float BUBBLE_SIDE_PAD  = 10.f;  // Horizontal padding each side
    constexpr float BADGE_SIZE       = 20.f;  // Modifier badge square size
    constexpr float BADGE_GAP        =  4.f;  // Gap between flower icon and badge

    //------------------------------------------------------------------------
    // Patience bar constants.
    //------------------------------------------------------------------------
    constexpr float PBAR_W        = 60.f;  // Full-bar width in world units
    constexpr float PBAR_H        =  8.f;  // Bar height in world units
    constexpr float PBAR_OFFSET_Y = 55.f;  // World units above customer centre

    //------------------------------------------------------------------------
    // Display name for a FlowerType (used in the order bubble).
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
    // Icon colour for a FlowerType — ported from old repo GetSeedColor().
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
    // Maps (state, moveState) to (texSlot, tagIndex) — identical to player.cpp.
    //   texSlot  : 0=Idle  1=Walk  2=Idle-Carry  3=Walk-Carry
    //   tagIndex : 0=Left  1=Right 2=Up           3=Down
    //------------------------------------------------------------------------
    void ResolveAnim(const Customer& c, unsigned int& texSlot, unsigned int& tagIndex)
    {
        // WAITING customers always use the idle animation regardless of moveState
        if (c.state == CustomerState::WAITING)
        {
            texSlot = 0u;
        }
        else
        {
            const bool carrying = (c.state == CustomerState::LEAVING && c.servedSuccessfully);
            texSlot = carrying
                      ? (c.moveState == Entity::MoveState::WALKING ? 3u : 2u)
                      : (c.moveState == Entity::MoveState::WALKING ? 1u : 0u);
        }

        switch (c.facing)
        {
        case Entity::FaceDirection::LEFT:  tagIndex = 0; break;
        case Entity::FaceDirection::RIGHT: tagIndex = 1; break;
        case Entity::FaceDirection::UP:    tagIndex = 2; break;
        default:                           tagIndex = 3; break;   // DOWN
        }
    }

    //------------------------------------------------------------------------
    // Moves c one step toward targetPos.  Updates position, velocity, facing,
    // and moveState.  Returns true when the customer has snapped to the target.
    //------------------------------------------------------------------------
    bool StepToward(Customer& c, AEVec2 targetPos, float dt)
    {
        float dx   = targetPos.x - c.position.x;
        float dy   = targetPos.y - c.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= CustomerSystem::ARRIVE_EPS)
        {
            c.position  = targetPos;
            c.velocity  = {};
            c.moveState = Entity::MoveState::IDLE;
            return true;
        }

        float step = CustomerSystem::SPEED * dt;
        if (step > dist) step = dist;

        float nx = dx / dist;
        float ny = dy / dist;

        c.velocity   = { nx * CustomerSystem::SPEED, ny * CustomerSystem::SPEED };
        c.position.x += nx * step;
        c.position.y += ny * step;
        c.moveState   = Entity::MoveState::WALKING;

        // Horizontal facing takes priority (matches player.cpp)
        if (std::fabsf(dx) >= std::fabsf(dy))
            c.facing = (dx < 0.f) ? Entity::FaceDirection::LEFT : Entity::FaceDirection::RIGHT;
        else
            c.facing = (dy > 0.f) ? Entity::FaceDirection::UP : Entity::FaceDirection::DOWN;

        return false;
    }

    //------------------------------------------------------------------------
    // Returns a random float uniformly in [lo, hi].
    //------------------------------------------------------------------------
    float RandRange(float lo, float hi)
    {
        return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
    }

    //------------------------------------------------------------------------
    // Draws the animated sprite for one customer.
    //------------------------------------------------------------------------
    void DrawCustomerSprite(AEGfxVertexList* mesh,
                            const Sprite::SpriteData& sprite,
                            const Customer& c)
    {
        unsigned int texSlot = 0, tagIndex = 1;
        ResolveAnim(c, texSlot, tagIndex);

        if (texSlot < sprite.count && sprite.textures[texSlot])
        {
            Sprite::DrawAnimation(mesh,
                                  sprite.textures[texSlot],
                                  c.animState,
                                  c.position,
                                  { 100.f, 100.f });
        }
    }

    //------------------------------------------------------------------------
    // Draws the patience bar and order bubble for one customer.
    // bubbleMesh — unit-square mesh used for both bar and bubble quads.
    // flowerIcons — optional array indexed by FlowerType (1–6).
    //------------------------------------------------------------------------
    void DrawCustomerUI(AEGfxVertexList* bubbleMesh, s8 fontId,
                        const Customer& c, AEGfxTexture** flowerIcons)
    {
        // ── Patience bar (WAITING only) ──────────────────────────────────────
        if (c.state == CustomerState::WAITING)
        {
            const float fill  = c.patience / CustomerSystem::PATIENCE_MAX;
            const float barCx = c.position.x;
            const float barCy = c.position.y + PBAR_OFFSET_Y;

            // Colour: green → yellow → red
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
            AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);

            // Filled portion (left-anchored)
            if (fill > 0.f)
            {
                AEGfxSetColorToMultiply(pr, pg, 0.f, 1.f);
                AEMtx33Scale(&sclMtx, PBAR_W * fill, PBAR_H);
                AEMtx33Trans(&trsMtx, barCx - PBAR_W * 0.5f + PBAR_W * fill * 0.5f, barCy);
                AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
                AEGfxSetTransform(mtx.m);
                AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);
            }
        }

		// -- Patience penalty (WRONG ORDER) — red flash over the customer sprite --
        
        if (c.receiveOrderResult == ReceiveOrder::WRONG_ORDER) {
			// Only visual effect, do NOT modify patience here!
            const float flashAlpha = RandRange(0.0f, 0.55f);
            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetTransparency(flashAlpha);
            AEGfxSetColorToMultiply(1.f, 0.f, 0.f, 1.f);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
            AEMtx33 sclMtx{}, trsMtx{}, mtx{};
            AEMtx33Scale(&sclMtx, 120.f, 120.f);
            AEMtx33Trans(&trsMtx, c.position.x, c.position.y);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);
        }

        // ── Order bubble (WAITING + valid order only) ────────────────────────
        if (c.state != CustomerState::WAITING || c.order == FlowerType::NONE) return;

        const bool hasModifier = (c.orderModifier != FlowerModifier::NONE);

        // Measure text width so the bubble can be sized dynamically
        constexpr float TEXT_SCALE = 0.6f;
        float normW = 0.f, normH = 0.f;
        AEGfxGetPrintSize(fontId, FlowerName(c.order), TEXT_SCALE, &normW, &normH);
        const float textWorldW = normW * (AEGfxGetWindowWidth() / 2.f);

        // Layout: [ SIDE_PAD | FlowerIcon | ICON_GAP | [Badge | BADGE_GAP |] Text | SIDE_PAD ]
        float tooltipW = BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE + BUBBLE_ICON_GAP
                       + textWorldW + BUBBLE_SIDE_PAD;
        if (hasModifier)
            tooltipW += BADGE_SIZE + BADGE_GAP;

        const float bubbleCx = c.position.x;
        const float bubbleCy = c.position.y + CustomerSystem::BUBBLE_OFFSET_Y;
        const float leftEdge = bubbleCx - tooltipW * 0.5f;
        const float iconCx   = leftEdge + BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE * 0.5f;

        float nextX  = leftEdge + BUBBLE_SIDE_PAD + BUBBLE_ICON_SIZE + BUBBLE_ICON_GAP;
        float badgeCx = 0.f;
        if (hasModifier)
        {
            badgeCx = nextX + BADGE_SIZE * 0.5f;
            nextX  += BADGE_SIZE + BADGE_GAP;
        }
        const float textCx = nextX + textWorldW * 0.5f;

        // Shared render state
        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        AEMtx33 sclMtx{}, trsMtx{}, mtx{};

        // Background
        AEGfxSetTransparency(0.75f);
        AEGfxSetColorToMultiply(0.1f, 0.1f, 0.1f, 1.f);
        AEMtx33Scale(&sclMtx, tooltipW, BUBBLE_H);
        AEMtx33Trans(&trsMtx, bubbleCx, bubbleCy);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);

        // Flower icon — texture preferred, coloured square fallback
        const int flowerIdx = static_cast<int>(c.order);
        AEGfxTexture* iconTex = (flowerIcons && flowerIdx >= 1 && flowerIdx <= 6)
                                ? flowerIcons[flowerIdx] : nullptr;
        if (iconTex)
        {
            BasicUtilities::drawUICard(bubbleMesh, iconTex,
                                       iconCx, bubbleCy,
                                       BUBBLE_ICON_SIZE, BUBBLE_ICON_SIZE);
            // Restore colour mode for subsequent draws
            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        }
        else
        {
            float ir, ig, ib;
            GetFlowerColor(c.order, ir, ig, ib);
            AEGfxSetTransparency(1.f);
            AEGfxSetColorToMultiply(ir, ig, ib, 1.f);
            AEMtx33Scale(&sclMtx, BUBBLE_ICON_SIZE, BUBBLE_ICON_SIZE);
            AEMtx33Trans(&trsMtx, iconCx, bubbleCy);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);
        }

        // Modifier badge: FIRE = orange, POISON = purple
        if (hasModifier)
        {
            float br, bg, bb;
            if (c.orderModifier == FlowerModifier::FIRE)
                { br = 1.0f; bg = 0.5f; bb = 0.0f; }
            else
                { br = 0.5f; bg = 0.0f; bb = 1.0f; }

            AEGfxSetColorToMultiply(br, bg, bb, 1.f);
            AEMtx33Scale(&sclMtx, BADGE_SIZE, BADGE_SIZE);
            AEMtx33Trans(&trsMtx, badgeCx, bubbleCy);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(bubbleMesh, AE_GFX_MDM_TRIANGLES);
        }

        // Flower name text
        BasicUtilities::drawText(fontId, FlowerName(c.order),
                                 textCx, bubbleCy,
                                 TEXT_SCALE, 1.f, 1.f, 1.f, 1.f);
    }

} // anonymous namespace

//============================================================================
// SINGLE-CUSTOMER API
//============================================================================

namespace CustomerSystem
{
    //========================================================================
    // Load — allocates GPU resources
    //========================================================================
    void CustomerSystem_Load(State& s)
    {
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
    // Init — configure positions and order; does NOT spawn the customer yet
    //========================================================================
    void CustomerSystem_Init(State& s, AEVec2 spawnPos, AEVec2 targetPos,
                             FlowerType order, FlowerModifier orderMod)
    {
        s.spawnPos  = spawnPos;
        s.targetPos = targetPos;
        s.active    = false;

        s.customer.order         = order;
        s.customer.orderModifier = orderMod;
        s.customer.animState     = Sprite::AnimationState{};
    }

    //========================================================================
    // Spawn — place customer at spawnPos and begin ARRIVING walk
    //========================================================================
    void CustomerSystem_Spawn(State& s)
    {
        s.customer.position           = s.spawnPos;
        s.customer.velocity           = {};
        s.customer.speed              = SPEED;
        s.customer.facing             = Entity::FaceDirection::RIGHT;
        s.customer.moveState          = Entity::MoveState::WALKING;
        s.customer.state              = CustomerState::ARRIVING;
        s.customer.stateTimer         = 0.f;
        s.customer.animState          = Sprite::AnimationState{};
        s.customer.patience           = CustomerSystem::PATIENCE_MAX;
        s.customer.patienceRanOut     = false;
        s.customer.servedSuccessfully = false;
        s.active                      = true;
    }

    //========================================================================
    // Update — movement + patience + animation
    //========================================================================
    void CustomerSystem_Update(State& s, float dt)
    {
        if (!s.active) return;

        Customer& c = s.customer;
        c.stateTimer += dt;


        if (c.state == CustomerState::ARRIVING)
        {
            if (StepToward(c, s.targetPos, dt))
            {
                c.state      = CustomerState::WAITING;
                c.stateTimer = 0.f;
                c.animState  = Sprite::AnimationState{};
            }
        }
        else if (c.state == CustomerState::WAITING)
        {
            c.moveState = Entity::MoveState::IDLE;  // stay idle while waiting
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
            if (StepToward(c, s.spawnPos, dt))
                s.active = false;   // despawn once back at spawn point
        }

        // Advance animation
        unsigned int texSlot = 0, tagIndex = 1;
        ResolveAnim(c, texSlot, tagIndex);
        if (texSlot < s.sprite.count && s.sprite.textures[texSlot])
            Sprite::UpdateAnimation(c.animState, s.sprite.metas[texSlot], tagIndex);
    }

    //========================================================================
    // Draw — sprite only (call before PlayerSystem::Draw)
    //========================================================================
    void CustomerSystem_Draw(const State& s, s8 fontId)
    {
        (void)fontId;
        if (!s.active) return;
        DrawCustomerSprite(s.spriteMesh, s.sprite, s.customer);
    }

    //========================================================================
    // DrawUI — patience bar + order bubble (call after PlayerSystem::Draw)
    //========================================================================
    void CustomerSystem_DrawUI(const State& s, s8 fontId, AEGfxTexture** flowerIcons)
    {
        if (!s.active) return;
        DrawCustomerUI(s.bubbleMesh, fontId, s.customer, flowerIcons);
    }

    //========================================================================
    // Free — release mesh memory
    //========================================================================
    void CustomerSystem_Free(State& s)
    {
        if (s.spriteMesh) { AEGfxMeshFree(s.spriteMesh); s.spriteMesh = nullptr; }
        if (s.bubbleMesh) { AEGfxMeshFree(s.bubbleMesh); s.bubbleMesh = nullptr; }
        s.active = false;
    }

    //========================================================================
    // Unload — release texture memory
    //========================================================================
    void CustomerSystem_Unload(State& s)
    {
        s.sprite.Free();
    }

    //========================================================================
    // Serve — mark customer as served and begin LEAVING walk
    //========================================================================
    void CustomerSystem_Serve(State& s)
    {
        if (!s.active) return;
        s.customer.servedSuccessfully = true;
        s.customer.moveState          = Entity::MoveState::WALKING;
        s.customer.state              = CustomerState::LEAVING;
        s.customer.stateTimer         = 0.f;
    }

    //============================================================================
    // MULTI-CUSTOMER POOL API
    //============================================================================

    //------------------------------------------------------------------------
    // Internal: advance one pool slot by dt (movement, patience, animation).
    // Releases the occupied target position when the customer despawns.
    //------------------------------------------------------------------------
    static void UpdateSlot(PoolState& p, PoolState::Slot& sl, float dt)
    {
        Customer& c = sl.customer;
        c.stateTimer += dt;


        if (c.state == CustomerState::ARRIVING)
        {
            if (StepToward(c, sl.targetPos, dt))
            {
                c.state      = CustomerState::WAITING;
                c.stateTimer = 0.f;
                c.animState  = Sprite::AnimationState{};
            }
        }
        else if (c.state == CustomerState::WAITING)
        {
            c.moveState = Entity::MoveState::IDLE;  // stay idle while waiting
            c.patience -= dt;

            // Apply patience penalty for wrong order
            if (c.receiveOrderResult == ReceiveOrder::WRONG_ORDER)
            {
                c.patience -= 5.f;
                c.receiveOrderResult = ReceiveOrder::NO_ORDER;
				
                std::cerr << "Customer received wrong order! Patience left with " << c.patience << " seconds.\n";

            }
            
            if (c.patience <= 0.f)
            {
                c.patience = 0.f;
                c.patienceRanOut = true;
                c.moveState = Entity::MoveState::WALKING;
                c.state = CustomerState::LEAVING;
                c.stateTimer = 0.f;
            }
        }
        else if (c.state == CustomerState::LEAVING)
        {
            if (StepToward(c, sl.spawnPos, dt))
            {
                // Free the target slot so the next customer can use it
                if (sl.targetIndex >= 0 && sl.targetIndex < PoolState::MAX_TARGETS)
                    p.targetOccupied[sl.targetIndex] = false;
                sl.active = false;
                return;
            }
        }

        // Advance animation using shared sprite sheet
        unsigned int texSlot = 0, tagIndex = 1;
        ResolveAnim(c, texSlot, tagIndex);
        if (texSlot < p.sprite.count && p.sprite.textures[texSlot])
            Sprite::UpdateAnimation(c.animState, p.sprite.metas[texSlot], tagIndex);
    }

    //========================================================================
    // CustomerPool_Load — allocates shared GPU resources (textures, meshes)
    //========================================================================
    void CustomerPool_Load(PoolState& p)
    {
        p.spriteMesh = BasicUtilities::createSquareMesh(0.25f, 0.25f);
        p.bubbleMesh = BasicUtilities::createSquareMesh();

        Sprite::LoadEntry(p.sprite, 0,
            "Assets/character-assets/AI_Customer_Idle.png",
            "Assets/character-assets/AI_Customer_Idle.json");
        Sprite::LoadEntry(p.sprite, 1,
            "Assets/character-assets/AI_Customer_Walk.png",
            "Assets/character-assets/AI_Customer_Walk.json");
        Sprite::LoadEntry(p.sprite, 2,
            "Assets/character-assets/AI_Customer_Idle_Carry.png",
            "Assets/character-assets/AI_Customer_Idle_Carry.json");
        Sprite::LoadEntry(p.sprite, 3,
            "Assets/character-assets/AI_Customer_Walk_Carry.png",
            "Assets/character-assets/AI_Customer_Walk_Carry.json");
    }

    //========================================================================
    // CustomerPool_Init — sets spawn point, copies target positions, resets slots
    // spawnTimer starts at 0 so the first customer appears on the first Update.
    //========================================================================
    void CustomerPool_Init(PoolState& p, AEVec2 spawnPos,
                           const AEVec2* targets, int targetCount)
    {
        p.spawnPoint  = spawnPos;
        p.spawnTimer  = 0.f;

        // Copy target positions and reset occupancy flags
        p.targetCount = (targetCount > PoolState::MAX_TARGETS)
                        ? PoolState::MAX_TARGETS : targetCount;
        for (int i = 0; i < p.targetCount; ++i)
        {
            p.targetPositions[i] = targets[i];
            p.targetOccupied[i]  = false;
        }

        // Reset all customer slots
        for (int i = 0; i < POOL_MAX; ++i)
            p.slots[i] = PoolState::Slot{};
    }

    //========================================================================
    // CustomerPool_Update — spawns new customers and advances all active slots
    //========================================================================
    void CustomerPool_Update(PoolState& p, float dt)
    {
        // Count how many customers are currently in play
        int activeCount = 0;
        for (int i = 0; i < POOL_MAX; ++i)
            if (p.slots[i].active) ++activeCount;

        // Count down the spawn timer; try to spawn when it reaches 0
        p.spawnTimer -= dt;
        if (p.spawnTimer <= 0.f && activeCount < POOL_MAX)
        {
            // Collect unoccupied target indices
            int freeTargets[PoolState::MAX_TARGETS];
            int freeCount = 0;
            for (int i = 0; i < p.targetCount; ++i)
                if (!p.targetOccupied[i])
                    freeTargets[freeCount++] = i;

            if (freeCount > 0)
            {
                // Pick a random unoccupied target
                const int chosenIdx = freeTargets[rand() % freeCount];

                // Find a free slot
                int slotIdx = -1;
                for (int i = 0; i < POOL_MAX; ++i)
                    if (!p.slots[i].active) { slotIdx = i; break; }

                if (slotIdx >= 0)
                {
                    // Random order: FlowerType 1–6
                    const FlowerType orderType = static_cast<FlowerType>((rand() % 6) + 1);

                    // Modifier distribution: 60% NONE, 20% FIRE, 20% POISON
                    FlowerModifier orderMod = FlowerModifier::NONE;
                    const int roll = rand() % 10;
                    if      (roll >= 8) orderMod = FlowerModifier::FIRE;
                    else if (roll >= 6) orderMod = FlowerModifier::POISON;

                    // Initialise the slot
                    PoolState::Slot& sl   = p.slots[slotIdx];
                    sl.spawnPos           = p.spawnPoint;
                    sl.targetPos          = p.targetPositions[chosenIdx];
                    sl.targetIndex        = chosenIdx;
                    sl.active             = true;

                    // Reset customer data
                    sl.customer                    = Customer{};
                    sl.customer.position           = p.spawnPoint;
                    sl.customer.velocity           = {};
                    sl.customer.speed              = SPEED;
                    sl.customer.facing             = Entity::FaceDirection::RIGHT;
                    sl.customer.moveState          = Entity::MoveState::WALKING;
                    sl.customer.state              = CustomerState::ARRIVING;
                    sl.customer.stateTimer         = 0.f;
                    sl.customer.animState          = Sprite::AnimationState{};
                    sl.customer.patience           = PATIENCE_MAX;
                    sl.customer.patienceRanOut     = false;
                    sl.customer.servedSuccessfully = false;
                    sl.customer.order              = orderType;
                    sl.customer.orderModifier      = orderMod;

                    p.targetOccupied[chosenIdx] = true;
                }
            }

            // Reschedule regardless of whether a spawn actually occurred
            p.spawnTimer = RandRange(SPAWN_INTERVAL_MIN, SPAWN_INTERVAL_MAX);
        }

        // Advance all active slots
        for (int i = 0; i < POOL_MAX; ++i)
            if (p.slots[i].active)
                UpdateSlot(p, p.slots[i], dt);
    }

    //========================================================================
    // CustomerPool_Draw — draw all customer sprites (call before player draw)
    //========================================================================
    void CustomerPool_Draw(const PoolState& p, s8 fontId)
    {
        (void)fontId;
        for (int i = 0; i < POOL_MAX; ++i)
        {
            if (!p.slots[i].active) continue;
            DrawCustomerSprite(p.spriteMesh, p.sprite, p.slots[i].customer);
        }
    }

    //========================================================================
    // CustomerPool_DrawUI — draw patience bars + order bubbles (after player)
    //========================================================================
    void CustomerPool_DrawUI(const PoolState& p, s8 fontId, AEGfxTexture** flowerIcons)
    {
        for (int i = 0; i < POOL_MAX; ++i)
        {
            if (!p.slots[i].active) continue;
            DrawCustomerUI(p.bubbleMesh, fontId, p.slots[i].customer, flowerIcons);
        }
    }

    //========================================================================
    // CustomerPool_Free — release shared mesh memory
    //========================================================================
    void CustomerPool_Free(PoolState& p)
    {
        if (p.spriteMesh) { AEGfxMeshFree(p.spriteMesh); p.spriteMesh = nullptr; }
        if (p.bubbleMesh) { AEGfxMeshFree(p.bubbleMesh); p.bubbleMesh = nullptr; }
        for (int i = 0; i < POOL_MAX; ++i) p.slots[i].active = false;
    }

    //========================================================================
    // CustomerPool_Unload — release shared texture memory
    //========================================================================
    void CustomerPool_Unload(PoolState& p)
    {
        p.sprite.Free();
    }

    //========================================================================
    // CustomerPool_TryServe — find the nearest WAITING customer within reach
    //   and attempt to serve them with the offered flower + modifier.
    //
    //   Returns GOLD_REWARD_PLAIN / GOLD_REWARD_MODIFIED on a correct match.
    //   Returns 0 if a customer is in range but the order does not match.
    //   Returns -1 if no WAITING customer is within SERVE_RADIUS.
    //========================================================================
    int CustomerPool_TryServe(PoolState& p, FlowerType type, FlowerModifier mod,
                          AEVec2 playerPos)
    {
        constexpr float SERVE_RADIUS = 120.f;  // [TUNE] world-unit interaction reach

        int   bestSlot  = -1;
        float bestDist  = SERVE_RADIUS;
        bool  anyInRange = false;

        for (int i = 0; i < POOL_MAX; ++i)
        {
            const PoolState::Slot& sl = p.slots[i];
            if (!sl.active || sl.customer.state != CustomerState::WAITING) continue;

            float dx   = sl.customer.position.x - playerPos.x;
            float dy   = sl.customer.position.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < SERVE_RADIUS)
            {
                anyInRange = true;
                if (dist < bestDist) { bestDist = dist; bestSlot = i; }
            }
        }

        if (!anyInRange || bestSlot < 0) return -1;

        PoolState::Slot& sl = p.slots[bestSlot];

        // Wrong flower or wrong modifier — mark the wrong-order visual/penalty but DO NOT
        // start leaving the customer here.
        //
        // NOTE: Previous behaviour returned 0 for a wrong order. Change this to return
        // -2 to give callers an unambiguous signal that a wrong order occurred so they
        // can reset player sprite/animation (stop "holding" animation and switch back to
        // normal walking). Existing callers should be updated to handle -2.
        if (type != sl.customer.order || mod != sl.customer.orderModifier)
        {
            sl.customer.receiveOrderResult = ReceiveOrder::WRONG_ORDER;
            return -2;
        }

        // Correct match — serve the customer and begin their LEAVING walk
        sl.customer.servedSuccessfully = true;
        sl.customer.moveState          = Entity::MoveState::WALKING;
        sl.customer.state              = CustomerState::LEAVING;
        sl.customer.stateTimer         = 0.f;

        return (mod == FlowerModifier::NONE) ? GOLD_REWARD_PLAIN : GOLD_REWARD_MODIFIED;
    }

} // namespace CustomerSystem
