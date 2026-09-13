#include "Physics.h"
#include "Ragdoll.h"
#include "Mob.h"
#include "CollisionSolver.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../rendering/Renderer.h"
#include "../particles/Particles.h"
#include "IslandDetection.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <map>
#include <set>

// ----- Bullet singletons -----
btDefaultCollisionConfiguration*     collisionConfiguration = nullptr;
btCollisionDispatcher*                dispatcher             = nullptr;
btBroadphaseInterface*                overlappingPairCache   = nullptr;
btSequentialImpulseConstraintSolver* solver                 = nullptr;
btDiscreteDynamicsWorld*             dynamicsWorld          = nullptr;
btRigidBody*                         playerRigidBody        = nullptr;
btCollisionShape*                    playerShape            = nullptr;

btRigidBody*                         hammerRigidBody        = nullptr;
btCollisionShape*                    hammerShape            = nullptr;

bool  hammerHitThisFrame = false;
glm::vec3 hammerHitWorldPos = glm::vec3(0.0f);
glm::vec3 hammerHitWorldNormal = glm::vec3(0.0f);

void initPhysics() {
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher             = new btCollisionDispatcher(collisionConfiguration);
    overlappingPairCache   = new btDbvtBroadphase();
    solver                 = new btSequentialImpulseConstraintSolver();
    dynamicsWorld = new btDiscreteDynamicsWorld(
        dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -5.0f, 0));

    // Ground plane: grass surface is around Y=40..48 voxels * 0.005 = 0.20..0.24
    // Set at 0.0 (plane at world origin) — chunks landing on terrain use voxel collision instead
    btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
    btDefaultMotionState* groundState = new btDefaultMotionState(
        btTransform(btQuaternion(0,0,0,1), btVector3(0, 0.0f, 0)));
    btRigidBody::btRigidBodyConstructionInfo ci(
        0.0f, groundState, groundShape, btVector3(0,0,0));
    dynamicsWorld->addRigidBody(new btRigidBody(ci));

    // --- Kinematic Player Body (for pushing debris) ---
    playerShape = new btCapsuleShape(0.08f, 0.15f); // 0.08 radius, 0.15 height
    btDefaultMotionState* playerMotion = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(cameraPos.x, cameraPos.y, cameraPos.z)));
    btRigidBody::btRigidBodyConstructionInfo pCi(0.0f, playerMotion, playerShape, btVector3(0,0,0));
    playerRigidBody = new btRigidBody(pCi);
    playerRigidBody->setCollisionFlags(playerRigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    playerRigidBody->setActivationState(DISABLE_DEACTIVATION);
    dynamicsWorld->addRigidBody(playerRigidBody);

    // --- Kinematic Hammer Body ---
    hammerShape = new btBoxShape(btVector3(0.12f, 0.12f, 0.12f)); 
    btDefaultMotionState* hammerMotion = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,0,0)));
    btRigidBody::btRigidBodyConstructionInfo hCi(0.0f, hammerMotion, hammerShape, btVector3(0,0,0));
    hammerRigidBody = new btRigidBody(hCi);
    hammerRigidBody->setCollisionFlags(hammerRigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    hammerRigidBody->setActivationState(DISABLE_DEACTIVATION);
    dynamicsWorld->addRigidBody(hammerRigidBody);
}







