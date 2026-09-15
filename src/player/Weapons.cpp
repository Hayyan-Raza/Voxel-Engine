#include "Weapons.h"
#include "WeaponSystem.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../physics/Physics.h"
#include "../physics/Raycast.h"
#include "../world/VoxelModifier.h"
#include "../particles/Particles.h"
#include <algorithm>
#include <iostream>

static Weapon* g_weapons[6] = {nullptr};

void InitWeapons() {
    g_weapons[0] = new HammerWeapon();
    g_weapons[1] = new AK47Weapon();
    g_weapons[2] = new DynamiteWeapon();
    g_weapons[3] = new GlockWeapon();
    g_weapons[4] = new ShotgunWeapon();
    g_weapons[5] = new VoxelPlacerWeapon();
}

Weapon* GetWeapon(int id) {
    if (id >= 0 && id < 6) return g_weapons[id];
    return nullptr;
}

// ----- Hammer Weapon -----
void HammerWeapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && !leftMouseWasPressed && swingTimer <= 0.0f) {
        swingTimer = 0.35f;
        destructionPending = true;
    }
}

// ----- AK-47 Weapon -----
void AK47Weapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && shootTimer <= 0.0f && !gamePaused) {
        shootTimer = 0.10f;
        
        // Recoil
        hammerRecoilVelPos.z = std::min(hammerRecoilVelPos.z - 0.06f, -0.02f);
        hammerRecoilVelPos.z = std::max(hammerRecoilVelPos.z, -0.18f);
        hammerRecoilVelRot.x = std::min(hammerRecoilVelRot.x + 2.5f, 8.0f);
        float yawKick = ((rand() % 200) / 100.0f - 1.0f) * 0.6f;
        hammerRecoilVelRot.y = glm::clamp(hammerRecoilVelRot.y + yawKick, -1.5f, 1.5f);

        cameraShakeVel.y = std::min(cameraShakeVel.y + 0.08f, 0.25f);
        
        // Muzzle flash
        glm::vec3 camRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        glm::vec3 camUpActual = glm::normalize(glm::cross(camRight, cameraFront));
        glm::vec3 muzzlePos = cameraPos + cameraFront * 0.38f + camRight * 0.05f - camUpActual * 0.04f;
        spawnSparks(muzzlePos, 2);
        spawnDust(muzzlePos, 1, 3);
        muzzleFlashTimer = 0.05f;
        muzzleFlashPos = muzzlePos;
        
        // Fire bullet!
        RaycastResult res = performWeaponRaycast(cameraPos, cameraFront, 6.0f, 7.0f, 0.12f);
        if (res.hitTerrain) {
            applyTerrainDestruction(res.hitVox, res.hitPos, 1, -cameraFront);
        }
    }
}

// ----- Dynamite Weapon -----
void DynamiteWeapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && !leftMouseWasPressed && dynamiteCount > 0) {
        dynamiteCount--;
        
        VoxelChunk dynamite;
        dynamite.center = cameraPos + cameraFront * 1.0f;
        dynamite.life = 2.5f;
        dynamite.isDynamite = true;
        
        auto addStick = [&](int cx, int cy) {
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (abs(x) == 1 && abs(y) == 1) continue;
                    int gx = cx + x, gy = cy + y;
                    for (int z = -4; z <= 3; z++)
                        dynamite.voxels.push_back({glm::ivec3(gx, gy, z), 6});
                    dynamite.voxels.push_back({glm::ivec3(gx, gy,  4), 7});
                    dynamite.voxels.push_back({glm::ivec3(gx, gy, -5), 7});
                }
            }
        };
        addStick(-2, -1);
        addStick( 2, -1);
        addStick( 0,  2);
        
        for (int x = -3; x <= 3; x++) {
            for (int y = -2; y <= 3; y++) {
                if (abs(x)==3 || y==3 || y==-2) {
                    dynamite.voxels.push_back({glm::ivec3(x,y, 0), 3});
                    dynamite.voxels.push_back({glm::ivec3(x,y,-1), 3});
                }
            }
        }
        dynamite.voxels.push_back({glm::ivec3(-2,-1, 5), 4});
        dynamite.voxels.push_back({glm::ivec3(-1, 0, 6), 4});
        dynamite.voxels.push_back({glm::ivec3( 2,-1, 5), 4});
        dynamite.voxels.push_back({glm::ivec3( 1, 0, 6), 4});
        dynamite.voxels.push_back({glm::ivec3( 0, 2, 5), 4});
        dynamite.voxels.push_back({glm::ivec3( 0, 1, 6), 4});
        dynamite.voxels.push_back({glm::ivec3( 0, 0, 7), 4});
        dynamite.voxels.push_back({glm::ivec3( 0, 0, 8), 4});
        dynamite.voxels.push_back({glm::ivec3( 1, 1, 9), 4});
        
        glm::vec3 throwVel = cameraFront * 7.0f + glm::vec3(0.0f, 2.5f, 0.0f);
        glm::vec3 throwAng = glm::vec3((rand()%20-10)*1.5f,(rand()%20-10)*1.5f,(rand()%20-10)*1.5f);
        createPhysicsForChunk(dynamite, throwVel, throwAng);
        dynamite.id = nextChunkId++; activeChunks.push_back(std::move(dynamite));
    }
}

