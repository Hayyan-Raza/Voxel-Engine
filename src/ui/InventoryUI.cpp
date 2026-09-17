#include "InventoryUI.h"
#include <imgui.h>
#include "../core/Globals.h"
#include "../world/World.h"
#include "../rendering/Renderer.h"
#include <stdio.h>

namespace UI {
namespace Inventory {

void drawResourceInventory() {
    float resX = ImGui::GetMainViewport()->WorkSize.x - 160.0f;
    ImGui::SetNextWindowPos(ImVec2(resX > 0 ? resX : 640.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(150, 0));
    ImGui::Begin("Resources", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "RESOURCES");
    ImGui::Separator();
    
    auto drawResource = [](int count, const char* name, ImVec4 color) {
        if (count > 0) {
            ImGui::ColorButton(name, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoAlpha, ImVec2(24, 24));
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::Text("%s: %d", name, count);
            return true;
        }
        return false;
    };

    bool hasResources = false;
    if (drawResource(resourceInventory[1], "Dirt", ImVec4(0.40f, 0.26f, 0.13f, 1.0f))) hasResources = true;
    if (drawResource(resourceInventory[2], "Grass", ImVec4(0.20f, 0.66f, 0.32f, 1.0f))) hasResources = true;
    if (drawResource(resourceInventory[3], "Stone", ImVec4(0.50f, 0.50f, 0.50f, 1.0f))) hasResources = true;
    if (drawResource(resourceInventory[4], "Wood", ImVec4(0.55f, 0.27f, 0.07f, 1.0f))) hasResources = true;
    if (drawResource(resourceInventory[5], "Leaves", ImVec4(0.13f, 0.55f, 0.13f, 1.0f))) hasResources = true;
    if (drawResource(resourceInventory[8], "Water", ImVec4(0.15f, 0.45f, 0.85f, 1.0f))) hasResources = true;
    
    if (!hasResources) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Empty");
    }
    ImGui::End();
}

void drawCreativeInventory(float fbW, float fbH) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(fbW / 2.0f, fbH / 2.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Creative Inventory (Press TAB to close)", nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("InventoryTabs")) {
            if (ImGui::BeginTabItem("Blocks")) {
                ImGui::Text("Click a material to assign it to your selected hotbar slot.");
                ImGui::Separator();

                if (ImGui::BeginChild("MaterialGrid", ImVec2(0, 0), true)) {
                    int columns = 10;
                    if (ImGui::BeginTable("MaterialsTable", columns)) {
                        for (int i = 1; i <= 255; i++) {
                            ImGui::TableNextColumn();
                            
                            glm::vec3 c = getVoxelColor(i);
                            ImVec4 color = ImVec4(c.r, c.g, c.b, 1.0f);
                            
                            char idStr[16];
                            sprintf(idStr, "##mat%d", i);
                            
                            if (ImGui::ColorButton(idStr, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoAlpha, ImVec2(40, 40))) {
                                inventory[selectedSlot] = 100 + i;
                                currentWeapon = 5;
                                currentBuildMaterial = i;
                            }
                            
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Material %d", i);
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Structs")) {
                ImGui::Text("Click a structure to assign it to your hotbar.");
                ImGui::Separator();
                if (ImGui::BeginChild("StructsGrid", ImVec2(0, 0), true)) {
                    for (size_t i = 0; i < availableStructures.size(); i++) {
                        if (ImGui::Button(availableStructures[i].c_str(), ImVec2(150, 30))) {
                            inventory[selectedSlot] = 2000 + i;
                            currentWeapon = 2000 + (int)i;
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

}
}
