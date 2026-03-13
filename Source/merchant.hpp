#pragma once

#include "entity.hpp"

enum class ShopItemType
{
    SPEED_BOOST,
    WATER_UPGRADE,
    COIN_MULTIPLIER,
    START_GOLD,
    MERCHANT_FAVOR,
    DISCOUNT_COUPON,
    MERCHANT_GAMBLE,
    NONE
};

struct ShopItem
{
    ShopItemType type = ShopItemType::NONE;
    int price = 0;
    char name[32]{};
    char desc[96]{};
    bool nextLevelOnly = false;
    bool nextMerchantOnly = false;
};

struct RunBonuses
{
    // Permanent through the rest of the game/run
    int speedLevel = 0;
    int waterLevel = 0;

    // Next level only
    float nextLevelCoinMultiplier = 1.0f;
    int nextLevelStartGoldBonus = 0;

    // Merchant-related
    bool freeRerollAvailable = false;
    float nextMerchantDiscount = 0.0f; // 0.25f = 25% off next merchant shop
};

struct Merchant
{
    bool active = false;
    bool arrived = false;
    bool shopOpen = false;

	AEVec2 position{}; // unc change this ltr on to match ur entity system coords
    AEVec2 velocity{}; // this too 

    /*Entity::Position position{};
    Entity::Velocity velocity{};*/

    float speed = 250.0f;
    Entity::FaceDirection facing = Entity::FaceDirection::DOWN;
    Entity::MoveState moveState = Entity::MoveState::IDLE;

    static constexpr int OFFER_COUNT = 3;

    ShopItem offers[OFFER_COUNT]{};
    float slotX[OFFER_COUNT]{};
    float slotY[OFFER_COUNT]{};

    int hoveredIndex = -1;

    char uiMessage[128]{};
    float uiMessageTimer = 0.0f;

    bool rerolledThisVisit = false;
    bool usedFreeRerollThisVisit = false;
};

namespace MerchantSystem
{
    void Load();
    void Init(Merchant& m);
    void Start(Merchant& m, const RunBonuses& bonuses);
    void Update(Merchant& m, float dt);
    void Draw(Merchant& m, s8 fontId);
    void HandleMousePurchase(Merchant& m, RunBonuses& bonuses);
    void Free();
    void Unload();
}