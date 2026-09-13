#include "Lighting.h"
#include "World.h"
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <unordered_set>

struct LightNode {
    int x, y, z;
};

struct LightRemovalNode {
    int x, y, z;
    uint8_t val;
};

static std::queue<LightNode> lightAddQueue;
static std::queue<LightRemovalNode> lightRemovalQueue;
static std::mutex lightingMutex;
static std::condition_variable lightingCV;
static bool lightingRunning = false;
static std::thread lightingThread;

static const int dirs[6][3] = {
    {1, 0, 0}, {-1, 0, 0},
    {0, 1, 0}, {0, -1, 0},
    {0, 0, 1}, {0, 0, -1}
};

bool isOpaque(uint8_t type) {
    if (type == 0 || type == 8 || type == 9 || type == 10 || type == 11 || type == 12 || type == 13 || type == 14 || type == 5 || type == 21 || type == 22 || type == 26 || type == 27 || type == 28) {
        return false;
    }
    return true;
}

uint8_t getEmittedLight(uint8_t type) {
    if (type == 31) return 15;
    return 0;
}

void processLighting() {
    std::unordered_set<glm::ivec3, ivec3_hash> chunksToUpdate;

    while (true) {
        std::queue<LightNode> addQ;
        std::queue<LightRemovalNode> remQ;

        {
            std::unique_lock<std::mutex> lock(lightingMutex);
            lightingCV.wait(lock, [] { return !lightAddQueue.empty() || !lightRemovalQueue.empty() || !lightingRunning; });
            
            if (!lightingRunning && lightAddQueue.empty() && lightRemovalQueue.empty()) break;
            
            std::swap(addQ, lightAddQueue);
            std::swap(remQ, lightRemovalQueue);
        }

        chunksToUpdate.clear();

        // 1. Process Removal Queue
        while (!remQ.empty()) {
            LightRemovalNode node = remQ.front();
            remQ.pop();

            chunksToUpdate.insert(glm::ivec3(node.x / CHUNK_SIZE, node.y / CHUNK_SIZE, node.z / CHUNK_SIZE));

            for (int i = 0; i < 6; i++) {
                int nx = node.x + dirs[i][0];
                int ny = node.y + dirs[i][1];
                int nz = node.z + dirs[i][2];

                uint8_t neighborLight = getLight(nx, ny, nz);
                if (neighborLight != 0 && neighborLight < node.val) {
                    setLight(nx, ny, nz, 0);
                    remQ.push({nx, ny, nz, neighborLight});
                } else if (neighborLight >= node.val) {
                    addQ.push({nx, ny, nz});
                }
            }
        }

        // 2. Process Add Queue
        while (!addQ.empty()) {
            LightNode node = addQ.front();
            addQ.pop();

            chunksToUpdate.insert(glm::ivec3(node.x / CHUNK_SIZE, node.y / CHUNK_SIZE, node.z / CHUNK_SIZE));

            uint8_t light = getLight(node.x, node.y, node.z);
            for (int i = 0; i < 6; i++) {
                int nx = node.x + dirs[i][0];
                int ny = node.y + dirs[i][1];
                int nz = node.z + dirs[i][2];

                uint8_t neighborType = getVoxel(nx, ny, nz);
                if (isOpaque(neighborType)) continue;

                uint8_t neighborLight = getLight(nx, ny, nz);
                uint8_t attenuation = (neighborType == 8) ? 2 : 1; 
                
                if (light > attenuation && neighborLight < light - attenuation) {
                    setLight(nx, ny, nz, light - attenuation);
                    addQ.push({nx, ny, nz});
                }
            }
        }

        // 3. Mark chunks dirty
        for (const auto& key : chunksToUpdate) {
            markChunkDirty(key.x * CHUNK_SIZE, key.y * CHUNK_SIZE, key.z * CHUNK_SIZE);
        }
    }
}

void OnBlockPlaced(int x, int y, int z, uint8_t blockType) {
    uint8_t emitted = getEmittedLight(blockType);
    
    std::lock_guard<std::mutex> lock(lightingMutex);
    if (emitted > 0) {
        setLight(x, y, z, emitted);
        lightAddQueue.push({x, y, z});
    } else if (isOpaque(blockType)) {
        uint8_t oldLight = getLight(x, y, z);
        if (oldLight > 0) {
            setLight(x, y, z, 0);
            lightRemovalQueue.push({x, y, z, oldLight});
        }
    }
    lightingCV.notify_one();
}

void OnBlockRemoved(int x, int y, int z) {
    uint8_t oldLight = getLight(x, y, z);
    
    std::lock_guard<std::mutex> lock(lightingMutex);
    setLight(x, y, z, 0);
    
    if (oldLight > 0) {
        lightRemovalQueue.push({x, y, z, oldLight});
    }

    for (int i = 0; i < 6; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        int nz = z + dirs[i][2];
        lightAddQueue.push({nx, ny, nz}); 
    }
    
    lightingCV.notify_one();
}

void initLightingThread() {
    lightingRunning = true;
    lightingThread = std::thread(processLighting);
}

void stopLightingThread() {
    {
        std::lock_guard<std::mutex> lock(lightingMutex);
        lightingRunning = false;
    }
    lightingCV.notify_all();
    if (lightingThread.joinable()) {
        lightingThread.join();
    }
}
