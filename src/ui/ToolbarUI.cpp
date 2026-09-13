#include "ToolbarUI.h"
#include <imgui.h>
#include "../core/Globals.h"
#include "../world/World.h"
#include "../rendering/Renderer.h"
#include "../physics/Ragdoll.h"
#include <stdio.h>
#include <string.h>

namespace UI {
namespace Toolbar {

GLuint handTexture = 0;
GLuint closedHandTexture = 0;
GLuint hammerIcon = 0;
GLuint ak47Icon = 0;
GLuint dynamiteIcon = 0;
GLuint glockIcon = 0;
GLuint shotgunIcon = 0;
GLuint placerIcon = 0;

void drawCrosshair(float fbW, float fbH) {
    ImGui::SetNextWindowPos(ImVec2(fbW/2.0f-64, fbH/2.0f-64));
    ImGui::SetNextWindowSize(ImVec2(128, 128));
    ImGui::Begin("##xhair", nullptr,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoTitleBar|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoDecoration);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImVec2(fbW/2.0f, fbH/2.0f);
    
    bool isHoldingTerrain = (getHeldChunk() != nullptr || grabbedRagdollId != 0);
    if (isHoldingTerrain) {
        if (closedHandTexture != 0) {
            dl->AddImage((void*)(intptr_t)closedHandTexture, ImVec2(c.x - 24.0f, c.y - 24.0f), ImVec2(c.x + 24.0f, c.y + 24.0f));
        }
    } else if (isLookingAtGrabbable && (currentWeapon == -1 || currentWeapon == 5)) {
        if (handTexture != 0) {
            dl->AddImage((void*)(intptr_t)handTexture, ImVec2(c.x - 24.0f, c.y - 24.0f), ImVec2(c.x + 24.0f, c.y + 24.0f));
        }
    } else {
        dl->AddCircleFilled(c, 2.0f, IM_COL32(255,255,255,200));
        dl->AddCircle(c, 2.5f, IM_COL32(0,0,0,150));
    }
    ImGui::End();
}

void drawWeaponBar(float fbW, float fbH) {
    float slotW = 70.0f, slotH = 52.0f, slotGap = 6.0f;
    float totalW = slotW * 9 + slotGap * 8;
    float barX = fbW / 2.0f - totalW / 2.0f;
    float barY = fbH - slotH - 14.0f;

    ImGui::SetNextWindowPos(ImVec2(barX - 10.0f, barY - 10.0f));
    ImGui::SetNextWindowSize(ImVec2(totalW + 20.0f, slotH + 24.0f));
    ImGui::Begin("##weaponBar", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
    ImDrawList* wdl = ImGui::GetWindowDrawList();

    const char* slotLabels[9]  = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    const char* slotNames[6]   = { "HAMMER", "AK-47", "DYNAMITE", "GLOCK", "SHOTGUN", "VOXEL PLACER" };
    ImU32 slotColors[6] = {
        IM_COL32(255, 190, 40, 255),    // gold for hammer
        IM_COL32(60, 200, 255, 255),    // cyan for AK-47
        IM_COL32(255, 80, 60, 255),     // red for dynamite
        IM_COL32(180, 255, 120, 255),   // bright green for glock
        IM_COL32(255, 120, 200, 255),   // pink/magenta for shotgun
        IM_COL32(255, 255, 25, 255)     // yellow for voxel placer
    };

    for (int i = 0; i < 9; i++) {
        float sx = barX + i * (slotW + slotGap);
        float sy = barY;
        ImVec2 p0(sx, sy), p1(sx + slotW, sy + slotH);
        
        int wId = inventory[i];
        bool active = (selectedSlot == i);
        bool hasItem = (wId != -1);
        
        // Background
        ImU32 bgCol  = active ? IM_COL32(45, 45, 50, 240) : IM_COL32(20, 20, 25, 180);
        ImU32 brdCol = active ? ((hasItem && wId < 100) ? slotColors[wId] : IM_COL32(220, 220, 255, 255)) : (hasItem ? IM_COL32(100, 100, 110, 160) : IM_COL32(50, 50, 55, 100));
        float brdThk = active ? 2.0f : 1.5f;

        // Base fill and outer border
        wdl->AddRectFilled(p0, p1, bgCol, 8.0f);
        
        // Shadow for polish
        wdl->AddRect(ImVec2(p0.x+2, p0.y+2), ImVec2(p1.x+2, p1.y+2), IM_COL32(0,0,0,100), 8.0f, 0, brdThk);
        
        // Main Border
        wdl->AddRect(p0, p1, brdCol, 8.0f, 0, brdThk);
        
        // Inner double border for active slots
        if (active) {
            ImVec2 innerP0(p0.x + 4.0f, p0.y + 4.0f);
            ImVec2 innerP1(p1.x - 4.0f, p1.y - 4.0f);
            ImU32 innerBrdCol = (brdCol & 0x00FFFFFF) | IM_COL32(0,0,0, 120); // Keep color, reduce alpha
            wdl->AddRect(innerP0, innerP1, innerBrdCol, 4.0f, 0, 1.0f);
        }

        // Key label (top-left corner)
        char keyBuf[4]; sprintf(keyBuf, "[%s]", slotLabels[i]);
        ImU32 keyCol = hasItem ? IM_COL32(140, 140, 140, 200) : IM_COL32(50, 50, 50, 100);
        wdl->AddText(ImVec2(sx + 5.0f, sy + 4.0f), keyCol, keyBuf);

        if (hasItem) {
            if (wId < 100) {
                GLuint iconTex = 0;
                if (wId == 0) iconTex = hammerIcon;
                else if (wId == 1) iconTex = ak47Icon;
                else if (wId == 2) iconTex = dynamiteIcon;
                else if (wId == 3) iconTex = glockIcon;
                else if (wId == 4) iconTex = shotgunIcon;
                else if (wId == 5) iconTex = placerIcon;

                if (iconTex != 0) {
                    float iconSize = 24.0f;
                    float iconX = sx + (slotW - iconSize) / 2.0f;
                    float iconY = sy + (slotH - iconSize) / 2.0f - 6.0f; // Shifted up a bit
                    wdl->AddImage((void*)(intptr_t)iconTex, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize));
                }

                // Weapon name (centered)
                ImU32 nameCol = active ? slotColors[wId] : IM_COL32(160, 160, 160, 200);
                float nameW = (float)strlen(slotNames[wId]) * 7.0f;
                wdl->AddText(ImVec2(sx + (slotW - nameW) / 2.0f, sy + slotH - 15.0f), nameCol, slotNames[wId]);

                // Dynamite count badge
                if (wId == 2) {
                    char cntBuf[8]; sprintf(cntBuf, "x%d", dynamiteCount);
                    wdl->AddText(ImVec2(sx + slotW - 30.0f, sy + slotH - 30.0f), IM_COL32(255, 80, 60, 220), cntBuf);
                }
            } else {
                // Block material logic
                int matId = wId - 100;
                glm::vec3 matColor = getVoxelColor(matId);
                
                ImVec2 p0_blk(sx + 15.0f, sy + 15.0f);
                ImVec2 p1_blk(sx + slotW - 15.0f, sy + slotH - 15.0f);
                
                ImU32 bCol = IM_COL32((int)(matColor.r * 255), (int)(matColor.g * 255), (int)(matColor.b * 255), 255);
                wdl->AddRectFilled(p0_blk, p1_blk, bCol, 2.0f);
                wdl->AddRect(p0_blk, p1_blk, IM_COL32(0,0,0,255), 2.0f, 0, 1.0f);

                // Name
                char matName[16];
                sprintf(matName, "Block %d", matId);
                float nameW = (float)strlen(matName) * 7.0f;
                wdl->AddText(ImVec2(sx + (slotW - nameW) / 2.0f, sy + slotH - 15.0f), IM_COL32(160,160,160,200), matName);
            }
        }
    }
    ImGui::End();

    if (currentWeapon == 5) {
        const char* modes[] = {
            "Freeform (Hold SHIFT to fill)",
            "Wall",
            "Pillar",
            "Roof",
            "Stairs"
        };
        char hintBuf[128];
        sprintf(hintBuf, "[R] Schematic Mode: %s", modes[currentSchematic]);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(barX, barY - 20.0f), IM_COL32(220, 220, 220, 255), hintBuf);
    }
}

}
}
