#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// --- Per-voxel position in island BFS ---
struct VoxelPos { int x, y, z; };

// --- Frustum Culling Types ---
struct Plane {
    glm::vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];

    void update(const glm::mat4& vp) {
        planes[0].normal = glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]);
        planes[0].distance = vp[3][3] + vp[3][0];
        planes[1].normal = glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]);
        planes[1].distance = vp[3][3] - vp[3][0];
        planes[2].normal = glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]);
        planes[2].distance = vp[3][3] + vp[3][1];
        planes[3].normal = glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]);
        planes[3].distance = vp[3][3] - vp[3][1];
        planes[4].normal = glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]);
        planes[4].distance = vp[3][3] + vp[3][2];
        planes[5].normal = glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]);
        planes[5].distance = vp[3][3] - vp[3][2];

        for (int i = 0; i < 6; i++) {
            float length = glm::length(planes[i].normal);
            planes[i].normal /= length;
            planes[i].distance /= length;
        }
    }

    bool isBoxVisible(const glm::vec3& min, const glm::vec3& max) const {
        for (int i = 0; i < 6; i++) {
            glm::vec3 p(min);
            if (planes[i].normal.x >= 0.0f) p.x = max.x;
            if (planes[i].normal.y >= 0.0f) p.y = max.y;
            if (planes[i].normal.z >= 0.0f) p.z = max.z;
            if (glm::dot(planes[i].normal, p) + planes[i].distance < 0.0f) {
                return false;
            }
        }
        return true;
    }

    bool isSphereVisible(const glm::vec3& center, float radius) const {
        for (int i = 0; i < 6; i++) {
            if (glm::dot(planes[i].normal, center) + planes[i].distance < -radius) {
                return false;
            }
        }
        return true;
    }
};

extern Frustum viewFrustum;

// --- Vertex format shared across static world and dynamic chunks ---
struct VoxelVertex {
    float x, y, z;      // 12
    uint8_t r, g, b;    // 3
    int8_t nx, ny, nz;  // 3
    uint8_t emissive;   // 1
    uint8_t light;      // 1 (0-15 block light)
    float ao = 1.0f;    // 4
}; // Total: 24 bytes

// --- Teardown-style voxel chunk: maintains shape, tumbles as rigid body ---
struct VoxelChunk {
    std::vector<std::pair<glm::ivec3, uint8_t>> voxels; // 24
    glm::quat rotation = glm::quat(1, 0, 0, 0); // 16
    glm::vec3 center; // 12
    glm::vec3 velocity; // 12
    glm::vec3 angularVelocity; // 12
    void* rigidBody = nullptr; // 8
    void* collisionShape = nullptr; // 8
    uint32_t  id = 0; // 4
    float     life     = 4.0f; // 4
    unsigned int VAO = 0; // 4
    unsigned int VBO = 0; // 4
    int          vertexCount = 0; // 4
    int          weaponType = -1; // -1 if not a weapon, 0-5 if it is a dropped weapon
    bool         isDynamite = false;
};

// --- Dust / smoke billboard particle ---
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float     life, maxLife;
    float     size;   // world-space radius
    float     alpha;
    glm::vec3 color;
    float     rotation;
    float     rotationSpeed;
    float     buoyancy;
    float     growthRate;
};

