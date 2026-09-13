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

static std::queue<glm::ivec2> genQueue;
static std::mutex genMutex;
static std::condition_variable genCV;
static bool genRunning = false;
static std::vector<std::thread> genThreads;
static std::thread decoratorThread;
static std::atomic<int> columnsRemaining(0);
static unsigned int globalSeed = 0;

static void generateVoxelGrassBush(int cx, int baseY, int cz) {
    const int numBlades = 16 + (rand() % 8);
    for (int i = 0; i < numBlades; i++) {
        float angle = (i * 6.2831853f / numBlades) + ((rand() % 100) * 0.005f);
        float radius = 0.8f + (rand() % 100) * 0.018f;
        int bx = cx + (int)round(cos(angle) * radius);
        int bz = cz + (int)round(sin(angle) * radius);
        
        if (bx < 2 || bx >= GRID_SIZE - 2 || bz < 2 || bz >= GRID_SIZE - 2) continue;
        uint8_t groundType = getVoxel(bx, baseY, bz);
        if (groundType != 1 && groundType != 13 && groundType != 14) continue;

        int height = 4 + (rand() % 5);
        uint8_t gType = (rand() % 3 == 0) ? 21 : ((rand() % 2 == 0) ? 22 : 5);

        int curX = bx;
        int curZ = bz;
        for (int h = 1; h <= height; h++) {
            int y = baseY + h;
            if (y >= GRID_SIZE) break;
            
            if (h >= height - 2 && (rand() % 2 == 0)) {
                curX += (cos(angle) > 0 ? 1 : -1);
                curZ += (sin(angle) > 0 ? 1 : -1);
                if (curX < 2 || curX >= GRID_SIZE - 2 || curZ < 2 || curZ >= GRID_SIZE - 2) break;
            }
            setVoxelFast(curX, y, curZ, gType);
        }
    }
}

