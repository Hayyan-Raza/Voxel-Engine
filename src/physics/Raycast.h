#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../core/Types.h"

struct RaycastResult {
    bool hit = false;
    bool hitRagdoll = false;
    bool hitChunk = false;
    bool hitTerrain = false;
    glm::vec3 hitPos{0.0f};
    glm::ivec3 hitVox{-1};
    uint8_t hitVType = 0;
};

// Returns a pointer to the closest loose physics chunk intersected by the ray
VoxelChunk* checkGrabbableChunkRaycast(const glm::vec3& startPt, const glm::vec3& rayDir, float maxReach);

// Raycasts against mobs, ragdolls, loose chunks, and terrain
RaycastResult performWeaponRaycast(const glm::vec3& startPt, const glm::vec3& rayDir, float maxReach, float impulseForce, float damageRadius);

// Special raycast for finding an empty voxel adjacent to a solid voxel
bool performPlacementRaycast(glm::vec3 startPt, glm::vec3 rayDir, float maxReach, glm::ivec3& outPlaceVox);
