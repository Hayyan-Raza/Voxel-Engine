#include "Ragdoll.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../rendering/Renderer.h"
#include "../particles/Particles.h"
#include "../physics/Physics.h"
#include "../physics/IslandDetection.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

std::vector<Ragdoll> activeRagdolls;
float ragdollStiffnessParam = 0.38f;
int nextRagdollId = 1;

// Grab & Carry State
int grabbedRagdollId = 0;
int grabbedPartIdx = -1;
float grabbedDistance = 1.8f;

#include "RagdollBuilder.h"
#include "Chick.h"
bool checkPartVoxelCollision(const RagdollPart& part, const glm::vec3& testPos, const glm::quat& testRot, glm::vec3& outNormal) {
    if (part.voxels.empty()) return false;

    bool hit = false;
    glm::vec3 avgNormal(0.0f);
    int count = 0;

    size_t step = std::max((size_t)1, part.voxels.size() / 24);

    for (size_t i = 0; i < part.voxels.size(); i += step) {
        if (part.voxels[i].destroyed) continue;

        glm::vec3 localPt = glm::vec3(part.voxels[i].localPos) * voxelSize;
        glm::vec3 worldPt = testPos + testRot * localPt;

        int gx = (int)round(worldPt.x / voxelSize);
        int gy = (int)round(worldPt.y / voxelSize);
        int gz = (int)round(worldPt.z / voxelSize);

        if (worldPt.y < 0.02f) {
            avgNormal += glm::vec3(0.0f, 1.0f, 0.0f);
            count++;
            hit = true;
            continue;
        }

        if (gy >= 0 && gy < WORLD_HEIGHT) {
            if (getVoxel(gx, gy, gz) > 0) {
                avgNormal += glm::vec3(0.0f, 1.0f, 0.0f);
                count++;
                hit = true;
            }
        }
    }

    if (hit && count > 0) {
        outNormal = glm::normalize(avgNormal);
        return true;
    }
    return false;
}

