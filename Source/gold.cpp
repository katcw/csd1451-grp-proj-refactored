/*===========================================================================
@filename   gold.cpp
@brief      See gold.hpp. Logic ported from the original repo's
            inventory_system.cpp (GoldWallet functions); simplified to a
            single balance and wrapped in a namespace.
============================================================================*/
#include "gold.hpp"
#include "utilities.hpp"
#include <cstdio>

namespace Gold
{
    //========================================================================
    // HUD position (screen-space; 0,0 = centre, ±800 X, ±450 Y)
    // Placed just below the time-of-day clock at (620, 390)
    //========================================================================
    constexpr float HUD_X = 620.f;
    constexpr float HUD_Y = 350.f;

    //========================================================================
    // STATE
    //========================================================================
    static int gBalance = 0;

    //========================================================================
    // LIFECYCLE
    //========================================================================
    void Init(int startingGold)
    {
        gBalance = (startingGold > 0) ? startingGold : 0;
    }

    //========================================================================
    // HUD
    //========================================================================
    void Draw(s8 fontId)
    {
        char buf[16];
        sprintf_s(buf, sizeof(buf), "G: %d", gBalance);
        // Gold-coloured text; screen-space so camera has no effect
        BasicUtilities::drawText(fontId, buf, HUD_X, HUD_Y, 1.f, 1.f, 0.85f, 0.f);
    }

    //========================================================================
    // MUTATIONS
    //========================================================================
    void Earn(int amount)
    {
        if (amount <= 0) return;
        gBalance += amount;
    }

    bool CanAfford(int cost)
    {
        if (cost <= 0) return true;
        return gBalance >= cost;
    }

    bool Spend(int cost)
    {
        if (cost <= 0) return true;
        if (gBalance < cost) return false;
        gBalance -= cost;
        return true;
    }

    void ApplyPenalty(int amount)
    {
        if (amount <= 0) return;
        gBalance -= amount;
        if (gBalance < 0) gBalance = 0;
    }

    //========================================================================
    // QUERY
    //========================================================================
    int GetTotal() { return gBalance; }
}
