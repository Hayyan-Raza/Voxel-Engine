#include "DebugUI.h"
#include <imgui.h>
#include "../core/Globals.h"
#include "../world/World.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include <glm/gtc/type_ptr.hpp>

namespace UI {
namespace Debug {

void drawPauseMenu(GLFWwindow* window, float fbW, float fbH) {
    ImGui::SetNextWindowPos(ImVec2(fbW/2.0f - 100, fbH/2.0f - 75), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 150));
    ImGui::Begin("Game Paused", nullptr,
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button("Resume Game", ImVec2(180, 40))) {
        gamePaused = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Quit to Desktop", ImVec2(180, 40)))
        glfwSetWindowShouldClose(window, true);
    ImGui::End();
}

void drawConfigMenu(GLFWwindow* window) {
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 250));
    ImGui::Begin("Hammer Config", nullptr);
    ImGui::SliderFloat("FOV", &fieldOfView, 30.0f, 120.0f);
    ImGui::Separator();
    ImGui::Text("Environment");
    ImGui::SliderFloat("Render Distance", &maxRenderDistance, 32.0f, 512.0f);
    ImGui::Checkbox("Enable Sun/Moon", &enableSunMoon);
    
    ImGui::Checkbox("Enable Directional Light", &enableDirLight);
    ImGui::Checkbox("Enable Wireframe Mode", &enableWireframe);
    ImGui::SliderFloat("Debris Lifetime", &chunkLifeMultiplier, 0.5f, 60.0f);
    ImGui::SliderFloat("Fog Density", &g_fogDensity, 0.0f, 0.20f);
    ImGui::SliderFloat("Day/Night Speed", &dayNightSpeed, 0.0f, 2.0f);
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Water Settings")) {
        ImGui::ColorEdit3("Shallow Color", glm::value_ptr(waterShallowColor));
        ImGui::ColorEdit3("Deep Color", glm::value_ptr(waterDeepColor));
        ImGui::SliderFloat("Sky Blend", &waterSkyBlend, 0.0f, 1.0f);
        ImGui::SliderFloat("Water Wave Speed", &waterWaveSpeed, 0.0f, 5.0f);
    }
    ImGui::Separator();
    ImGui::Text("Vegetation Settings");
    ImGui::SliderFloat("Veg Sway Speed", &vegetationSwaySpeed, 0.0f, 5.0f);
    ImGui::SliderFloat("Veg Sway Intensity", &vegetationSwayIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat("Leaf Particle Density", &leafParticleDensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Leaf Particle Speed", &leafParticleSpeed, 0.0f, 2.0f);
    ImGui::Separator();
    ImGui::Text("Hammer Position");
    ImGui::SliderFloat("Pos X", &hammerBasePos.x, -2.0f, 2.0f);
    ImGui::SliderFloat("Pos Y", &hammerBasePos.y, -2.0f, 2.0f);
    ImGui::SliderFloat("Pos Z", &hammerBasePos.z, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::Text("Hammer Rotation");
    ImGui::SliderFloat("Rot Y (Yaw)", &hammerBaseRot.x, -90.0f, 90.0f);
    ImGui::SliderFloat("Rot X (Pitch)", &hammerBaseRot.y, -90.0f, 90.0f);
    
    if (ImGui::Button("Close Config (C)", ImVec2(280, 30))) {
        configMode = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
    ImGui::End();
}

void drawVibePackMenu() {
    ImGui::SetNextWindowPos(ImVec2(10, 310), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 220));
    ImGui::Begin("Vibe Pack (Graphics)", nullptr);
    ImGui::SliderFloat("Ambient Occlusion", &vAOScale, 0.0f, 2.0f);
    ImGui::SliderFloat("Bloom Intensity", &vBloom, 0.0f, 2.0f);
    ImGui::SliderFloat("Chrom. Aberration", &vChromAb, 0.0f, 5.0f);
    ImGui::SliderFloat("Film Grain", &vGrain, 0.0f, 0.2f);
    ImGui::SliderFloat("Exposure", &vExposure, 0.01f, 2.0f); 
    ImGui::Separator();
    ImGui::Checkbox("Wireframe Mode", &enableWireframe);
    ImGui::SliderFloat("Particle Life", &particleLifespan, 0.1f, 20.0f);
    ImGui::Separator();
    if (ImGui::Button("Reset to Default", ImVec2(280, 30))) {
        vAOScale = 1.0f;
        vBloom = 0.35f;
        vChromAb = 0.0f;
        vGrain = 0.0f;
        vExposure = 1.15f;
        enableWireframe = false;
        particleLifespan = 3.0f;
    }
    ImGui::End();
}

void drawRagdollMenu(const glm::vec3& cameraPos, const glm::vec3& cameraFront) {
    ImGui::SetNextWindowPos(ImVec2(10, 540), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 190));
    ImGui::Begin("Ragdoll Physics", nullptr);
    ImGui::Text("Active Ragdolls: %d", (int)activeRagdolls.size());
    if (ImGui::Button("Spawn Ragdoll (R)", ImVec2(280, 30))) {
        float scaleFactor = voxelSize / 0.01f;
        spawnRagdoll(cameraPos + cameraFront * (1.2f * scaleFactor), cameraFront * (3.5f * scaleFactor) + glm::vec3(0.0f, 0.8f, 0.0f));
    }
    if (ImGui::Button("Spawn Chick (F)", ImVec2(280, 30))) {
        float scaleFactor = voxelSize / 0.01f;
        spawnLivingChick(cameraPos + cameraFront * (1.2f * scaleFactor));
    }
    if (ImGui::Button("Clear All Ragdolls", ImVec2(280, 30))) {
        clearAllRagdolls();
        clearAllMobs();
    }
    ImGui::SliderFloat("Joint Stiffness", &ragdollStiffnessParam, 0.1f, 1.0f);
    ImGui::End();
}

void drawFPSCounter() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::Begin("##fps", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextColored(ImVec4(1,1,0,1), "FPS: %.1f | Active Ragdolls: %d (Press [R] to spawn)", ImGui::GetIO().Framerate, (int)activeRagdolls.size());
    ImGui::End();
}

}
}
