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

static glm::vec3 mushRedColor(0.85f, 0.15f, 0.15f);
static glm::vec3 mushWhiteColor(0.95f, 0.95f, 0.95f);
static glm::vec3 mushBeigeColor(0.93f, 0.88f, 0.78f);
static glm::vec3 mushBrownColor(0.40f, 0.25f, 0.15f);
static glm::vec3 mushBlueColor(0.20f, 0.55f, 0.90f);
static glm::vec3 mushGreenColor(0.40f, 0.75f, 0.30f);

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
        case RAGDOLL_MUSH_RED:    return mushRedColor;
        case RAGDOLL_MUSH_WHITE:  return mushWhiteColor;
        case RAGDOLL_MUSH_BEIGE:  return mushBeigeColor;
        case RAGDOLL_MUSH_BROWN:  return mushBrownColor;
        case RAGDOLL_MUSH_BLUE:   return mushBlueColor;
        case RAGDOLL_MUSH_GREEN:  return mushGreenColor;
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
                v.type = outerType;
                v.unbreakable = false;
                part.voxels.push_back(v);
            }
        }
    }
}

static void addMushroomCap(RagdollPart& part, int radius) {
    for (int x = -radius; x <= radius; x++) {
        for (int y = -2; y <= radius; y++) {
            for (int z = -radius; z <= radius; z++) {
                // Ellipsoid for cap (wider than tall)
                if (x*x + (y*1.5)*(y*1.5) + z*z <= radius*radius) {
                    RagdollVoxel v;
                    v.localPos = glm::ivec3(x, y, z);
                    v.destroyed = false;
                    
                    bool isSpot = false;
                    if (y > 1 && x*x + (y*1.5)*(y*1.5) + z*z > (radius-3)*(radius-3)) {
                        float angleX = atan2(x, z);
                        float angleY = atan2(y, sqrt(x*x + z*z));
                        if (sin(angleX * 5.0) > 0.6 && sin(angleY * 4.0) > 0.6) isSpot = true;
                        // Add some random smaller spots
                        if (rand() % 40 == 0) isSpot = true;
                    }
                    
                    v.type = isSpot ? RAGDOLL_MUSH_WHITE : RAGDOLL_MUSH_RED;
                    v.unbreakable = false;
                    part.voxels.push_back(v);
                }
            }
        }
    }
}

static void addMushroomBody(RagdollPart& part, int radius, int height) {
    for (int x = -radius; x <= radius; x++) {
        for (int y = -height/2; y <= height/2; y++) {
            for (int z = -radius; z <= radius; z++) {
                float dist = sqrt(x*x + z*z);
                bool inside = false;
                if (y > 0) {
                    if (dist + y*0.3f <= radius) inside = true;
                } else {
                    if (x*x + (y*1.5)*(y*1.5) + z*z <= radius*radius) inside = true;
                }
                
                if (inside) {
                    RagdollVoxel v;
                    v.localPos = glm::ivec3(x, y, z);
                    v.destroyed = false;
                    v.type = RAGDOLL_MUSH_BEIGE;
                    v.unbreakable = false;
                    
                    // Eyes (front +z)
                    if (z >= radius - 2 && y >= height/4 - 1 && y <= height/4 + 1) {
                        if (abs(x) == 2 || abs(x) == 3) {
                            if (z == radius - 1 || z == radius) {
                                v.type = RAGDOLL_EYES;
                            }
                        }
                    }
                    part.voxels.push_back(v);
                }
            }
        }
    }
}

