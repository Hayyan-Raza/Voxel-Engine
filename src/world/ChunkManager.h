#pragma once
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <glm/glm.hpp>

#define GRID_SIZE 1024
#define CHUNK_SIZE 32
#define CHUNKS_PER_AXIS (GRID_SIZE / CHUNK_SIZE)

struct ivec3_hash {
    std::size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
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
