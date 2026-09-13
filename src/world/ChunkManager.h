#pragma once
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <glm/glm.hpp>

#define CHUNK_SIZE 32
#define WORLD_HEIGHT_CHUNKS 8
#define WORLD_HEIGHT (WORLD_HEIGHT_CHUNKS * CHUNK_SIZE)

inline int getChunkCoord(int pos) {
    return pos < 0 ? ((pos + 1) / CHUNK_SIZE) - 1 : pos / CHUNK_SIZE;
}

inline int getLocalIdx(int pos) {
    int mod = pos % CHUNK_SIZE;
    return mod < 0 ? mod + CHUNK_SIZE : mod;
}

struct ivec3_hash {
    std::size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

struct ivec2_hash {
    std::size_t operator()(const glm::ivec2& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

struct ChunkData {
    uint8_t blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE] = {0};
    uint8_t lightData[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE / 2] = {0};
};

extern std::unordered_map<glm::ivec3, ChunkData*, ivec3_hash> chunkManager;
extern std::shared_mutex chunkMutex;
extern std::atomic<uint32_t> worldGenerationId;

uint8_t getVoxel(int x, int y, int z);
void setVoxel(int x, int y, int z, uint8_t type);
void setVoxelFast(int x, int y, int z, uint8_t type);
uint8_t getLight(int x, int y, int z);
void setLight(int x, int y, int z, uint8_t val);
void clearWorld();
