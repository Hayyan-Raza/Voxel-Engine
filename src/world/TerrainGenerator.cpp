#include "TerrainGenerator.h"
#include "World.h"
#include "TreeGenerator.h"
#include "PerlinNoise.h"
#include "../core/Globals.h"
#include <cstdlib>
#include <cmath>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <iostream>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

#include <unordered_set>

static std::vector<glm::ivec2> genQueue;
static std::unordered_set<glm::ivec2, ivec2_hash> generatingColumns;
static std::mutex genMutex;
static std::condition_variable genCV;
static bool genRunning = false;
static std::vector<std::thread> genThreads;
static std::atomic<int> columnsRemaining(0);
static unsigned int globalSeed = 0;

bool isTerrainGenerating() {
    return columnsRemaining > 0;
}

void queueChunkGeneration(int cx, int cz) {
    std::lock_guard<std::mutex> lock(genMutex);
    glm::ivec2 task(cx, cz);
    if (generatingColumns.find(task) != generatingColumns.end()) return; // Already generating
    
    // Check if already in chunkManager
    bool alreadyLoaded = false;
    {
        std::shared_lock<std::shared_mutex> clock(chunkMutex);
        if (chunkManager.find(glm::ivec3(cx, 0, cz)) != chunkManager.end()) {
            alreadyLoaded = true;
        }
    }
    if (alreadyLoaded) return;

    generatingColumns.insert(task);
    genQueue.push_back(task);
    columnsRemaining++;
    genCV.notify_one();
}

std::queue<SaveTask> saveQueue;
std::mutex saveMutex;
std::condition_variable saveCV;
bool saveRunning = true;
std::thread saveThread;

void saveWorker() {
    while (true) {
        SaveTask task;
        {
            std::unique_lock<std::mutex> lock(saveMutex);
            saveCV.wait(lock, [] { return !saveQueue.empty() || !saveRunning; });
            if (!saveRunning && saveQueue.empty()) break;
            task = saveQueue.front();
            saveQueue.pop();
        }

        std::string filename = "world_data/col_" + std::to_string(task.cx) + "_" + std::to_string(task.cz) + ".bin";
        std::string tempFilename = filename + ".tmp";
        std::ofstream outFile(tempFilename, std::ios::binary);
        if (outFile.is_open()) {
            for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                uint8_t hasChunk = (task.chunks[cy] != nullptr) ? 1 : 0;
                outFile.write((char*)&hasChunk, 1);
                if (hasChunk) {
                    outFile.write((char*)task.chunks[cy]->blocks, sizeof(ChunkData::blocks));
                    delete task.chunks[cy];
                }
            }
            outFile.close();
            std::error_code ec;
            std::filesystem::rename(tempFilename, filename, ec);
            if (ec) {
                // Fallback if rename fails
                std::filesystem::remove(filename, ec);
                std::filesystem::rename(tempFilename, filename, ec);
            }
        } else {
            // If we couldn't open the file, we still need to delete the memory!
            for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                if (task.chunks[cy]) delete task.chunks[cy];
            }
        }
    }
}

void queueChunkSave(const SaveTask& task) {
    {
        std::lock_guard<std::mutex> lock(saveMutex);
        saveQueue.push(task);
    }
    saveCV.notify_one();
}

void initSaveThread() {
    saveRunning = true;
    saveThread = std::thread(saveWorker);
}

void stopSaveThread() {
    {
        std::lock_guard<std::mutex> lock(saveMutex);
        saveRunning = false;
    }
    saveCV.notify_all();
    if (saveThread.joinable()) {
        saveThread.join();
    }
}


