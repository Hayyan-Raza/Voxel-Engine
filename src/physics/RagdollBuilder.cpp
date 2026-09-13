#include "RagdollBuilder.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../world/GreedyMesher.h"
#include "../rendering/Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glad/gl.h>
// Color Palette for Multi-Layer Ragdoll
static glm::vec3 boneColor       (0.92f, 0.90f, 0.85f); // Unbreakable white bone
static glm::vec3 fleshColor      (0.75f, 0.10f, 0.12f); // Deep blood red muscle flesh
static glm::vec3 skinColor       (0.95f, 0.78f, 0.62f); // Natural skin tone
static glm::vec3 shirtColor      (0.20f, 0.50f, 0.70f); // Cool cyan-blue shirt
static glm::vec3 pantsColor      (0.22f, 0.25f, 0.30f); // Slate grey pants
static glm::vec3 shoeColor       (0.12f, 0.10f, 0.08f); // Shoes
static glm::vec3 eyeColor        (0.05f, 0.05f, 0.05f); // Facial features

static glm::vec3 chickFeathersColor(1.0f, 0.9f, 0.2f);
static glm::vec3 chickBeakColor(1.0f, 0.6f, 0.1f);
static glm::vec3 chickLegsColor(0.9f, 0.5f, 0.1f);

static glm::vec3 getRagdollVoxelColor(uint8_t type) {
    switch (type) {
        case RAGDOLL_BONE:        return boneColor;
        case RAGDOLL_FLESH:       return fleshColor;
        case RAGDOLL_SKIN:        return skinColor;
        case RAGDOLL_CLOTH_SHIRT: return shirtColor;
        case RAGDOLL_CLOTH_PANTS: return pantsColor;
        case RAGDOLL_SHOES:       return shoeColor;
        case RAGDOLL_EYES:        return eyeColor;
        case RAGDOLL_CHICK_FEATHERS: return chickFeathersColor;
        case RAGDOLL_CHICK_BEAK:  return chickBeakColor;
        case RAGDOLL_CHICK_LEGS:  return chickLegsColor;
        default:                  return skinColor;
    }
}

