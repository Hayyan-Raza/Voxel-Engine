#pragma once

#include <queue>
#include <mutex>
#include <glm/glm.hpp>
#include <unordered_set>

struct ivec3_hash_ws {
    std::size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

class WaterSimulator {
public:
    static WaterSimulator& getInstance() {
        static WaterSimulator instance;
        return instance;
    }

    void init();
    void update();
    void wakeUp(int x, int y, int z);

private:
    WaterSimulator() = default;
    
    std::queue<glm::ivec3> activeBlocks;
    std::unordered_set<glm::ivec3, ivec3_hash_ws> inQueue;
    std::mutex queueMutex;
    
    bool isAir(int x, int y, int z);
    bool isWaterBlock(int x, int y, int z);
    int getLevel(int x, int y, int z);
    void setWater(int x, int y, int z, int level);
    void clearWater(int x, int y, int z);
};