// ----- Glock Weapon -----
void GlockWeapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && !leftMouseWasPressed && shootTimer <= 0.0f && !gamePaused) {
        shootTimer = 0.18f;

        hammerRecoilVelPos.z = std::min(hammerRecoilVelPos.z - 0.04f, -0.01f);
        hammerRecoilVelRot.x = std::min(hammerRecoilVelRot.x + 2.0f, 6.0f);
        
        glm::vec3 camRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        glm::vec3 camUpActual = glm::normalize(glm::cross(camRight, cameraFront));
        glm::vec3 muzzlePos = cameraPos + cameraFront * 0.25f + camRight * 0.05f - camUpActual * 0.03f;
        spawnSparks(muzzlePos, 1);
        spawnDust(muzzlePos, 1, 2);
        muzzleFlashTimer = 0.05f;
        muzzleFlashPos = muzzlePos;

        RaycastResult res = performWeaponRaycast(cameraPos, cameraFront, 6.0f, 5.0f, 0.12f);
        if (res.hitTerrain) {
            applyTerrainDestruction(res.hitVox, res.hitPos, 1, -cameraFront);
        }
    }
}

// ----- Shotgun Weapon -----
void ShotgunWeapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && !leftMouseWasPressed && shootTimer <= 0.0f && !gamePaused) {
        shootTimer = 0.85f;

        hammerRecoilVelPos.z = std::min(hammerRecoilVelPos.z - 0.15f, -0.05f);
        hammerRecoilVelRot.x = std::min(hammerRecoilVelRot.x + 7.5f, 15.0f);
        cameraShakeVel.y = std::min(cameraShakeVel.y + 0.35f, 0.60f);

        glm::vec3 camRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        glm::vec3 camUpActual = glm::normalize(glm::cross(camRight, cameraFront));
        glm::vec3 muzzlePos = cameraPos + cameraFront * 0.42f + camRight * 0.05f - camUpActual * 0.04f;
        spawnSparks(muzzlePos, 6);
        spawnDust(muzzlePos, 2, 4);
        muzzleFlashTimer = 0.05f;
        muzzleFlashPos = muzzlePos;

        for (int p = 0; p < 6; p++) {
            float spread = 0.08f;
            glm::vec3 pelletDir = glm::normalize(cameraFront + 
                camRight * ((rand()%200-100)*0.01f * spread) + 
                camUpActual * ((rand()%200-100)*0.01f * spread));

            RaycastResult res = performWeaponRaycast(cameraPos, pelletDir, 5.0f, 8.0f, 0.14f);
            if (res.hitTerrain) {
                applyTerrainDestruction(res.hitVox, res.hitPos, 1, -pelletDir);
            }
        }
    }
}

// ----- Voxel Placer Weapon -----
void VoxelPlacerWeapon::Update(float dt, bool leftClick, bool leftMouseWasPressed) {
    if (leftClick && !leftMouseWasPressed && showGhostVox && !gamePaused) {
        setVoxel(ghostVox.x, ghostVox.y, ghostVox.z, currentBuildMaterial);
        std::cout << "Placed voxel at " << ghostVox.x << " " << ghostVox.y << " " << ghostVox.z << " of type " << (int)currentBuildMaterial << std::endl;
        
        // Dirty the main block and its adjacent neighbors to ensure chunk borders update
        markChunkDirty(ghostVox.x, ghostVox.y, ghostVox.z);
        markChunkDirty(ghostVox.x - 1, ghostVox.y, ghostVox.z);
        markChunkDirty(ghostVox.x + 1, ghostVox.y, ghostVox.z);
        markChunkDirty(ghostVox.x, ghostVox.y - 1, ghostVox.z);
        markChunkDirty(ghostVox.x, ghostVox.y + 1, ghostVox.z);
        markChunkDirty(ghostVox.x, ghostVox.y, ghostVox.z - 1);
        markChunkDirty(ghostVox.x, ghostVox.y, ghostVox.z + 1);
        
        rebuildChunkSync(getChunkCoord(ghostVox.x), getChunkCoord(ghostVox.y), getChunkCoord(ghostVox.z));
        
        // Add a small recoil/shake effect for placement
        cameraShakeVel.y = std::min(cameraShakeVel.y + 0.05f, 0.15f);
    }
}
