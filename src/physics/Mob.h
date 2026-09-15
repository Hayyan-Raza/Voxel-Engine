#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Ragdoll.h"

enum MobType {
    MOB_CHICK,
    MOB_MUSHROOM
};

struct Mob {
    int id = 0;
    bool active = true;
    float health = 100.0f;
    MobType type = MOB_CHICK;
    
    glm::vec3 position;
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f; // facing direction

    std::vector<RagdollPart> renderParts; // Used for drawing the mob's distinct pieces

    float wanderTimer = 0.0f;
    float panicTimer = 0.0f;
    float animTime = 0.0f; // Animation timer for bobbing/walking
};

extern std::vector<Mob*> activeMobs;

void spawnLivingChick(const glm::vec3& position);
void spawnLivingMushroom(const glm::vec3& position);
void updateMobs(float dt);
void drawMobs(int modelLoc, int colorLoc, int shadowLoc);
void clearAllMobs();

bool damageMobAtWorldPos(const glm::vec3& hitPos, const glm::vec3& impulse, float damageRadius, float damageAmount);