static void addStaff(RagdollPart& part, int xOff, int yOff, int zOff) {
    // Staff Pole
    for (int y = -8; y <= 12; y++) {
        for (int x = -1; x <= 1; x++) {
            for (int z = -1; z <= 1; z++) {
                if (x*x + z*z <= 1) {
                    RagdollVoxel v;
                    v.localPos = glm::ivec3(x + xOff, y + yOff, z + zOff);
                    v.destroyed = false;
                    v.type = RAGDOLL_MUSH_BROWN;
                    v.unbreakable = false;
                    part.voxels.push_back(v);
                }
            }
        }
    }
    // Blue Crystal
    for (int y = 13; y <= 16; y++) {
        for (int x = -2; x <= 2; x++) {
            for (int z = -2; z <= 2; z++) {
                if (abs(x) + abs(y-14) + abs(z) <= 3) {
                    RagdollVoxel v;
                    v.localPos = glm::ivec3(x + xOff, y + yOff, z + zOff);
                    v.destroyed = false;
                    v.type = RAGDOLL_MUSH_BLUE;
                    v.unbreakable = false;
                    part.voxels.push_back(v);
                }
            }
        }
    }
    // Green Leaf
    for (int x = 2; x <= 4; x++) {
        for (int y = 8; y <= 11; y++) {
            RagdollVoxel v;
            v.localPos = glm::ivec3(x + xOff, y + yOff, zOff);
            v.destroyed = false;
            v.type = RAGDOLL_MUSH_GREEN;
            v.unbreakable = false;
            part.voxels.push_back(v);
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

    // --- 1. TORSO (Mushroom Body) ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::TORSO];
        p.name = "Torso";
        p.center = position + glm::vec3(0.0f, 0.20f * scale, 0.0f);
        p.mass = 15.0f;
        p.extents = glm::vec3(0.08f, 0.10f, 0.08f) * scale;
        p.radius = 0.12f * scale;

        addMushroomBody(p, 6, 12);
    }

    // --- 2. HEAD (Mushroom Cap) ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::HEAD];
        p.name = "Head";
        p.center = position + glm::vec3(0.0f, 0.35f * scale, 0.0f);
        p.mass = 6.0f;
        p.extents = glm::vec3(0.12f, 0.06f, 0.12f) * scale;
        p.radius = 0.15f * scale;

        addMushroomCap(p, 10);
    }
    
    // PELVIS (Unused, leave empty or very small)
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::PELVIS];
        p.name = "Pelvis";
        p.center = position + glm::vec3(0.0f, 0.20f * scale, 0.0f);
        p.mass = 1.0f;
        addLayeredBox(p, 0, 0, 0, 0, 0, 0, RAGDOLL_MUSH_BEIGE, 0); // Tiny dummy
    }

    // --- LEFT ARM (Stub) ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_UPPER_ARM];
        p.name = "L_Arm";
        p.center = position + glm::vec3(-0.08f * scale, 0.20f * scale, 0.0f);
        p.mass = 2.0f;
        p.extents = glm::vec3(0.04f, 0.02f, 0.02f) * scale;
        p.radius = 0.05f * scale;
        for (int x = -4; x <= 0; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    float dx = (x + 2.0f) / 2.0f;
                    if (dx*dx + y*y + z*z <= 2.2f) {
                        RagdollVoxel v;
                        v.localPos = glm::ivec3(x - 6 + 6, y, z); // shift local center to match original
                        v.type = RAGDOLL_MUSH_BEIGE;
                        v.destroyed = false; v.unbreakable = false;
                        p.voxels.push_back(v);
                    }
                }
            }
        }
    }
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_LOWER_ARM]; // Dummy
        p.name = "L_Arm_Dummy";
        p.center = position + glm::vec3(-0.08f * scale, 0.20f * scale, 0.0f);
        p.mass = 0.5f;
        addLayeredBox(p, 0, 0, 0, 0, 0, 0, RAGDOLL_MUSH_BEIGE, 0);
    }

    // --- RIGHT ARM & STAFF ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_UPPER_ARM];
        p.name = "R_Arm_Staff";
        p.center = position + glm::vec3(0.08f * scale, 0.20f * scale, 0.0f);
        p.mass = 5.0f;
        p.extents = glm::vec3(0.04f, 0.15f, 0.04f) * scale;
        p.radius = 0.18f * scale;
        for (int x = 0; x <= 4; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    float dx = (x - 2.0f) / 2.0f;
                    if (dx*dx + y*y + z*z <= 2.2f) {
                        RagdollVoxel v;
                        v.localPos = glm::ivec3(x + 6 - 6, y, z); // shift local center to match original
                        v.type = RAGDOLL_MUSH_BEIGE;
                        v.destroyed = false; v.unbreakable = false;
                        p.voxels.push_back(v);
                    }
                }
            }
        }
        addStaff(p, 4, 0, 0); // Attach staff to the end of the stub
    }
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_LOWER_ARM]; // Dummy
        p.name = "R_Arm_Dummy";
        p.center = position + glm::vec3(0.08f * scale, 0.20f * scale, 0.0f);
        p.mass = 0.5f;
        addLayeredBox(p, 0, 0, 0, 0, 0, 0, RAGDOLL_MUSH_BEIGE, 0);
    }

    // --- LEFT LEG (Stub) ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_UPPER_LEG];
        p.name = "L_Leg";
        p.center = position + glm::vec3(-0.04f * scale, 0.10f * scale, 0.0f);
        p.mass = 3.0f;
        p.extents = glm::vec3(0.02f, 0.04f, 0.02f) * scale;
        p.radius = 0.05f * scale;
        for (int x = -2; x <= 1; x++) {
            for (int y = -4; y <= 0; y++) {
                for (int z = -1; z <= 1; z++) {
                    float dy = (y + 2.0f) / 2.0f;
                    float dx = (x + 0.5f) / 1.5f;
                    if (dx*dx + dy*dy + z*z <= 2.0f) {
                        RagdollVoxel v;
                        v.localPos = glm::ivec3(x - 2 + 1, y - 6 + 4, z); // shift local center
                        v.type = RAGDOLL_MUSH_BEIGE;
                        v.destroyed = false; v.unbreakable = false;
                        p.voxels.push_back(v);
                    }
                }
            }
        }
    }
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::L_LOWER_LEG]; // Dummy
        p.name = "L_Leg_Dummy";
        p.center = position + glm::vec3(-0.04f * scale, 0.10f * scale, 0.0f);
        p.mass = 0.5f;
        addLayeredBox(p, 0, 0, 0, 0, 0, 0, RAGDOLL_MUSH_BEIGE, 0);
    }

    // --- RIGHT LEG (Stub) ---
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_UPPER_LEG];
        p.name = "R_Leg";
        p.center = position + glm::vec3(0.04f * scale, 0.10f * scale, 0.0f);
        p.mass = 3.0f;
        p.extents = glm::vec3(0.02f, 0.04f, 0.02f) * scale;
        p.radius = 0.05f * scale;
        for (int x = -1; x <= 2; x++) {
            for (int y = -4; y <= 0; y++) {
                for (int z = -1; z <= 1; z++) {
                    float dy = (y + 2.0f) / 2.0f;
                    float dx = (x - 0.5f) / 1.5f;
                    if (dx*dx + dy*dy + z*z <= 2.0f) {
                        RagdollVoxel v;
                        v.localPos = glm::ivec3(x + 2 - 1, y - 6 + 4, z); // shift local center
                        v.type = RAGDOLL_MUSH_BEIGE;
                        v.destroyed = false; v.unbreakable = false;
                        p.voxels.push_back(v);
                    }
                }
            }
        }
    }
    {
        RagdollPart& p = ragdoll.parts[Ragdoll::R_LOWER_LEG]; // Dummy
        p.name = "R_Leg_Dummy";
        p.center = position + glm::vec3(0.04f * scale, 0.10f * scale, 0.0f);
        p.mass = 0.5f;
        addLayeredBox(p, 0, 0, 0, 0, 0, 0, RAGDOLL_MUSH_BEIGE, 0);
    }

    // Apply initial velocities
    for (auto& part : ragdoll.parts) {
        glm::vec3 jitter((rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f, (rand() % 100 - 50) * 0.005f);
        part.velocity = initialVelocity + jitter;
        part.angularVelocity = glm::vec3((rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f, (rand() % 100 - 50) * 0.04f);
        initRagdollPartMesh(part);
    }

    // --- JOINTS DEFINITION ---
    auto addJoint = [&](int parent, int child, glm::vec3 anchorP, glm::vec3 anchorC) {
        RagdollJoint j;
        j.partA = parent;
        j.partB = child;
        j.localAnchorA = anchorP * scale;
        j.localAnchorB = anchorC * scale;
        j.stiffness = ragdollStiffnessParam * 0.9f;
        j.targetDistance = 0.0f;
        ragdoll.joints.push_back(j);
    };

    // Neck: Torso top to Head bottom
    addJoint(Ragdoll::TORSO, Ragdoll::HEAD, glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0.0f, -0.02f, 0.0f));

    // Arms to Torso
    addJoint(Ragdoll::TORSO, Ragdoll::L_UPPER_ARM, glm::vec3(-0.06f, 0.0f, 0.0f), glm::vec3(0.02f, 0.0f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::R_UPPER_ARM, glm::vec3(0.06f, 0.0f, 0.0f), glm::vec3(-0.02f, 0.0f, 0.0f));

    // Legs to Torso (Pelvis is bypassed for the physical links to keep the body contiguous)
    addJoint(Ragdoll::TORSO, Ragdoll::L_UPPER_LEG, glm::vec3(-0.03f, -0.05f, 0.0f), glm::vec3(0.0f, 0.02f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::R_UPPER_LEG, glm::vec3(0.03f, -0.05f, 0.0f), glm::vec3(0.0f, 0.02f, 0.0f));

    // Tie dummy parts tightly to Torso so they don't flop around
    addJoint(Ragdoll::TORSO, Ragdoll::PELVIS, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::L_LOWER_ARM, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::R_LOWER_ARM, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::L_LOWER_LEG, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    addJoint(Ragdoll::TORSO, Ragdoll::R_LOWER_LEG, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    activeRagdolls.push_back(std::move(ragdoll));
}
