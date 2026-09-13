#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

struct DetachedIsland {
    glm::vec3 center;
    std::vector<std::pair<glm::ivec3, uint8_t>> voxels;
};

extern std::queue<std::vector<glm::ivec3>> islandTaskQueue;
extern std::mutex islandTaskMutex;
extern std::condition_variable islandTaskCV;

extern std::queue<DetachedIsland> islandResultQueue;
extern std::mutex islandResultMutex;

extern std::atomic<bool> islandThreadRunning;

void initIslandThread();
void stopIslandThread();
void detectIslands(const std::vector<glm::ivec3>& startNodes);
void processIslandResults();
