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

using FP = void (*)(void);

extern int currentState, previousState, nextState; // Import ints from "game_state_manager.cpp"
extern FP fpLoad, fpInitialise, fpUpdate, fpDraw, fpFree, fpUnload; // Import function pointers from "game_state_manager.cpp"

void GSM_Initialise(int startingState);
void GSM_Update();
