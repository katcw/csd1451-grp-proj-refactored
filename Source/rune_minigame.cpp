/**
 * @file rune_minigame.cpp
 * @brief Rune minigame implementation.
 * @details Implements a minigame where the player must touch runes in sequential
 *          order by clicking and dragging the mouse over them. Touching a rune
 *          out of order resets the sequence. The minigame is complete when all
 *          runes are touched in the correct order. A connecting line is drawn
 *          between touched runes and from the last touched rune to the mouse.
 */

#include <cstdlib> // For rand()
#include <cmath>   // For sqrtf, atan2f
#include <iostream>
#include "rune_minigame.hpp"
#include "utilities.hpp"
#include "game_state_manager.hpp"

/// -----------------------------------------------------------------------------
///                               DEBUG FLAG
/// -----------------------------------------------------------------------------
static const bool debug = false;

/// -----------------------------------------------------------------------------
///                              Definitions
/// -----------------------------------------------------------------------------
#define RUNE_COUNT 5              ///< Total number of runes in the minigame
#define RUNE_SIZE 40.0f           ///< Width and height of each rune in world units
#define RUNE_MIN_DISTANCE 50.0f   ///< Minimum distance between any two runes
#define LINE_THICKNESS 4.0f       ///< Thickness of the connecting line in world units
#define RUNE_RANGE 400         ///< Range for random rune placement (from -RUNE_RANGE/2 to +RUNE_RANGE/2)

/// -----------------------------------------------------------------------------
///                          Global variables
/// -----------------------------------------------------------------------------
RuneMinigame::Rune* runes;                   ///< Dynamic array of runes
bool holdRune = false;                        ///< Whether the player is currently holding (dragging)
bool complete = false;                        ///< Whether the minigame has been completed
AEVec2 mousePos, runeScreen;                 ///< Current mouse position and half-window dimensions
AEGfxVertexList* runeMesh = nullptr;          ///< Rune sprite mesh (red, untouched)
AEGfxVertexList* runeTouchedMesh = nullptr;   ///< Rune sprite mesh (green, touched)
AEGfxVertexList* screenMesh = nullptr;        ///< Background sprite mesh
AEGfxVertexList* lineMesh = nullptr;          ///< Line segment mesh (unit-length, stretched via transform)

namespace 
{
	// -------------------------------------------------------------------------
	//                         Helper function declarations
	// -------------------------------------------------------------------------

	/**
	* @brief Checks if the mouse is currently hovering over a given rune.
	* @details Compares the mouse position with the rune's position and size to determine if it's being hovered.
	* @param mousePos Current mouse position in world coordinates
	* @param rune The rune to check for hover
	* 
	* @return true if the mouse is hovering over the rune, false otherwise
	*/
	bool isMouseOverRune(const AEVec2& mousePos, const RuneMinigame::Rune& rune);

	/**
	 * @brief Draws a line segment between two world positions.
	 * @details Builds a scale-rotate-translate matrix to stretch the unit
	 *          line mesh from point A to point B with the defined thickness.
	 * @param[in] from  Start position in world coordinates.
	 * @param[in] to    End position in world coordinates.
	 */
	void drawLine(const AEVec2& from, const AEVec2& to);
}

namespace RuneMinigame
{
	void Load() 
	{
		// No textures to load for this minigame, but we could initialize shared resources here if needed.
	}

	void Init() 
	{
		// Get windows size
		AEVec2 windowSize;
		windowSize.x = static_cast<f32>(AEGfxGetWindowWidth());
		windowSize.y = static_cast<f32>(AEGfxGetWindowHeight());
		
		// Store half-window dimensions for coordinate conversion and centering
		runeScreen = { static_cast<f32>(windowSize.x) / 2.f, static_cast<f32>(windowSize.y) / 2.f };

		// Initialize runes with minimum distance apart
		runes = new Rune[RUNE_COUNT];
		for (int i = 0; i < RUNE_COUNT; i++)
		{
			bool validPosition = false;
			while (!validPosition)
			{
				runes[i].position.x = static_cast<f32>(rand() % RUNE_RANGE - RUNE_RANGE / 2);
				runes[i].position.y = static_cast<f32>(rand() % RUNE_RANGE - RUNE_RANGE / 2);

				// Check distance against all previously placed runes
				validPosition = true;
				for (int j = 0; j < i; j++)
				{
					float dx = runes[i].position.x - runes[j].position.x;
					float dy = runes[i].position.y - runes[j].position.y;
					float distSq = dx * dx + dy * dy;

					if (distSq < RUNE_MIN_DISTANCE * RUNE_MIN_DISTANCE)
					{
						validPosition = false;
						break;
					}
				}
			}
			runes[i].touched = false;
		}

		runeMesh = BasicUtilities::createSquareMesh(1.f, 1.f, 0xFF0000FF);        // Red (untouched)
		runeTouchedMesh = BasicUtilities::createSquareMesh(1.f, 1.f, 0xFF00FF00);  // Green (touched)
		screenMesh = BasicUtilities::createSquareMesh();
		lineMesh = BasicUtilities::createSquareMesh(1.f, 1.f, 0xFFEEEE00);        // Yellow line

		if (debug)
		{
			std::cout << "[DEBUG] RuneMinigame::Init() — Rune positions:\n";
			for (int i = 0; i < RUNE_COUNT; i++)
			{
				std::cout << "  Rune " << i << ": (" << runes[i].position.x << ", " << runes[i].position.y << ")\n";
			}
		}
	}