void updateRagdolls(float dt) {
    if (activeRagdolls.empty()) return;
    dt = std::min(dt, 0.033f);

    float scaleFactor = voxelSize / 0.01f;

    for (auto& ragdoll : activeRagdolls) {
        if (!ragdoll.active) continue;
        ragdoll.life -= dt;

        // 1. --- Motion & Gravity ---
        for (auto& p : ragdoll.parts) {
            p.velocity.y -= (GRAVITY * scaleFactor) * dt;

            p.velocity *= 0.994f;
            p.angularVelocity *= 0.990f;

            float speed = glm::length(p.velocity);
            if (speed > 0.4f) {
                glm::vec3 flailTorque = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), p.velocity) * 1.5f;
                p.angularVelocity += flailTorque * dt;
            }

            glm::vec3 nextCenter = p.center + p.velocity * dt;
            glm::quat nextRot = p.rotation;

            float rotAngle = glm::length(p.angularVelocity) * dt;
            if (rotAngle > 0.001f) {
                glm::vec3 rotAxis = glm::normalize(p.angularVelocity);
                nextRot = glm::normalize(glm::angleAxis(rotAngle, rotAxis) * p.rotation);
            }

            // 2. --- Terrain Collision & High-Velocity Wrecking Ball Building Demolition ---
            glm::vec3 colNormal(0.0f);
            if (checkPartVoxelCollision(p, nextCenter, nextRot, colNormal)) {
                if (speed > 4.5f) {
                    // Heavy thrown ragdoll smashes through voxel walls & buildings!
                    int gx = (int)round(nextCenter.x / voxelSize);
                    int gy = (int)round(nextCenter.y / voxelSize);
                    int gz = (int)round(nextCenter.z / voxelSize);
                    
                    std::vector<glm::ivec3> stabilityNodes;
                    int demolishR = (speed > 8.0f) ? 2 : 1;
                    
                    for (int dx = -demolishR; dx <= demolishR; dx++) {
                        for (int dy = -demolishR; dy <= demolishR; dy++) {
                            for (int dz = -demolishR; dz <= demolishR; dz++) {
                                int tx = gx + dx, ty = gy + dy, tz = gz + dz;
                                if (ty < 0 || ty >= WORLD_HEIGHT) continue;
                                if (getVoxel(tx, ty, tz) == 3 && ty <= 8) continue; // Bedrock protection
                                if (getVoxel(tx, ty, tz) > 0) {
                                    uint8_t vType = getVoxel(tx, ty, tz);
                                    if (rand() % 3 == 0) {
                                        VoxelChunk shard;
                                        shard.center = glm::vec3(tx, ty, tz) * voxelSize;
                                        shard.life = 0.8f + (rand() % 100) / 100.0f;
                                        shard.rotation = glm::quat(1,0,0,0);
                                        shard.voxels.push_back(std::make_pair(glm::ivec3(0,0,0), vType));
                                        glm::vec3 ejectDir = glm::normalize(p.velocity + glm::vec3((rand()%200-100)*0.1f, (rand()%200-100)*0.1f, (rand()%200-100)*0.1f));
                                        shard.velocity = ejectDir * 0.5f + glm::vec3(0.0f, 0.2f, 0.0f);
                                        createPhysicsForChunk(shard, shard.velocity, glm::vec3(0.0f));
                                        activeChunks.push_back(std::move(shard));
                                    }
                                    const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                                    for (int i = 0; i < 6; i++) {
                                        int nx = tx + nb[i][0], ny = ty + nb[i][1], nz = tz + nb[i][2];
                                        if (ny >= 0 && ny < WORLD_HEIGHT && getVoxel(nx, ny, nz) > 0)
                                            stabilityNodes.push_back(glm::ivec3(nx,ny,nz));
                                    }
                                    setVoxel(tx, ty, tz, 0);
                                    markChunkDirty(tx, ty, tz);
                                }
                            }
                        }
                    }
                    updateStaticMesh(glm::vec3(0,0,0));
                    if (!stabilityNodes.empty()) detectIslands(stabilityNodes);
                }

                float vn = glm::dot(p.velocity, colNormal);
                if (vn < 0.0f) {
                    glm::vec3 vNormal = colNormal * vn;
                    glm::vec3 vTangential = p.velocity - vNormal;
                    p.velocity = vTangential * 0.40f - colNormal * (vn * 0.20f);
                    p.angularVelocity = p.angularVelocity * 0.7f + glm::cross(colNormal, p.velocity) * 6.5f;
                }

                p.center += colNormal * (voxelSize * 0.8f);
                p.rotation = nextRot;
            } else {
                p.center = nextCenter;
                p.rotation = nextRot;
            }
        }

        // 3. --- Solve Joint Constraints ---
        const int iterations = 8;
        for (int iter = 0; iter < iterations; iter++) {
            for (auto& j : ragdoll.joints) {
                RagdollPart& pA = ragdoll.parts[j.partA];
                RagdollPart& pB = ragdoll.parts[j.partB];

                glm::vec3 anchorAWorld = pA.center + pA.rotation * j.localAnchorA;
                glm::vec3 anchorBWorld = pB.center + pB.rotation * j.localAnchorB;

                glm::vec3 delta = anchorBWorld - anchorAWorld;
                float currentDist = glm::length(delta);

                if (currentDist > 0.0001f) {
                    glm::vec3 dir = delta / currentDist;
                    float error = currentDist - j.targetDistance;

                    float totalMass = pA.mass + pB.mass;
                    float wA = pB.mass / totalMass;
                    float wB = pA.mass / totalMass;

                    glm::vec3 correction = dir * error * j.stiffness;

                    pA.center += correction * wA;
                    pB.center -= correction * wB;

                    glm::vec3 relVel = pB.velocity - pA.velocity;
                    glm::vec3 dampForce = dir * glm::dot(relVel, dir) * 0.5f;
                    pA.velocity += dampForce * wA;
                    pB.velocity -= dampForce * wB;
                }
            }
        }
    }

    activeRagdolls.erase(
        std::remove_if(activeRagdolls.begin(), activeRagdolls.end(), [](const Ragdoll& r) {
            if (r.life <= 0.0f) {
                for (auto& p : const_cast<Ragdoll&>(r).parts) {
                    cleanupRagdollPartMesh(p);
                }
                return true;
            }
            return false;
        }),
        activeRagdolls.end()
    );
}

