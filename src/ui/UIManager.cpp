#include "UIManager.h"
#include "../rendering/Renderer.h"
#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "../core/Globals.h"
#include "../world/World.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include "../rendering/WeaponModels.h"

#include "DebugUI.h"
#include "InventoryUI.h"
#include "DialogUI.h"
#include "ToolbarUI.h"
#include "ToolbarUI.h"

// Stolen from main.cpp
GLuint UIManager::loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 4);
    if (data) {
        // Remove background (make it transparent) based on top-left pixel
        unsigned char bgR = data[0];
        unsigned char bgG = data[1];
        unsigned char bgB = data[2];
        for (int i = 0; i < width * height * 4; i += 4) {
            bool isBg = (abs(data[i] - bgR) < 40 && abs(data[i+1] - bgG) < 40 && abs(data[i+2] - bgB) < 40);
            bool isWhite = (data[i] > 215 && data[i+1] > 215 && data[i+2] > 215);
            
            if (isBg || isWhite) {
                data[i+3] = 0; // Set alpha to 0
            }
        }

        GLenum format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
}

void UIManager::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    // Apply polished modern styling
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(10, 10);
    
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    
    style.WindowBorderSize = 2.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 2.0f;
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);
    colors[ImGuiCol_Border]         = ImVec4(0.35f, 0.35f, 0.40f, 0.60f);
    colors[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_FrameBg]        = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg]        = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_Button]         = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_Header]         = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    UI::Toolbar::handTexture = loadTexture("assets/textures/hand.png");
    UI::Toolbar::closedHandTexture = loadTexture("assets/textures/closed_hand.png");
    woodTexture = loadTexture("assets/textures/wood.png");
    UI::Toolbar::hammerIcon = loadTexture("assets/textures/hammer.png");
    UI::Toolbar::ak47Icon = loadTexture("assets/textures/ak47.png");
    UI::Toolbar::dynamiteIcon = loadTexture("assets/textures/dynamite.png");
    UI::Toolbar::glockIcon = loadTexture("assets/textures/glock.png");
    UI::Toolbar::shotgunIcon = loadTexture("assets/textures/shotgun.png");
    UI::Toolbar::placerIcon = loadTexture("assets/textures/placer.png");
}

void UIManager::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::renderLoadingScreenInitial(GLFWwindow* window) {
    int fbW_int, fbH_int;
    glfwGetFramebufferSize(window, &fbW_int, &fbH_int);
    glViewport(0, 0, fbW_int, fbH_int);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::SetNextWindowPos(ImVec2(fbW_int/2.0f - 150.0f, fbH_int/2.0f - 50.0f));
    ImGui::SetNextWindowSize(ImVec2(300, 100));
    ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Generating World Terrain...");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Please wait...");
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void UIManager::renderLoadingScreenMeshes(GLFWwindow* window, int loadedMeshes, int maxMeshes) {
    int fbW_int, fbH_int;
    glfwGetFramebufferSize(window, &fbW_int, &fbH_int);
    glViewport(0, 0, fbW_int, fbH_int);
    
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::SetNextWindowPos(ImVec2(fbW_int/2.0f - 150.0f, fbH_int/2.0f - 50.0f));
    ImGui::SetNextWindowSize(ImVec2(300, 100));
    ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Generating World...");
    ImGui::SetWindowFontScale(1.0f);
    if (maxMeshes > 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Meshes loaded: %d / %d", loadedMeshes, maxMeshes);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Meshes loaded: %d", loadedMeshes);
    }
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(window);
}

void UIManager::renderStartupMenu(GLFWwindow* window) {
    int fbW_int, fbH_int;
    glfwGetFramebufferSize(window, &fbW_int, &fbH_int);
    glViewport(0, 0, fbW_int, fbH_int);
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::SetNextWindowPos(ImVec2(fbW_int/2.0f - 200.0f, fbH_int/2.0f - 150.0f));
    ImGui::SetNextWindowSize(ImVec2(400, 300));
    ImGui::Begin("World Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Select Game Mode");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Infinite World", ImVec2(-1, 60))) {
        g_gameMode = GameMode::Infinite;
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Skyblock Version (9 Chunks)", ImVec2(-1, 60))) {
        g_gameMode = GameMode::Skyblock;
    }
    
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void UIManager::renderUI(GLFWwindow* window, float deltaTime, const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::mat4& view, const glm::mat4& proj) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int fbW_int, fbH_int;
    glfwGetFramebufferSize(window, &fbW_int, &fbH_int);
    float ffbW = (float)fbW_int;
    float ffbH = (float)fbH_int;

    if (!gamePaused && !configMode) {
        UI::Toolbar::drawCrosshair(ffbW, ffbH);
        UI::Toolbar::drawWeaponBar(ffbW, ffbH);
    } else if (gamePaused) {
        UI::Debug::drawPauseMenu(window, ffbW, ffbH);
    }

    if (configMode) {
        UI::Debug::drawConfigMenu(window);
        UI::Debug::drawVibePackMenu();
        UI::Debug::drawRagdollMenu(cameraPos, cameraFront);
    }

    UI::Debug::drawFPSCounter();
    UI::Inventory::drawResourceInventory();

    if (showCreativeInventory) {
        UI::Inventory::drawCreativeInventory(ffbW, ffbH);
    }

    if (!gamePaused && g_buildingSystem.GetCurrentState() != UI::BuildModeState::Inactive) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(ffbW, ffbH));
        ImGui::Begin("##BuildingUI", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration);
        
        int winW, winH;
        glfwGetWindowSize(window, &winW, &winH);
        glm::mat4 viewProj = proj * view;
        g_buildingSystem.RenderUI(ImGui::GetWindowDrawList(), glm::vec2(winW / 2.0f, winH / 2.0f), viewProj, glm::vec2(winW, winH));
        ImGui::End();
    }

    if (showBlueprintSaveDialog) {
        UI::Dialog::drawBlueprintSaveDialog(ffbW, ffbH);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


