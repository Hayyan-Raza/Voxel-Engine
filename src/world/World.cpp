#include "World.h"
#include "PerlinNoise.h"
#include "TreeGenerator.h"
#include "HouseGenerator.h"
#include "GreedyMesher.h"
#include "../core/Types.h"
#include "../core/Globals.h"
#include <cmath>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <cstring>
#include <shared_mutex>
#include <algorithm>
#include <unordered_set>
#include "GreedyMesher.h"
#include "TerrainGenerator.h"
#include "Lighting.h"

float voxelSize = 0.05f; // Vast land scale!
std::unordered_map<glm::ivec3, ChunkMesh*, ivec3_hash> chunkMeshes;
std::mutex meshMapMutex;

ChunkMesh* getChunkMesh(int cx, int cy, int cz) {
    glm::ivec3 key(cx, cy, cz);
    std::lock_guard<std::mutex> lock(meshMapMutex);
    auto it = chunkMeshes.find(key);
    if (it != chunkMeshes.end()) return it->second;
    ChunkMesh* cm = new ChunkMesh();
    cm->cx = cx; cm->cy = cy; cm->cz = cz;
    cm->minAABB = glm::vec3(cx * CHUNK_SIZE * voxelSize, cy * CHUNK_SIZE * voxelSize, cz * CHUNK_SIZE * voxelSize);
    cm->maxAABB = glm::vec3((cx + 1) * CHUNK_SIZE * voxelSize, (cy + 1) * CHUNK_SIZE * voxelSize, (cz + 1) * CHUNK_SIZE * voxelSize);
    chunkMeshes[key] = cm;
    return cm;
}

void clearMeshes() {
    std::lock_guard<std::mutex> lock(meshMapMutex);
    std::lock_guard<std::mutex> lock2(dirtyChunksMutex);
    for (auto& pair : chunkMeshes) {
        ChunkMesh* cm = pair.second;
        if (cm->VAO) glDeleteVertexArrays(1, &cm->VAO);
        if (cm->VBO) glDeleteBuffers(1, &cm->VBO);
        if (cm->waterVAO) glDeleteVertexArrays(1, &cm->waterVAO);
        if (cm->waterVBO) glDeleteBuffers(1, &cm->waterVBO);
        delete cm;
    }
    chunkMeshes.clear();
    dirtyChunks.clear();
    activeStaticMeshes.clear();
    activeWaterMeshes.clear();
}

void markAllChunksDirty() {
    std::lock_guard<std::mutex> lock(meshMapMutex);
    for (auto& pair : chunkMeshes) {
        ChunkMesh* cm = pair.second;
        cm->isDirty = true;
        cm->isMeshing = false;
        cm->isDirtyListed = true;
        cm->isActiveStatic = false;
        cm->isActiveWater = false;
        cm->isMeshedOnce = false;
        
        {
            std::lock_guard<std::mutex> lock2(dirtyChunksMutex);
            dirtyChunks.push_back(cm);
        }
    }
}

struct MeshTask {
    int cx, cy, cz;
};

struct MeshResult {
    int cx, cy, cz;
    std::vector<VoxelVertex> vertices;
    std::vector<VoxelVertex> waterVertices;
};

static std::vector<MeshTask> meshQueue;
static std::queue<MeshResult> resultQueue;
static std::mutex meshMutex;
static std::mutex resultMutex;
static std::condition_variable meshCV;
static bool mesherRunning = false;
static std::vector<std::thread> mesherThreads;

