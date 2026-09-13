#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include "../core/Types.h"

// --- Layered Voxel Types ---
enum RagdollVoxelType : uint8_t {
    RAGDOLL_BONE = 1,        // Unbreakable white bone skeleton
    RAGDOLL_FLESH = 2,       // Crimson red muscle/flesh
    RAGDOLL_SKIN = 3,        // Skin tone
    RAGDOLL_CLOTH_SHIRT = 4, // Blue shirt
    RAGDOLL_CLOTH_PANTS = 5, // Dark slate pants
    RAGDOLL_SHOES = 6,       // Dark leather shoes
    RAGDOLL_EYES = 7,        // Facial features
    RAGDOLL_CHICK_FEATHERS = 8, // Chick yellow feathers
    RAGDOLL_CHICK_BEAK = 9,  // Chick orange beak
    RAGDOLL_CHICK_LEGS = 10  // Chick orange legs
};

struct RagdollVoxel {
    glm::ivec3 localPos;
    uint8_t type = RAGDOLL_SKIN;
    uint8_t unbreakable : 1;
    uint8_t destroyed : 1;
};

// --- Individual Body Part of a Ragdoll ---
struct RagdollPart {
    std::vector<RagdollVoxel> voxels;
    const char* name = "Unknown";
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 center{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    glm::vec3 extents{0.04f, 0.04f, 0.04f}; // local half-extents
    float mass = 1.0f;
    float radius = 0.08f;
    GLuint VAO = 0;
    GLuint VBO = 0;
    int vertexCount = 0;
};

// --- Joint connecting parent and child body parts ---
struct RagdollJoint {
    int partA; // parent part index
    int partB; // child part index
    glm::vec3 localAnchorA; // local offset on part A
    glm::vec3 localAnchorB; // local offset on part B
    float minAngle = -1.2f;
    float maxAngle = 1.2f;
    float stiffness = 0.85f;
    float targetDistance = 0.0f; // Rest distance between anchors
};

// --- Humanoid Ragdoll Mannequin ---
struct Ragdoll {
    int id = 0;
    bool active = true;
    float life = 180.0f; // stays active for 3 minutes or until cleared
    std::vector<RagdollPart> parts;
    std::vector<RagdollJoint> joints;

    enum PartIndex {
        HEAD = 0,
        TORSO,
        PELVIS,
        L_UPPER_ARM,
        L_LOWER_ARM,
        R_UPPER_ARM,
        R_LOWER_ARM,
        L_UPPER_LEG,
        L_LOWER_LEG,
        R_UPPER_LEG,
        R_LOWER_LEG,
        PART_COUNT
    };
};

extern std::vector<Ragdoll> activeRagdolls;
extern float ragdollStiffnessParam;
extern int nextRagdollId;

// --- Public API ---
void spawnRagdoll(const glm::vec3& position, const glm::vec3& initialVelocity);
void spawnChick(const glm::vec3& position, const glm::vec3& initialVelocity);

bool checkPartVoxelCollision(const RagdollPart& part, const glm::vec3& testPos, const glm::quat& testRot, glm::vec3& outNormal);
void updateRagdolls(float dt);
void drawRagdolls(GLint modelLoc, GLint colorLoc, GLint shadowLoc);
void clearAllRagdolls();
void initRagdollPartMesh(RagdollPart& part);
void cleanupRagdollPartMesh(RagdollPart& part);

// --- Damage & Interactions ---
bool damageRagdollAtWorldPos(const glm::vec3& hitPos, const glm::vec3& impulse, float damageRadius);
void applyImpulseToRagdolls(const glm::vec3& hitPos, const glm::vec3& impulse, float radius);
void applyExplosionToRagdolls(const glm::vec3& explosionPos, float blastRadius, float blastForce);
void pushRagdollsWithPlayer(const glm::vec3& playerPos, float playerRadius, float playerHeight);

// --- Ragdoll Grab & Heavy Throwing API ---
extern int grabbedRagdollId;
extern int grabbedPartIdx;
extern float grabbedDistance;

bool grabRagdollRaycast(const glm::vec3& cameraPos, const glm::vec3& cameraFront, float maxReach = 3.5f);
void updateGrabbedRagdoll(const glm::vec3& cameraPos, const glm::vec3& cameraFront, float dt);
void throwGrabbedRagdoll(const glm::vec3& throwVel);
void releaseGrabbedRagdoll();

