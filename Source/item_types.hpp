/*===========================================================================
@filename   item_types.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Shared item/held-state types used across the different core 
            gameplay mechanics systems
            
            [1] SeedType contains the different types of seeds
            [2] FlowerType contains the different flowers
            [3] HeldItem contains the states of whichever type is
                being held in hand
            [4] HeldState struct contains holding information
============================================================================*/
#pragma once

enum class SeedType
{
    // 1 LILY 2 ORCHID 3 DAISY 4 ROSE 5 SUNFLOWER 6 TULIP
    NONE,
    LILY, 
    ORCHID,
    DAISY,
    ROSE,
    SUNFLOWER,
    TULIP,
    COUNT
};

enum class FlowerType
{
    NONE,
    LILY,
    ORCHID,
    DAISY,
    ROSE,
    SUNFLOWER,
    TULIP
};

enum class FlowerModifier { NONE, FIRE, POISON };

enum class HeldItem { NONE, SEED, FLOWER };

struct HeldState
{
    HeldItem       type     = HeldItem::NONE;
    SeedType       seed     = SeedType::NONE;
    FlowerType     flower   = FlowerType::NONE;
    FlowerModifier modifier = FlowerModifier::NONE;  // active modifier on a held flower
};
