/*===========================================================================
@filename   sprite.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Implements sprite animation loading, updating, and drawing using
            Aseprite JSON exports and nlohmann/json.
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <fstream>
#include "AEEngine.h"
#include "AEFrameRateController.h"
#include "nlohmann/json.hpp"
#include "sprite.hpp"
#include "utilities.hpp"

using json = nlohmann::json;

namespace Sprite
{
	//============================================================================
	// SpriteData::Free
	//============================================================================
	void SpriteData::Free()
	{
		for (unsigned int i = 0; i < MAX_ANIM_SLOTS; ++i)
		{
			if (textures[i] != nullptr)
			{
				AEGfxTextureUnload(textures[i]);
				textures[i] = nullptr;
			}
		}
		count = 0;
	}

	//============================================================================
	// LoadJsonFile
	//============================================================================
	json LoadJsonFile(const char* filepath)
	{
		std::ifstream file(filepath);
		if (!file.is_open())
		{
			std::cerr << "[Sprite] ERROR: Cannot open JSON file: " << filepath << '\n';
			return json{};
		}

		json data;
		file >> data;
		return data;
	}

	//============================================================================
	// LoadMeta
	//============================================================================
	JMeta LoadMeta(const json& j)
	{
		JMeta meta;

		if (j.empty() || !j.contains("frames") || !j.contains("meta"))
		{
			std::cerr << "[Sprite] ERROR: Invalid or empty JSON sprite data\n";
			return meta;
		}

		// ── Parse frames ──────────────────────────────────────────────────────
		// Aseprite exports "frames" as either an array or an object
		const json& framesNode = j["frames"];

		auto parseFrame = [](const json& f) -> JFrame
		{
			JFrame frame;
			if (f.contains("filename"))
				frame.filename = f["filename"].get<std::string>();
			if (f.contains("frame"))
			{
				frame.x      = f["frame"].value("x", 0.f);
				frame.y      = f["frame"].value("y", 0.f);
				frame.width  = f["frame"].value("w", 0.f);
				frame.height = f["frame"].value("h", 0.f);
			}
			frame.duration = f.value("duration", 200.f);
			return frame;
		};

		if (framesNode.is_array())
		{
			for (const auto& f : framesNode)
				meta.frames.push_back(parseFrame(f));
		}
		else if (framesNode.is_object())
		{
			for (auto it = framesNode.begin(); it != framesNode.end(); ++it)
				meta.frames.push_back(parseFrame(it.value()));
		}

		// ── Parse meta ────────────────────────────────────────────────────────
		const json& metaNode = j["meta"];

		if (metaNode.contains("size"))
		{
			meta.sheetWidth  = metaNode["size"].value("w", 0);
			meta.sheetHeight = metaNode["size"].value("h", 0);
		}

		if (metaNode.contains("frameTags"))
		{
			for (const auto& tag : metaNode["frameTags"])
			{
				JAnimationTag t;
				t.name      = tag.value("name", "");
				t.from      = tag.value("from", 0);
				t.to        = tag.value("to",   0);
				t.direction = tag.value("direction", "forward");
				meta.frameTags.push_back(t);
			}
		}

		return meta;
	}

	//============================================================================
	// LoadEntry
	//============================================================================
	bool LoadEntry(SpriteData& data, unsigned int slot,
	               const char* texPath, const char* jsonPath)
	{
		if (slot >= MAX_ANIM_SLOTS)
		{
			std::cerr << "[Sprite] ERROR: Slot " << slot << " out of range\n";
			return false;
		}

		// Load texture
		data.textures[slot] = BasicUtilities::loadTexture(texPath);
		if (!data.textures[slot])
		{
			std::cerr << "[Sprite] ERROR: Failed to load texture: " << texPath << '\n';
			return false;
		}

		// Load and parse JSON
		json j = LoadJsonFile(jsonPath);
		if (j.empty())
		{
			std::cerr << "[Sprite] ERROR: Failed to load JSON: " << jsonPath << '\n';
			return false;
		}

		data.metas[slot] = LoadMeta(j);
		if (data.metas[slot].frames.empty())
		{
			std::cerr << "[Sprite] ERROR: No frames parsed from: " << jsonPath << '\n';
			return false;
		}

		data.count++;
		return true;
	}

	//============================================================================
	// UpdateAnimation
	//============================================================================
	void UpdateAnimation(AnimationState& state, const JMeta& meta,
	                     unsigned int tagIndex)
	{
		if (meta.frames.empty() || tagIndex >= meta.frameTags.size())
			return;

		const JAnimationTag& tag = meta.frameTags[tagIndex];

		// Accumulate time
		state.timer += static_cast<float>(AEFrameRateControllerGetFrameTime());

		// Clamp currentIndex to the tag's range on tag switch
		if (state.currentIndex < static_cast<unsigned int>(tag.from) ||
		    state.currentIndex > static_cast<unsigned int>(tag.to))
		{
			state.currentIndex = static_cast<unsigned int>(tag.from);

			const JFrame& f = meta.frames[state.currentIndex];
			state.uvOffsetX = f.x / static_cast<float>(meta.sheetWidth);
			state.uvOffsetY = f.y / static_cast<float>(meta.sheetHeight);
		}

		// Frame duration from JSON (in seconds)
		float frameDuration = meta.frames[state.currentIndex].duration / 1000.f;

		// Advance frame when timer exceeds duration
		if (state.timer >= frameDuration)
		{
			state.timer = 0.f;
			state.currentIndex++;

			// Loop back to start of tag range
			if (state.currentIndex > static_cast<unsigned int>(tag.to))
				state.currentIndex = static_cast<unsigned int>(tag.from);

			// Compute UV offset from frame position in the sheet
			const JFrame& f = meta.frames[state.currentIndex];
			state.uvOffsetX = f.x / static_cast<float>(meta.sheetWidth);
			state.uvOffsetY = f.y / static_cast<float>(meta.sheetHeight);
		}
	}

	//============================================================================
	// DrawAnimation
	//============================================================================
	void DrawAnimation(AEGfxVertexList* mesh, AEGfxTexture* texture,
	                   const AnimationState& state,
	                   AEVec2 position, AEVec2 scale)
	{
		if (!mesh || !texture)
		{
			std::cerr << "[Sprite] WARN: DrawAnimation called with null "
			          << (!mesh ? "mesh" : "texture") << '\n';
			return;
		}

		AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
		AEGfxSetColorToMultiply(1.f, 1.f, 1.f, 1.f);
		AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxSetTransparency(1.f);

		// Apply texture with current UV offset (selects the active frame)
		AEGfxTextureSet(texture, state.uvOffsetX, state.uvOffsetY);

		// Build transform: scale first, then translate
		AEMtx33 scaleMtx  = { 0 };
		AEMtx33 translate = { 0 };
		AEMtx33 transform = { 0 };

		AEMtx33Scale(&scaleMtx, scale.x, scale.y);
		AEMtx33Trans(&translate, position.x, position.y);
		AEMtx33Concat(&transform, &translate, &scaleMtx);
		AEGfxSetTransform(transform.m);

		AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
	}

} // namespace Sprite
