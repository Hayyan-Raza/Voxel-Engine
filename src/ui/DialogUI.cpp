#include "DialogUI.h"
#include <imgui.h>
#include "../core/Globals.h"
#include "../world/World.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace UI {
namespace Dialog {

void drawBlueprintSaveDialog(float fbW, float fbH) {
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(fbW / 2.0f, fbH / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Save Structure", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        static char structName[64] = "MyStruct";
        ImGui::InputText("Name", structName, IM_ARRAYSIZE(structName));
        
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            glm::ivec3 minV = glm::min(blueprintCornerA, blueprintCornerB);
            glm::ivec3 maxV = glm::max(blueprintCornerA, blueprintCornerB);
            
            glm::ivec3 realMin = maxV;
            glm::ivec3 realMax = minV;
            bool foundAny = false;
            for (int x = minV.x; x <= maxV.x; x++) {
                for (int y = minV.y; y <= maxV.y; y++) {
                    for (int z = minV.z; z <= maxV.z; z++) {
                        if (getVoxel(x, y, z) != 0) {
                            realMin = glm::min(realMin, glm::ivec3(x, y, z));
                            realMax = glm::max(realMax, glm::ivec3(x, y, z));
                            foundAny = true;
                        }
                    }
                }
            }
            if (foundAny) {
                std::filesystem::create_directories("structures");
                std::string path = "structures/" + std::string(structName) + ".bin";
                std::ofstream ofs(path, std::ios::binary);
                int w = realMax.x - realMin.x + 1;
                int h = realMax.y - realMin.y + 1;
                int d = realMax.z - realMin.z + 1;
                ofs.write(reinterpret_cast<const char*>(&w), sizeof(int));
                ofs.write(reinterpret_cast<const char*>(&h), sizeof(int));
                ofs.write(reinterpret_cast<const char*>(&d), sizeof(int));
                for (int y = realMin.y; y <= realMax.y; y++) {
                    for (int x = realMin.x; x <= realMax.x; x++) {
                        for (int z = realMin.z; z <= realMax.z; z++) {
                            uint8_t v = getVoxel(x, y, z);
                            ofs.write(reinterpret_cast<const char*>(&v), sizeof(uint8_t));
                        }
                    }
                }
                ofs.close();
                std::cout << "Blueprint saved as " << structName << std::endl;
                
                bool exists = false;
                for (const auto& name : availableStructures) {
                    if (name == structName) { exists = true; break; }
                }
                if (!exists) availableStructures.push_back(structName);
            } else {
                std::cout << "Blueprint area is empty!" << std::endl;
            }
            showBlueprintSaveDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showBlueprintSaveDialog = false;
        }
    }
    ImGui::End();
}

}
}
