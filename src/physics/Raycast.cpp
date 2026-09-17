#include "Raycast.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include "../particles/Particles.h"

VoxelChunk* checkGrabbableChunkRaycast(const glm::vec3& startPt, const glm::vec3& rayDir, float maxReach) {
    int hitChunkIdx = -1;
    float bestChunkDist = maxReach;
    for (int ci = 0; ci < (int)activeChunks.size(); ci++) {
        const auto& c = activeChunks[ci];
        glm::vec3 toCenter = c.center - startPt;
        float projDist = glm::dot(toCenter, rayDir);
        if (projDist > 0.05f && projDist < bestChunkDist) {
            glm::vec3 closestPoint = startPt + rayDir * projDist;
            float d = glm::distance(c.center, closestPoint);
            float chunkRadius = sqrtf((float)c.voxels.size()) * voxelSize * 1.5f;
            if (chunkRadius < 0.06f) chunkRadius = 0.06f;
            if (d < chunkRadius) {
                bestChunkDist = projDist;
                hitChunkIdx = ci;
            }
        }
    }
    if (hitChunkIdx >= 0) return &activeChunks[hitChunkIdx];
    return nullptr;
}

RaycastResult performWeaponRaycast(const glm::vec3& startPt, const glm::vec3& rayDir, float maxReach, float impulseForce, float damageRadius, float damageAmount) {
    RaycastResult res;

    // 1. Raycast against active ragdolls and mobs
    for (float d = 0.15f; d <= maxReach; d += 0.015f) {
        glm::vec3 pt = startPt + rayDir * d;
        bool hitMob = damageMobAtWorldPos(pt, rayDir * impulseForce, damageRadius, damageAmount);
        bool hitRagdoll = damageRagdollAtWorldPos(pt, rayDir * impulseForce, damageRadius);
        if (hitMob || hitRagdoll) {
            res.hit = true;
            res.hitRagdoll = true;
            res.hitPos = pt;
            return res;
        }
    }

    // 2. Raycast against active loose physics chunks
    int hitChunkIdx = -1;
    float bestChunkDist = maxReach;
    for (int ci = 0; ci < (int)activeChunks.size(); ci++) {
        const auto& c = activeChunks[ci];
        glm::vec3 toCenter = c.center - startPt;
        float projDist = glm::dot(toCenter, rayDir);
        if (projDist > 0.05f && projDist < bestChunkDist) {
            glm::vec3 closestPoint = startPt + rayDir * projDist;
            float d = glm::distance(c.center, closestPoint);
            float chunkRadius = sqrtf((float)c.voxels.size()) * voxelSize * 1.5f;
            if (chunkRadius < 0.06f) chunkRadius = 0.06f;
            if (d < chunkRadius) {
                bestChunkDist = projDist;
                hitChunkIdx = ci;
            }
        }
    }

    if (hitChunkIdx >= 0) {
        auto& c = activeChunks[hitChunkIdx];
        res.hitPos = startPt + rayDir * bestChunkDist;
        res.hit = true;
        res.hitChunk = true;
        c.velocity += rayDir * (impulseForce * 0.35f) + glm::vec3(0.0f, 0.4f, 0.0f);
        c.angularVelocity += glm::vec3((rand()%20-10)*0.5f, (rand()%20-10)*0.5f, (rand()%20-10)*0.5f);
        uint8_t sampleType = c.voxels.empty() ? 1 : c.voxels[0].second;
        spawnDust(res.hitPos, 4, sampleType);
        if (sampleType == 3) spawnSparks(res.hitPos, 6);
        return res;
    }

    // 3. Raycast against static terrain
    float step = 0.002f;
    for (float d = 0.0f; d <= maxReach; d += step) {
        glm::vec3 pt = startPt + rayDir * d;
        glm::ivec3 currVox(static_cast<int>(floor(pt.x / voxelSize)), 
                           static_cast<int>(floor(pt.y / voxelSize)), 
                           static_cast<int>(floor(pt.z / voxelSize)));
        if (currVox.y >= 0 && currVox.y < WORLD_HEIGHT) {
            uint8_t t = getVoxel(currVox.x, currVox.y, currVox.z);
            if (t > 0 && t != 8) {
                res.hit = true;
                res.hitTerrain = true;
                res.hitVox = currVox;
                res.hitPos = pt;
                res.hitVType = t;
                break;
            }
        }
    }

    return res;
}

bool performPlacementRaycast(glm::vec3 startPt, glm::vec3 rayDir, float maxReach, glm::ivec3& outPlaceVox) {
    float step = 0.002f;
    glm::ivec3 lastEmptyVox(-1);
    for (float d = 0.0f; d <= maxReach; d += step) {
        glm::vec3 pt = startPt + rayDir * d;
        glm::ivec3 currVox(static_cast<int>(floor(pt.x / voxelSize)), 
                           static_cast<int>(floor(pt.y / voxelSize)), 
                           static_cast<int>(floor(pt.z / voxelSize)));
                           
        if (currVox.y >= 0 && currVox.y < WORLD_HEIGHT) {
            if (getVoxel(currVox.x, currVox.y, currVox.z) > 0) {
                if (lastEmptyVox.x != -1) {
                    outPlaceVox = lastEmptyVox;
                    return true;
                }
                return false;
            } else {
                lastEmptyVox = currVox;
            }
        }
    }
    if (lastEmptyVox.x != -1) {
        outPlaceVox = lastEmptyVox;
        return false;
    }
    return false;
}
