#include "Mob.h"
#include "Physics.h"
#include "RagdollBuilder.h"
#include "../core/Globals.h"
#include <iostream>
#include <glm/gtx/norm.hpp>
#include "../world/World.h"
#include <cmath>

std::vector<Mob*> activeMobs;
static int nextMobId = 1;

void spawnLivingChick(const glm::vec3& position) {
    Mob* mob = new Mob();
    mob->id = nextMobId++;
    mob->position = position;
    mob->health = 100.0f;

    float scale = voxelSize / 0.01f;

    mob->renderParts.resize(1);
    RagdollPart& p = mob->renderParts[0];
    p.name = "LivingChick";
    p.center = position;
    p.mass = 30.0f; // Increased mass so it doesn't go flying when pushed
    // Extents for capsule shape physics (Radius 1.5, Half-height 1.5 = total height 6 voxels)
    p.extents = glm::vec3(0.015f, 0.015f, 0.015f) * scale;
    
    // Add voxels for the smaller chick
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

    initRagdollPartMesh(p);
    
    mob->velocity = glm::vec3(0.0f);

    activeMobs.push_back(mob);
}

void spawnLivingMushroom(const glm::vec3& position) {
    Mob* mob = new Mob();
    mob->id = nextMobId++;
    mob->position = position;
    mob->health = 150.0f;
    mob->type = MOB_MUSHROOM;

    float scale = voxelSize / 0.01f;

    mob->renderParts.resize(5);
    for(int i=0; i<5; i++) {
        mob->renderParts[i].name = "MushroomPart";
        mob->renderParts[i].center = position;
        mob->renderParts[i].mass = 10.0f;
    }
    mob->renderParts[0].extents = glm::vec3(0.06f, 0.10f, 0.06f) * scale;
    mob->renderParts[1].extents = glm::vec3(0.02f, 0.06f, 0.02f) * scale;
    mob->renderParts[2].extents = glm::vec3(0.02f, 0.06f, 0.02f) * scale;
    mob->renderParts[3].extents = glm::vec3(0.02f, 0.04f, 0.02f) * scale;
    mob->renderParts[4].extents = glm::vec3(0.02f, 0.04f, 0.02f) * scale;

    auto addVoxelSafe = [&](int partIdx, int x, int y, int z, uint8_t type) {
        RagdollPart& p = mob->renderParts[partIdx];
        for (auto& v : p.voxels) {
            if (v.localPos.x == x && v.localPos.y == y && v.localPos.z == z) {
                v.type = type;
                return;
            }
        }
        RagdollVoxel v;
        v.localPos = glm::ivec3(x, y, z);
        v.type = type;
        v.destroyed = false;
        v.unbreakable = false;
        p.voxels.push_back(v);
    };

    // --- Mushroom Body (same as ragdoll version) ---
    int bodyRadius = 6;
    int bodyHeight = 12;
    for (int x = -bodyRadius; x <= bodyRadius; x++) {
        for (int y = -bodyHeight/2; y <= bodyHeight/2; y++) {
            for (int z = -bodyRadius; z <= bodyRadius; z++) {
                float dist = sqrt((float)(x*x + z*z));
                bool inside = false;
                if (y > 0) {
                    if (dist + y*0.3f <= bodyRadius) inside = true;
                } else {
                    if (x*x + (y*1.5f)*(y*1.5f) + z*z <= bodyRadius*bodyRadius) inside = true;
                }
                if (inside) {
                    uint8_t vtype = RAGDOLL_MUSH_BEIGE;
                    // Eyes
                    if (z >= bodyRadius - 2 && y >= bodyHeight/4 - 1 && y <= bodyHeight/4 + 1) {
                        if (abs(x) == 2 || abs(x) == 3) {
                            if (z == bodyRadius - 1 || z == bodyRadius) {
                                vtype = RAGDOLL_EYES;
                            }
                        }
                    }
                    addVoxelSafe(0, x, y, z, vtype);
                }
            }
        }
    }

    // --- Mushroom Cap (same as ragdoll version) ---
    int capRadius = 10;
    int capYOff = 10; // Offset above body
    for (int x = -capRadius; x <= capRadius; x++) {
        for (int y = -2; y <= capRadius; y++) {
            for (int z = -capRadius; z <= capRadius; z++) {
                if (x*x + (y*1.5f)*(y*1.5f) + z*z <= capRadius*capRadius) {
                    bool isSpot = false;
                    if (y > 1 && x*x + (y*1.5f)*(y*1.5f) + z*z > (capRadius-3)*(capRadius-3)) {
                        float angleX = atan2((float)x, (float)z);
                        float angleY = atan2((float)y, sqrt((float)(x*x + z*z)));
                        if (sin(angleX * 5.0f) > 0.6f && sin(angleY * 4.0f) > 0.6f) isSpot = true;
                        if (rand() % 40 == 0) isSpot = true;
                    }
                    addVoxelSafe(0, x, y + capYOff, z, isSpot ? RAGDOLL_MUSH_WHITE : RAGDOLL_MUSH_RED);
                }
            }
        }
    }

    // --- Rounded stub arms (small ellipsoids instead of cubes) ---
    // Left arm (part 1)
    for (int x = -4; x <= 0; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                float dx = (x + 2.0f) / 2.0f;
                if (dx*dx + y*y + z*z <= 2.2f) {
                    addVoxelSafe(1, x, y, z, RAGDOLL_MUSH_BEIGE);
                }
            }
        }
    }
    // Right arm (part 2)
    for (int x = 0; x <= 4; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                float dx = (x - 2.0f) / 2.0f;
                if (dx*dx + y*y + z*z <= 2.2f) {
                    addVoxelSafe(2, x, y, z, RAGDOLL_MUSH_BEIGE);
                }
            }
        }
    }

    // --- Staff (attached to right arm, part 2) ---
    int staffX = 3; // Relative to right arm center
    for (int y = -8; y <= 12; y++) {
        for (int sx = -1; sx <= 1; sx++) {
            for (int sz = -1; sz <= 1; sz++) {
                if (sx*sx + sz*sz <= 1) {
                    addVoxelSafe(2, sx + staffX, y, sz, RAGDOLL_MUSH_BROWN);
                }
            }
        }
    }
    // Blue crystal on top
    for (int y = 13; y <= 16; y++) {
        for (int x = -2; x <= 2; x++) {
            for (int z = -2; z <= 2; z++) {
                if (abs(x) + abs(y-14) + abs(z) <= 3) {
                    addVoxelSafe(2, x + staffX, y, z, RAGDOLL_MUSH_BLUE);
                }
            }
        }
    }
    // Green leaf
    for (int x = 2; x <= 4; x++) {
        for (int y = 8; y <= 11; y++) {
            addVoxelSafe(2, x + staffX, y, 0, RAGDOLL_MUSH_GREEN);
        }
    }

    // --- Rounded stub legs (small ellipsoids) ---
    // Left leg (part 3)
    for (int x = -2; x <= 1; x++) {
        for (int y = -4; y <= 0; y++) {
            for (int z = -1; z <= 1; z++) {
                float dy = (y + 2.0f) / 2.0f;
                float dx = (x + 0.5f) / 1.5f;
                if (dx*dx + dy*dy + z*z <= 2.0f) {
                    addVoxelSafe(3, x, y, z, RAGDOLL_MUSH_BEIGE);
                }
            }
        }
    }
    // Right leg (part 4)
    for (int x = -1; x <= 2; x++) {
        for (int y = -4; y <= 0; y++) {
            for (int z = -1; z <= 1; z++) {
                float dy = (y + 2.0f) / 2.0f;
                float dx = (x - 0.5f) / 1.5f;
                if (dx*dx + dy*dy + z*z <= 2.0f) {
                    addVoxelSafe(4, x, y, z, RAGDOLL_MUSH_BEIGE);
                }
            }
        }
    }

    for (int i=0; i<5; i++) {
        initRagdollPartMesh(mob->renderParts[i]);
    }
    mob->velocity = glm::vec3(0.0f);

    activeMobs.push_back(mob);
}

