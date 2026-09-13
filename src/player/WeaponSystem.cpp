#include "WeaponSystem.h"
#include "Weapons.h"
#include <iostream>
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
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>
#include <imgui.h>
#include "../physics/Raycast.h"
#include "../world/VoxelModifier.h"

void handleDestruction(GLFWwindow* window) {
    if (getHeldChunk() != nullptr) {
        glm::vec3 targetPos = cameraPos + cameraFront * 2.5f;
        getHeldChunk()->velocity = (targetPos - getHeldChunk()->center) * 10.0f;
        getHeldChunk()->velocity.y += GRAVITY * deltaTime; // Counteract gravity roughly
        getHeldChunk()->angularVelocity *= 0.9f; // Dampen rotation
        isLookingAtGrabbable = false;
    } else {
        isLookingAtGrabbable = (checkGrabbableChunkRaycast(cameraPos, cameraFront, 4.0f) != nullptr);
        if (!isLookingAtGrabbable) {
            RaycastResult res = performWeaponRaycast(cameraPos, cameraFront, 4.0f, 0.0f, 0.0f);
            if (res.hitTerrain) isLookingAtGrabbable = true;
        }
    }
    static bool eKeyWasPressed = false;
    bool eNow = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
    if (eNow && !eKeyWasPressed && !gamePaused) {
        if (getHeldChunk() != nullptr) {
            getHeldChunk()->velocity *= 0.5f; // gentle drop
            heldChunkId = 0;
        } else {
            if (VoxelChunk* c = checkGrabbableChunkRaycast(cameraPos, cameraFront, 4.0f)) {
                heldChunkId = c->id;
            } else {
                // Must be terrain. Pluck it!
                RaycastResult res = performWeaponRaycast(cameraPos, cameraFront, 4.0f, 0.0f, 0.0f);
                if (res.hitTerrain) {
                    extractTerrainToChunk(res.hitVox, -cameraFront);
                    if (!activeChunks.empty()) {
                        heldChunkId = activeChunks.back().id;
                    }
                }
            }
        }
    }
    eKeyWasPressed = eNow;

    bool leftNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) && !ImGui::GetIO().WantCaptureMouse && !configMode && !gamePaused && !showCreativeInventory;
    static bool rightMouseWasPressed = false;
    bool rightNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) && !ImGui::GetIO().WantCaptureMouse && !configMode && !gamePaused && !showCreativeInventory;

    // --- Blueprint Mode Intercept ---
    if (blueprintMode && showGhostVox && !gamePaused) {
        if (leftNow && !leftMouseWasPressed) {
            blueprintCornerA = ghostVox;
            hasCornerA = true;
            std::cout << "Blueprint Corner A set" << std::endl;
        }
        if (rightNow && !rightMouseWasPressed) {
            blueprintCornerB = ghostVox;
            hasCornerB = true;
            std::cout << "Blueprint Corner B set" << std::endl;
        }
        if (hasCornerA && hasCornerB) {
            showBlueprintSaveDialog = true;
            blueprintMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        leftNow = false;
        rightNow = false;
    }

    // --- Right Mouse Button (RMB): Grab & Carry Ragdoll / Chunk / Cycle Material ---
    bool isBuildMode = g_buildingSystem.GetCurrentState() != UI::BuildModeState::Inactive;
    if (rightNow && !rightMouseWasPressed && !gamePaused && !isBuildMode) {
        if (currentWeapon == 5 && grabbedRagdollId == 0 && getHeldChunk() == nullptr && !isLookingAtGrabbable) {
            currentBuildMaterial++;
            if (currentBuildMaterial > 255) currentBuildMaterial = 1;
        } else {
            if (grabbedRagdollId == 0 && getHeldChunk() == nullptr) {
                if (isLookingAtGrabbable) {
                    if (VoxelChunk* c = checkGrabbableChunkRaycast(cameraPos, cameraFront, 4.0f)) {
                        heldChunkId = c->id;
                    }
                    if (getHeldChunk() != nullptr && getHeldChunk()->weaponType != -1) {
                        int emptySlot = -1;
                        for (int i = 0; i < 6; i++) {
                            if (inventory[i] == -1) {
                                emptySlot = i;
                                break;
                            }
                        }
                        
                        if (emptySlot != -1) {
                            inventory[emptySlot] = getHeldChunk()->weaponType;
                            selectedSlot = emptySlot;
                            currentWeapon = getHeldChunk()->weaponType;
                            getHeldChunk()->life = -1.0f; // mark for destruction
                            heldChunkId = 0;     // drop from hands immediately
                        } else {
                            heldChunkId = 0; // Inventory full, drop immediately
                        }
                    }
                } else {
                    grabRagdollRaycast(cameraPos, cameraFront, 3.5f);
                }
            } else {
                releaseGrabbedRagdoll();
                heldChunkId = 0;
            }
        }
    } else if (!rightNow && rightMouseWasPressed && !isBuildMode) {
        float scaleFactor = voxelSize / 0.01f;
        if (grabbedRagdollId != 0) {
            throwGrabbedRagdoll(cameraFront * (16.0f * scaleFactor) + glm::vec3(0.0f, 3.5f * scaleFactor, 0.0f));
        } else if (getHeldChunk() != nullptr) {
            getHeldChunk()->velocity = cameraFront * (12.0f * scaleFactor) + glm::vec3(0.0f, 2.5f * scaleFactor, 0.0f);
            getHeldChunk()->angularVelocity = glm::vec3((rand()%20-10)*0.2f, (rand()%20-10)*0.2f, (rand()%20-10)*0.2f);
            heldChunkId = 0;
        }
    }
    rightMouseWasPressed = rightNow;

    // --- Left Click while carrying ragdoll: MAX WRECKING BALL THROW ---
    if (leftNow && grabbedRagdollId != 0 && !gamePaused && !isBuildMode) {
        float scaleFactor = voxelSize / 0.01f;
        throwGrabbedRagdoll(cameraFront * (24.0f * scaleFactor) + glm::vec3(0.0f, 5.0f * scaleFactor, 0.0f));
        leftNow = false; // Consume left click so weapon does not fire
    }

    // --- Left Mouse Button (LMB): Drag chunks / Use Weapon ---
    if (leftNow && !leftMouseWasPressed && !gamePaused && !isBuildMode) {
        if (currentWeapon == -1 && getHeldChunk() == nullptr && isLookingAtGrabbable) {
            // Empty hand punching/breaking terrain without grabbing
            RaycastResult res = performWeaponRaycast(cameraPos, cameraFront, 4.0f, 0.0f, 0.0f);
            if (res.hitTerrain) {
                extractTerrainToChunk(res.hitVox, -cameraFront);
            }
        }
    }

    if (leftNow && !isBuildMode) {
        if (currentWeapon >= 0 && currentWeapon < 6) {
            if (Weapon* w = GetWeapon(currentWeapon)) {
                w->Update(deltaTime, leftNow, leftMouseWasPressed);
            }
            if (hammerHitThisFrame) {
                glm::ivec3 hitVox(
                    static_cast<int>(floor((hammerHitWorldPos.x - hammerHitWorldNormal.x * 0.05f) / voxelSize)),
                    static_cast<int>(floor((hammerHitWorldPos.y - hammerHitWorldNormal.y * 0.05f) / voxelSize)),
                    static_cast<int>(floor((hammerHitWorldPos.z - hammerHitWorldNormal.z * 0.05f) / voxelSize))
                );
                applyTerrainDestruction(hitVox, hammerHitWorldPos, 1, -hammerHitWorldNormal);
                hammerHitThisFrame = false;
            }
        } else if (currentWeapon >= 2000) {
            // Weapon 2000+: Spawn structure
            if (!leftMouseWasPressed && showGhostVox && !gamePaused) {
                int structIdx = currentWeapon - 2000;
                if (structIdx >= 0 && structIdx < availableStructures.size()) {
                    std::string structName = availableStructures[structIdx];
                    std::string path = "structures/" + structName + ".bin";
                    if (std::filesystem::exists(path)) {
                        std::ifstream ifs(path, std::ios::binary);
                        int w, h, d;
                        ifs.read(reinterpret_cast<char*>(&w), sizeof(int));
                        ifs.read(reinterpret_cast<char*>(&h), sizeof(int));
                        ifs.read(reinterpret_cast<char*>(&d), sizeof(int));
                        
                        // Center blueprint horizontally around ghostVox, bottom align
                        int startX = ghostVox.x - w/2;
                        int startY = ghostVox.y;
                        int startZ = ghostVox.z - d/2;
                        
                        for (int y = 0; y < h; y++) {
                            for (int x = 0; x < w; x++) {
                                for (int z = 0; z < d; z++) {
                                    uint8_t v;
                                    ifs.read(reinterpret_cast<char*>(&v), sizeof(uint8_t));
                                    if (v != 0) {
                                        setVoxel(startX + x, startY + y, startZ + z, v);
                                        markChunkDirty(startX + x, startY + y, startZ + z);
                                    }
                                }
                            }
                        }
                        ifs.close();
                        std::cout << "Spawned structure: " << structName << std::endl;
                    }
                }
            }
        }
    }
    
    leftMouseWasPressed = leftNow;
    
    // Update Ghost Vox
    bool wasShowingGhostVox = showGhostVox;
    showGhostVox = false;
    if ((currentWeapon >= 2000 || currentWeapon == 5) && !gamePaused) {
        glm::ivec3 placeVox(-1);
        bool hit = performPlacementRaycast(cameraPos, cameraFront, 15.0f, placeVox);
        
        if (hit && placeVox.x >= 0 && placeVox.x < GRID_SIZE && placeVox.y >= 0 && placeVox.y < GRID_SIZE && placeVox.z >= 0 && placeVox.z < GRID_SIZE) {
            ghostVox = placeVox;
            showGhostVox = true;
        }
    }

    if (shootTimer > 0.0f) {
        shootTimer -= deltaTime;
    }
    if (muzzleFlashTimer > 0.0f) {
        muzzleFlashTimer -= deltaTime;
    }
}

void triggerTestHit() { applyTerrainDestruction(glm::ivec3(500,45,500), glm::vec3(500,45,500)*voxelSize, 1, glm::vec3(0,-1,0)); }
