/*===========================================================================
@filename   gold.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Manages the player's gold balance. The balance is hidden inside
            gold.cpp and can only be read or changed through the functions
            below. Call Init once at level start, then use Earn/Spend/
            ApplyPenalty to change the balance during gameplay.
============================================================================*/
#pragma once
#include "AEEngine.h"

namespace Gold
{
    /******************************************************************************/
    /*
    @brief  Sets the player's starting gold balance at the beginning of a level.
            If a negative value is given, the balance is set to 0 instead.

    @param  startingGold  The amount of gold to start with. Defaults to 0.
    */
    /******************************************************************************/
    void Init(int startingGold = 0);

    /******************************************************************************/
    /*
    @brief  Draws the current gold balance on screen as "G: {amount}".
            The text is drawn in gold colour and is not affected by the camera,
            so it always appears in the same position regardless of where the
            player is in the world.

    @param  fontId  The font to use when drawing the text.
    */
    /******************************************************************************/
    void Draw(s8 fontId);

    /******************************************************************************/
    /*
    @brief  Adds gold to the player's balance. Does nothing if the amount
            is zero or negative.

    @param  amount  How much gold to add.
    */
    /******************************************************************************/
    void Earn(int amount);

    /******************************************************************************/
    /*
    @brief  Checks whether the player has enough gold to cover a cost,
            without actually spending any. A cost of zero or less always
            returns true.

    @param  cost  The amount to check against the current balance.

    @return true if the player can afford it, false if not.
    */
    /******************************************************************************/
    bool CanAfford(int cost);

    /******************************************************************************/
    /*
    @brief  Deducts gold from the player's balance if they can afford it.
            Does nothing and returns true if the cost is zero or less.
            Does nothing and returns false if the balance is too low.

    @param  cost  How much gold to deduct.

    @return true if the purchase went through, false if the player
            could not afford it.
    */
    /******************************************************************************/
    bool Spend(int cost);

    /******************************************************************************/
    /*
    @brief  Deducts gold from the player's balance as a penalty.
            Unlike Spend, this always goes through regardless of the current
            balance — but the balance will never drop below zero.

    @param  amount  How much gold to deduct.
    */
    /******************************************************************************/
    void ApplyPenalty(int amount);

    /******************************************************************************/
    /*
    @brief  Returns the player's current gold balance without changing it.

    @return The current gold balance.
    */
    /******************************************************************************/
    int GetTotal();
}