void shatterChunk(const VoxelChunk& parent, const glm::vec3& impactPos, std::vector<VoxelChunk>& outNewChunks) {
    if (parent.voxels.size() <= 4) return;

    int numSubChunks = std::min(4, (int)parent.voxels.size() / 2);
    if (numSubChunks < 2) numSubChunks = 2;

    std::vector<VoxelChunk> subChunks(numSubChunks);
    std::vector<glm::vec3> seeds(numSubChunks);
    std::vector<glm::vec3> sumPos(numSubChunks, glm::vec3(0.0f));

    for (int i = 0; i < numSubChunks; i++) {
        int randIdx = rand() % parent.voxels.size();
        seeds[i] = glm::vec3(parent.voxels[randIdx].first) * voxelSize;
        subChunks[i].life = 2.5f;
    }

    // Limit debris size to prevent std::bad_alloc and extreme lag
    int maxVoxelsToProcess = 1500;
    int step = std::max(1, (int)(parent.voxels.size() / maxVoxelsToProcess));

    for (size_t i = 0; i < parent.voxels.size(); i += step) {
        const auto& v = parent.voxels[i];
        glm::vec3 vLocalPos = glm::vec3(v.first) * voxelSize;
        int bestSeed = 0;
        float minDist = 1e10f;
        for (int s = 0; s < numSubChunks; s++) {
            float d = glm::distance(vLocalPos, seeds[s]);
            if (d < minDist) {
                minDist = d;
                bestSeed = s;
            }
        }
        subChunks[bestSeed].voxels.push_back(v);
        sumPos[bestSeed] += vLocalPos;
    }

    uint8_t sampleType = parent.voxels.empty() ? 2 : parent.voxels[0].second;
    spawnDust(impactPos, 3, sampleType);
    if (sampleType == 3) {
        spawnSparks(impactPos, 4);
    }

    for (int i = 0; i < numSubChunks; i++) {
        if (subChunks[i].voxels.empty()) continue;

        glm::vec3 localAvg = sumPos[i] / (float)subChunks[i].voxels.size();
        glm::vec3 worldAvg = parent.center + parent.rotation * localAvg;
        subChunks[i].center = worldAvg;

        glm::ivec3 localCentroid(
            (int)round(localAvg.x / voxelSize), 
            (int)round(localAvg.y / voxelSize), 
            (int)round(localAvg.z / voxelSize)
        );
        for (auto& v : subChunks[i].voxels) {
            v.first -= localCentroid;
        }

        glm::vec3 diff = worldAvg - impactPos;
        glm::vec3 scatterDir;
        if (glm::length2(diff) < 0.0001f) {
            scatterDir = glm::normalize(glm::vec3((rand()%200-100)*0.01f, 1.0f, (rand()%200-100)*0.01f));
        } else {
            scatterDir = glm::normalize(diff);
        }
        float speed = 0.15f + (rand() % 100) * 0.003f; // 0.15 to 0.45 m/s scatter
        subChunks[i].velocity = parent.velocity * 0.3f + scatterDir * speed;
        subChunks[i].angularVelocity = parent.angularVelocity * 0.5f + glm::vec3((rand()%20-10)*0.4f, (rand()%20-10)*0.4f, (rand()%20-10)*0.4f);
        subChunks[i].rotation = parent.rotation;

        createPhysicsForChunk(subChunks[i], subChunks[i].velocity, subChunks[i].angularVelocity);
        outNewChunks.push_back(std::move(subChunks[i]));
    }
}

