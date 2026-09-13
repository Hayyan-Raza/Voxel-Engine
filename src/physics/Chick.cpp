#include "Chick.h"
#include "RagdollBuilder.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
static glm::vec3 chickFeathersColor(1.0f, 0.9f, 0.2f);
static glm::vec3 chickBeakColor(1.0f, 0.6f, 0.1f);
static glm::vec3 chickLegsColor(0.9f, 0.5f, 0.1f);

void spawnChick(const glm::vec3& position, const glm::vec3& initialVelocity) {
    Ragdoll ragdoll;
    ragdoll.id = nextRagdollId++;
    ragdoll.active = true;
    ragdoll.life = 180.0f;
    ragdoll.parts.resize(1); // Single solid part

    float scale = voxelSize / 0.02f; // Smaller overall

    RagdollPart& p = ragdoll.parts[0];
    p.name = "ChickBody";
    p.center = position;
    p.mass = 3.0f;
    // Extents for capsule shape physics (Radius 1.5, Half-height 1.5 = total height 6 voxels)
    p.extents = glm::vec3(0.015f, 0.015f, 0.015f) * scale;
    p.radius = 0.015f * scale;
    
    // Add voxels for the chick
    auto addVoxel = [&](int x, int y, int z, uint8_t type) {
        RagdollVoxel v;
        v.localPos = glm::ivec3(x, y, z);
        v.type = type;
        v.destroyed = false;
        v.unbreakable = false;
        p.voxels.push_back(v);
    };

    auto addVoxelSafe = [&](int x, int y, int z, uint8_t type) {
        for (auto& v : p.voxels) {
            if (v.localPos.x == x && v.localPos.y == y && v.localPos.z == z) {
                v.type = type;
                return;
            }
        }
        addVoxel(x, y, z, type);
    };

    auto addBox = [&](int x0, int x1, int y0, int y1, int z0, int z1, uint8_t type) {
        for (int x = x0; x <= x1; x++) {
            for (int y = y0; y <= y1; y++) {
                for (int z = z0; z <= z1; z++) {
                    addVoxelSafe(x, y, z, type);
                }
            }
        }
    };

    // Body (Yellow) - 3x3x2 block
    addBox(-1, 1, -2, -1, -1, 1, RAGDOLL_CHICK_FEATHERS);

    // Head (Yellow) - 3x3x3 block
    addBox(-1, 1, 0, 2, -1, 1, RAGDOLL_CHICK_FEATHERS);

    // Beak (Orange) - 1x1x1 block
    addBox(0, 0, 0, 0, -2, -2, RAGDOLL_CHICK_BEAK);

    // Eyes (Black) - Top front corners of the head
    addVoxelSafe(-1, 2, -1, RAGDOLL_EYES);
    addVoxelSafe(1, 2, -1, RAGDOLL_EYES);

    // Feet (Orange) - 1 voxel wide, 2 long, sticking out front
    addBox(-1, -1, -3, -3, -2, -1, RAGDOLL_CHICK_LEGS);
    addBox(1, 1, -3, -3, -2, -1, RAGDOLL_CHICK_LEGS);

    // Wings (Yellow) - 2x2 attached to the sides, towards the back
    addBox(-2, -2, -2, -1, 0, 1, RAGDOLL_CHICK_FEATHERS);
    addBox(2, 2, -2, -1, 0, 1, RAGDOLL_CHICK_FEATHERS);

    // Initialize mesh
    glm::vec3 jitter((rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f);
    // Actually C++ rand():
    // glm::vec3 jitter((rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f);
    
    // We will just replace it cleanly
    
    p.velocity = initialVelocity + glm::vec3((rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f);
    p.angularVelocity = glm::vec3((rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f);
    
    initRagdollPartMesh(p);

    activeRagdolls.push_back(std::move(ragdoll));
}