void updateMobs(float dt) {
    for (size_t i = 0; i < activeMobs.size();) {
        Mob* m = activeMobs[i];
        if (!m->active) {
            for (auto& p : m->renderParts) {
                cleanupRagdollPartMesh(p);
            }
            delete m;
            activeMobs.erase(activeMobs.begin() + i);
            continue;
        }

        // Wandering AI & Panic AI
        if (m->panicTimer > 0.0f) {
            m->panicTimer -= dt;
            m->wanderTimer -= dt * 3.0f; // Switch directions much faster when panicking
        } else {
            m->wanderTimer -= dt;
        }

        if (m->wanderTimer <= 0.0f) {
            m->wanderTimer = 1.5f + (rand() % 200) / 100.0f; // 1.5 to 3.5 seconds
            
            // Random direction
            float randAngle = (rand() % 360) * 3.14159f / 180.0f;
            m->yaw = randAngle;
        }

        float scaleFactor = voxelSize / 0.01f;

        // Apply Gravity
        m->velocity.y -= (GRAVITY * scaleFactor) * dt;

        // Apply friction
        m->velocity.x *= 0.8f;
        m->velocity.z *= 0.8f;

        // Minecraft chicken slow fall!
        if (m->velocity.y < -3.0f * scaleFactor) {
            m->velocity.y = -3.0f * scaleFactor;
        }

        if (m->wanderTimer > 1.0f || m->panicTimer > 0.0f) {
            // Walking part of the cycle
            float speed = (m->panicTimer > 0.0f) ? 5.0f : 2.0f; // Walk speed or run speed
            m->velocity.x = sin(m->yaw) * speed;
            m->velocity.z = cos(m->yaw) * speed;
        }

        // Horizontal movement first
        glm::vec3 nextCenter = m->position;
        nextCenter.x += m->velocity.x * dt;
        nextCenter.z += m->velocity.z * dt;
        glm::quat nextRot = glm::angleAxis(m->yaw, glm::vec3(0, 1, 0));
        glm::vec3 colNormal(0.0f);

        if (!checkPartVoxelCollision(m->renderParts[0], nextCenter, nextRot, colNormal)) {
            m->position.x = nextCenter.x;
            m->position.z = nextCenter.z;
        }

        // Vertical movement
        nextCenter = m->position;
        nextCenter.y += m->velocity.y * dt;
        if (checkPartVoxelCollision(m->renderParts[0], nextCenter, nextRot, colNormal)) {
            if (m->velocity.y < 0.0f) m->velocity.y = 0.0f; // hit ground
        } else {
            m->position.y = nextCenter.y;
        }

        // Sync render part(s)
        m->renderParts[0].center = m->position;
        
        // Animation
        m->animTime += dt;
        
        if (m->type == MOB_MUSHROOM && m->renderParts.size() == 5) {
            // Cute bobbing animation
            float bobSpeed = (m->panicTimer > 0.0f) ? 12.0f : 5.0f;
            float bobAmount = (m->panicTimer > 0.0f) ? 0.015f : 0.008f;
            float bob = sin(m->animTime * bobSpeed) * bobAmount * scaleFactor;
            m->renderParts[0].center.y += bob;
            
            // Slight tilt when walking (lean into the walk)
            float tiltAmount = (m->panicTimer > 0.0f) ? 0.12f : 0.06f;
            float tilt = sin(m->animTime * bobSpeed) * tiltAmount;
            glm::quat walkTilt = glm::angleAxis(tilt, glm::vec3(0, 0, 1));
            glm::quat baseRot = glm::angleAxis(m->yaw, glm::vec3(0, 1, 0));
            m->renderParts[0].rotation = baseRot * walkTilt;
            
            // Walk cycle for limbs
            float walkAnim = sin(m->animTime * bobSpeed * 1.5f); // Phase of walk
            float armSwing = walkAnim * 0.4f;
            float legSwing = walkAnim * 0.5f;
            
            // Left Arm
            m->renderParts[1].rotation = baseRot * glm::angleAxis(armSwing, glm::vec3(1, 0, 0));
            m->renderParts[1].center = m->position + baseRot * glm::vec3(-0.06f * scaleFactor, 0.04f * scaleFactor + bob, 0.0f);
            
            // Right Arm
            m->renderParts[2].rotation = baseRot * glm::angleAxis(-armSwing, glm::vec3(1, 0, 0));
            m->renderParts[2].center = m->position + baseRot * glm::vec3(0.06f * scaleFactor, 0.04f * scaleFactor + bob, 0.0f);
            
            // Left Leg
            m->renderParts[3].rotation = baseRot * glm::angleAxis(-legSwing, glm::vec3(1, 0, 0));
            m->renderParts[3].center = m->position + baseRot * glm::vec3(-0.03f * scaleFactor, -0.05f * scaleFactor, 0.0f);
            
            // Right Leg
            m->renderParts[4].rotation = baseRot * glm::angleAxis(legSwing, glm::vec3(1, 0, 0));
            m->renderParts[4].center = m->position + baseRot * glm::vec3(0.03f * scaleFactor, -0.05f * scaleFactor, 0.0f);

        } else {
            m->renderParts[0].rotation = glm::angleAxis(m->yaw, glm::vec3(0, 1, 0));
        }

        i++;
    }
}

