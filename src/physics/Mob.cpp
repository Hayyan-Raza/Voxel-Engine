#include "Mob.h"
#include "Physics.h"
#include "../core/Globals.h"
#include <iostream>
#include <glm/gtx/norm.hpp>
#include "../world/World.h"

std::vector<Mob*> activeMobs;
static int nextMobId = 1;

void spawnLivingChick(const glm::vec3& position) {
    Mob* mob = new Mob();
    mob->id = nextMobId++;
    mob->position = position;
    mob->health = 100.0f;

    float scale = voxelSize / 0.01f;

    RagdollPart& p = mob->renderPart;
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

void updateMobs(float dt) {
    for (size_t i = 0; i < activeMobs.size();) {
        Mob* m = activeMobs[i];
        if (!m->active) {
            cleanupRagdollPartMesh(m->renderPart);
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

        if (!checkPartVoxelCollision(m->renderPart, nextCenter, nextRot, colNormal)) {
            m->position.x = nextCenter.x;
            m->position.z = nextCenter.z;
        }

        // Vertical movement
        nextCenter = m->position;
        nextCenter.y += m->velocity.y * dt;
        if (checkPartVoxelCollision(m->renderPart, nextCenter, nextRot, colNormal)) {
            if (m->velocity.y < 0.0f) m->velocity.y = 0.0f; // hit ground
        } else {
            m->position.y = nextCenter.y;
        }

        // Sync render part
        m->renderPart.center = m->position;
        m->renderPart.rotation = nextRot;

        i++;
    }
}

void drawMobs(int modelLoc, int colorLoc, int shadowLoc) {
    // Requires glUniformMatrix4fv and drawing logic
    // But Globals.h doesn't include OpenGL headers directly in a way we can just call it here without repeating drawing code.
    // Instead, we will draw them the same way we draw ragdoll parts.
    for (Mob* m : activeMobs) {
        if (!m->active || m->renderPart.voxels.empty()) continue;
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m->renderPart.center);
        model *= glm::mat4_cast(m->renderPart.rotation);
        
        if (modelLoc != -1) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        }
        
        glBindVertexArray(m->renderPart.VAO);
        glDrawArrays(GL_TRIANGLES, 0, m->renderPart.vertexCount);
    }
}

void clearAllMobs() {
    for (Mob* m : activeMobs) {
        cleanupRagdollPartMesh(m->renderPart);
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
                
                // Spawn ragdoll corpse
                glm::vec3 ragdollVel = m->velocity;
                ragdollVel += impulse * 2.0f; // launch it
                
                spawnChick(m->position, ragdollVel);
            }
            hitAnything = true;
        }
    }
    return hitAnything;
}
