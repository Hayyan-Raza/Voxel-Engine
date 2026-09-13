#include "IslandDetection.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include <vector>
#include <glm/glm.hpp>
#include <set>
#include "../physics/Physics.h"
#include <cstring>
#include <unordered_map>
#include <unordered_set>

std::queue<std::vector<glm::ivec3>> islandTaskQueue;
std::mutex islandTaskMutex;
std::condition_variable islandTaskCV;

std::queue<DetachedIsland> islandResultQueue;
std::mutex islandResultMutex;

std::atomic<bool> islandThreadRunning(false);
std::thread islandWorkerThread;

void islandWorker() {
    const int PERFORMANCE_THRESHOLD = 80000;
    std::unordered_map<int, uint8_t> nodeStatus;
    std::unordered_set<int> clearedVoxels;

    while (islandThreadRunning) {
        std::vector<glm::ivec3> startNodes;
        {
            std::unique_lock<std::mutex> lock(islandTaskMutex);
            islandTaskCV.wait(lock, [] { return !islandTaskQueue.empty() || !islandThreadRunning; });
            if (!islandThreadRunning) break;
            
            startNodes = islandTaskQueue.front();
            islandTaskQueue.pop();
        }

        std::vector<DetachedIsland> localResults;
        nodeStatus.clear();
        clearedVoxels.clear();

        for (const auto& startPos : startNodes) {
            if (startPos.x < 0 || startPos.x >= GRID_SIZE || 
                startPos.y < 0 || startPos.y >= GRID_SIZE || 
                startPos.z < 0 || startPos.z >= GRID_SIZE) continue;

            int startIdx = startPos.x * GRID_SIZE * GRID_SIZE + startPos.y * GRID_SIZE + startPos.z;
            uint8_t voxelType = getVoxel(startPos.x, startPos.y, startPos.z);
            if (voxelType == 0 || voxelType == 8 || clearedVoxels.count(startIdx)) continue;
            if (nodeStatus[startIdx] != 0) continue;

            std::vector<glm::ivec3> cluster;
            std::vector<glm::ivec3> queue;
            queue.push_back(startPos);
            nodeStatus[startIdx] = 2;

            bool isStatic = false;
            size_t head = 0;

            while (head < queue.size()) {
                glm::ivec3 p = queue[head++];
                cluster.push_back(p);

                if (p.y <= 40) { isStatic = true; break; }
                if (queue.size() > PERFORMANCE_THRESHOLD) { isStatic = true; break; }

                const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                for (const auto& d : nb) {
                    glm::ivec3 n(p.x + d[0], p.y + d[1], p.z + d[2]);
                    if (n.x < 0 || n.x >= GRID_SIZE || n.y < 0 || n.y >= GRID_SIZE || n.z < 0 || n.z >= GRID_SIZE) continue;
                    int nIdx = n.x * GRID_SIZE * GRID_SIZE + n.y * GRID_SIZE + n.z;
                    
                    if (getVoxel(n.x, n.y, n.z) > 0 && getVoxel(n.x, n.y, n.z) != 8 && !clearedVoxels.count(nIdx) && nodeStatus[nIdx] == 0) {
                        nodeStatus[nIdx] = 2;
                        queue.push_back(n);
                    }
                }
            }

            if (isStatic) {
                for (const auto& v : cluster) {
                    int idx = v.x * GRID_SIZE * GRID_SIZE + v.y * GRID_SIZE + v.z;
                    nodeStatus[idx] = 1;
                }
                continue;
            }

            if (cluster.empty()) continue;

            glm::vec3 centroid(0.0f);
            for (const auto& v : cluster) centroid += glm::vec3(v.x, v.y, v.z);
            centroid /= (float)cluster.size();

            DetachedIsland island;
            island.center = centroid;
            for (const auto& v : cluster) {
                int idx = v.x * GRID_SIZE * GRID_SIZE + v.y * GRID_SIZE + v.z;
                island.voxels.push_back({v, getVoxel(v.x, v.y, v.z)});
                clearedVoxels.insert(idx); // Prevent duplicate detection
            }
            localResults.push_back(std::move(island));
        }

        if (!localResults.empty()) {
            std::lock_guard<std::mutex> resLock(islandResultMutex);
            for (auto& r : localResults) {
                islandResultQueue.push(std::move(r));
            }
        }
    }
}

void initIslandThread() {
    islandThreadRunning = true;
    islandWorkerThread = std::thread(islandWorker);
}

void stopIslandThread() {
    {
        std::lock_guard<std::mutex> lock(islandTaskMutex);
        islandThreadRunning = false;
    }
    islandTaskCV.notify_all();
    if (islandWorkerThread.joinable()) {
        islandWorkerThread.join();
    }
}

void detectIslands(const std::vector<glm::ivec3>& startNodes) {
    if (startNodes.empty()) return;
    std::lock_guard<std::mutex> lock(islandTaskMutex);
    islandTaskQueue.push(startNodes);
    islandTaskCV.notify_one();
}

void processIslandResults() {
    std::vector<DetachedIsland> batch;
    {
        std::lock_guard<std::mutex> lock(islandResultMutex);
        while (!islandResultQueue.empty()) {
            batch.push_back(std::move(islandResultQueue.front()));
            islandResultQueue.pop();
        }
    }

    for (auto& islandData : batch) {

        VoxelChunk island;
        island.center = islandData.center * voxelSize;
        island.life = 15.0f;

        for (const auto& pair : islandData.voxels) {
            glm::ivec3 v = pair.first;
            uint8_t type = pair.second;
            glm::ivec3 localOff(v.x - (int)islandData.center.x, v.y - (int)islandData.center.y, v.z - (int)islandData.center.z);
            island.voxels.push_back({localOff, type});
            
            if (getVoxel(v.x, v.y, v.z) != 0) {
                setVoxel(v.x, v.y, v.z, 0); 
                markChunkDirty(v.x, v.y, v.z);
            }
        }

        if (!island.voxels.empty()) {
            if (island.voxels.size() > 500) {
                // If it's massive, just delete it instead of freezing the game
                continue;
            } else if (island.voxels.size() > 200) {
                std::vector<VoxelChunk> subChunks;
                shatterChunk(island, island.center, subChunks);
                for (auto& sc : subChunks) {
                    createPhysicsForChunk(sc, glm::vec3(0, -0.02f, 0), glm::vec3(0));
                    sc.id = nextChunkId++; 
                    activeChunks.push_back(std::move(sc));
                }
            } else {
                createPhysicsForChunk(island, glm::vec3(0, -0.02f, 0), glm::vec3(0));
                island.id = nextChunkId++; 
                activeChunks.push_back(std::move(island));
            }
        }
    }
}