static void mesherWorker() {
    while (true) {
        MeshTask task;
        {
            std::unique_lock<std::mutex> lock(meshMutex);
            meshCV.wait(lock, [] { return !meshQueue.empty() || !mesherRunning; });
            if (!mesherRunning) break;
            task = meshQueue.back();
            meshQueue.pop_back();
        }

        MeshResult res;
        res.cx = task.cx; res.cy = task.cy; res.cz = task.cz;
        int x0 = task.cx * CHUNK_SIZE;
        int x1 = (task.cx + 1) * CHUNK_SIZE;
        int y0 = task.cy * CHUNK_SIZE;
        int y1 = (task.cy + 1) * CHUNK_SIZE;
        int z0 = task.cz * CHUNK_SIZE;
        int z1 = (task.cz + 1) * CHUNK_SIZE;

        performGreedyMeshing(res.vertices, res.waterVertices, x0, x1, y0, y1, z0, z1);

        {
            std::lock_guard<std::mutex> lock(resultMutex);
            resultQueue.push(std::move(res));
        }
    }
}

void initMesherThread() {
    mesherRunning = true;
    unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < numThreads; i++) {
        mesherThreads.emplace_back(mesherWorker);
    }
}

void stopMesherThread() {
    {
        std::lock_guard<std::mutex> lock(meshMutex);
        mesherRunning = false;
    }
    meshCV.notify_all();
    for (auto& t : mesherThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    mesherThreads.clear();
}

// --- Public API ---
void markChunkDirty(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return;
    int cx = getChunkCoord(x);
    int cy = getChunkCoord(y);
    int cz = getChunkCoord(z);
    
    auto mark = [&](int cX, int cY, int cZ) {
        ChunkMesh* cm = getChunkMesh(cX, cY, cZ);
        cm->isDirty = true;
        if (!cm->isDirtyListed) {
            std::lock_guard<std::mutex> lock(dirtyChunksMutex);
            if (!cm->isDirtyListed) {
                cm->isDirtyListed = true;
                dirtyChunks.push_back(cm);
            }
        }
    };

    mark(cx, cy, cz);

    // Check borders to dirty neighbors
    if (getLocalIdx(x) == 0) mark(cx - 1, cy, cz);
    if (getLocalIdx(x) == CHUNK_SIZE - 1) mark(cx + 1, cy, cz);
    if (getLocalIdx(y) == 0 && cy > 0) mark(cx, cy - 1, cz);
    if (getLocalIdx(y) == CHUNK_SIZE - 1 && cy < WORLD_HEIGHT_CHUNKS - 1) mark(cx, cy + 1, cz);
    if (getLocalIdx(z) == 0) mark(cx, cy, cz - 1);
    if (getLocalIdx(z) == CHUNK_SIZE - 1) mark(cx, cy, cz + 1);
}

void rebuildChunkSync(int cx, int cy, int cz) {
    if (cy < 0 || cy >= WORLD_HEIGHT_CHUNKS) return;
    ChunkMesh* cmp = getChunkMesh(cx, cy, cz);
    ChunkMesh& cm = *cmp;
    
    // Don't sync rebuild if it's already in progress by background thread
    if (cm.isMeshing) return;
    
    std::vector<VoxelVertex> vertices;
    std::vector<VoxelVertex> waterVertices;
    
    int x0 = cx * CHUNK_SIZE;
    int y0 = cy * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;
    int x1 = x0 + CHUNK_SIZE;
    int y1 = y0 + CHUNK_SIZE;
    int z1 = z0 + CHUNK_SIZE;
    
    performGreedyMeshing(vertices, waterVertices, x0, x1, y0, y1, z0, z1);
    
    cm.isMeshedOnce = true;
    cm.isDirty = false;
    
    if (cm.VAO == 0) glGenVertexArrays(1, &cm.VAO);
    if (cm.VBO == 0) glGenBuffers(1, &cm.VBO);

    glBindVertexArray(cm.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cm.VBO);

    if (!vertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VoxelVertex), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, emissive));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, light));
        glEnableVertexAttribArray(5);
        cm.vertexCount = vertices.size();
    } else {
        cm.vertexCount = 0;
    }

    if (cm.waterVAO == 0) glGenVertexArrays(1, &cm.waterVAO);
    if (cm.waterVBO == 0) glGenBuffers(1, &cm.waterVBO);

    glBindVertexArray(cm.waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cm.waterVBO);

    if (!waterVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(VoxelVertex), waterVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, emissive));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, light));
        glEnableVertexAttribArray(5);
        cm.waterVertexCount = waterVertices.size();
    } else {
        cm.waterVertexCount = 0;
    }
}