static void generateVoxelGrassBush(int cx, int baseY, int cz) {
    const int numBlades = 16 + (rand() % 8);
    for (int i = 0; i < numBlades; i++) {
        float angle = (i * 6.2831853f / numBlades) + ((rand() % 100) * 0.005f);
        float radius = 0.8f + (rand() % 100) * 0.018f;
        int bx = cx + (int)round(cos(angle) * radius);
        int bz = cz + (int)round(sin(angle) * radius);
        
        if (bx < cx * CHUNK_SIZE || bx >= (cx + 1) * CHUNK_SIZE || bz < cz * CHUNK_SIZE || bz >= (cz + 1) * CHUNK_SIZE) continue;
        uint8_t groundType = getVoxel(bx, baseY, bz);
        if (groundType != 1 && groundType != 13 && groundType != 14) continue;

        int height = 4 + (rand() % 5);
        uint8_t gType = (rand() % 3 == 0) ? 21 : ((rand() % 2 == 0) ? 22 : 5);

        int curX = bx;
        int curZ = bz;
        for (int h = 1; h <= height; h++) {
            int y = baseY + h;
            if (y >= WORLD_HEIGHT) break;
            
            if (h >= height - 2 && (rand() % 2 == 0)) {
                curX += (cos(angle) > 0 ? 1 : -1);
                curZ += (sin(angle) > 0 ? 1 : -1);
                if (curX < cx * CHUNK_SIZE || curX >= (cx + 1) * CHUNK_SIZE || curZ < cz * CHUNK_SIZE || curZ >= (cz + 1) * CHUNK_SIZE) break;
            }
            setVoxelFast(curX, y, curZ, gType);
        }
    }
}

static void generateVoxelFlower(int x, int baseY, int z, int flowerIndex) {
    uint8_t groundType = getVoxel(x, baseY, z);
    if (groundType != 1 && groundType != 13 && groundType != 14) return;

    switch (flowerIndex % 7) {
        case 0: { // Allium (Purple Globe)
            int stemH = 7;
            for (int h = 1; h <= stemH; h++) setVoxelFast(x, baseY + h, z, 5);
            setVoxelFast(x + 1, baseY + 2, z, 5);
            setVoxelFast(x - 1, baseY + 3, z, 5);
            int headCenterY = baseY + stemH + 2;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        setVoxelFast(x + dx, headCenterY + dy, z + dz, 11);
                    }
                }
            }
            break;
        }
        case 1: { // Daisy (White Petals + Yellow Center)
            int stemH = 6;
            for (int h = 1; h <= stemH; h++) setVoxelFast(x, baseY + h, z, 5);
            setVoxelFast(x + 1, baseY + 2, z, 5);
            setVoxelFast(x, baseY + 2, z - 1, 5);
            int fy = baseY + stemH + 1;
            setVoxelFast(x, fy, z, 10);
            setVoxelFast(x + 1, fy, z, 12);
            setVoxelFast(x - 1, fy, z, 12);
            setVoxelFast(x, fy + 1, z, 12);
            setVoxelFast(x, fy - 1, z, 12);
            setVoxelFast(x + 1, fy + 1, z, 12);
            setVoxelFast(x - 1, fy + 1, z, 12);
            setVoxelFast(x + 1, fy - 1, z, 12);
            setVoxelFast(x - 1, fy - 1, z, 12);
            break;
        }
        case 2: { // Red Rose / Poppy (Curved Stem + Red Bloom)
            int fy = baseY;
            setVoxelFast(x, fy + 1, z, 5);
            setVoxelFast(x, fy + 2, z, 5);
            setVoxelFast(x + 1, fy + 3, z, 5);
            setVoxelFast(x + 1, fy + 4, z, 5);
            setVoxelFast(x + 2, fy + 5, z, 5);
            setVoxelFast(x - 1, fy + 2, z, 5);
            setVoxelFast(x + 3, fy + 4, z + 1, 5);
            int rx = x + 2, ry = fy + 6;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dy = 0; dy <= 1; dy++) {
                        setVoxelFast(rx + dx, ry + dy, z + dz, 9);
                    }
                }
            }
            break;
        }
        case 3: { // Lavender Spire
            int stemH = 4;
            for (int h = 1; h <= stemH; h++) setVoxelFast(x, baseY + h, z, 5);
            setVoxelFast(x + 1, baseY + 1, z, 5);
            setVoxelFast(x - 1, baseY + 2, z, 5);
            for (int s = 0; s < 6; s++) {
                int sy = baseY + stemH + 1 + s;
                setVoxelFast(x, sy, z, 11);
                if (s % 2 == 1) {
                    setVoxelFast(x + 1, sy, z, 11);
                    setVoxelFast(x - 1, sy, z, 11);
                    setVoxelFast(x, sy, z + 1, 11);
                    setVoxelFast(x, sy, z - 1, 11);
                }
            }
            break;
        }
        case 4: { // Cactus with Pink Blossoms
            int trunkH = 7;
            for (int dx = 0; dx <= 1; dx++) {
                for (int dz = 0; dz <= 1; dz++) {
                    for (int h = 1; h <= trunkH; h++) {
                        setVoxelFast(x + dx, baseY + h, z + dz, 5);
                    }
                }
            }
            setVoxelFast(x - 1, baseY + 3, z, 5);
            setVoxelFast(x - 1, baseY + 4, z, 5);
            setVoxelFast(x + 2, baseY + 4, z + 1, 5);
            setVoxelFast(x + 2, baseY + 5, z + 1, 5);
            setVoxelFast(x, baseY + trunkH + 1, z, 12);
            setVoxelFast(x + 1, baseY + trunkH + 1, z + 1, 12);
            setVoxelFast(x - 1, baseY + 5, z, 12);
            break;
        }
        case 5: { // Sunflower (Yellow Petals + Dark Center)
            int stemH = 7;
            for (int h = 1; h <= stemH; h++) setVoxelFast(x, baseY + h, z, 5);
            setVoxelFast(x + 1, baseY + 3, z, 5);
            setVoxelFast(x - 1, baseY + 4, z, 5);
            int fy = baseY + stemH + 2;
            setVoxelFast(x, fy, z, 26);
            setVoxelFast(x + 1, fy, z, 10);
            setVoxelFast(x - 1, fy, z, 10);
            setVoxelFast(x, fy + 1, z, 10);
            setVoxelFast(x, fy - 1, z, 10);
            setVoxelFast(x + 1, fy + 1, z, 10);
            setVoxelFast(x - 1, fy + 1, z, 10);
            setVoxelFast(x + 1, fy - 1, z, 10);
            setVoxelFast(x - 1, fy - 1, z, 10);
            break;
        }
        case 6: { // Simple 4-petal flower flat on ground
            setVoxelFast(x + 1, baseY + 1, z, 9);
            setVoxelFast(x - 1, baseY + 1, z, 9);
            setVoxelFast(x, baseY + 1, z + 1, 9);
            setVoxelFast(x, baseY + 1, z - 1, 9);
            setVoxelFast(x, baseY + 1, z, 10); // Yellow center
            break;
        }
    }
}

