/*===========================================================================
@filename   main_menu.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief
============================================================================*/

#include <iostream>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "main_menu.hpp"
#include "game_state_manager.hpp"
#include "game_state_list.hpp"
#include "utilities.hpp"

namespace Sprite = BasicUtilities::Sprite;

namespace
{
	// All necessary textures
	AEGfxTexture* playTexture		= nullptr;
	AEGfxTexture* creditsTexture	= nullptr;
	AEGfxTexture* quitTexture		= nullptr;
	AEGfxTexture* settingsTexture	= nullptr;
	AEGfxTexture* tutorialTexture	= nullptr;

	// All necessary vertex lists
	AEGfxVertexList* buttonMesh		= nullptr;
	AEGfxVertexList* miniButtonMesh	= nullptr;
	AEGfxVertexList* spriteMesh		= nullptr;

	// All necessary matrices
	AEMtx33 buttonScale{}, miniButtonScale{};
	AEMtx33 playTranslate{}, playTransform{};
	AEMtx33 creditsTranslate{}, creditsTransform{};
	AEMtx33 quitTranslate{}, quitTransform{};
	AEMtx33 settingsTranslate{}, settingsTransform{};
	AEMtx33 tutorialTranslate{}, tutorialTransform{};

	// Button struct to store button data
	BasicUtilities::Button play, credits, quit, settings, tutorial;

	// Character sprite data 
	Sprite::SpriteData characterSprite;
	Sprite::AnimationState characterSpriteAnimState;
	AEVec2 menuCharacterPosition{ 400.0f, -180.0f };
	AEVec2 menuCharacterScale{ 800.0f, 800.0f };
	constexpr unsigned int idleTextureSlot	= 0;
	constexpr unsigned int idleFacingTag	= 3;

	// Hover ease speed
	float easeSpeed	= 8.0f;
}

void MainMenu_Load()
{
	// Console message for debug
	std::cout << "MAIN MENU LOADED" << '\n';

	playTexture		= BasicUtilities::loadTexture("Assets/ui-assets/play_button.png");
	creditsTexture	= BasicUtilities::loadTexture("Assets/ui-assets/credits_button.png");
	quitTexture		= BasicUtilities::loadTexture("Assets/ui-assets/exit_button.png");
	settingsTexture = BasicUtilities::loadTexture("Assets/ui-assets/settings_button.png");
	tutorialTexture = BasicUtilities::loadTexture("Assets/ui-assets/tutorial_button.png");

	Sprite::LoadEntry(characterSprite, idleTextureSlot, "Assets/character-assets/MC_Idle.png", "Assets/character-assets/MC_Idle.json");
}

void MainMenu_Initialise()
{
	// Create square mesh as template
	buttonMesh = BasicUtilities::createSquareMesh();
	miniButtonMesh = BasicUtilities::createSquareMesh();
	spriteMesh = BasicUtilities::createSquareMesh(0.25, 0.25);

	// Button struct initialisation
	// Adjust values here to change button positions and scales
	play = { -450.0f, 0.0f, 350.0f, 80.0f , 1.0f, 1.1f, 1.0f, false };
	credits = { -450.0f, -130.0f, 350.0f, 80.0f, 1.0f, 1.1f, 1.0f, false };
	quit = { -450.0f, -260.0f, 350.0f, 80.0f, 1.0f, 1.1f, 1.0f, false };
	settings = { 580.0f, 350.0f, 150.0f, 150.0f, 1.0f, 1.08f, 1.0f, false };
	tutorial = { 700.0, 350.0f, 150.0f, 150.0f, 1.0f, 1.08f, 1.0f, false };

	// Translation matrices to apply positions
	AEMtx33Trans(&playTranslate, play.x, play.y);
	AEMtx33Trans(&creditsTranslate, credits.x, credits.y);
	AEMtx33Trans(&quitTranslate, quit.x, quit.y);
	AEMtx33Trans(&settingsTranslate, settings.x, settings.y);
	AEMtx33Trans(&tutorialTranslate, tutorial.x, tutorial.y);
}