void drawMobs(int modelLoc, int colorLoc, int shadowLoc) {
    // Requires glUniformMatrix4fv and drawing logic
    // But Globals.h doesn't include OpenGL headers directly in a way we can just call it here without repeating drawing code.
    // Instead, we will draw them the same way we draw ragdoll parts.
    for (Mob* m : activeMobs) {
        if (!m->active) continue;
        
        for (auto& p : m->renderParts) {
            if (p.voxels.empty()) continue;
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, p.center);
            model *= glm::mat4_cast(p.rotation);
            
            if (modelLoc != -1) {
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
            }
            
            glBindVertexArray(p.VAO);
            glDrawArrays(GL_TRIANGLES, 0, p.vertexCount);
        }
    }
}

void clearAllMobs() {
    for (Mob* m : activeMobs) {
        for (auto& p : m->renderParts) {
            cleanupRagdollPartMesh(p);
        }
        delete m;
    }
    activeMobs.clear();
}

bool damageMobAtWorldPos(const glm::vec3& hitPos, const glm::vec3& impulse, float damageRadius, float damageAmount) {
    bool hitAnything = false;
    float r2 = damageRadius * damageRadius;

    for (Mob* m : activeMobs) {
        if (!m->active) continue;
        
        float dist2 = glm::distance2(m->position, hitPos);
        if (dist2 <= r2 || dist2 < 1.0f) { // generous hitbox
            m->health -= damageAmount;
            m->panicTimer = 3.0f; // Panic for 3 seconds when hurt
            if (m->health <= 0.0f) {
                m->active = false; // Will be cleaned up next update
                
                // Spawn ragdoll corpse based on mob type
                glm::vec3 ragdollVel = m->velocity;
                ragdollVel += impulse * 2.0f; // launch it
                
                if (m->type == MOB_MUSHROOM) {
                    spawnRagdoll(m->position, ragdollVel);
                } else {
                    spawnChick(m->position, ragdollVel);
                }
            }
            hitAnything = true;
        }
    }
    return hitAnything;
}
