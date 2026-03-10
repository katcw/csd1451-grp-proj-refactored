/*===========================================================================
@filename   main_menu.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "main_menu.hpp"
#include "game_state_manager.hpp"
#include "game_state_list.hpp"
#include "utilities.hpp"

namespace
{
	AEGfxTexture* playButtonTexture = nullptr;
	AEGfxTexture* creditsButtonTexture = nullptr;
	AEGfxTexture* exitButtonTexture = nullptr;

	AEGfxVertexList* buttonMesh = nullptr;

	AEMtx33 buttonScale = { 0 },
		playButtonTranslate = { 0 }, creditsButtonTranslate = { 0 }, exitButtonTranslate = { 0 },
		playButtonTransform = { 0 }, creditsButtonTransform = { 0 }, exitButtonTransform = { 0 };

	BasicUtilities::Button playButton, creditsButton, exitButton;

	float easeSpeed = 8.0f; // Adjust this to control hover speed
}

void MainMenu_Load()
{
	std::cout << "MAIN MENU LOADED" << '\n';
	playButtonTexture = BasicUtilities::loadTexture("Assets/prototype_play_button.png");
	creditsButtonTexture = BasicUtilities::loadTexture("Assets/prototype_credits_button.png");
	exitButtonTexture = BasicUtilities::loadTexture("Assets/prototype_exit_button.png");
}

void MainMenu_Initialise()
{
	buttonMesh = BasicUtilities::createSquareMesh();

	playButton = { -400.f, 0.0f, 200.0f, 200.0f, 1.0f, 1.1f, 1.0f, false };
	creditsButton = { 0.0f, 0.0f, 200.0f, 200.0f, 1.0f, 1.1f, 1.0f, false };
	exitButton = { 400.f, 0.0f, 200.0f, 200.0f, 1.0f, 1.1f, 1.0f, false };

	AEMtx33Trans(&playButtonTranslate, playButton.x, playButton.y);
	AEMtx33Trans(&creditsButtonTranslate, creditsButton.x, creditsButton.y);
	AEMtx33Trans(&exitButtonTranslate, exitButton.x, exitButton.y);
}

void MainMenu_Update()
{
	s32 mouseX, mouseY;
	AEInputGetCursorPosition(&mouseX, &mouseY);

	float worldMouseX, worldMouseY;
	BasicUtilities::screenCoordsToWorldCoords(mouseX, mouseY, worldMouseX, worldMouseY);

	float deltaTime = static_cast<float>(AEFrameRateControllerGetFrameTime());

	playButton.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	creditsButton.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	exitButton.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);

	if (playButton.isClicked(worldMouseX, worldMouseY) &&
		AEInputCheckTriggered(AEVK_LBUTTON))
	{
		nextState = GS_TUTORIAL;
	}

	if (exitButton.isClicked(worldMouseX, worldMouseY) &&
		AEInputCheckTriggered(AEVK_LBUTTON))
	{
		nextState = GS_EXIT;
	}

	AEMtx33Scale(&buttonScale, 200.0f * playButton.currentScale, 200.0f * playButton.currentScale);
	AEMtx33Concat(&playButtonTransform, &playButtonTranslate, &buttonScale);

	AEMtx33Scale(&buttonScale, 200.0f * creditsButton.currentScale, 200.0f * creditsButton.currentScale);
	AEMtx33Concat(&creditsButtonTransform, &creditsButtonTranslate, &buttonScale);

	AEMtx33Scale(&buttonScale, 200.0f * exitButton.currentScale, 200.0f * exitButton.currentScale);
	AEMtx33Concat(&exitButtonTransform, &exitButtonTranslate, &buttonScale);
}

void MainMenu_Draw()
{
	AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);

	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);
	AEGfxTextureSet(playButtonTexture, 0, 0);
	AEGfxSetTransform(playButtonTransform.m);
	AEGfxMeshDraw(buttonMesh, AE_GFX_MDM_TRIANGLES);

	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);
	AEGfxTextureSet(creditsButtonTexture, 0, 0);
	AEGfxSetTransform(creditsButtonTransform.m);
	AEGfxMeshDraw(buttonMesh, AE_GFX_MDM_TRIANGLES);

	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);
	AEGfxTextureSet(exitButtonTexture, 0, 0);
	AEGfxSetTransform(exitButtonTransform.m);
	AEGfxMeshDraw(buttonMesh, AE_GFX_MDM_TRIANGLES);
}

void MainMenu_Free()
{
	AEGfxMeshFree(buttonMesh);
}

void MainMenu_Unload()
{
	AEGfxTextureUnload(playButtonTexture);
	AEGfxTextureUnload(creditsButtonTexture);
	AEGfxTextureUnload(exitButtonTexture);
}