void drawRagdolls(GLint modelLoc, GLint colorLoc, GLint shadowLoc) {
    if (activeRagdolls.empty()) return;

    for (const auto& ragdoll : activeRagdolls) {
        if (!ragdoll.active) continue;

        for (const auto& part : ragdoll.parts) {
            if (part.vertexCount == 0 || part.VAO == 0) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), part.center);
            model *= glm::toMat4(part.rotation);

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            glUniform1f(shadowLoc, 0.0f);

            glBindVertexArray(part.VAO);
            glDrawArrays(GL_TRIANGLES, 0, part.vertexCount);
        }
    }
    glBindVertexArray(0);
}

void clearAllRagdolls() {
    releaseGrabbedRagdoll();
    for (auto& r : activeRagdolls) {
        for (auto& p : r.parts) {
            cleanupRagdollPartMesh(p);
        }
    }
    activeRagdolls.clear();
}

// --- Flesh Damage Mechanics (Blood Spray, NO Smoke/Sparks) ---
bool damageRagdollAtWorldPos(const glm::vec3& hitPos, const glm::vec3& impulse, float damageRadius) {
    bool hitAny = false;

    for (auto& ragdoll : activeRagdolls) {
        if (!ragdoll.active) continue;

        for (auto& part : ragdoll.parts) {
            float partDist = glm::distance(part.center, hitPos);
            float maxPartRadius = 0.25f * (voxelSize / 0.01f);
            if (partDist > maxPartRadius + damageRadius) continue;

            bool partDamaged = false;
            for (auto& vox : part.voxels) {
                if (vox.destroyed) continue;

                glm::vec3 voxWorldPos = part.center + part.rotation * (glm::vec3(vox.localPos) * voxelSize);
                float dist = glm::distance(voxWorldPos, hitPos);

                if (dist <= damageRadius) {
                    if (!vox.unbreakable) {
                        vox.destroyed = true;
                        partDamaged = true;
                        hitAny = true;

                        // REALISTIC BLOOD SPRAY & FLESH SPLATTER (NO smoke, NO fire, NO sparks!)
                        spawnBlood(voxWorldPos, 6);
                    } else {
                        // Bone strike: minor blood spray
                        spawnBlood(voxWorldPos, 3);
                        hitAny = true;
                    }
                }
            }

            if (partDamaged) {
                initRagdollPartMesh(part);
                part.velocity += (impulse / part.mass) * 0.8f;
                part.angularVelocity += glm::cross(part.center - hitPos, impulse) * 1.5f;
            }
        }
    }

    return hitAny;
}

void applyImpulseToRagdolls(const glm::vec3& hitPos, const glm::vec3& impulse, float radius) {
    damageRagdollAtWorldPos(hitPos, impulse, radius);
}

void applyExplosionToRagdolls(const glm::vec3& explosionPos, float blastRadius, float blastForce) {
    for (auto& r : activeRagdolls) {
        for (auto& p : r.parts) {
            float d = glm::distance(p.center, explosionPos);
            if (d < blastRadius) {
                glm::vec3 dir = p.center - explosionPos;
                if (glm::length(dir) < 0.001f) {
                    dir = glm::vec3(0.0f, 1.0f, 0.0f);
                } else {
                    dir = glm::normalize(dir);
                }
                float falloff = 1.0f - (d / blastRadius);
                glm::vec3 force = (dir * blastForce + glm::vec3(0.0f, blastForce * 0.4f, 0.0f)) * falloff;
                
                damageRagdollAtWorldPos(p.center, force, 0.12f * (blastRadius / 2.0f));

                p.velocity += force / p.mass;
                p.angularVelocity += glm::vec3((rand()%100-50)*0.1f, (rand()%100-50)*0.1f, (rand()%100-50)*0.1f) * falloff;
            }
        }
    }
}

