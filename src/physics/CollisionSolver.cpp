#include "CollisionSolver.h"
#include "../world/World.h"
#include <glm/gtx/norm.hpp>

bool checkChunkCollision(const VoxelChunk& c, const glm::vec3& testCenter, const glm::quat& testRot, glm::vec3& outNormal, glm::vec3& outContactPt) {
    if (c.voxels.empty()) return false;

    size_t step = 1;
    if (c.voxels.size() > 64) {
        step = c.voxels.size() / 64;
    }

    bool hit = false;
    glm::vec3 avgNormal(0.0f);
    glm::vec3 avgContact(0.0f);
    int hitCount = 0;

    for (size_t i = 0; i < c.voxels.size(); i += step) {
        glm::ivec3 localPos = c.voxels[i].first;
        glm::vec3 worldPos = testCenter + testRot * (glm::vec3(localPos) * voxelSize);
        int gx = (int)round(worldPos.x / voxelSize);
        int gy = (int)round(worldPos.y / voxelSize);
        int gz = (int)round(worldPos.z / voxelSize);

        // Off-grid: let it fall off the world (no artificial floor)
        if (worldPos.y < 0.0f) {
            // Below absolute world floor — mark as ground hit to stop sliding under map
            avgNormal += glm::vec3(0.0f, 1.0f, 0.0f);
            avgContact += glm::vec3(worldPos.x, 0.0f, worldPos.z);
            hitCount++;
            hit = true;
            continue;
        }

        if (gy >= 0 && gy < WORLD_HEIGHT) {
            if (getVoxel(gx, gy, gz) > 0) {
                glm::vec3 normal(0.0f);
                int emptyCount = 0;

                const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                for (int n = 0; n < 6; n++) {
                    int nx = gx + nb[n][0];
                    int ny = gy + nb[n][1];
                    int nz = gz + nb[n][2];
                    if (ny < 0 || ny >= WORLD_HEIGHT || getVoxel(nx, ny, nz) == 0) {
                        normal += glm::vec3(nb[n][0], nb[n][1], nb[n][2]);
                        emptyCount++;
                    }
                }

                if (emptyCount > 0) {
                    avgNormal += normal;
                } else {
                    avgNormal += glm::vec3(0.0f, 1.0f, 0.0f);
                }
                avgContact += worldPos;
                hitCount++;
                hit = true;
            }
        }
    }

    if (hit && hitCount > 0) {
        if (glm::length2(avgNormal) > 0.001f) {
            outNormal = glm::normalize(avgNormal);
        } else {
            outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        outContactPt = avgContact / (float)hitCount;
        return true;
    }
    return false;
}
