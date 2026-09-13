#include "ChunkManager.h"
#include "World.h" // For OnBlockRemoved / OnBlockPlaced events
#include "Lighting.h"
#include <mutex>
#include <atomic>

std::unordered_map<glm::ivec3, ChunkData*, ivec3_hash> chunkManager;
std::shared_mutex chunkMutex;
std::atomic<uint32_t> worldGenerationId = 0;

thread_local glm::ivec3 lastVoxelChunkKey(-1000, -1000, -1000);
thread_local ChunkData* lastVoxelChunkData = nullptr;
thread_local uint32_t lastVoxelGenerationId = 0;

thread_local glm::ivec3 lastLightChunkKey(-1000, -1000, -1000);
thread_local ChunkData* lastLightChunkData = nullptr;
thread_local uint32_t lastLightGenerationId = 0;

uint8_t getVoxel(int x, int y, int z) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return 0;
    
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    glm::ivec3 key(cx, cy, cz);
    
    if (key == lastVoxelChunkKey && lastVoxelGenerationId == worldGenerationId.load()) {
        if (lastVoxelChunkData) return lastVoxelChunkData->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE];
        return 0;
    }
    
    std::shared_lock<std::shared_mutex> lock(chunkMutex);
    auto it = chunkManager.find(key);
    lastVoxelChunkKey = key;
    lastVoxelGenerationId = worldGenerationId.load();
    if (it != chunkManager.end()) {
        lastVoxelChunkData = it->second;
        return lastVoxelChunkData->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE];
    }
    lastVoxelChunkData = nullptr;
    return 0; // Empty air chunk
}

void setVoxel(int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return;
    
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    glm::ivec3 key(cx, cy, cz);
    
    uint8_t oldType = getVoxel(x, y, z);
    if (oldType == type) return;

    {
        std::unique_lock<std::shared_mutex> lock(chunkMutex);
        auto it = chunkManager.find(key);
        if (it == chunkManager.end()) {
            if (type == 0) return; // Don't allocate a chunk just for air
            ChunkData* newChunk = new ChunkData();
            newChunk->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE] = type;
            chunkManager[key] = newChunk;
            worldGenerationId++;
        } else {
            it->second->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE] = type;
        }
    }

    if (type == 0) {
        OnBlockRemoved(x, y, z);
    } else {
        if (oldType != 0) {
            OnBlockRemoved(x, y, z);
        }
        OnBlockPlaced(x, y, z, type);
    }
}

void setVoxelFast(int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return;
    
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    glm::ivec3 key(cx, cy, cz);
    
    uint8_t oldType = getVoxel(x, y, z);
    if (oldType == type) return;

    std::unique_lock<std::shared_mutex> lock(chunkMutex);
    auto it = chunkManager.find(key);
    if (it == chunkManager.end()) {
        if (type == 0) return; // Don't allocate a chunk just for air
        ChunkData* newChunk = new ChunkData();
        newChunk->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE] = type;
        chunkManager[key] = newChunk;
        worldGenerationId++;
    } else {
        it->second->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE] = type;
    }
}

uint8_t getLight(int x, int y, int z) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return 0;
    
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    glm::ivec3 key(cx, cy, cz);
    
    if (key == lastLightChunkKey && lastLightGenerationId == worldGenerationId.load()) {
        if (lastLightChunkData) {
            uint8_t packed = lastLightChunkData->lightData[x % CHUNK_SIZE][y % CHUNK_SIZE][(z % CHUNK_SIZE) / 2];
            return ((z % CHUNK_SIZE) % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
        }
        return 0;
    }
    
    std::shared_lock<std::shared_mutex> lock(chunkMutex);
    auto it = chunkManager.find(key);
    lastLightChunkKey = key;
    lastLightGenerationId = worldGenerationId.load();
    if (it != chunkManager.end()) {
        lastLightChunkData = it->second;
        uint8_t packed = lastLightChunkData->lightData[x % CHUNK_SIZE][y % CHUNK_SIZE][(z % CHUNK_SIZE) / 2];
        return ((z % CHUNK_SIZE) % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
    }
    lastLightChunkData = nullptr;
    return 0; 
}

void setLight(int x, int y, int z, uint8_t val) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return;
    
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    glm::ivec3 key(cx, cy, cz);
    
    std::unique_lock<std::shared_mutex> lock(chunkMutex);
    auto it = chunkManager.find(key);
    if (it == chunkManager.end()) {
        if (val == 0) return; // Don't allocate a chunk just for 0 light
        ChunkData* newChunk = new ChunkData();
        int lx = x % CHUNK_SIZE;
        int ly = y % CHUNK_SIZE;
        int lz = z % CHUNK_SIZE;
        if (lz % 2 == 0) {
            newChunk->lightData[lx][ly][lz / 2] = (newChunk->lightData[lx][ly][lz / 2] & 0xF0) | (val & 0x0F);
        } else {
            newChunk->lightData[lx][ly][lz / 2] = (newChunk->lightData[lx][ly][lz / 2] & 0x0F) | ((val & 0x0F) << 4);
        }
        chunkManager[key] = newChunk;
        worldGenerationId++;
    } else {
        int lx = x % CHUNK_SIZE;
        int ly = y % CHUNK_SIZE;
        int lz = z % CHUNK_SIZE;
        if (lz % 2 == 0) {
            it->second->lightData[lx][ly][lz / 2] = (it->second->lightData[lx][ly][lz / 2] & 0xF0) | (val & 0x0F);
        } else {
            it->second->lightData[lx][ly][lz / 2] = (it->second->lightData[lx][ly][lz / 2] & 0x0F) | ((val & 0x0F) << 4);
        }
    }
}

void clearWorld() {
    worldGenerationId++;
    lastVoxelChunkKey = glm::ivec3(-1000, -1000, -1000);
    lastVoxelChunkData = nullptr;
    lastLightChunkKey = glm::ivec3(-1000, -1000, -1000);
    lastLightChunkData = nullptr;
    {
        std::unique_lock<std::shared_mutex> lock(chunkMutex);
        for (auto& pair : chunkManager) {
            delete pair.second;
        }
        chunkManager.clear();
    }
}
