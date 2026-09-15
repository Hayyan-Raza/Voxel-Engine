#include "Input.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../physics/Physics.h"
#include "../physics/IslandDetection.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include "../particles/Particles.h"
#include "../core/Audio.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>
#include <imgui.h>

void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
    // Handled in main loop
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (gamePaused) return;

    if (currentWeapon == 5) {
        // Scroll materials
        if (yoffset > 0) {
            currentBuildMaterial--;
            if (currentBuildMaterial < 1) currentBuildMaterial = 255;
        } else if (yoffset < 0) {
            currentBuildMaterial++;
            if (currentBuildMaterial > 255) currentBuildMaterial = 1;
        }
    } else {
        // Scroll slots
        if (yoffset > 0) {
            selectedSlot--;
            if (selectedSlot < 0) selectedSlot = 5;
        } else if (yoffset < 0) {
            selectedSlot++;
            if (selectedSlot > 5) selectedSlot = 0;
        }
        currentWeapon = inventory[selectedSlot];
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    auto bState = g_buildingSystem.GetCurrentState();
    bool inRadialMenu = (bState == UI::BuildModeState::RadialMenu_Main || bState == UI::BuildModeState::RadialMenu_Shapes);
    if (gamePaused || configMode || showCreativeInventory || inRadialMenu) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    if (spectatorMode) {
        spectatorYaw += xoffset;
        spectatorPitch = std::clamp(spectatorPitch + yoffset, -89.0f, 89.0f);
        glm::vec3 front;
        front.x    = cosf(glm::radians(spectatorYaw)) * cosf(glm::radians(spectatorPitch));
        front.y    = sinf(glm::radians(spectatorPitch));
        front.z    = sinf(glm::radians(spectatorYaw)) * cosf(glm::radians(spectatorPitch));
        spectatorFront = glm::normalize(front);
    } else {
        yaw   += xoffset;
        pitch = std::clamp(pitch + yoffset, -89.0f, 89.0f);

        hammerOffset.x -= xoffset * 1.5f; 
        hammerOffset.y += yoffset * 1.5f;

        glm::vec3 front;
        front.x    = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front.y    = sinf(glm::radians(pitch));
        front.z    = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
}



void processInput(GLFWwindow* window) {
    

    bool escNow = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escNow && !escapeKeyWasPressed) {
        gamePaused = !gamePaused;
        if (gamePaused) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    escapeKeyWasPressed = escNow;

    static bool f11KeyWasPressed = false;
    bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
    if (f11Now && !f11KeyWasPressed) {
        static bool isFullscreen = false;
        static int winX, winY, winW, winH;
        if (!isFullscreen) {
            glfwGetWindowPos(window, &winX, &winY);
            glfwGetWindowSize(window, &winW, &winH);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            glfwSwapInterval(0); // Force disable VSync on transition to fix 30 FPS cap
            isFullscreen = true;
        } else {
            glfwSetWindowMonitor(window, nullptr, winX, winY, winW, winH, 0);
            glfwSwapInterval(0); // Force disable VSync on transition
            isFullscreen = false;
        }
    }
    f11KeyWasPressed = f11Now;

    bool cNow = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
    if (cNow && !cKeyWasPressed && !gamePaused) {
        configMode = !configMode;
        if (configMode) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    cKeyWasPressed = cNow;
    
    static bool hKeyWasPressed = false;
    bool hNow = (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS);
    if (hNow && !hKeyWasPressed && !gamePaused && !configMode) {
        g_buildingSystem.ToggleGizmoMode();
    }
    hKeyWasPressed = hNow;

    static bool tabKeyWasPressed = false;
    bool tabNow = (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS);
    if (tabNow && !tabKeyWasPressed && !gamePaused && !configMode) {
        showCreativeInventory = !showCreativeInventory;
        if (showCreativeInventory) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    tabKeyWasPressed = tabNow;

    auto updateWeaponSelection = []() {
        int w = inventory[selectedSlot];
        if (w >= 2000) {
            currentWeapon = w;
        } else if (w >= 100) {
            currentWeapon = 5;
            currentBuildMaterial = w - 100;
        } else {
            currentWeapon = w;
        }
    };

    bool oneNow = (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS);
    if (oneNow && !oneKeyWasPressed && !gamePaused) {
        selectedSlot = 0; updateWeaponSelection();
    }
    oneKeyWasPressed = oneNow;

    bool twoNow = (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS);
    if (twoNow && !twoKeyWasPressed && !gamePaused) {
        selectedSlot = 1; updateWeaponSelection();
    }
    twoKeyWasPressed = twoNow;

    bool threeNow = (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS);
    if (threeNow && !threeKeyWasPressed && !gamePaused) {
        selectedSlot = 2; updateWeaponSelection();
    }
    threeKeyWasPressed = threeNow;

    bool fourNow = (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS);
    if (fourNow && !fourKeyWasPressed && !gamePaused) {
        selectedSlot = 3; updateWeaponSelection();
    }
    fourKeyWasPressed = fourNow;

    bool fiveNow = (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS);
    if (fiveNow && !fiveKeyWasPressed && !gamePaused) {
        selectedSlot = 4; updateWeaponSelection();
    }
    fiveKeyWasPressed = fiveNow;

    static bool sixKeyWasPressed = false;
    bool sixNow = (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS);
    if (sixNow && !sixKeyWasPressed && !gamePaused) {
        selectedSlot = 5; updateWeaponSelection();
    }
    sixKeyWasPressed = sixNow;

    static bool sevenKeyWasPressed = false;
    bool sevenNow = (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS);
    if (sevenNow && !sevenKeyWasPressed && !gamePaused) {
        selectedSlot = 6; updateWeaponSelection();
    }
    sevenKeyWasPressed = sevenNow;

    static bool eightKeyWasPressed = false;
    bool eightNow = (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS);
    if (eightNow && !eightKeyWasPressed && !gamePaused) {
        selectedSlot = 7; updateWeaponSelection();
    }
    eightKeyWasPressed = eightNow;

    static bool nineKeyWasPressed = false;
    bool nineNow = (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS);
    if (nineNow && !nineKeyWasPressed && !gamePaused) {
        selectedSlot = 8; updateWeaponSelection();
    }
    nineKeyWasPressed = nineNow;

    bool rNow = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
    if (rNow && !rKeyWasPressed && !gamePaused) {
        if (currentWeapon == 5) {
            currentSchematic = (currentSchematic + 1) % 5;
        } else {
            float scaleFactor = voxelSize / 0.01f;
            spawnLivingMushroom(cameraPos + cameraFront * (3.5f * scaleFactor) + glm::vec3(0.0f, 2.5f, 0.0f));
        }
    }
    rKeyWasPressed = rNow;

    static bool fKeyWasPressed = false;
    bool fNow = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
    if (fNow && !fKeyWasPressed && !gamePaused && !configMode) {
        float scaleFactor = voxelSize / 0.01f;
        spawnLivingMushroom(cameraPos + cameraFront * (3.5f * scaleFactor) + glm::vec3(0.0f, 2.5f, 0.0f));
    }
    fKeyWasPressed = fNow;

    static bool gKeyWasPressed = false;
    bool gNow = (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS);
    if (gNow && !gKeyWasPressed && !gamePaused && !configMode) {
        if (currentWeapon != 5 && currentWeapon != -1) {
            int droppedWeapon = currentWeapon;
            inventory[selectedSlot] = -1;
            currentWeapon = -1;
            
            // Spawn weapon chunk
            VoxelChunk wChunk;
            wChunk.center = cameraPos + cameraFront * 1.5f;
            wChunk.weaponType = droppedWeapon;
            wChunk.life = 999999.0f; // Don't despawn quickly
            
            auto addBox = [&wChunk](int x0, int x1, int y0, int y1, int z0, int z1, uint8_t color) {
                for(int x=x0; x<=x1; x++) {
                    for(int y=y0; y<=y1; y++) {
                        for(int z=z0; z<=z1; z++) {
                            wChunk.voxels.push_back({glm::ivec3(x, y, z), color});
                        }
                    }
                }
            };

            if (droppedWeapon == 0) {
                // Hammer
                addBox(0, 0, -1, 1, 0, 0, 1);
            } else if (droppedWeapon == 1) {
                // AK-47
                addBox(0, 0, 0, 0, -1, 1, 1);
            } else if (droppedWeapon == 2) {
                // Dynamite
                addBox(0, 0, 0, 0, 0, 0, 1);
            } else if (droppedWeapon == 3) {
                // Glock
                addBox(0, 0, 0, 0, 0, 0, 1);
            } else if (droppedWeapon == 4) {
                // Shotgun
                addBox(0, 0, 0, 0, -1, 1, 1);
            }

            glm::vec3 throwVel = cameraFront * 8.0f + glm::vec3(0.0f, 1.5f, 0.0f);
            glm::vec3 throwAng = glm::vec3((rand()%20-10)*0.5f,(rand()%20-10)*0.5f,(rand()%20-10)*0.5f);
            createPhysicsForChunk(wChunk, throwVel, throwAng);
            wChunk.id = nextChunkId++; activeChunks.push_back(std::move(wChunk));
        }
    }
    gKeyWasPressed = gNow;

    // Toggle Inspector (Free Cam) Mode
    static bool iKeyWasPressed = false;
    bool iNow = (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS);
    if (iNow && !iKeyWasPressed && !gamePaused) {
        inspectorMode = !inspectorMode;
        if (inspectorMode) {
            // Reset velocities when entering inspector mode
            playerVelocityY = 0.0f;
            playerVelocityH = glm::vec3(0.0f);
        }
    }
    iKeyWasPressed = iNow;
    
    // Toggle Spectator Mode
    bool f5Now = (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS);
    if (f5Now && !f5KeyWasPressed && !gamePaused) {
        spectatorMode = !spectatorMode;
        if (spectatorMode) {
            spectatorPos = cameraPos;
            spectatorFront = cameraFront;
            spectatorUp = cameraUp;
            spectatorYaw = yaw;
            spectatorPitch = pitch;
        }
    }
    f5KeyWasPressed = f5Now;

    // Blueprint Mode Toggle (P)
    static bool pWasPressed = false;
    bool pNow = (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);
    if (pNow && !pWasPressed && !gamePaused && !configMode && !showCreativeInventory) {
        blueprintMode = !blueprintMode;
        hasCornerA = false;
        hasCornerB = false;
        showBlueprintSaveDialog = false;
        
        if (blueprintMode) {
            std::cout << "Blueprint Mode enabled." << std::endl;
        } else {
            std::cout << "Blueprint Mode disabled." << std::endl;
        }
    }
    pWasPressed = pNow;

    // Building Mode Toggle (B)
    static bool bWasPressed = false;
    bool bNow = (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS);
    if (bNow && !bWasPressed && !gamePaused && !configMode && !showCreativeInventory && !blueprintMode) {
        g_buildingSystem.ToggleBuildMode();
    }
    bWasPressed = bNow;

    static auto lastBuildState = UI::BuildModeState::Inactive;
    auto curBuildState = g_buildingSystem.GetCurrentState();
    if (curBuildState != lastBuildState) {
        bool inRadial = (curBuildState == UI::BuildModeState::RadialMenu_Main || curBuildState == UI::BuildModeState::RadialMenu_Shapes);
        if (inRadial) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
        lastBuildState = curBuildState;
    }

    bool inRadialMenu = (curBuildState == UI::BuildModeState::RadialMenu_Main || curBuildState == UI::BuildModeState::RadialMenu_Shapes);
    if (gamePaused || configMode || showCreativeInventory || inRadialMenu) return;

    float scaleFactor = voxelSize / 0.01f;

    if (spectatorMode) {
        // ---- Spectator Mode (Visualizing Frustum) ----
        float flySpeed = 12.0f * scaleFactor;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) flySpeed *= 3.0f;

        glm::vec3 moveDir(0.0f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += spectatorFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= spectatorFront;
        glm::vec3 right = glm::normalize(glm::cross(spectatorFront, spectatorUp));
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir += spectatorUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) moveDir -= spectatorUp;

        if (glm::length(moveDir) > 0.001f) moveDir = glm::normalize(moveDir);
        spectatorPos += moveDir * flySpeed * deltaTime;
        
        // No gravity, no velocity for the actual player body while in spectator
        playerVelocityY = 0.0f;
        playerVelocityH = glm::vec3(0.0f);
    } else if (inspectorMode) {
        // ---- Inspector / Free Cam Mode ----
        float flySpeed = 12.0f * scaleFactor;
        // Shift to go faster
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            flySpeed *= 3.0f;

        glm::vec3 moveDir(0.0f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= cameraFront;
        glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir += cameraUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) moveDir -= cameraUp;

        if (glm::length(moveDir) > 0.001f)
            moveDir = glm::normalize(moveDir);

        cameraPos += moveDir * flySpeed * deltaTime;

        // No gravity, no velocity
        playerVelocityY = 0.0f;
        playerVelocityH = glm::vec3(0.0f);
    } else {
        // ---- Normal Mode ----
        float accel = 18.0f * scaleFactor;
        float friction = 12.0f;
        glm::vec3 inputDir(0.0f);
        
        glm::vec3 flat  = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
        glm::vec3 right = glm::normalize(glm::cross(flat, cameraUp));
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputDir += flat;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputDir -= flat;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputDir -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputDir += right;

        if (glm::length(inputDir) > 0.0f) {
            inputDir = glm::normalize(inputDir);
            playerVelocityH += inputDir * accel * deltaTime;
        }
        
        float vLen = glm::length(playerVelocityH);
        if (vLen > 0.001f) {
            glm::vec3 vDir = playerVelocityH / vLen;
            float drop = vLen * friction * deltaTime;
            playerVelocityH = vDir * std::max(0.0f, vLen - drop);
        }
        
        float maxSpeed = 4.5f * scaleFactor;
        if (glm::length(playerVelocityH) > maxSpeed) 
            playerVelocityH = glm::normalize(playerVelocityH) * maxSpeed;

        cameraPos += playerVelocityH * deltaTime;

        if (isGrounded && glm::length(playerVelocityH) > 0.1f) {
            float bobFreq = 12.0f;
            float bobAmp = 0.02f * scaleFactor;
            cameraPos.y += sinf(static_cast<float>(glfwGetTime()) * bobFreq) * bobAmp * (glm::length(playerVelocityH) / maxSpeed);
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
            playerVelocityY = JUMP_FORCE * scaleFactor;
            isGrounded      = false;
        }
    }

    hammerSwayTimer += deltaTime * 2.0f;
    hammerOffset *= 0.85f; 
    
    float targetTilt = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) targetTilt =  15.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) targetTilt = -15.0f;
    hammerTilt = hammerTilt * 0.9f + targetTilt * 0.1f;

    // Update carrying position of grabbed ragdoll
    updateGrabbedRagdoll(cameraPos, cameraFront, deltaTime);
}