static void generationWorker() {
    while (true) {
        glm::ivec2 task;
        {
            std::unique_lock<std::mutex> lock(genMutex);
            genCV.wait(lock, [] { return !genQueue.empty() || !genRunning; });
            if (!genRunning && genQueue.empty()) break;
            if (genQueue.empty()) continue;
            
            int px = playerCurrentChunkX.load(std::memory_order_relaxed);
            int pz = playerCurrentChunkZ.load(std::memory_order_relaxed);
            
            auto bestIt = genQueue.begin();
            int bestDistSq = 999999999;
            for (auto it = genQueue.begin(); it != genQueue.end(); ++it) {
                int dx = it->x - px;
                int dz = it->y - pz;
                int distSq = dx * dx + dz * dz;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestIt = it;
                }
            }
            
            task = *bestIt;
            genQueue.erase(bestIt);
            
            int adx = std::abs(task.x - px);
            int adz = std::abs(task.y - pz);
            if (std::max(adx, adz) > renderDistanceChunks + 3) {
                generatingColumns.erase(task);
                columnsRemaining--;
                continue; // Skip generating this stale chunk
            }
        }

        int cx = task.x;
        int cz = task.y;

        ChunkData* localColumn[WORLD_HEIGHT_CHUNKS] = {nullptr};
        bool loadedFromDisk = false;
        
        std::string filename = "world_data/col_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
        std::ifstream inFile(filename, std::ios::binary);
        if (inFile.is_open()) {
            bool readFailed = false;
            for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                uint8_t hasChunk = 0;
                inFile.read((char*)&hasChunk, 1);
                if (inFile.fail()) { readFailed = true; break; }
                if (hasChunk) {
                    localColumn[cy] = new ChunkData();
                    inFile.read((char*)localColumn[cy]->blocks, sizeof(ChunkData::blocks));
                    if (inFile.fail()) { readFailed = true; break; }
                }
            }
            inFile.close();
            
            if (readFailed) {
                for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                    if (localColumn[cy]) { delete localColumn[cy]; localColumn[cy] = nullptr; }
                }
            } else {
                loadedFromDisk = true;
            }
        }

        struct DecoratorSpot { int x, y, z; int type; };
        std::vector<DecoratorSpot> decorators;
        
        if (!loadedFromDisk) {
            PerlinNoise noise(globalSeed);
            for (int x = cx * CHUNK_SIZE; x < (cx + 1) * CHUNK_SIZE; x++) {
                for (int z = cz * CHUNK_SIZE; z < (cz + 1) * CHUNK_SIZE; z++) {
                    auto localSetVoxel = [&](int lx, int ly, int lz, uint8_t type) {
                        if (ly < 0 || ly >= WORLD_HEIGHT) return;
                        int cy = ly / CHUNK_SIZE;
                        if (!localColumn[cy]) localColumn[cy] = new ChunkData();
                        localColumn[cy]->blocks[getLocalIdx(lx)][ly % CHUNK_SIZE][getLocalIdx(lz)] = type;
                    };

                    // FBM noise for terrain height
                    double n = noise.fbm2D(x * 0.004, z * 0.004, 4, 0.5, 2.0); // Range roughly [-1, 1]
                    double m = noise.fbm2D(x * 0.001, z * 0.001, 3, 0.5, 2.0); // Mountain noise
                    
                    // Base elevation
                    int height = 28 + (int)(n * 12);
                    
                    // Add mountains
                    if (m > 0.1) {
                        height += (int)((m - 0.1) * 80);
                    }

                    if (height < 5) height = 5;
                    if (height >= WORLD_HEIGHT - 5) height = WORLD_HEIGHT - 5;

                    localSetVoxel(x, 0, z, 3); // Bedrock
                    
                    int waterLevel = 26;
                    
                    if (height < waterLevel) {
                        // Underwater (Sand)
                        for (int y = 1; y < height - 2; y++) {
                            uint32_t hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
                            uint8_t stoneType = (hash % 100 < 20) ? 3 : 15;
                            localSetVoxel(x, y, z, stoneType); 
                        }
                        localSetVoxel(x, height - 2, z, 18); // Sand
                        localSetVoxel(x, height - 1, z, 18); // Sand
                        localSetVoxel(x, height, z, 18); // Sand
                        
                        // Fill water up to waterLevel
                        for (int y = height + 1; y <= waterLevel; y++) {
                            localSetVoxel(x, y, z, 8); // Water
                        }
                    } else if (height <= waterLevel + 1) {
                        // Beach (Sand above water)
                        for (int y = 1; y < height - 2; y++) {
                            uint32_t hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
                            uint8_t stoneType = (hash % 100 < 20) ? 3 : 15;
                            localSetVoxel(x, y, z, stoneType); 
                        }
                        localSetVoxel(x, height - 2, z, 18); // Sand
                        localSetVoxel(x, height - 1, z, 18); // Sand
                        localSetVoxel(x, height, z, 18); // Sand
                    } else {
                        // Normal Grass/Dirt
                        for (int y = 1; y < height - 2; y++) {
                            uint32_t hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
                            uint8_t stoneType = 15; // Darker Stone shade
                            int mod = hash % 100;
                            if (mod < 15) stoneType = 3; // Stone
                            else if (mod < 30) stoneType = 16; // Lighter Stone shade
                            
                            localSetVoxel(x, y, z, stoneType); 
                        }
                        localSetVoxel(x, height - 2, z, 2); // Dirt
                        localSetVoxel(x, height - 1, z, 2); // Dirt
                        
                        uint32_t grassHash = (x * 73856093) ^ (height * 19349663) ^ (z * 83492791);
                        uint8_t grassType = 1; // Vibrant Green
                        int gMod = grassHash % 100;
                        if (gMod < 15) grassType = 13; // Dark Olive
                        else if (gMod < 30) grassType = 14; // Bright Lime
                        
                        localSetVoxel(x, height, z, grassType);
                        
                        // Collect decorators deterministically
                        if (grassHash % 20000 == 0) {
                            decorators.push_back({x, height, z, 0}); // Tree
                        } else if (grassHash % 50 == 0) {
                            decorators.push_back({x, height, z, 1}); // Grass bush
                        } else if (grassHash % 60 == 0) {
                            decorators.push_back({x, height, z, 2 + (int)(grassHash % 7)}); // Flower
                        }
                    }
                }
            }
        }

        bool generated[WORLD_HEIGHT_CHUNKS] = {false};
        
        // Insert into global chunkManager safely
        {
            std::unique_lock<std::shared_mutex> lock(chunkMutex);
            for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                if (localColumn[cy]) {
                    generated[cy] = true;
                    glm::ivec3 key(cx, cy, cz);
                    auto it = chunkManager.find(key);
                    if (it == chunkManager.end()) {
                        chunkManager[key] = localColumn[cy];
                        worldGenerationId++;
                    } else {
                        for (int bx = 0; bx < CHUNK_SIZE; bx++) {
                            for (int by = 0; by < CHUNK_SIZE; by++) {
                                for (int bz = 0; bz < CHUNK_SIZE; bz++) {
                                    uint8_t type = localColumn[cy]->blocks[bx][by][bz];
                                    if (type != 0) {
                                        it->second->blocks[bx][by][bz] = type;
                                    }
                                }
                            }
                        }
                        delete localColumn[cy];
                    }
                    localColumn[cy] = nullptr;
                }
            }
        }

        for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
            if (generated[cy]) {
                markChunkDirty(cx * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE);
                markChunkDirty(cx * CHUNK_SIZE + CHUNK_SIZE - 1, cy * CHUNK_SIZE + CHUNK_SIZE - 1, cz * CHUNK_SIZE + CHUNK_SIZE - 1);
            }
        }

        // Apply decorators globally now that the base terrain is inserted
        for (const auto& dec : decorators) {
            if (dec.type == 0) {
                generateOrganicTree(dec.x, dec.y, dec.z);
            } else if (dec.type == 1) {
                generateVoxelGrassBush(dec.x, dec.y, dec.z);
            } else {
                generateVoxelFlower(dec.x, dec.y, dec.z, dec.type - 2);
            }
        }

        columnsRemaining--;
        {
            std::lock_guard<std::mutex> lock(genMutex);
            generatingColumns.erase(task);
        }
    }
}

void initGenerationThreads() {
    genRunning = true;
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < numThreads; i++) {
        genThreads.emplace_back(generationWorker);
    }
}

void stopGenerationThreads() {
    {
        std::lock_guard<std::mutex> lock(genMutex);
        genRunning = false;
    }
    genCV.notify_all();
    for (auto& t : genThreads) {
        if (t.joinable()) t.join();
    }
    genThreads.clear();
}

void generateTerrain(unsigned int seed) {
    clearWorld();
    globalSeed = seed;
    std::cout << "Generating terrain..." << std::endl;

    worldGenerationId++;
    {
        std::lock_guard<std::mutex> lock(genMutex);
        std::vector<glm::ivec2> spawnOrder;
        
        for (int x = -5; x <= 5; x++) {
            for (int z = -5; z <= 5; z++) {
                spawnOrder.push_back({x, z});
            }
        }
        
        columnsRemaining = spawnOrder.size();

        for (const auto& task : spawnOrder) {
            generatingColumns.insert(task);
            genQueue.push_back(task);
        }
    }
    genCV.notify_all();
}
