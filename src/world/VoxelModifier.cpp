#include "VoxelModifier.h"
#include "../world/World.h"
#include "../physics/Physics.h"
#include "../physics/IslandDetection.h"
#include "../particles/Particles.h"
#include "../core/Globals.h"

void applyTerrainDestruction(const glm::ivec3& hitVox, const glm::vec3& hitWorldPos, int radius, const glm::vec3& ejectBaseDir) {
    uint8_t hitVType = getVoxel(hitVox.x, hitVox.y, hitVox.z);
    std::vector<glm::ivec3> stabilityNodes;

    VoxelChunk debrisChunk;
    glm::vec3 sumPos(0.0f);

    for (int dx = -(radius+1); dx <= (radius+1); dx++) {
        for (int dy = -(radius+1); dy <= (radius+1); dy++) {
            for (int dz = -(radius+1); dz <= (radius+1); dz++) {
                if (radius > 0) {
                    float dist = sqrt(static_cast<float>(dx*dx + dy*dy + dz*dz));
                    // 3D pseudo-noise for jagged fracture shape
                    float noise = (sin((hitVox.x + dx) * 1.5f) * cos((hitVox.y + dy) * 2.1f) * sin((hitVox.z + dz) * 1.3f));
                    float threshold = radius + noise * 1.5f;
                    
                    if (dist > threshold) continue;
                    
                    // Randomly skip blocks on the edge for micro fracture physics
                    if (dist > threshold - 1.0f && (rand() % 100) < 40) continue;
                }

                int gx = hitVox.x + dx, gy = hitVox.y + dy, gz = hitVox.z + dz;
                if (gy < 0 || gy >= WORLD_HEIGHT) continue;
                uint8_t vType = getVoxel(gx, gy, gz);
                if (vType == 3 && gy <= 8) continue; // Bedrock protection
                if (vType == 8) continue; // Ignore water

                if (vType > 0) {
                    
                    debrisChunk.voxels.push_back(std::make_pair(glm::ivec3(gx, gy, gz), vType));
                    resourceInventory[vType]++; // Harvest item
                    sumPos += glm::vec3(gx, gy, gz) * voxelSize;

                    const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                    for (int i = 0; i < 6; i++) {
                        int nx = gx + nb[i][0], ny = gy + nb[i][1], nz = gz + nb[i][2];
                        if (ny >= 0 && ny < WORLD_HEIGHT && getVoxel(nx, ny, nz) > 0)
                            stabilityNodes.push_back(glm::ivec3(nx,ny,nz));
                    }
                    setVoxel(gx, gy, gz, 0);
                    markChunkDirty(gx, gy, gz);
                }
            }
        }
    }

    if (!debrisChunk.voxels.empty()) {
        glm::vec3 centroid = sumPos / (float)debrisChunk.voxels.size();
        debrisChunk.center = centroid;
        debrisChunk.rotation = glm::quat(1,0,0,0);
        debrisChunk.life = 4.0f * chunkLifeMultiplier;
        
        glm::ivec3 localCentroid((int)round(centroid.x / voxelSize), (int)round(centroid.y / voxelSize), (int)round(centroid.z / voxelSize));
        for (auto& v : debrisChunk.voxels) {
            v.first -= localCentroid;
        }

        glm::vec3 ejectDir = glm::normalize(ejectBaseDir + glm::vec3((rand()%200-100)*0.1f, (rand()%200-100)*0.1f, (rand()%200-100)*0.1f));
        debrisChunk.velocity = ejectDir * 1.5f + glm::vec3(0.0f, 1.0f, 0.0f);
        
        if (debrisChunk.voxels.size() > 4) {
            std::vector<VoxelChunk> outChunks;
            shatterChunk(debrisChunk, hitWorldPos, outChunks);
            for (auto& c : outChunks) {
                c.velocity += debrisChunk.velocity;
                c.life = (4.0f + (rand()%200)/100.0f) * chunkLifeMultiplier;
                createPhysicsForChunk(c, c.velocity, glm::vec3((rand()%20-10)*0.2f, (rand()%20-10)*0.2f, (rand()%20-10)*0.2f));
                c.id = nextChunkId++; activeChunks.push_back(std::move(c));
            }
        } else {
            createPhysicsForChunk(debrisChunk, debrisChunk.velocity, glm::vec3((rand()%20-10)*0.2f, (rand()%20-10)*0.2f, (rand()%20-10)*0.2f));
            debrisChunk.id = nextChunkId++; activeChunks.push_back(std::move(debrisChunk));
        }
    }

    updateStaticMesh(glm::vec3(0,0,0));
    if (!stabilityNodes.empty()) detectIslands(stabilityNodes);
    spawnDust(hitWorldPos, 1, hitVType);
    if (hitVType == 3) spawnSparks(hitWorldPos, 1);
}

void extractTerrainToChunk(const glm::ivec3& hitVox, const glm::vec3& pullDir) {
    uint8_t hitVType = getVoxel(hitVox.x, hitVox.y, hitVox.z);
    std::vector<glm::ivec3> stabilityNodes;

    VoxelChunk debrisChunk;
    glm::vec3 sumPos(0.0f);

    if (hitVType > 0 && !(hitVType == 3 && hitVox.y <= 8)) {
        debrisChunk.voxels.push_back(std::make_pair(hitVox, hitVType));
        resourceInventory[hitVType]++; // Harvest item
        sumPos = glm::vec3(hitVox) * voxelSize;

        const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int i = 0; i < 6; i++) {
            int nx = hitVox.x + nb[i][0], ny = hitVox.y + nb[i][1], nz = hitVox.z + nb[i][2];
            if (ny >= 0 && ny < WORLD_HEIGHT && getVoxel(nx, ny, nz) > 0)
                stabilityNodes.push_back(glm::ivec3(nx,ny,nz));
        }
        setVoxel(hitVox.x, hitVox.y, hitVox.z, 0);
        markChunkDirty(hitVox.x, hitVox.y, hitVox.z);
        rebuildChunkSync(getChunkCoord(hitVox.x), getChunkCoord(hitVox.y), getChunkCoord(hitVox.z));
    }

    if (!debrisChunk.voxels.empty()) {
        debrisChunk.center = sumPos + (pullDir * voxelSize * 2.0f); // offset to prevent clipping
        debrisChunk.rotation = glm::quat(1,0,0,0);
        debrisChunk.life = 4.0f; // Life gets updated dynamically later

        for (auto& v : debrisChunk.voxels) {
            v.first = glm::ivec3(0,0,0); // only one voxel anyway
        }

        debrisChunk.velocity = pullDir * 1.5f; // Gentle pull towards player initially
        
        createPhysicsForChunk(debrisChunk, debrisChunk.velocity, glm::vec3(0.0f));
        debrisChunk.id = nextChunkId++; activeChunks.push_back(std::move(debrisChunk));
    }

    updateStaticMesh(glm::vec3(0,0,0));
    if (!stabilityNodes.empty()) detectIslands(stabilityNodes);
}