void updateChunkPhysics() {
    std::vector<size_t> chunksToRemove;
    std::vector<VoxelChunk> chunksToAdd;

    for (size_t i = 0; i < activeChunks.size(); i++) {
        VoxelChunk& c = activeChunks[i];
        c.life -= deltaTime;
        if (c.life <= 0.0f || c.center.y < -50.0f) { 
            if (c.isDynamite && c.life <= 0.0f) {
                triggerExplosion(c.center);
            }
            chunksToRemove.push_back(i);
            continue; 
        }

        if (c.rigidBody) {
            btRigidBody* body = static_cast<btRigidBody*>(c.rigidBody);
            btTransform trans;
            if (body->getMotionState()) {
                body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = body->getWorldTransform();
            }
            c.rotation = glm::quat(trans.getRotation().w(), trans.getRotation().x(), trans.getRotation().y(), trans.getRotation().z());
            btVector3 origin = trans.getOrigin();
            c.center = glm::vec3(origin.x(), origin.y(), origin.z());
        } else {
            c.velocity.y -= (GRAVITY * (voxelSize / 0.01f)) * deltaTime;
            glm::vec3 nextCenter = c.center + c.velocity * deltaTime;
            
            glm::quat nextRotation = c.rotation;
            float angle = glm::length(c.angularVelocity) * deltaTime;
            if (angle > 0.001f) {
                glm::vec3 axis = glm::normalize(c.angularVelocity);
                nextRotation = glm::normalize(glm::angleAxis(angle, axis) * c.rotation);
            }

            glm::vec3 colNormal(0.0f), colContact(0.0f);
            bool skipCollision = (c.isDynamite && c.life > 2.38f);
            if (!skipCollision && checkChunkCollision(c, nextCenter, nextRotation, colNormal, colContact)) {
                float e = 0.1f;
                float friction = 0.3f;
                
                float vn = glm::dot(c.velocity, colNormal);
                if (vn < 0.0f) {
                    glm::vec3 vNormal = colNormal * vn;
                    glm::vec3 vTangential = c.velocity - vNormal;
                    
                    if (vn < -0.2f) {
                        c.velocity = vTangential * friction - colNormal * (vn * e);
                    } else {
                        // Resting contact: cancel normal velocity completely
                        c.velocity = vTangential * friction;
                    }
                    c.angularVelocity = c.angularVelocity * 0.4f + glm::cross(colNormal, c.velocity) * 1.2f;
                    
                    if (vn < -1.5f && c.voxels.size() > 4 && !c.isDynamite && c.weaponType == -1) {
                        chunksToRemove.push_back(i);
                        shatterChunk(c, colContact, chunksToAdd);
                        continue;
                    }

                }
                
                // Allow dynamic sliding/tumbling along collision surfaces
                c.center += c.velocity * deltaTime + colNormal * (voxelSize * 0.04f);
                float resolvedAngle = glm::length(c.angularVelocity) * deltaTime;
                if (resolvedAngle > 0.001f) {
                    glm::vec3 axis = glm::normalize(c.angularVelocity);
                    c.rotation = glm::normalize(glm::angleAxis(resolvedAngle, axis) * c.rotation);
                }
            } else {
                c.center = nextCenter;
                c.rotation = nextRotation;
            }
        }
    }

    std::sort(chunksToRemove.begin(), chunksToRemove.end(), [](size_t a, size_t b) { return a > b; });
    chunksToRemove.erase(std::unique(chunksToRemove.begin(), chunksToRemove.end()), chunksToRemove.end());

    for (size_t idx : chunksToRemove) {
        cleanupChunkPhysics(activeChunks[idx]);
        activeChunks.erase(activeChunks.begin() + idx);
    }

    for (auto& newChunk : chunksToAdd) {
        newChunk.id = nextChunkId++; activeChunks.push_back(std::move(newChunk));
    }

    updateRagdolls(deltaTime);
    updateMobs(deltaTime);
}


void createPhysicsForChunk(VoxelChunk& chunk, glm::vec3 initialVelocity, glm::vec3 initialAngularVel) {
    if (chunk.voxels.empty()) return;

    chunk.velocity = initialVelocity;
    chunk.angularVelocity = initialAngularVel;

    // Initialize OpenGL mesh for optimized drawing
    initChunkMesh(chunk);
}

void cleanupChunkPhysics(VoxelChunk& chunk) {
    // Cleanup OpenGL mesh
    cleanupChunkMesh(chunk);

    if (chunk.rigidBody) {
        btRigidBody* body = static_cast<btRigidBody*>(chunk.rigidBody);
        dynamicsWorld->removeRigidBody(body);
        if (body->getMotionState()) {
            delete body->getMotionState();
        }
        delete body;
        chunk.rigidBody = nullptr;
    }
    if (chunk.collisionShape) {
        btCompoundShape* compoundShape = static_cast<btCompoundShape*>(chunk.collisionShape);
        // We reuse the same boxShape for all children in this chunk, so we need to delete it.
        // It's at index 0 because we added the same pointer multiple times.
        if (compoundShape->getNumChildShapes() > 0) {
            delete compoundShape->getChildShape(0); 
        }
        delete compoundShape;
        chunk.collisionShape = nullptr;
    }
}