void initRagdollPartMesh(RagdollPart& part) {
    if (part.voxels.empty()) return;
    cleanupRagdollPartMesh(part);

    std::vector<VoxelVertex> verts;
    float vs = voxelSize;

    int minX = 9999, maxX = -9999;
    int minY = 9999, maxY = -9999;
    int minZ = 9999, maxZ = -9999;
    for (const auto& v : part.voxels) {
        if (!v.destroyed) {
            minX = std::min(minX, v.localPos.x); maxX = std::max(maxX, v.localPos.x);
            minY = std::min(minY, v.localPos.y); maxY = std::max(maxY, v.localPos.y);
            minZ = std::min(minZ, v.localPos.z); maxZ = std::max(maxZ, v.localPos.z);
        }
    }
    
    int dx = std::max(1, maxX - minX + 1);
    int dy = std::max(1, maxY - minY + 1);
    int dz = std::max(1, maxZ - minZ + 1);
    
    std::vector<bool> voxelGridMap(dx * dy * dz, false);
    for (const auto& v : part.voxels) {
        if (!v.destroyed) {
            int ix = v.localPos.x - minX;
            int iy = v.localPos.y - minY;
            int iz = v.localPos.z - minZ;
            voxelGridMap[ix + iy * dx + iz * dx * dy] = true;
        }
    }

    auto hasVoxel = [&](int x, int y, int z) -> bool {
        int ix = x - minX;
        int iy = y - minY;
        int iz = z - minZ;
        if (ix < 0 || ix >= dx || iy < 0 || iy >= dy || iz < 0 || iz >= dz) return false;
        return voxelGridMap[ix + iy * dx + iz * dx * dy];
    };

    for (const auto& vox : part.voxels) {
        if (vox.destroyed) continue;

        glm::vec3 col = getRagdollVoxelColor(vox.type);
        glm::vec3 voxCenter = glm::vec3(vox.localPos) * vs;

        for (int i = 0; i < 36; i++) {
            float vx = unitCubeVertices[i * 9 + 0];
            float vy = unitCubeVertices[i * 9 + 1];
            float vz = unitCubeVertices[i * 9 + 2];
            float nx = unitCubeVertices[i * 9 + 6];
            float ny = unitCubeVertices[i * 9 + 7];
            float nz = unitCubeVertices[i * 9 + 8];

            int d1x = 0, d1y = 0, d1z = 0;
            int d2x = 0, d2y = 0, d2z = 0;

            if (nx != 0.0f) {
                d1y = (vy > 0.0f ? 1 : -1);
                d2z = (vz > 0.0f ? 1 : -1);
            } else if (ny != 0.0f) {
                d1x = (vx > 0.0f ? 1 : -1);
                d2z = (vz > 0.0f ? 1 : -1);
            } else if (nz != 0.0f) {
                d1x = (vx > 0.0f ? 1 : -1);
                d2y = (vy > 0.0f ? 1 : -1);
            }

            int fx = vox.localPos.x + (int)nx;
            int fy = vox.localPos.y + (int)ny;
            int fz = vox.localPos.z + (int)nz;

            bool s1 = hasVoxel(fx + d1x, fy + d1y, fz + d1z);
            bool s2 = hasVoxel(fx + d2x, fy + d2y, fz + d2z);
            bool cr = hasVoxel(fx + d1x + d2x, fy + d1y + d2y, fz + d1z + d2z);

            float ao = 1.0f;
            if (s1 && s2) ao = 0.0f;
            else ao = 1.0f - (s1 + s2 + cr) * 0.25f;

            verts.push_back({
                
                voxCenter.x + vx * vs, 
                voxCenter.y + vy * vs, 
                voxCenter.z + vz * vs,
                (uint8_t)(
                col.r * 255.0f), (uint8_t)( col.g * 255.0f), (uint8_t)( col.b * 255.0f),
                (int8_t)(
                nx * 127.0f), (int8_t)( ny * 127.0f), (int8_t)( nz * 127.0f),
                0, (uint8_t)0,
                
                ao
            
            });
        }
    }

    if (verts.empty()) return;

    glGenVertexArrays(1, &part.VAO);
    glGenBuffers(1, &part.VBO);
    glBindVertexArray(part.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, part.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VoxelVertex), verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
    glEnableVertexAttribArray(3);

    part.vertexCount = (int)verts.size();
    glBindVertexArray(0);
}

void cleanupRagdollPartMesh(RagdollPart& part) {
    if (part.VAO) { glDeleteVertexArrays(1, &part.VAO); part.VAO = 0; }
    if (part.VBO) { glDeleteBuffers(1, &part.VBO); part.VBO = 0; }
    part.vertexCount = 0;
}

static void addLayeredBox(RagdollPart& part, 
                          int x0, int x1, int y0, int y1, int z0, int z1, 
                          uint8_t outerType, int boneRadius = 1) {
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            for (int z = z0; z <= z1; z++) {
                RagdollVoxel v;
                v.localPos = glm::ivec3(x, y, z);
                v.destroyed = false;

                int distFromCenterX = std::abs(x);
                int distFromCenterZ = std::abs(z);
                bool isOuterBorder = (x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1);

                if (distFromCenterX <= boneRadius && distFromCenterZ <= boneRadius) {
                    v.type = RAGDOLL_BONE;
                    v.unbreakable = true;
                } else if (!isOuterBorder) {
                    v.type = RAGDOLL_FLESH;
                    v.unbreakable = false;
                } else {
                    v.type = outerType;
                    v.unbreakable = false;
                }

                part.voxels.push_back(v);
            }
        }
    }
}