void pushRagdollsWithPlayer(const glm::vec3& playerPos, float playerRadius, float playerHeight) {
    glm::vec3 playerCenter = playerPos - glm::vec3(0.0f, playerHeight * 0.5f, 0.0f);

    for (auto& r : activeRagdolls) {
        for (auto& p : r.parts) {
            glm::vec3 diff = p.center - playerCenter;
            float hDist = glm::length(glm::vec2(diff.x, diff.z));
            float vDist = fabsf(diff.y);

            float minDist = playerRadius + p.radius;
            if (hDist < minDist && vDist < playerHeight * 0.6f) {
                glm::vec3 pushDir(0.0f);
                if (hDist > 0.001f) {
                    pushDir = glm::normalize(glm::vec3(diff.x, 0.2f, diff.z));
                } else {
                    pushDir = glm::vec3(0.0f, 0.5f, 1.0f);
                }
                float force = (minDist - hDist) * 12.0f;
                p.velocity += pushDir * force;
            }
        }
    }
}

// --- Ragdoll Grab & Carry API ---
bool grabRagdollRaycast(const glm::vec3& cameraPos, const glm::vec3& cameraFront, float maxReach) {
    grabbedRagdollId = 0;
    grabbedPartIdx = -1;

    float bestDist = maxReach;

    for (auto& r : activeRagdolls) {
        if (!r.active) continue;
        for (int i = 0; i < (int)r.parts.size(); i++) {
            auto& p = r.parts[i];
            glm::vec3 toPart = p.center - cameraPos;
            float projDist = glm::dot(toPart, cameraFront);
            if (projDist > 0.1f && projDist < bestDist) {
                glm::vec3 closestPoint = cameraPos + cameraFront * projDist;
                float distToRay = glm::distance(p.center, closestPoint);
                float grabRadius = p.radius + 0.15f;
                if (distToRay < grabRadius) {
                    bestDist = projDist;
                    grabbedRagdollId = r.id;
                    grabbedPartIdx = i;
                    grabbedDistance = std::clamp(projDist, 1.2f, 2.5f);
                }
            }
        }
    }

    return (grabbedRagdollId != 0);
}

void updateGrabbedRagdoll(const glm::vec3& cameraPos, const glm::vec3& cameraFront, float dt) {
    if (grabbedRagdollId == 0 || grabbedPartIdx < 0) return;

    for (auto& r : activeRagdolls) {
        if (r.id == grabbedRagdollId && r.active) {
            if (grabbedPartIdx < (int)r.parts.size()) {
                auto& p = r.parts[grabbedPartIdx];
                glm::vec3 targetPos = cameraPos + cameraFront * grabbedDistance;
                glm::vec3 delta = targetPos - p.center;
                
                p.velocity = delta * 22.0f;
                p.angularVelocity *= 0.85f;
                r.life = 180.0f;
            }
            break;
        }
    }
}

void throwGrabbedRagdoll(const glm::vec3& throwVel) {
    if (grabbedRagdollId == 0) return;

    for (auto& r : activeRagdolls) {
        if (r.id == grabbedRagdollId && r.active) {
            for (auto& p : r.parts) {
                p.velocity = throwVel;
                p.angularVelocity += glm::vec3(
                    ((rand() % 100) / 50.0f - 1.0f) * 8.0f,
                    ((rand() % 100) / 50.0f - 1.0f) * 8.0f,
                    ((rand() % 100) / 50.0f - 1.0f) * 8.0f
                );
            }
            break;
        }
    }

    releaseGrabbedRagdoll();
}

void releaseGrabbedRagdoll() {
    grabbedRagdollId = 0;
    grabbedPartIdx = -1;
}
