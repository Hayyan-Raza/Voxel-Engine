#include "WaterSimulator.h"
#include "ChunkManager.h"
#include "World.h"
#include <vector>
#include <iostream>

void WaterSimulator::init() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<glm::ivec3> empty;
    std::swap(activeBlocks, empty);
    inQueue.clear();
}

void WaterSimulator::wakeUp(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return;
    
    std::lock_guard<std::mutex> lock(queueMutex);
    glm::ivec3 pos(x, y, z);
    
    if (inQueue.find(pos) == inQueue.end()) {
        activeBlocks.push(pos);
        inQueue.insert(pos);
    }
}

bool WaterSimulator::isAir(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return false;
    return getVoxel(x, y, z) == 0;
}

bool WaterSimulator::isWaterBlock(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return false;
    return isWater(getVoxel(x, y, z));
}

int WaterSimulator::getLevel(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return -1;
    uint8_t type = getVoxel(x, y, z);
    if (!isWater(type)) return -1;
    return getWaterLevel(type);
}

void WaterSimulator::setWater(int x, int y, int z, int level) {
    uint8_t type = getWaterTypeForLevel(level);
    setVoxel(x, y, z, type);
    markChunkDirty(x, y, z);
    wakeUp(x, y, z);
    wakeUp(x + 1, y, z);
    wakeUp(x - 1, y, z);
    wakeUp(x, y, z + 1);
    wakeUp(x, y, z - 1);
    wakeUp(x, y - 1, z);
}

void WaterSimulator::clearWater(int x, int y, int z) {
    setVoxel(x, y, z, 0);
    markChunkDirty(x, y, z);
    wakeUp(x + 1, y, z);
    wakeUp(x - 1, y, z);
    wakeUp(x, y, z + 1);
    wakeUp(x, y, z - 1);
    wakeUp(x, y - 1, z);
}

void WaterSimulator::update() {
    std::vector<glm::ivec3> toProcess;
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        // Process up to 1000 blocks per frame
        int count = 0;
        while (!activeBlocks.empty() && count < 1000) {
            glm::ivec3 pos = activeBlocks.front();
            activeBlocks.pop();
            inQueue.erase(pos);
            toProcess.push_back(pos);
            count++;
        }
    }
    
    for (const auto& pos : toProcess) {
        if (!isWaterBlock(pos.x, pos.y, pos.z)) continue;
        
        int currentLevel = getLevel(pos.x, pos.y, pos.z);
        
        // 1. Decay check: If this is flowing water (level > 0), check if it's supported
        if (currentLevel > 0) {
            bool supported = false;
            // Supported by water directly above
            if (isWaterBlock(pos.x, pos.y + 1, pos.z)) {
                supported = true;
            } else {
                // Or supported by an adjacent water block with a LOWER level
                glm::ivec3 dirs[4] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
                for (auto& dir : dirs) {
                    int neighborLevel = getLevel(pos.x + dir.x, pos.y, pos.z + dir.z);
                    if (neighborLevel != -1 && neighborLevel < currentLevel) {
                        supported = true;
                        break;
                    }
                }
            }
            
            if (!supported) {
                clearWater(pos.x, pos.y, pos.z);
                continue; // Block was removed, stop processing
            }
        }
        
        // 2. Spread downwards
        if (isAir(pos.x, pos.y - 1, pos.z)) {
            // Create falling water (level 1)
            setWater(pos.x, pos.y - 1, pos.z, 1);
        } else if (isWaterBlock(pos.x, pos.y - 1, pos.z)) {
            // If water is below, and its level is > 1, override it to 1 to simulate continuous falling
            if (getLevel(pos.x, pos.y - 1, pos.z) > 1) {
                setWater(pos.x, pos.y - 1, pos.z, 1);
            }
        } else {
            // Blocked below by solid, try to spread sideways
            if (currentLevel < 7) {
                glm::ivec3 dirs[4] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
                for (auto& dir : dirs) {
                    glm::ivec3 n = pos + dir;
                    if (isAir(n.x, n.y, n.z)) {
                        setWater(n.x, n.y, n.z, currentLevel + 1);
                    } else if (isWaterBlock(n.x, n.y, n.z)) {
                        // If it's water but with a higher level, override it (flow into it)
                        if (getLevel(n.x, n.y, n.z) > currentLevel + 1) {
                            setWater(n.x, n.y, n.z, currentLevel + 1);
                        }
                    }
                }
            }
        }
    }
}
