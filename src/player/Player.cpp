#include "Player.h"
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
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>
#include <imgui.h>

// Dev Mode Blueprint State
// Previously declared static, now extern from Globals.h
// The static declarations have been removed.




void updatePhysics() {
    hammerHeadWorldPrevPos = hammerHeadWorldPos;

    // --- Recoil and Camera Shake Physics Updates (Spring-Damper) ---
    glm::vec3 recoilAccPos = -350.0f * hammerRecoilPos - 22.0f * hammerRecoilVelPos;
    hammerRecoilVelPos += recoilAccPos * deltaTime;
    hammerRecoilPos += hammerRecoilVelPos * deltaTime;

    glm::vec3 recoilAccRot = -350.0f * hammerRecoilRot - 22.0f * hammerRecoilVelRot;
    hammerRecoilVelRot += recoilAccRot * deltaTime;
    hammerRecoilRot += hammerRecoilVelRot * deltaTime;

    glm::vec3 shakeAcc = -500.0f * cameraShakeOffset - 25.0f * cameraShakeVel;
    cameraShakeVel += shakeAcc * deltaTime;
    cameraShakeOffset += cameraShakeVel * deltaTime;

    if (swingTimer > 0.0f) swingTimer -= deltaTime;

    // --- Calculate Swing Animation & Hammer Pos ---
    swingX = 0.0f; swingY = 0.0f; swingZ = 0.0f;
    swingRotX = 0.0f; swingRotY = 0.0f;
    if (swingTimer > 0.0f) {
        float f = 1.0f - (swingTimer / 0.35f);
        if (f < 0.15f) {
            float t = f / 0.15f;
            swingX   =  0.02f * t; 
            swingY   =  0.05f * t; 
            swingZ   =  0.10f * t; 
            swingRotX =  15.0f * t; 
            swingRotY = -5.0f * t;
        } else if (f < 0.45f) {
            float t = (f - 0.15f) / 0.3f;
            swingX   =  0.02f - 0.10f * t; 
            swingY   =  0.05f - 0.40f * t; 
            swingZ   =  0.10f - 0.60f * t; 
            swingRotX =  15.0f - 105.0f * t; 
            swingRotY = -5.0f + 10.0f * t;
        } else {
            float t = (f - 0.45f) / 0.55f;
            swingX   = -0.08f + 0.08f * t; 
            swingY   = -0.35f + 0.35f * t; 
            swingZ   = -0.50f + 0.50f * t; 
            swingRotX = -90.0f + 90.0f * t; 
            swingRotY =   5.0f -  5.0f * t;
        }
    }

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    glm::vec3 up = glm::normalize(glm::cross(right, cameraFront));
    
    float swayX = sinf(hammerSwayTimer * 1.5f) * 0.015f;
    float swayY = cosf(hammerSwayTimer * 2.0f) * 0.012f;
    float lagX = hammerOffset.x * 0.015f;
    float lagY = hammerOffset.y * 0.015f;
    float jumpSway = -playerVelocityY * 0.02f;

    float scaleFactor = voxelSize / 0.01f;

    float cx = hammerBasePos.x + swayX + lagX + swingX + hammerRecoilPos.x;
    float cy = hammerBasePos.y + swayY + lagY + swingY + jumpSway + hammerRecoilPos.y;
    float cz = hammerBasePos.z + swingZ + hammerRecoilPos.z;

    hammerHeadWorldPos = cameraPos 
        + right * cx 
        + up * cy 
        - cameraFront * cz 
        + cameraFront * (0.4f * scaleFactor);
        
    float baseRotationY = hammerBaseRot.x + hammerOffset.x * 0.5f + swingRotY + hammerRecoilRot.y;
    float baseRotationX = hammerBaseRot.y + swingRotX + hammerRecoilRot.x;
    
    glm::mat4 rotMatrix = glm::mat4(1.0f);
    rotMatrix = glm::rotate(rotMatrix, glm::radians(baseRotationX), right);
    rotMatrix = glm::rotate(rotMatrix, glm::radians(baseRotationY), up);
    glm::vec3 headReachDir = glm::vec3(rotMatrix * glm::vec4(up, 0.0f));
    
    hammerHeadWorldPos += headReachDir * (0.8f * scaleFactor);

    const float playerHeight = 0.25f * scaleFactor;

    // --- Gravity & Grounding (skip in inspector/spectator mode) ---
    if (!inspectorMode && !spectatorMode) {
        int px = static_cast<int>(round(cameraPos.x / voxelSize));
        int py = static_cast<int>(round(cameraPos.y / voxelSize));
        int pz = static_cast<int>(round(cameraPos.z / voxelSize));
        int py_feet = static_cast<int>(round((cameraPos.y - playerHeight) / voxelSize));
        int py_legs = static_cast<int>(round((cameraPos.y - playerHeight * 0.5f) / voxelSize));
        bool inWater = false;
        if (px >= 0 && px < GRID_SIZE && pz >= 0 && pz < GRID_SIZE) {
            if ((py_feet >= 0 && py_feet < GRID_SIZE && getVoxel(px, py_feet, pz) == 8) ||
                (py_legs >= 0 && py_legs < GRID_SIZE && getVoxel(px, py_legs, pz) == 8) ||
                (py >= 0 && py < GRID_SIZE && getVoxel(px, py, pz) == 8)) {
                inWater = true;
            }
        }

        if (inWater) {
            // Natural buoyancy (pushes player up to surface)
            if (playerVelocityY < 1.0f * scaleFactor) {
                playerVelocityY += (GRAVITY * scaleFactor * 0.2f) * deltaTime;
            }
            // Terminal sinking velocity
            if (playerVelocityY < -1.0f * scaleFactor) playerVelocityY = -1.0f * scaleFactor;
            
            // Swim up
            if (g_window && glfwGetKey(g_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                playerVelocityY += (GRAVITY * scaleFactor * 0.5f) * deltaTime;
                if (playerVelocityY > 3.0f * scaleFactor) playerVelocityY = 3.0f * scaleFactor;
                isGrounded = false;
            }
            
            // Apply water drag to horizontal movement
            playerVelocityH *= 0.90f; 
        } else {
            // Normal gravity
            playerVelocityY -= (GRAVITY * scaleFactor) * deltaTime;
        }

        cameraPos.y     += playerVelocityY * deltaTime;

        float physHitY = -1.0f;
        float voxelHitY = -1.0f;
        int gx = static_cast<int>(round(cameraPos.x / voxelSize));
        int gz = static_cast<int>(round(cameraPos.z / voxelSize));
        int currentGroundType = 0;
        if (gx >= 0 && gx < GRID_SIZE && gz >= 0 && gz < GRID_SIZE) {
            int startY = std::min(GRID_SIZE - 1, static_cast<int>(floor((cameraPos.y - 0.02f) / voxelSize)));
            for (int gy = startY; gy >= 0; gy--) {
                if (getVoxel(gx, gy, gz) > 0 && getVoxel(gx, gy, gz) != 8) { // Ignore water for grounding
                    currentGroundType = getVoxel(gx, gy, gz);
                    voxelHitY = gy * voxelSize + voxelSize * 0.5f;
                    if (voxelHitY < cameraPos.y - playerHeight + 0.05f) break;
                    break; 
                }
            }
        }

        float finalHitY = std::max(physHitY, voxelHitY);
        isGrounded = false;
        
        if (finalHitY > -0.5f) {
            if (playerVelocityY <= 0.0f && cameraPos.y <= finalHitY + playerHeight + 0.02f) {
                isGrounded = true;
                cameraPos.y = finalHitY + playerHeight;
                playerVelocityY = 0.0f;
            }
        } else if (cameraPos.y < playerHeight) {
            cameraPos.y = playerHeight;
            isGrounded = true;
            playerVelocityY = 0.0f;
        }
        
        if (isGrounded && glm::length(playerVelocityH) > 0.1f) {
            uint8_t gt = currentGroundType;
            if (gt == 1 || gt == 2 || gt == 5 || gt >= 9 && gt <= 14 || gt == 17) {
                playFootstep();
            } else {
                stopFootstep();
            }
        } else {
            stopFootstep();
        }

        // Water audio spatialization (approximate by using height relative to seaLevel)
        float distToWater = std::abs((cameraPos.y / voxelSize) - 36.0f);
        float waterVol = 0.0f;
        // if (distToWater < 30.0f) { // Within ~30 blocks of seaLevel
        //     waterVol = (1.0f - (distToWater / 30.0f)) * 0.5f; // Max volume 0.5
        // }
        setWaterVolume(0.0f);
    }

    // --- Sync Kinematic Body ---
    if (playerRigidBody) {
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(cameraPos.x, cameraPos.y - playerHeight*0.6f, cameraPos.z));
        playerRigidBody->getMotionState()->setWorldTransform(trans);
    }

    hammerHitThisFrame = false;

    if (hammerRigidBody) {
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(hammerHeadWorldPos.x, hammerHeadWorldPos.y, hammerHeadWorldPos.z));
        hammerRigidBody->getMotionState()->setWorldTransform(trans);
        hammerRigidBody->setWorldTransform(trans);

        if (destructionPending && (swingTimer > 0.0f && swingTimer < 0.16f)) {
            btVector3 startPos(hammerHeadWorldPrevPos.x, hammerHeadWorldPrevPos.y, hammerHeadWorldPrevPos.z);
            btVector3 endPos(hammerHeadWorldPos.x, hammerHeadWorldPos.y, hammerHeadWorldPos.z);
            
            btVector3 sweepDir = endPos - startPos;
            if (sweepDir.length2() < 0.001f) {
                sweepDir = btVector3(cameraFront.x, cameraFront.y, cameraFront.z);
                startPos -= sweepDir * 0.5f;
            }

            btCollisionWorld::ClosestConvexResultCallback sweepCallback(startPos, endPos);
            btTransform startTrans; startTrans.setIdentity(); startTrans.setOrigin(startPos);
            btTransform endTrans; endTrans.setIdentity(); endTrans.setOrigin(endPos);

            btBoxShape* shape = static_cast<btBoxShape*>(hammerShape);
            dynamicsWorld->convexSweepTest(shape, startTrans, endTrans, sweepCallback);

            if (sweepCallback.hasHit() && sweepCallback.m_hitCollisionObject != playerRigidBody) {
                hammerHitThisFrame = true;
                hammerHitWorldPos = glm::vec3(sweepCallback.m_hitPointWorld.x(), sweepCallback.m_hitPointWorld.y(), sweepCallback.m_hitPointWorld.z());
                hammerHitWorldNormal = glm::vec3(sweepCallback.m_hitNormalWorld.x(), sweepCallback.m_hitNormalWorld.y(), sweepCallback.m_hitNormalWorld.z());
            }
        }
    }

    // --- Player Pushes Active Loose Chunks ---
    float playerRadius = 0.08f; 
    float playerHeightHalf = 0.15f * 0.5f;
    glm::vec3 playerFeetPos = cameraPos - glm::vec3(0.0f, playerHeight, 0.0f);
    glm::vec3 playerCenterPos = playerFeetPos + glm::vec3(0.0f, playerHeightHalf, 0.0f);

    for (auto& c : activeChunks) {
        float chunkRadius = sqrtf((float)c.voxels.size()) * voxelSize * 0.5f;
        if (chunkRadius < 0.04f) chunkRadius = 0.04f;

        glm::vec3 diff = c.center - playerCenterPos;
        float hDist = glm::length(glm::vec2(diff.x, diff.z));
        float vDist = fabsf(diff.y);

        float minHDist = playerRadius + chunkRadius;
        float minVDist = playerHeightHalf + chunkRadius;

        if (hDist < minHDist && vDist < minVDist) {
            glm::vec3 pushDir(0.0f);
            if (hDist > 0.001f) {
                pushDir = glm::normalize(glm::vec3(diff.x, 0.0f, diff.z));
            } else {
                pushDir = glm::vec3((rand()%200-100)*0.01f, 0.0f, (rand()%200-100)*0.01f);
                if (glm::length(pushDir) > 0.01f) pushDir = glm::normalize(pushDir);
            }
            pushDir.y = 0.25f;
            pushDir = glm::normalize(pushDir);

            float penetration = minHDist - hDist;
            float force = penetration * 8.0f;
            
            c.velocity += pushDir * force;
            c.angularVelocity += glm::vec3((rand()%20-10)*0.3f, (rand()%20-10)*0.3f, (rand()%20-10)*0.3f);
        }
    }

    pushRagdollsWithPlayer(cameraPos, 0.12f, playerHeight);
}





#include <iostream>