void triggerExplosion(glm::vec3 pos) {
    int ex = (int)round(pos.x / voxelSize);
    int ey = (int)round(pos.y / voxelSize);
    int ez = (int)round(pos.z / voxelSize);
    
    float impactRadius = 34.0f;
    float coreRadius = 16.0f;
    
    std::vector<long long> adjacentNodes;
    adjacentNodes.reserve(100000);
    std::vector<glm::ivec3> stabilityNodes;

    // 1. Extract a few solid chunks from the boundary before deleting the voxels
    const int numDebris = 8;
    struct DebrisChunkData {
        glm::vec3 worldPos;
        std::vector<std::pair<glm::ivec3, uint8_t>> voxels;
        glm::vec3 sumPos = glm::vec3(0.0f);
    };
    std::vector<DebrisChunkData> debrisChunks(numDebris);
    
    // Choose random boundary points
    for (int i = 0; i < numDebris; i++) {
        float theta = (rand() % 3141) / 1000.0f * 2.0f;
        float phi = (rand() % 3141) / 1000.0f;
        float dist = coreRadius + (rand() % 100) * 0.01f * (impactRadius - coreRadius);
        
        glm::vec3 dir(sin(phi)*cos(theta), sin(phi)*sin(theta), cos(phi));
        glm::ivec3 bp = glm::ivec3(ex, ey, ez) + glm::ivec3(dir * dist);
        
        // Extract a small solid block around this boundary point
        int sz = (rand() % 2 == 0) ? 1 : 2; // size radius (1 = 3x3x3, 2 = 5x5x5)
        debrisChunks[i].worldPos = glm::vec3(bp) * voxelSize;
        
        for (int dx = -sz; dx <= sz; dx++) {
            for (int dy = -sz; dy <= sz; dy++) {
                for (int dz = -sz; dz <= sz; dz++) {
                    int gx = bp.x + dx, gy = bp.y + dy, gz = bp.z + dz;
                    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE || gz < 0 || gz >= GRID_SIZE) continue;
                    if (getVoxel(gx, gy, gz) == 3 && gy <= 8) continue; // bedrock protection
                    if (getVoxel(gx, gy, gz) > 0) {
                        uint8_t vType = getVoxel(gx, gy, gz);
                        debrisChunks[i].voxels.push_back({glm::ivec3(dx, dy, dz), vType});
                        debrisChunks[i].sumPos += glm::vec3(gx, gy, gz) * voxelSize;
                        
                        // Delete this voxel from the world
                        setVoxel(gx, gy, gz, 0);
                        markChunkDirty(gx, gy, gz);
                        
                        // Gather stability nodes for island detection
                        const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                        for (int n = 0; n < 6; n++) {
                            int nx = gx + nb[n][0], ny = gy + nb[n][1], nz = gz + nb[n][2];
                            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && nz >= 0 && nz < GRID_SIZE) {
                                if (getVoxel(nx, ny, nz) > 0) {
                                    adjacentNodes.push_back((long long)nx << 40 | (long long)ny << 20 | (long long)nz);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 2. Perform the main terrain deletion (vaporization of everything else inside the blast sphere)
    for (int x = -impactRadius; x <= impactRadius; x++) {
        for (int y = -impactRadius; y <= impactRadius; y++) {
            for (int z = -impactRadius; z <= impactRadius; z++) {
                int gx = ex + x, gy = ey + y, gz = ez + z;
                if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE || gz < 0 || gz >= GRID_SIZE) continue;
                if (getVoxel(gx, gy, gz) == 0) continue;
                if (getVoxel(gx, gy, gz) == 3 && gy <= 8) continue; // Bedrock protection

                float dist = sqrtf((float)(x*x + y*y + z*z));
                bool destroy = false;
                if (dist <= coreRadius) destroy = true;
                else if (dist <= impactRadius) {
                    float chance = 1.0f - (dist - coreRadius) / (impactRadius - coreRadius);
                    if ((rand() % 100) < (chance * 100.0f)) destroy = true;
                }

                if (destroy) {
                    int cx = gx / CHUNK_SIZE, cy = gy / CHUNK_SIZE, cz = gz / CHUNK_SIZE;
                    if (chunkMeshes[cx][cy][cz].isMeshing) {
                        continue; // Skip destroying if mesher thread is reading it
                    }
                    // Record adjacent stability nodes before deleting
                    const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                    for (int i = 0; i < 6; i++) {
                        int nx = gx + nb[i][0], ny = gy + nb[i][1], nz = gz + nb[i][2];
                        if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && nz >= 0 && nz < GRID_SIZE) {
                            if (getVoxel(nx, ny, nz) > 0) {
                                adjacentNodes.push_back((long long)nx << 40 | (long long)ny << 20 | (long long)nz);
                            }
                        }
                    }
                    setVoxel(gx, gy, gz, 0);
                    markChunkDirty(gx, gy, gz);
                }
            }
        }
    }

    // 3. Spawn the solid debris chunks
    for (int i = 0; i < numDebris; i++) {
        if (debrisChunks[i].voxels.empty()) continue;
        
        VoxelChunk shard;
        glm::vec3 avgPos = debrisChunks[i].sumPos / (float)debrisChunks[i].voxels.size();
        shard.center = avgPos;
        shard.life = 6.0f; // clean up after 6s
        
        for (const auto& v : debrisChunks[i].voxels) {
            glm::ivec3 localCoord(v.first.x, v.first.y, v.first.z);
            shard.voxels.push_back({localCoord, v.second});
        }
        
        glm::vec3 ejectionDir = glm::normalize(shard.center - pos);
        if (glm::length(shard.center - pos) < 0.01f) {
            ejectionDir = glm::vec3((rand()%200-100)*0.01f, 1.0f, (rand()%200-100)*0.01f);
        }
        float speed = 2.2f + (rand() % 100) * 0.035f;
        createPhysicsForChunk(shard, ejectionDir * speed + glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3((rand()%20-10)*1.8f));
        shard.id = nextChunkId++; activeChunks.push_back(std::move(shard));
    }

    // 4. Update the static chunk meshes and process island stability
    updateStaticMesh(glm::vec3(0,0,0), 1000.0f);

    std::sort(adjacentNodes.begin(), adjacentNodes.end());
    adjacentNodes.erase(std::unique(adjacentNodes.begin(), adjacentNodes.end()), adjacentNodes.end());

    for (long long packed : adjacentNodes) {
        int nx = (int)((packed >> 40) & 0xFFFFF), ny = (int)((packed >> 20) & 0xFFFFF), nz = (int)(packed & 0xFFFFF);
        if (getVoxel(nx, ny, nz) > 0) {
            stabilityNodes.push_back(glm::ivec3(nx, ny, nz));
        }
    }
    if (!stabilityNodes.empty()) {
        detectIslands(stabilityNodes);
    }
    
    spawnDust(pos, 45, 6);
    spawnSparks(pos, 16);

    for (auto& c : activeChunks) {
        float d = glm::distance(c.center, pos);
        float range = 2.5f * (voxelSize / 0.005f);
        if (d < range) {
            glm::vec3 pushDir = glm::normalize(c.center - pos + glm::vec3(0.0f, 0.2f, 0.0f));
            c.velocity += pushDir * (range - d) * 3.5f;
        }
    }

    applyExplosionToRagdolls(pos, 3.5f * (voxelSize / 0.005f), 15.0f);

    spawnDust(pos, 85, 6);
    spawnSparks(pos, 25);
}