	void Update()
	{
		// Always update mouse position in world coordinates
		int posX, posY;
		AEInputGetCursorPosition(&posX, &posY);
		float worldX, worldY;
		BasicUtilities::screenCoordsToWorldCoords(posX, posY, worldX, worldY);
		mousePos.x = worldX;
		mousePos.y = worldY;

		// Track which rune is the next one to touch
		int nextRuneIndex = 0;
		for (int i = 0; i < RUNE_COUNT; i++)
		{
			if (!runes[i].touched)
			{
				nextRuneIndex = i;
				break;
			}
		}

		// Check for mouse input to pick up and move runes
		if (holdRune)
		{
			// Only check the next rune in sequence
			if (nextRuneIndex < RUNE_COUNT && isMouseOverRune(mousePos, runes[nextRuneIndex]))
			{
				runes[nextRuneIndex].touched = true;

				if (debug)
				{
					std::cout << "[DEBUG] Rune " << nextRuneIndex << " touched in sequence.\n";
				}

				// Check if the last rune was just touched
				if (runes[RUNE_COUNT - 1].touched)
				{
					complete = true;

					if (debug)
					{
						std::cout << "[DEBUG] Minigame complete!\n";
					}
				}
			}

			// Reset if mouse hovers over any rune that is NOT the next in sequence
			for (int i = 0; i < RUNE_COUNT; i++)
			{
				if (i != nextRuneIndex && !runes[i].touched && isMouseOverRune(mousePos, runes[i]))
				{
					if (debug)
					{
						std::cout << "[DEBUG] Wrong rune " << i << " hovered (expected " << nextRuneIndex << ") — resetting.\n";
					}

					// Wrong rune touched — reset all
					for (int j = 0; j < RUNE_COUNT; j++)
					{
						runes[j].touched = false;
					}
					holdRune = false;
					break;
				}
			}

			// Check for mouse release to drop the rune
			if (!AEInputCheckCurr(AEVK_LBUTTON))
			{
				holdRune = false;
			}
		}

		if (AEInputCheckCurr(AEVK_LBUTTON))
		{
			if (!holdRune)
			{
				// Can only start by touching the first untouched rune (runes[0])
				if (isMouseOverRune(mousePos, runes[0]))
				{
					holdRune = true;
					runes[0].touched = true; // Mark the first rune as touched

					if (debug)
					{
						std::cout << "[DEBUG] Started holding from rune 0.\n";
					}
				}
			}
		}
		else
		{
			if (holdRune)
			{
				holdRune = false; // Drop the rune when the mouse button is released
			}

			if (!complete)
			{
				// Reset touched state for all runes when the mouse button is released
				for (int i = 0; i < RUNE_COUNT; i++)
				{
					runes[i].touched = false;
				}
			}
		}

		// Print debug info about held rune and mouse position
		if (debug)
		{
			std::cout << "Holding rune at mouse position: (" << mousePos.x << ", " << mousePos.y << ")\n";
			std::cout << "Rune Positions:\n";
			for (int i = 0; i < RUNE_COUNT; i++)
			{
				std::cout << "Rune " << i << ": (" << runes[i].position.x << ", " << runes[i].position.y << ") "
						  << (runes[i].touched ? "[TOUCHED]" : "") << "\n";
			}
			std::cout << "Current mouse position: (" << mousePos.x << ", " << mousePos.y << ")\n";
		}

		if (AEInputCheckTriggered(AEVK_ESCAPE))
		{
			nextState = GS_MAINMENU; // Example: return to main menu on Escape
		}
	}

	void Draw()
	{
		// Set render mode
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);

		AEMtx33 scaleMtx = { 0 };
		AEMtx33 translate = { 0 };
		AEMtx33 transform = { 0 };

