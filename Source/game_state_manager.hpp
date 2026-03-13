/*===========================================================================
@filename   game_state_manager.hpp
@author(s)  Kalen Tan
@course     CSD1451
@section    Section B

@brief	
============================================================================*/

#pragma once

#include <iostream>
#include "game_state_list.hpp"
#include "merchant.hpp"   // ADD THIS

using FP = void (*)(void);

extern int currentState, previousState, nextState;
extern FP fpLoad, fpInitialise, fpUpdate, fpDraw, fpFree, fpUnload;

// ADDED THIS GLOBAL RUN BONUS STORAGE
extern RunBonuses gRunBonuses;

void GSM_Initialise(int startingState);
void GSM_Update();