void MainMenu_Update()
{
	// dT
	float deltaTime = static_cast<float>(AEFrameRateControllerGetFrameTime());

	// Get current mouse position
	s32 mouseX, mouseY;
	AEInputGetCursorPosition(&mouseX, &mouseY);

	// Convert mouse positions from screen to world coordinates
	float worldMouseX, worldMouseY;
	BasicUtilities::screenCoordsToWorldCoords(mouseX, mouseY, worldMouseX, worldMouseY);

	// Apply hover easing to all visible buttons
	play.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	credits.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	quit.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	settings.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);
	tutorial.updateHover(worldMouseX, worldMouseY, deltaTime, easeSpeed);

	// Button functionality
	if (play.isClicked(worldMouseX, worldMouseY) &&
		AEInputCheckTriggered(AEVK_LBUTTON))
	{
		nextState = GS_LEVEL1;
	}

	if (tutorial.isClicked(worldMouseX, worldMouseY) &&
		AEInputCheckTriggered(AEVK_LBUTTON))
	{
		nextState = GS_TUTORIAL;
	}

	if (quit.isClicked(worldMouseX, worldMouseY) &&
		AEInputCheckTriggered(AEVK_LBUTTON))
	{
		nextState = GS_EXIT;
	}

	AEMtx33Scale(&buttonScale, play.width * play.currentScale, play.height * play.currentScale);
	AEMtx33Concat(&playTransform, &playTranslate, &buttonScale);

	AEMtx33Scale(&buttonScale, credits.width * credits.currentScale, credits.height * credits.currentScale);
	AEMtx33Concat(&creditsTransform, &creditsTranslate, &buttonScale);

	AEMtx33Scale(&buttonScale, quit.width * quit.currentScale, quit.height * quit.currentScale);
	AEMtx33Concat(&quitTransform, &quitTranslate, &buttonScale);

	AEMtx33Scale(&miniButtonScale, settings.width * settings.currentScale, settings.height * settings.currentScale);
	AEMtx33Concat(&settingsTransform, &settingsTranslate, &miniButtonScale);

	AEMtx33Scale(&miniButtonScale, tutorial.width * tutorial.currentScale, tutorial.height * tutorial.currentScale);
	AEMtx33Concat(&tutorialTransform, &tutorialTranslate, &miniButtonScale);

	Sprite::UpdateAnimation(characterSpriteAnimState, characterSprite.jsonList[idleTextureSlot], idleFacingTag);

	// [!] RUNE TEST FOR HH
	//if (AEInputCheckTriggered(AEVK_M))
	//{
	//	nextState = GS_RUNE_TEST;
	//}
}

void MainMenu_Draw()
{
	AEGfxSetBackgroundColor(0.85f, 0.84f, 0.80f);
	AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);

	if (spriteMesh && characterSprite.textureList[idleTextureSlot])
	{
		Sprite::DrawAnimation(spriteMesh,
			characterSprite.textureList[idleTextureSlot],
			characterSpriteAnimState,
			menuCharacterPosition,
			menuCharacterScale);
	}

	BasicUtilities::Draw_UI_Element(buttonMesh, playTexture, playTransform);
	BasicUtilities::Draw_UI_Element(buttonMesh, creditsTexture, creditsTransform);
	BasicUtilities::Draw_UI_Element(buttonMesh, quitTexture, quitTransform);
	BasicUtilities::Draw_UI_Element(miniButtonMesh, settingsTexture, settingsTransform);
	BasicUtilities::Draw_UI_Element(miniButtonMesh, tutorialTexture, tutorialTransform);
}

void MainMenu_Free()
{
	if (buttonMesh)
	{
		AEGfxMeshFree(buttonMesh);
		buttonMesh = nullptr;
	}

	if (miniButtonMesh)
	{
		AEGfxMeshFree(miniButtonMesh);
		miniButtonMesh = nullptr;
	}

	if (spriteMesh)
	{
		AEGfxMeshFree(spriteMesh);
		spriteMesh = nullptr;
	}
}

void MainMenu_Unload()
{
	AEGfxTextureUnload(playTexture);
	AEGfxTextureUnload(creditsTexture);
	AEGfxTextureUnload(quitTexture);
	AEGfxTextureUnload(settingsTexture);
	AEGfxTextureUnload(tutorialTexture);

	playTexture = nullptr;
	creditsTexture = nullptr;
	quitTexture = nullptr;
	settingsTexture = nullptr;
	tutorialTexture = nullptr;

	characterSprite.Free();
	characterSpriteAnimState = Sprite::AnimationState{};
}