		AEMtx33Scale(&scaleMtx, runeScreen.x, runeScreen.y);
		AEMtx33Trans(&translate, 0.f, 0.f);
		AEMtx33Concat(&transform, &translate, &scaleMtx);
		AEGfxSetTransform(transform.m);

		// Draw background screen
		AEGfxMeshDraw(screenMesh, AE_GFX_MDM_TRIANGLES);

		// ---- Draw connecting lines between touched runes ----
		AEGfxSetColorToMultiply(1.0f, 1.0f, 0.0f, 1.0f); // Yellow lines
		int lastTouchedIndex = -1;
		for (int i = 0; i < RUNE_COUNT; i++)
		{
			if (runes[i].touched)
			{
				if (lastTouchedIndex >= 0)
				{
					// Draw line from previous touched rune to this one
					drawLine(runes[lastTouchedIndex].position, runes[i].position);
				}
				lastTouchedIndex = i;
			}
		}

		// Draw trailing line from last touched rune to mouse (only while holding, not when complete)
		if (holdRune && lastTouchedIndex >= 0 && !complete)
		{
			drawLine(runes[lastTouchedIndex].position, mousePos);
		}

		// ---- Draw runes ----
		for (int i = 0; i < RUNE_COUNT; i++)
		{
			AEMtx33Scale(&scaleMtx, RUNE_SIZE, RUNE_SIZE);
			AEMtx33Trans(&translate, runes[i].position.x, runes[i].position.y);
			AEMtx33Concat(&transform, &translate, &scaleMtx);
			AEGfxSetTransform(transform.m);

			// Draw green mesh if touched, red mesh if untouched
			AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxMeshDraw(runes[i].touched ? runeTouchedMesh : runeMesh, AE_GFX_MDM_TRIANGLES);
		}
	}

	void Free()
	{
		// Clean up runes
		if (runes)
		{
			delete[] runes;
			runes = nullptr;
		}

		// Free meshes
		if (runeMesh)
		{
			AEGfxMeshFree(runeMesh);
			runeMesh = nullptr;
		}

		if (runeTouchedMesh)
		{
			AEGfxMeshFree(runeTouchedMesh);
			runeTouchedMesh = nullptr;
		}

		if (screenMesh)
		{
			AEGfxMeshFree(screenMesh);
			screenMesh = nullptr;
		}

		if (lineMesh)
		{
			AEGfxMeshFree(lineMesh);
			lineMesh = nullptr;
		}

		// Reset state
		holdRune = false;
		complete = false;

		if (debug)
		{
			std::cout << "[DEBUG] RuneMinigame::Free() — All resources released.\n";
		}
	}

	void Unload()
	{
		// No textures to unload, but we could release shared resources here if needed.
	}
}

namespace
{
	bool isMouseOverRune(const AEVec2& checkPos, const RuneMinigame::Rune& rune)
	{
		if (checkPos.x >= rune.position.x - RUNE_SIZE/2.f && checkPos.x <= rune.position.x + RUNE_SIZE / 2.f &&
			checkPos.y >= rune.position.y - RUNE_SIZE / 2.f && checkPos.y <= rune.position.y + RUNE_SIZE / 2.f)
		{
			return true; // Mouse is over the rune
		}
		return false; // Placeholder return value
	}

	void drawLine(const AEVec2& from, const AEVec2& to)
	{
		if (!lineMesh) return;

		float dx = to.x - from.x;
		float dy = to.y - from.y;
		float length = sqrtf(dx * dx + dy * dy);

		if (length < 0.001f) return; // Avoid degenerate transform

		// Angle from 'from' to 'to'
		float angle = atan2f(dy, dx);

		// Midpoint between the two positions
		float midX = (from.x + to.x) * 0.5f;
		float midY = (from.y + to.y) * 0.5f;

		// Build transform: scale unit quad to line dimensions, rotate, translate to midpoint
		AEMtx33 scaleMtx = { 0 }, rotateMtx = { 0 }, translateMtx = { 0 };
		AEMtx33 temp = { 0 }, transform = { 0 };

		AEMtx33Scale(&scaleMtx, length, LINE_THICKNESS);
		AEMtx33Rot(&rotateMtx, angle);
		AEMtx33Trans(&translateMtx, midX, midY);

		// transform = translate * (rotate * scale)
		AEMtx33Concat(&temp, &rotateMtx, &scaleMtx);
		AEMtx33Concat(&transform, &translateMtx, &temp);
		AEGfxSetTransform(transform.m);

		AEGfxSetColorToMultiply(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
		AEGfxMeshDraw(lineMesh, AE_GFX_MDM_TRIANGLES);
	}
}