/*===========================================================================
@filename   tables.hpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief      Table prop system. Handles rune tables (map ID 5) and poison
            tables (map ID 6). Each table type has its own texture and is
            solid for AABB collision. Supports flower infusion: the player
            places a held flower on a table (E key), waits INFUSE_TIME
            seconds, then picks it up with a FIRE or POISON modifier applied.

    TableSystem_Load(state)
    TableSystem_Init(state, runePosArr, runeCount, poisonPosArr, poisonCount)
    TableSystem_Update(state, dt, playerPos, held)
    TableSystem_Draw(state)
    TableSystem_Free(state)
    TableSystem_Unload(state)
============================================================================*/
#pragma once

#include "AEEngine.h"
#include "item_types.hpp"

namespace TableSystem
{
    constexpr int MAP_ID_RUNE   = 5;  // tile ID for rune table in map files
    constexpr int MAP_ID_POISON = 6;  // tile ID for poison table in map files

    constexpr int MAX_TABLES = 8;   // [TUNE] maximum instances per table type

    // Draw size — table sprites are 64×32 px source; displayed at 2× scale
    constexpr float TABLE_DRAW_W = 128.f;   // world units wide
    constexpr float TABLE_DRAW_H =  64.f;   // world units tall

    // Collision half-extents — covers the full drawn sprite
    constexpr float TABLE_COLL_HW = TABLE_DRAW_W * 0.5f;  // 64.f
    constexpr float TABLE_COLL_HH = TABLE_DRAW_H * 0.5f;  // 32.f

    // Interaction constants
    constexpr float INFUSE_TIME           = 3.f;   // [TUNE] seconds to apply modifier
    constexpr float TABLE_INTERACT_RADIUS = 80.f;  // [TUNE] world-unit reach for E key

    enum class TableType { RUNE, POISON };

    struct Table
    {
        AEVec2    pos  = {};
        TableType type = TableType::RUNE;
    };

    // Per-table slot — tracks a flower being infused on this table.
    struct TableSlot
    {
        bool       occupied = false;
        FlowerType flower   = FlowerType::NONE;
        float      timer    = 0.f;   // counts down from INFUSE_TIME; 0 = infusion complete
        bool       ready    = false; // true when infusion is done and flower can be picked up
    };

    struct State
    {
        Table            tables[MAX_TABLES * 2];  // rune entries first, then poison
        int              count     = 0;
        AEGfxTexture*    runeTex   = nullptr;
        AEGfxTexture*    poisonTex = nullptr;
        AEGfxVertexList* mesh      = nullptr;
        TableSlot        slots[MAX_TABLES * 2];   // one infusion slot per table entry
    };

    // Load — allocates GPU resources. Call once in the state's Load().
    void TableSystem_Load(State& s);

    // Init — populates the table list from two world-position arrays.
    //        Rune tables are stored first, then poison tables.
    void TableSystem_Init(State& s,
                          const AEVec2* runePosArr,    int runeCount,
                          const AEVec2* poisonPosArr,  int poisonCount);

    // Update — E-key interaction: place held flower (starts infusion) or
    //          pick up completed flower (applies FIRE/POISON modifier to held).
    //          Call before PlantSystem_Update() each frame.
    void TableSystem_Update(State& s, float dt, AEVec2 playerPos, HeldState& held);

    // Draw — renders all tables at their world positions (128×64 world units).
    //        Also draws an on-table flower indicator when a slot is occupied.
    //        flowerIcons: optional texture array indexed by FlowerType (1–6);
    //        if nullptr or texture missing, falls back to a coloured square.
    void TableSystem_Draw(const State& s, AEGfxTexture** flowerIcons = nullptr);

    // Free — releases mesh memory. Call in the state's Free().
    void TableSystem_Free(State& s);

    // Unload — releases texture memory. Call in the state's Unload().
    void TableSystem_Unload(State& s);
}
