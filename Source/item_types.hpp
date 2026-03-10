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
    NONE,
    ROSE,
    TULIP,
    SUNFLOWER,
    DAISY,
    LILY,
    ORCHID,
    COUNT
};

enum class FlowerType
{
    NONE,
    ROSE,
    TULIP,
    SUNFLOWER,
    DAISY,
    LILY,
    ORCHID
};

enum class HeldItem { NONE, SEED, FLOWER };

struct HeldState
{
    HeldItem   type   = HeldItem::NONE;
    SeedType   seed   = SeedType::NONE;
    FlowerType flower = FlowerType::NONE;  
};
