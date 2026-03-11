#pragma once

#include "AEEngine.h"
#include "entity.hpp"
#include "plant_system.hpp"

enum class ShopItemType
{
    NONE = 0,
    SEED,
    BONUS_GOLD,
    EXTRA_STORAGE
};

struct ShopItem
{
    ShopItemType type = ShopItemType::NONE;
    SeedType     seed = SeedType::NONE;
    int          price = 0;
    const char* name = "";
};

struct Merchant
{
    AEVec2 position{ 0.f, -400.f };
    AEVec2 velocity{ 0.f, 0.f };

    float speed = 250.f;

    bool active = false;
    bool arrived = false;
    bool shopOpen = false;

    Entity::FaceDirection facing = Entity::FaceDirection::UP;
    Entity::MoveState     moveState = Entity::MoveState::IDLE;

    ShopItem items[9]{};

    float slotX[9]{};
    float slotY[9]{};

    int   hoveredIndex = -1;
    char  uiMessage[64] = "";
    float uiMessageTimer = 0.0f;
};

namespace MerchantSystem
{
    void Load();
    void Init(Merchant& m);
    void Start(Merchant& m);
    void Update(Merchant& m, float dt);
    void Draw(Merchant& m, s8 fontId);
    void HandleMousePurchase(Merchant& m, HeldState& held);
    void Free();
    void Unload();
}