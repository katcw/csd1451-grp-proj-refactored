/*===========================================================================
@filename   utilities.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Contains helper function definitions
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <cstdio>
#include <cstring>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "utilities.hpp"
#include "entity.hpp"

// Alias for nlohmann::json
using json = nlohmann::json;

// DEBUG FLAG
static const bool debug = false;

namespace BasicUtilities
{
    AEGfxTexture* loadTexture(const char* filepath)
    {
        AEGfxTexture* returnedTexture = AEGfxTextureLoad(filepath);

        if (returnedTexture == nullptr)
        {
            std::cout << "ERROR! COULD NOT LOCATE " << filepath << '\n';
        }
        else
        {
            std::cout << "SUCCESS! " << filepath << " LOADED" << '\n';
        }

        return returnedTexture;
    }

    AEGfxVertexList* createSquareMesh(float uvHeight, float uvWidth, unsigned int color)
    {
        AEGfxMeshStart();
        AEGfxTriAdd(
            -0.5f, -0.5f, color, 0.0f, uvHeight,
            0.5f, -0.5f, color, uvWidth, uvHeight,
            -0.5f, 0.5f, color, 0.0f, 0.0f);
        AEGfxTriAdd(
            0.5f, -0.5f, color, uvWidth, uvHeight,
            0.5f, 0.5f, color, uvWidth, 0.0f,
            -0.5f, 0.5f, color, 0.0f, 0.0f);

        return AEGfxMeshEnd();
    }

    void screenCoordsToWorldCoords(s32 screenX, s32 screenY, float& worldX, float& worldY)
    {
        worldX = screenX - (AEGfxGetWindowWidth() / 2.0f);
        worldY = (AEGfxGetWindowHeight() / 2.0f) - screenY;
    }

    void drawText(s8 fontId, const char* text, float worldX, float worldY,
                  float scale, float r, float g, float b, float alpha)
    {
        // Convert world coordinates to normalised
        float normalX = worldX / (AEGfxGetWindowWidth()  / 2.0f);
        float normalY = worldY / (AEGfxGetWindowHeight() / 2.0f);

        // Measure the rendered text 
        // [!] Necessary to shift text to centre
        float textWidth = 0.f, textHeight = 0.f;
        AEGfxGetPrintSize(fontId, text, scale, &textWidth, &textHeight);

        // [!] AEGfxPrint anchors at the bottom-left; shift left and down by half extents to centre
        normalX -= textWidth  / 2.0f;
        normalY -= textHeight / 2.0f;

        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxPrint(fontId, text, normalX, normalY, scale, r, g, b, alpha);
    }

    float drawTextWrapped(s8 fontId, const char* text, float cx, float topY,
                          float scale, float r, float g, float b, float maxW)
    {
        float dummy, lineH;
        AEGfxGetPrintSize(fontId, "A", scale, &dummy, &lineH);
        const float lineGap = lineH * (AEGfxGetWindowHeight() / 2.0f) * 1.4f;
        const float normalMaxW = maxW / (AEGfxGetWindowWidth() / 2.0f);

        char currentLine[256] = {};
        char testLine[256];
        float y = topY;

        const char* p = text;
        while (*p)
        {
            while (*p == ' ') ++p;                          // skip spaces
            if (!*p) break;

            const char* wordEnd = p;
            while (*wordEnd && *wordEnd != ' ') ++wordEnd;  // find word boundary

            if (currentLine[0])
                sprintf_s(testLine, sizeof(testLine), "%s %.*s",
                          currentLine, static_cast<int>(wordEnd - p), p);
            else
                sprintf_s(testLine, sizeof(testLine), "%.*s",
                          static_cast<int>(wordEnd - p), p);

            float w, h;
            AEGfxGetPrintSize(fontId, testLine, scale, &w, &h);

            if (w > normalMaxW && currentLine[0])  // overflow: flush current line, restart
            {
                drawText(fontId, currentLine, cx, y, scale, r, g, b, 1.0f);
                y -= lineGap;
                sprintf_s(currentLine, sizeof(currentLine), "%.*s",
                          static_cast<int>(wordEnd - p), p);
            }
            else
            {
                strcpy_s(currentLine, sizeof(currentLine), testLine);
            }
            p = wordEnd;
        }
        if (currentLine[0])
        {
            drawText(fontId, currentLine, cx, y, scale, r, g, b, 1.0f);
            y -= lineGap;
        }
        return y;
    }

    void drawUICard(AEGfxVertexList* mesh, AEGfxTexture* texture,
                    float worldX, float worldY,
                    float width, float height, float alpha)
    {
        AEMtx33 translate = { 0 }, scale = { 0 }, transform = { 0 };
        AEMtx33Trans(&translate, worldX, worldY);
        AEMtx33Scale(&scale, width, height);
        AEMtx33Concat(&transform, &translate, &scale);

        AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
        AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
        AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(alpha);
        AEGfxTextureSet(texture, 0, 0);
        AEGfxSetTransform(transform.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }

   void drawTooltip(AEGfxVertexList* mesh,
        AEGfxTexture* iconTex,
        float ir, float ig, float ib,
        const char* label,
        float cx, float cy,
        s8 fontId,
        float scale, float bgAlpha,
        float t)
    {
        if (t <= 0.f) return;

        constexpr float ICON_SIZE = 28.f;
        constexpr float ICON_GAP = 6.f;
        constexpr float SIDE_PAD = 10.f;
        constexpr float BAR_H = 40.f;

        // Measure label text in world units
        float normW = 0.f, normH = 0.f;
        AEGfxGetPrintSize(fontId, label, scale, &normW, &normH);
        const float textWorldW = normW * (AEGfxGetWindowWidth() / 2.f);
        const float barW = SIDE_PAD + ICON_SIZE + ICON_GAP + textWorldW + SIDE_PAD;


        // Dark background
        AEMtx33 sclMtx{}, trsMtx{}, mtx{};
        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        AEGfxSetColorToMultiply(0.1f, 0.1f, 0.1f, 1.f);
        AEGfxSetTransparency(bgAlpha * t);
        AEMtx33Scale(&sclMtx, barW * t, BAR_H * t);
        AEMtx33Trans(&trsMtx, cx, cy);
        AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);

        // Only draw icon and text once box is mostly open
        if (t < 0.5f) return;
        const float contentAlpha = (t - 0.5f) * 2.0f; // fade in during second half

        // Icon
        const float iconCx = cx - barW * 0.5f + SIDE_PAD + ICON_SIZE * 0.5f;
        if (iconTex)
        {
            drawUICard(mesh, iconTex, iconCx, cy, ICON_SIZE, ICON_SIZE, contentAlpha);
            // Restore colour mode for subsequent draws
            AEGfxSetRenderMode(AE_GFX_RM_COLOR);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        }
        else
        {
            AEGfxSetColorToMultiply(ir, ig, ib, contentAlpha);
            AEGfxSetTransparency(contentAlpha);
            AEMtx33Scale(&sclMtx, ICON_SIZE, ICON_SIZE);
            AEMtx33Trans(&trsMtx, iconCx, cy);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
        }

        // Label text
        const float textCx = iconCx + ICON_SIZE * 0.5f + ICON_GAP + textWorldW * 0.5f;
        drawText(fontId, label, textCx, cy, scale,
            contentAlpha, contentAlpha, contentAlpha, contentAlpha);
    }

    // Draws UI element
    void Draw_UI_Element(AEGfxVertexList* mesh, AEGfxTexture* texture, AEMtx33& transform)
    {
        AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
        AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.0f);
        AEGfxTextureSet(texture, 0, 0);
        AEGfxSetTransform(transform.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }

    void drawFillBar(AEGfxVertexList* mesh,
        float cx, float cy, float w, float h,
        float fill,
        float fillR, float fillG, float fillB)
    {
        AEMtx33 scl{}, trs{}, mtx{};
        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);

        // Background
        AEGfxSetColorToMultiply(0.2f, 0.2f, 0.2f, 1.f);
        AEMtx33Scale(&scl, w, h);
        AEMtx33Trans(&trs, cx, cy);
        AEMtx33Concat(&mtx, &trs, &scl);
        AEGfxSetTransform(mtx.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);

        // Fill
        if (fill > 0.f)
        {
            AEGfxSetColorToMultiply(fillR, fillG, fillB, 1.f);
            float fillW = w * fill;
            AEMtx33Scale(&scl, fillW, h);
            AEMtx33Trans(&trs, cx - w * 0.5f + fillW * 0.5f, cy);
            AEMtx33Concat(&mtx, &trs, &scl);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
        }
    }



    /**
     * @brief Loads and parses a JSON file from disk.
     *
     * Opens the specified file, parses its contents as JSON,
     * and returns the parsed object. Returns an empty JSON
     * object if the file cannot be opened.
     *
     * @param filename Path to JSON file (relative or absolute)
     * @return Parsed JSON object, or empty object on failure
     *
     * @warning Prints error message to stderr if file cannot be opened
     */
    json LoadJsonFile(const char* filename)
    {
        std::ifstream file(filename);

        if (!file.is_open())
        {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return json{};
        }

        json data;
        file >> data;

        return data;
    }

    namespace Sprite
    {
        /**
             * @brief Loads all sprite metadata from Aseprite JSON export.
             *
             * Parses the JSON file exported from Aseprite and extracts:
             * - Frame data (position, size, duration)
             * - Animation tags (name, frame range, direction)
             * - Spritesheet dimensions (width, height)
             * - Image filename
             *
             * @param spriteData Parsed JSON object from Aseprite export
             * @return JMeta struct containing all sprite metadata
             *
             * @note Expects JSON format from Aseprite "Export Sprite Sheet" with:
             *       - "frames" array
             *       - "meta.frameTags" array
             *       - "meta.size" object with "w" and "h"
             */
        JMeta LoadMeta(const json& spriteData)
        {
            JMeta meta;

            // ----------------------------------------------------------------
            // Validate JSON structure
            // ----------------------------------------------------------------
            if (spriteData.empty() || !spriteData.contains("frames") || !spriteData.contains("meta"))
            {
                std::cerr << "Error: Invalid or empty JSON data\n";
                return meta;
            }

            const auto& framesData = spriteData["frames"];
            const auto& metaObj = spriteData["meta"];

            // ----------------------------------------------------------------
            // Load frame data from "frames" (supports Array and Hash formats)
            // ----------------------------------------------------------------
            if (framesData.is_array())
            {
                for (const auto& frame : framesData)
                {
                    JFrame f;
                    if (frame.contains("filename")) f.filename = frame["filename"];
                    if (frame.contains("frame"))
                    {
                        const auto& r = frame["frame"];
                        if (r.contains("x")) f.x = r["x"];
                        if (r.contains("y")) f.y = r["y"];
                        if (r.contains("w")) f.width = r["w"];
                        if (r.contains("h")) f.height = r["h"];
                    }
                    if (frame.contains("duration")) f.duration = frame["duration"];
                    meta.frames.push_back(f);
                }
            }
            else if (framesData.is_object())
            {
                for (auto it = framesData.begin(); it != framesData.end(); ++it)
                {
                    JFrame f;
                    f.filename = it.key();
                    const auto& frame = it.value();
                    if (frame.contains("frame"))
                    {
                        const auto& r = frame["frame"];
                        if (r.contains("x")) f.x = r["x"];
                        if (r.contains("y")) f.y = r["y"];
                        if (r.contains("w")) f.width = r["w"];
                        if (r.contains("h")) f.height = r["h"];
                    }
                    if (frame.contains("duration")) f.duration = frame["duration"];
                    meta.frames.push_back(f);
                }
            }

            // ----------------------------------------------------------------
            // Load animation tags from "meta.frameTags" array
            // ----------------------------------------------------------------
            if (metaObj.contains("frameTags"))
            {
                for (const auto& tag : metaObj["frameTags"])
                {
                    JAnimationTag t;
                    if (tag.contains("name"))      t.name = tag["name"];
                    if (tag.contains("from"))      t.from = tag["from"];
                    if (tag.contains("to"))        t.to = tag["to"];
                    if (tag.contains("direction")) t.direction = tag["direction"];
                    meta.frameTags.push_back(t);
                }
            }

            // ----------------------------------------------------------------
            // Load spritesheet dimensions from "meta.size"
            // ----------------------------------------------------------------
            if (metaObj.contains("size"))
            {
                const auto& size = metaObj["size"];
                if (size.contains("w")) meta.sheetWidth = size["w"];
                if (size.contains("h")) meta.sheetHeight = size["h"];
            }

            // ----------------------------------------------------------------
            // Load image filename if available
            // ----------------------------------------------------------------
            if (metaObj.contains("image"))
            {
                meta.imageFile = metaObj["image"];
            }

            return meta;
        }

        // Rendering
        void UpdateAnimation(AnimationState& state, u32 row, float frameDuration, u32 rows, u32 cols)
        {
            float uvWidth = 1.0f / cols;
            float uvHeight = 1.0f / rows;

            // ----------------------------------------------------------------
            // Accumulate frame time
            // ----------------------------------------------------------------
            state.timer += static_cast<float>(AEFrameRateControllerGetFrameTime());

            // ----------------------------------------------------------------
            // Advance frame when timer exceeds duration
            // ----------------------------------------------------------------
            if (state.timer >= frameDuration)
            {
                state.timer = 0.0f;
                state.currentIndex = (state.currentIndex + 1) % cols;
                state.uvOffsetX = uvWidth * state.currentIndex;
                state.uvOffsetY = uvHeight * row;
            }
        }

        void UpdateAnimation(AnimationState& state,
            JMeta meta,
            u32 tagIndex,
            float customDuration)
        {
            // ----------------------------------------------------------------
            // Validate input data
            // ----------------------------------------------------------------
            if (meta.frames.empty() || tagIndex >= meta.frameTags.size())
            {
                return;
            }

            const JAnimationTag& tag = meta.frameTags[tagIndex];

            // ----------------------------------------------------------------
            // Accumulate frame time
            // ----------------------------------------------------------------
            state.timer += static_cast<float>(AEFrameRateControllerGetFrameTime());

            // ----------------------------------------------------------------
            // Clamp currentIndex to tag range and initialize UV
            // ----------------------------------------------------------------
            if (state.currentIndex < static_cast<u32>(tag.from) ||
                state.currentIndex > static_cast<u32>(tag.to))
            {
                state.currentIndex = static_cast<u32>(tag.from);

                const JFrame& f = meta.frames[state.currentIndex];
                state.uvOffsetX = f.x / static_cast<float>(meta.sheetWidth);
                state.uvOffsetY = f.y / static_cast<float>(meta.sheetHeight);
            }

            // ----------------------------------------------------------------
            // Calculate frame duration (custom or from JSON)
            // ----------------------------------------------------------------
            float frameDuration = customDuration > 0.0f
                ? customDuration
                : meta.frames[state.currentIndex].duration / 1000.0f;

            // ----------------------------------------------------------------
            // Advance frame when timer exceeds duration
            // ----------------------------------------------------------------
            if (state.timer >= frameDuration)
            {
                state.timer = 0.0f;
                state.currentIndex++;

                // Loop back to start of tag range
                if (state.currentIndex > static_cast<u32>(tag.to))
                {
                    state.currentIndex = static_cast<u32>(tag.from);
                }

                // Calculate UV using frame position from JSON
                const JFrame& f = meta.frames[state.currentIndex];
                state.uvOffsetX = f.x / static_cast<float>(meta.sheetWidth);
                state.uvOffsetY = f.y / static_cast<float>(meta.sheetHeight);

                if (debug)
                {
                    std::cout << "JSON Anim: tag=" << tagIndex
                        << " frame=" << state.currentIndex
                        << " pos=(" << f.x << "," << f.y << ")"
                        << " uvX=" << state.uvOffsetX
                        << " uvY=" << state.uvOffsetY << "\n";
                }
            }
        }

        void DrawAnimation(AEGfxVertexList* mesh, AEGfxTexture* texture,
            const AnimationState& state,
            AEVec2 position, AEVec2 scale)
        {
            // ----------------------------------------------------------------
            // Validate pointers
            // ----------------------------------------------------------------
            if (!texture || !mesh)
            {
                if (debug)
                {
                    std::cerr << "[WARN] Draw_Animation: null "
                        << (!mesh ? "mesh" : "texture") << "\n";
                }
                return;
            }

            // ----------------------------------------------------------------
            // Set up render state
            // ----------------------------------------------------------------
            AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
            //AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
            //AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetTransparency(1.0f);

            // ----------------------------------------------------------------
            // Apply texture with UV offset
            // ----------------------------------------------------------------
            AEGfxTextureSet(texture, state.uvOffsetX, state.uvOffsetY);

            // ----------------------------------------------------------------
            // Build transformation matrix (scale then translate)
            // ----------------------------------------------------------------
            AEMtx33 scaleMtx = { 0 };
            AEMtx33 translate = { 0 };
            AEMtx33 transform = { 0 };

            AEMtx33Scale(&scaleMtx, scale.x, scale.y);
            AEMtx33Trans(&translate, position.x, position.y);
            AEMtx33Concat(&transform, &translate, &scaleMtx);
            AEGfxSetTransform(transform.m);

            // ----------------------------------------------------------------
            // Draw mesh
            // ----------------------------------------------------------------
            AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);

            // ----------------------------------------------------------------
            // Debug output
            // ----------------------------------------------------------------
            if (debug)
            {
                std::cout << "Draw_Animation: pos(" << position.x << ", " << position.y << ") "
                    << "scale(" << scale.x << ", " << scale.y << ") "
                    << "uv(" << state.uvOffsetX << ", " << state.uvOffsetY << ")\n";
                AEGfxMeshDraw(mesh, AE_GFX_MDM_LINES_STRIP);
            }
        }

        void DrawSprite(AEGfxVertexList* mesh, AEGfxTexture* texture,
            AEVec2 position, AEVec2 scale)
        {
            // ----------------------------------------------------------------
            // Set up render state
            // ----------------------------------------------------------------
            AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
            AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
            AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
            AEGfxSetBlendMode(AE_GFX_BM_BLEND);
            AEGfxSetTransparency(1.0f);

            // ----------------------------------------------------------------
            // Apply texture with no UV offset
            // ----------------------------------------------------------------
            AEGfxTextureSet(texture, 0.0f, 0.0f);

            // ----------------------------------------------------------------
            // Build transformation matrix (scale then translate)
            // ----------------------------------------------------------------
            AEMtx33 scaleMtx = { 0 };
            AEMtx33 translate = { 0 };
            AEMtx33 transform = { 0 };

            AEMtx33Scale(&scaleMtx, scale.x, scale.y);
            AEMtx33Trans(&translate, position.x, position.y);
            AEMtx33Concat(&transform, &translate, &scaleMtx);
            AEGfxSetTransform(transform.m);

            // ----------------------------------------------------------------
            // Draw mesh
            // ----------------------------------------------------------------
            AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
        }

        bool LoadEntry(SpriteData& list, u32 index, const char* texturePath, const char* jsonPath)
        {
            if (index >= Sprite::MAX_SPRITE_ANIMS)
            {
                if (debug) std::cerr << "[ERROR] LoadSpriteEntry: index out of range: " << index << "\n";
                return false;
            }

            // Load texture
            list.textureList[index] = loadTexture(texturePath);
            if (debug)
            {
                std::cout << "[DEBUG] Texture[" << index << "] "
                    << (list.textureList[index] ? "loaded" : "FAILED")
                    << " : " << texturePath << "\n";
            }

            if (!list.textureList[index])
            {
                std::cerr << "[ERROR] Failed to load texture: " << texturePath << "\n";
                return false;
            }

            // Load JSON
            json j = LoadJsonFile(jsonPath);
            if (j.empty())
            {
                std::cerr << "[ERROR] Failed to load JSON: " << jsonPath << "\n";
                return false;
            }

            // Parse meta
            list.jsonList[index] = LoadMeta(j);
            if (list.jsonList[index].frames.empty())
            {
                std::cerr << "[ERROR] Parsed meta frames empty for " << jsonPath << "\n";
                return false;
            }
            else if (debug)
            {
                std::cout << "[DEBUG] Parsed " << jsonPath << " frames: "
                    << list.jsonList[index].frames.size() << "\n";
            }
            list.count++;
            return true;
        }

        void LoadEntry4(const char* name, SpriteData& list)
        {
            char texturePath[256];
            char jsonPath[256];
            list.count = 0;
            for (u32 i = 0; i < 4; ++i)
            {
                const char* suffix = nullptr;
                switch (i)
                {
                case 0: suffix = "_Idle"; break;
                case 1: suffix = "_Walking"; break;
                case 2: suffix = "_Idle_Carry"; break;
                case 3: suffix = "_Walking_Carry"; break;
                default: suffix = ""; break;
                }
                snprintf(texturePath, sizeof(texturePath), "Assets/character-assets/%s%s.png", name, suffix);
                snprintf(jsonPath, sizeof(jsonPath), "Assets/Character-assets/%s%s.json", name, suffix);
                if (!LoadEntry(list, i, texturePath, jsonPath))
                {
                    std::cerr << "[ERROR] Failed to load sprite entry " << i << " for " << name << suffix << "\n";
                    // Ensure slot is explicitly null so Get_Texture_For_State can skip it
                    list.textureList[i] = nullptr;
                }
                // Always increment count so indices stay aligned:
                // slot 0=idle, 1=walk, 2=idle_carry, 3=walk_carry
                list.count++;
            }

            if (debug)
            {
                std::cout << "[DEBUG] LoadSpriteEntry4('" << name << "'): count=" << list.count << "\n";
                for (u32 i = 0; i < list.count; ++i)
                {
                    std::cout << "  [" << i << "] tex=" << (list.textureList[i] ? "OK" : "NULL")
                        << " frames=" << list.jsonList[i].frames.size() << "\n";
                }
            }
        }

        AEGfxTexture* GetTextureForState(Entity::MoveState state,
            bool isHolding,
            AEGfxTexture* animArray[])
        {
            // Indices: [0]=idle, [1]=walk, [2]=idleHold, [3]=walkHold
            AEGfxTexture* result = nullptr;

            switch (state)
            {
            case Entity::MoveState::DEFAULT:
            case Entity::MoveState::IDLE:
                if (isHolding && animArray[2] != nullptr)
                    result = animArray[2];
                else
                    result = animArray[0];
                break;

            case Entity::MoveState::WALKING:
            case Entity::MoveState::RUNNING:
                if (isHolding && animArray[3] != nullptr)
                    result = animArray[3];
                else
                    result = animArray[1];
                break;

            default:
                result = animArray[0];
                break;
            }

            // Final fallback: if the chosen slot is null, try idle, then walk
            if (!result) result = animArray[0];
            if (!result) result = animArray[1];

            return result;
        }
    }
}