static void generateVoxelFlower(int x, int baseY, int z, int flowerIndex) {
    if (x < 4 || x >= GRID_SIZE - 4 || z < 4 || z >= GRID_SIZE - 4) return;
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
            task = genQueue.front();
            genQueue.pop();
        }

        int cx = task.x;
        int cz = task.y;

        ChunkData* localColumn[CHUNKS_PER_AXIS] = {nullptr};

        for (int x = cx * CHUNK_SIZE; x < (cx + 1) * CHUNK_SIZE; x++) {
            for (int z = cz * CHUNK_SIZE; z < (cz + 1) * CHUNK_SIZE; z++) {
                auto localSetVoxel = [&](int lx, int ly, int lz, uint8_t type) {
                    if (ly < 0 || ly >= GRID_SIZE) return;
                    int cy = ly / CHUNK_SIZE;
                    if (!localColumn[cy]) localColumn[cy] = new ChunkData();
                    localColumn[cy]->blocks[lx % CHUNK_SIZE][ly % CHUNK_SIZE][lz % CHUNK_SIZE] = type;
                };

                int height = 30;

                localSetVoxel(x, 0, z, 3); // Bedrock
                for (int y = 1; y < height - 2; y++) {
                    localSetVoxel(x, y, z, 15); // Stone (using 15 for stone)
                }
                localSetVoxel(x, height - 2, z, 2); // Dirt
                localSetVoxel(x, height - 1, z, 2); // Dirt
                localSetVoxel(x, height, z, 1); // Grass
            }
        }

        bool generated[CHUNKS_PER_AXIS] = {false};
        
        // Insert into global chunkManager safely
        {
            std::unique_lock<std::shared_mutex> lock(chunkMutex);
            for (int cy = 0; cy < CHUNKS_PER_AXIS; cy++) {
                if (localColumn[cy]) {
                    generated[cy] = true;
                    glm::ivec3 key(cx, cy, cz);
                    auto it = chunkManager.find(key);
                    if (it == chunkManager.end()) {
                        chunkManager[key] = localColumn[cy];
                        worldGenerationId++;
                    } else {
                        // Merge blocks into existing chunk (created by lighting thread)
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

        // Mark chunks dirty so mesher can pick them up immediately
        for (int cy = 0; cy < CHUNKS_PER_AXIS; cy++) {
            if (generated[cy]) {
                // Call on minimum corner to trigger -1 neighbor updates
                markChunkDirty(cx * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE);
                // Call on maximum corner to trigger +1 neighbor updates
                markChunkDirty(cx * CHUNK_SIZE + CHUNK_SIZE - 1, cy * CHUNK_SIZE + CHUNK_SIZE - 1, cz * CHUNK_SIZE + CHUNK_SIZE - 1);
            }
        }

        columnsRemaining--;
    }
}

static void decoratorWorker() {
    // Wait until base terrain is fully generated
    while (columnsRemaining > 0 && genRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!genRunning) return;

    srand(globalSeed);
    
    auto getSurfaceY = [](int x, int z) -> int {
        for (int y = GRID_SIZE - 1; y >= 0; y--) {
            uint8_t t = getVoxel(x, y, z);
            if (t != 0 && t != 8 && t != 5 && t != 27 && t != 28) {
                if (t == 1 || t == 13 || t == 14 || t == 2 || t == 17 || t == 18 || t == 19) {
                    return y;
                }
                return -1;
            }
        }
        return -1;
    };

    int seaLevel = 36;
    for (int t = 0; t < 10; t++) {
        int tx = 50 + rand() % (GRID_SIZE - 100);
        int tz = 50 + rand() % (GRID_SIZE - 100);
        int ty = getSurfaceY(tx, tz);
        if (ty > seaLevel + 4 && ty < 80) generateOrganicTree(tx, ty, tz);
    }

    for (int b = 0; b < 200; b++) {
        int bx = 20 + rand() % (GRID_SIZE - 40);
        int bz = 20 + rand() % (GRID_SIZE - 40);
        int by = getSurfaceY(bx, bz);
        if (by > seaLevel + 3) generateVoxelGrassBush(bx, by, bz);
    }

    for (int f = 0; f < 300; f++) {
        int fx = 20 + rand() % (GRID_SIZE - 40);
        int fz = 20 + rand() % (GRID_SIZE - 40);
        int fy = getSurfaceY(fx, fz);
        if (fy > seaLevel + 3) generateVoxelFlower(fx, fy, fz, f % 7);
    }

    // Remesh dirty chunks around decorators
    for (int cx = 0; cx < CHUNKS_PER_AXIS; cx++) {
        for (int cy = 0; cy < CHUNKS_PER_AXIS; cy++) {
            for (int cz = 0; cz < CHUNKS_PER_AXIS; cz++) {
                ChunkMesh& cm = chunkMeshes[cx][cy][cz];
                if (cm.isDirty && !cm.isDirtyListed) {
                    std::lock_guard<std::mutex> lock(dirtyChunksMutex);
                    if (!cm.isDirtyListed) {
                        cm.isDirtyListed = true;
                        dirtyChunks.push_back(&cm);
                    }
                }
            }
        }
    }
}

void initGenerationThreads() {
    genRunning = true;
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency() - 2);
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
    if (decoratorThread.joinable()) {
        decoratorThread.join();
    }
}

void generateTerrain(unsigned int seed) {
    clearWorld();
    globalSeed = seed;

    {
        std::lock_guard<std::mutex> lock(genMutex);
        std::vector<glm::ivec2> spawnOrder;
        int center = CHUNKS_PER_AXIS / 2;
        spawnOrder.push_back({center, center});
        columnsRemaining = 1;

        for (const auto& task : spawnOrder) {
            genQueue.push(task);
        }
    }
    genCV.notify_all();
    
    // Launch background decorator thread to add trees/flowers when done
    // if (decoratorThread.joinable()) decoratorThread.join();
    // decoratorThread = std::thread(decoratorWorker);
}