#include "TerrainGenerator.h"
#include "../core/Globals.h"

void updateActiveChunks(glm::vec3 cameraPos) {
    int pcx = getChunkCoord((int)std::floor(cameraPos.x / voxelSize));
    int pcz = getChunkCoord((int)std::floor(cameraPos.z / voxelSize));
    
    playerCurrentChunkX.store(pcx, std::memory_order_relaxed);
    playerCurrentChunkZ.store(pcz, std::memory_order_relaxed);
    
    std::unordered_set<glm::ivec2, ivec2_hash> desiredColumns;
    desiredColumns.insert(glm::ivec2(spawnChunkPos.x, spawnChunkPos.y)); // Spawn always active

    for (int dx = -renderDistanceChunks; dx <= renderDistanceChunks; dx++) {
        for (int dz = -renderDistanceChunks; dz <= renderDistanceChunks; dz++) {
            if (std::max(std::abs(dx), std::abs(dz)) <= renderDistanceChunks) {
                desiredColumns.insert(glm::ivec2(pcx + dx, pcz + dz));
            }
        }
    }

    std::unordered_set<glm::ivec2, ivec2_hash> currentColumns;
    {
        std::shared_lock<std::shared_mutex> lock(chunkMutex);
        for (auto& pair : chunkManager) {
            currentColumns.insert(glm::ivec2(pair.first.x, pair.first.z));
        }
    }

    std::unordered_set<glm::ivec2, ivec2_hash> keepColumns = desiredColumns;
    int unloadDistance = renderDistanceChunks + 2;
    for (int dx = -unloadDistance; dx <= unloadDistance; dx++) {
        for (int dz = -unloadDistance; dz <= unloadDistance; dz++) {
            if (std::max(std::abs(dx), std::abs(dz)) <= unloadDistance) {
                keepColumns.insert(glm::ivec2(pcx + dx, pcz + dz));
            }
        }
    }

    // Unload chunks not in keep
    for (const auto& col : currentColumns) {
        if (keepColumns.find(col) == keepColumns.end()) {
            SaveTask task;
            task.cx = col.x;
            task.cz = col.y;
            
            // Remove from chunkManager and detach data
            {
                std::unique_lock<std::shared_mutex> lock(chunkMutex);
                bool erasedAny = false;
                for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                    glm::ivec3 key(col.x, cy, col.y);
                    auto it = chunkManager.find(key);
                    if (it != chunkManager.end()) {
                        task.chunks[cy] = it->second;
                        chunkManager.erase(it);
                        erasedAny = true;
                    } else {
                        task.chunks[cy] = nullptr;
                    }
                }
                if (erasedAny) {
                    worldGenerationId++;
                }
            }
            
            queueChunkSave(task);
            
            // Remove meshes safely
            {
                std::lock_guard<std::mutex> mLock(meshMapMutex);
                std::lock_guard<std::mutex> dLock(dirtyChunksMutex);
                for (int cy = 0; cy < WORLD_HEIGHT_CHUNKS; cy++) {
                    glm::ivec3 key(col.x, cy, col.y);
                    auto it = chunkMeshes.find(key);
                    if (it != chunkMeshes.end()) {
                        ChunkMesh* cm = it->second;
                        
                        // Erase from dirtyChunks
                        auto dIt = std::find(dirtyChunks.begin(), dirtyChunks.end(), cm);
                        if (dIt != dirtyChunks.end()) dirtyChunks.erase(dIt);
                        
                        // Active vectors
                        auto smIt = std::find(activeStaticMeshes.begin(), activeStaticMeshes.end(), cm);
                        if (smIt != activeStaticMeshes.end()) activeStaticMeshes.erase(smIt);
                        
                        auto wmIt = std::find(activeWaterMeshes.begin(), activeWaterMeshes.end(), cm);
                        if (wmIt != activeWaterMeshes.end()) activeWaterMeshes.erase(wmIt);
                        
                        if (cm->VAO) glDeleteVertexArrays(1, &cm->VAO);
                        if (cm->VBO) glDeleteBuffers(1, &cm->VBO);
                        if (cm->waterVAO) glDeleteVertexArrays(1, &cm->waterVAO);
                        if (cm->waterVBO) glDeleteBuffers(1, &cm->waterVBO);
                        
                        delete cm;
                        chunkMeshes.erase(it);
                    }
                }
            }
        }
    }

    // Load desired columns (sort them so closest are queued first)
    std::vector<glm::ivec2> chunksToLoad;
    for (const auto& col : desiredColumns) {
        if (currentColumns.find(col) == currentColumns.end()) {
            chunksToLoad.push_back(col);
        }
    }
    
    if (!chunksToLoad.empty()) {
        std::sort(chunksToLoad.begin(), chunksToLoad.end(), [pcx, pcz](const glm::ivec2& a, const glm::ivec2& b) {
            int distA = (a.x - pcx) * (a.x - pcx) + (a.y - pcz) * (a.y - pcz);
            int distB = (b.x - pcx) * (b.x - pcx) + (b.y - pcz) * (b.y - pcz);
            return distA < distB; // Ascending order (closest first)
        });
        
        for (const auto& col : chunksToLoad) {
            queueChunkGeneration(col.x, col.y);
        }
    }
}

