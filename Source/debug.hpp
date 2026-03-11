/*===========================================================================
@filename   debug.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Debug overlay settings. Include in any game state that draws
            debug information. Edit the colour constants below to re-tune
            hitbox colours without touching game-state code.

            Usage in a game state:
              #include "debug.hpp"
              // Toggle (wire up in Update):
              if (AEInputCheckTriggered(AEVK_LBRACKET)) Debug::enabled = !Debug::enabled;
              // Draw (use in Draw):
              if (Debug::enabled) { ... }
============================================================================*/
#pragma once

namespace Debug
{
    // Global toggle for debug mode
    extern bool enabled;

    /******************************************************************************/
    /*
    Bounding box colours in debug mode
    */
    /******************************************************************************/
    constexpr float PLAYER_R   = 1.f, PLAYER_G   = 1.f, PLAYER_B   = 0.f;  // Yellow
    constexpr float POT_R      = 1.f, POT_G      = 1.f, POT_B      = 0.f;  // Yellow
    constexpr float CHEST_R    = 0.f, CHEST_G    = 1.f, CHEST_B    = 0.f;  // Green
    constexpr float CUSTOMER_R     = 0.f,  CUSTOMER_G     = 1.f,  CUSTOMER_B     = 1.f;  // Cyan
    constexpr float RUNE_TABLE_R   = 0.6f, RUNE_TABLE_G   = 0.4f, RUNE_TABLE_B   = 1.f;  // Purple
    constexpr float POISON_TABLE_R = 0.2f, POISON_TABLE_G = 0.8f, POISON_TABLE_B = 0.2f; // Lime

    // Bounding box transparency
    constexpr float HITBOX_ALPHA = 0.35f;
}