void spawnRagdoll(const glm::vec3& position, const glm::vec3& initialVelocity) {
    Ragdoll ragdoll;
    ragdoll.id = nextRagdollId++;
    ragdoll.active = true;
    ragdoll.life = 180.0f;
    ragdoll.parts.resize(Ragdoll::PART_COUNT);

    float scale = voxelSize / 0.01f;

    // --- 1. TORSO --- (Heavy mass for realistic body weight)
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::TORSO];
        p.name = "Torso";
        p.center = position + glm::vec3(0.0f, 0.35f * scale, 0.0f);
        p.mass = 12.0f;
        p.extents = glm::vec3(0.08f, 0.10f, 0.06f) * scale;
        p.radius = 0.15f * scale;

        addLayeredBox(p, -4, 4, -5, 5, -3, 3, RAGDOLL_CLOTH_SHIRT, 1);
    }

    // --- 2. HEAD ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::HEAD];
        p.name = "Head";
        p.center = position + glm::vec3(0.0f, 0.43f * scale, 0.0f);
        p.mass = 4.0f;
        p.extents = glm::vec3(0.05f, 0.05f, 0.05f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -3, 3, -3, 3, -3, 3, RAGDOLL_SKIN, 1);

        for (auto& v : p.voxels) {
            if (v.localPos.z == 3) {
                if ((v.localPos.x == -1 || v.localPos.x == 1) && v.localPos.y == 0) {
                    v.type = RAGDOLL_EYES;
                } else if ((v.localPos.x >= -1 && v.localPos.x <= 1) && v.localPos.y == -2) {
                    v.type = RAGDOLL_EYES;
                }
            }
        }
    }

    // --- 3. PELVIS ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::PELVIS];
        p.name = "Pelvis";
        p.center = position + glm::vec3(0.0f, 0.28f * scale, 0.0f);
        p.mass = 8.0f;
        p.extents = glm::vec3(0.07f, 0.04f, 0.05f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -4, 4, -2, 2, -3, 3, RAGDOLL_CLOTH_PANTS, 1);
    }

    // --- 4. LEFT UPPER ARM ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_UPPER_ARM];
        p.name = "L_UpperArm";
        p.center = position + glm::vec3(-0.04f * scale, 0.35f * scale, 0.0f);
        p.mass = 3.5f;
        p.extents = glm::vec3(0.03f, 0.06f, 0.03f) * scale;
        p.radius = 0.09f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_CLOTH_SHIRT, 0);
    }

    // --- 5. LEFT LOWER ARM ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_LOWER_ARM];
        p.name = "L_LowerArm";
        p.center = position + glm::vec3(-0.04f * scale, 0.27f * scale, 0.0f);
        p.mass = 2.8f;
        p.extents = glm::vec3(0.03f, 0.06f, 0.03f) * scale;
        p.radius = 0.09f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_SKIN, 0);
    }

    // --- 6. RIGHT UPPER ARM ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_UPPER_ARM];
        p.name = "R_UpperArm";
        p.center = position + glm::vec3(0.04f * scale, 0.35f * scale, 0.0f);
        p.mass = 3.5f;
        p.extents = glm::vec3(0.03f, 0.06f, 0.03f) * scale;
        p.radius = 0.09f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_CLOTH_SHIRT, 0);
    }

    // --- 7. RIGHT LOWER ARM ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_LOWER_ARM];
        p.name = "R_LowerArm";
        p.center = position + glm::vec3(0.04f * scale, 0.27f * scale, 0.0f);
        p.mass = 2.8f;
        p.extents = glm::vec3(0.03f, 0.06f, 0.03f) * scale;
        p.radius = 0.09f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_SKIN, 0);
    }

    // --- 8. LEFT UPPER LEG ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_UPPER_LEG];
        p.name = "L_UpperLeg";
        p.center = position + glm::vec3(-0.03f * scale, 0.22f * scale, 0.0f);
        p.mass = 5.5f;
        p.extents = glm::vec3(0.03f, 0.07f, 0.03f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_CLOTH_PANTS, 0);
    }

    // --- 9. LEFT LOWER LEG ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_LOWER_LEG];
        p.name = "L_LowerLeg";
        p.center = position + glm::vec3(-0.03f * scale, 0.14f * scale, 0.0f);
        p.mass = 4.5f;
        p.extents = glm::vec3(0.03f, 0.07f, 0.03f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -2, 2, -4, 2, -2, 2, RAGDOLL_CLOTH_PANTS, 0);
        addLayeredBox(p, -2, 2, -5, -3, -2, 3, RAGDOLL_SHOES, 0);
    }

    // --- 10. RIGHT UPPER LEG ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_UPPER_LEG];
        p.name = "R_UpperLeg";
        p.center = position + glm::vec3(0.03f * scale, 0.22f * scale, 0.0f);
        p.mass = 5.5f;
        p.extents = glm::vec3(0.03f, 0.07f, 0.03f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -2, 2, -4, 4, -2, 2, RAGDOLL_CLOTH_PANTS, 0);
    }

    // --- 11. RIGHT LOWER LEG ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_LOWER_LEG];
        p.name = "R_LowerLeg";
        p.center = position + glm::vec3(0.03f * scale, 0.14f * scale, 0.0f);
        p.mass = 4.5f;
        p.extents = glm::vec3(0.03f, 0.07f, 0.03f) * scale;
        p.radius = 0.10f * scale;

        addLayeredBox(p, -2, 2, -4, 2, -2, 2, RAGDOLL_CLOTH_PANTS, 0);
        addLayeredBox(p, -2, 2, -5, -3, -2, 3, RAGDOLL_SHOES, 0);
    }

    // Apply initial velocities
    for (auto& part : ragdoll.parts) {
        glm::vec3 jitter((rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f);
        part.velocity = initialVelocity + jitter;
        part.angularVelocity = glm::vec3((rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f);
        initRagdollPartMesh(part);
    }

    // --- JOINTS DEFINITION (Zero Target Distance) ---
    auto addJoint = [&](int parent, int child, glm::vec3 anchorP, glm::vec3 anchorC) {
        RagdollJoint j;
        j.partA = parent;
        j.partB = child;
        j.localAnchorA = anchorP * scale;
        j.localAnchorB = anchorC * scale;
        j.stiffness = ragdollStiffnessParam;
        j.targetDistance = 0.0f;
        ragdoll.joints.push_back(j);
    };

    // Neck: Torso top to Head bottom
    addJoint(Ragdoll::TORSO, Ragdoll::HEAD, glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(0.0f, -0.03f, 0.0f));

    // Spine: Torso bottom to Pelvis top
    addJoint(Ragdoll::TORSO, Ragdoll::PELVIS, glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.0f, 0.02f, 0.0f));

    // Left Shoulder: Torso L-shoulder to L_UpperArm top
    addJoint(Ragdoll::TORSO, Ragdoll::L_UPPER_ARM, glm::vec3(-0.04f, 0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Left Elbow: L_UpperArm bottom to L_LowerArm top
    addJoint(Ragdoll::L_UPPER_ARM, Ragdoll::L_LOWER_ARM, glm::vec3(0.0f, -0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Right Shoulder: Torso R-shoulder to R_UpperArm top
    addJoint(Ragdoll::TORSO, Ragdoll::R_UPPER_ARM, glm::vec3(0.04f, 0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Right Elbow: R_UpperArm bottom to R_LowerArm top
    addJoint(Ragdoll::R_UPPER_ARM, Ragdoll::R_LOWER_ARM, glm::vec3(0.0f, -0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Left Hip: Pelvis L-hip to L_UpperLeg top
    addJoint(Ragdoll::PELVIS, Ragdoll::L_UPPER_LEG, glm::vec3(-0.03f, -0.02f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Left Knee: L_UpperLeg bottom to L_LowerLeg top
    addJoint(Ragdoll::L_UPPER_LEG, Ragdoll::L_LOWER_LEG, glm::vec3(0.0f, -0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Right Hip: Pelvis R-hip to R_UpperLeg top
    addJoint(Ragdoll::PELVIS, Ragdoll::R_UPPER_LEG, glm::vec3(0.03f, -0.02f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    // Right Knee: R_UpperLeg bottom to R_LowerLeg top
    addJoint(Ragdoll::R_UPPER_LEG, Ragdoll::R_LOWER_LEG, glm::vec3(0.0f, -0.04f, 0.0f), glm::vec3(0.0f, 0.04f, 0.0f));

    activeRagdolls.push_back(std::move(ragdoll));
}