void updateStaticMesh(glm::vec3 cameraPos) {

    std::vector<ChunkMesh*> localDirtyChunks;
    {
        std::lock_guard<std::mutex> lock(dirtyChunksMutex);
        if (!dirtyChunks.empty()) {
            localDirtyChunks = std::move(dirtyChunks);
            dirtyChunks.clear();
        }
    }

    if (!localDirtyChunks.empty()) {
        // Only sort if we have a lot of chunks, otherwise it's cheap enough
        // Use squared distance for sorting to avoid thousands of sqrt calls
        std::sort(localDirtyChunks.begin(), localDirtyChunks.end(), [&](ChunkMesh* a, ChunkMesh* b) {
            glm::vec3 ca((a->cx + 0.5f) * CHUNK_SIZE * voxelSize, (a->cy + 0.5f) * CHUNK_SIZE * voxelSize, (a->cz + 0.5f) * CHUNK_SIZE * voxelSize);
            glm::vec3 cb((b->cx + 0.5f) * CHUNK_SIZE * voxelSize, (b->cy + 0.5f) * CHUNK_SIZE * voxelSize, (b->cz + 0.5f) * CHUNK_SIZE * voxelSize);
            glm::vec3 da = ca - cameraPos;
            glm::vec3 db = cb - cameraPos;
            float distA = glm::dot(da, da);
            float distB = glm::dot(db, db);
            if (std::isnan(distA)) distA = 0.0f;
            if (std::isnan(distB)) distB = 0.0f;
            
            if (distA != distB) return distA > distB; // Descending
            if (a->cx != b->cx) return a->cx > b->cx;
            if (a->cy != b->cy) return a->cy > b->cy;
            return a->cz > b->cz;
        });

        std::vector<ChunkMesh*> stillDirty;
        std::vector<MeshTask> batchQueue;
        
        const int MAX_DISPATCHES = isInitialLoading ? 4096 : 128;
        int dispatched = 0;

        // Iterate backwards so closest chunks (at front) are pushed last to batchQueue
        for (auto it = localDirtyChunks.rbegin(); it != localDirtyChunks.rend(); ++it) {
            ChunkMesh* cm = *it;
            if (!cm->isMeshing && dispatched < MAX_DISPATCHES) {
                cm->isMeshing = true;
                cm->isDirty = false;
                cm->isDirtyListed = false;
                batchQueue.push_back({cm->cx, cm->cy, cm->cz});
                dispatched++;
            } else {
                stillDirty.push_back(cm);
            }
        }
        // stillDirty is already sorted closest-first. We want the closest to be at the front of dirtyChunks.
        {
            std::lock_guard<std::mutex> lock(dirtyChunksMutex);
            dirtyChunks.insert(dirtyChunks.end(), stillDirty.begin(), stillDirty.end());
        }

        if (!batchQueue.empty()) {
            // We want the absolute closest (which is at the front of batchQueue) to be at the BACK of meshQueue
            std::reverse(batchQueue.begin(), batchQueue.end());
            {
                std::lock_guard<std::mutex> lock(meshMutex);
                for (const auto& task : batchQueue) {
                    meshQueue.push_back(task);
                }
            }
            meshCV.notify_all();
        }
    }

    // 2. Retrieve completed meshes and upload to GPU
    std::vector<MeshResult> completedMeshes;
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        int uploadsThisFrame = 0;
        const int MAX_UPLOADS = isInitialLoading ? 512 : 24; 
        while (!resultQueue.empty() && uploadsThisFrame < MAX_UPLOADS) {
            completedMeshes.push_back(std::move(resultQueue.front()));
            resultQueue.pop();
            uploadsThisFrame++;
        }
    }

    for (auto& res : completedMeshes) {
        ChunkMesh* cmp = getChunkMesh(res.cx, res.cy, res.cz);
        ChunkMesh& cm = *cmp;
        cm.isMeshedOnce = true;
        
        if (cm.VAO == 0) glGenVertexArrays(1, &cm.VAO);
        if (cm.VBO == 0) glGenBuffers(1, &cm.VBO);

        glBindVertexArray(cm.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cm.VBO);

        if (!res.vertices.empty()) {
            glBufferData(GL_ARRAY_BUFFER, res.vertices.size() * sizeof(VoxelVertex), res.vertices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, emissive));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, light));
            glEnableVertexAttribArray(5);
        } else {
            glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        }
        
        cm.vertexCount = (int)res.vertices.size();
        
        if (cm.vertexCount > 0 && !cm.isActiveStatic) {
            cm.isActiveStatic = true;
            activeStaticMeshes.push_back(&cm);
        } else if (cm.vertexCount == 0 && cm.isActiveStatic) {
            cm.isActiveStatic = false;
            auto it = std::find(activeStaticMeshes.begin(), activeStaticMeshes.end(), &cm);
            if (it != activeStaticMeshes.end()) activeStaticMeshes.erase(it);
        }
        
        if (cm.waterVAO == 0) glGenVertexArrays(1, &cm.waterVAO);
        if (cm.waterVBO == 0) glGenBuffers(1, &cm.waterVBO);

        glBindVertexArray(cm.waterVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cm.waterVBO);

        if (!res.waterVertices.empty()) {
            glBufferData(GL_ARRAY_BUFFER, res.waterVertices.size() * sizeof(VoxelVertex), res.waterVertices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, emissive));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, light));
            glEnableVertexAttribArray(5);
        } else {
            glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        }
        
        cm.waterVertexCount = (int)res.waterVertices.size();
        
        if (cm.waterVertexCount > 0 && !cm.isActiveWater) {
            cm.isActiveWater = true;
            activeWaterMeshes.push_back(&cm);
        } else if (cm.waterVertexCount == 0 && cm.isActiveWater) {
            cm.isActiveWater = false;
            auto it = std::find(activeWaterMeshes.begin(), activeWaterMeshes.end(), &cm);
            if (it != activeWaterMeshes.end()) activeWaterMeshes.erase(it);
        }
        
        cm.isMeshing = false;
    }
}
