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
#include "GreedyMesher.h"
#include "TerrainGenerator.h"
#include "Lighting.h"

float voxelSize = 0.05f; // Vast land scale!
ChunkMesh chunkMeshes[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];

void markAllChunksDirty() {
    for (int cx = 0; cx < CHUNKS_PER_AXIS; cx++) {
        for (int cy = 0; cy < CHUNKS_PER_AXIS; cy++) {
            for (int cz = 0; cz < CHUNKS_PER_AXIS; cz++) {
                ChunkMesh& cm = chunkMeshes[cx][cy][cz];
                cm.isDirty = true;
                cm.isMeshing = false;
                cm.isDirtyListed = true;
                cm.isActiveStatic = false;
                cm.isActiveWater = false;
                cm.isMeshedOnce = false;
                cm.cx = cx; cm.cy = cy; cm.cz = cz;
                
                cm.minAABB = glm::vec3(cx * CHUNK_SIZE * voxelSize, cy * CHUNK_SIZE * voxelSize, cz * CHUNK_SIZE * voxelSize);
                cm.maxAABB = glm::vec3((cx + 1) * CHUNK_SIZE * voxelSize, (cy + 1) * CHUNK_SIZE * voxelSize, (cz + 1) * CHUNK_SIZE * voxelSize);
                
                {
                    std::lock_guard<std::mutex> lock(dirtyChunksMutex);
                    dirtyChunks.push_back(&cm);
                }
            }
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
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency() - 2);
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
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || z < 0 || z >= GRID_SIZE) return;
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    
    auto mark = [&](int cX, int cY, int cZ) {
        ChunkMesh& cm = chunkMeshes[cX][cY][cZ];
        cm.isDirty = true;
        cm.cx = cX; cm.cy = cY; cm.cz = cZ;
        if (!cm.isDirtyListed) {
            std::lock_guard<std::mutex> lock(dirtyChunksMutex);
            if (!cm.isDirtyListed) {
                cm.isDirtyListed = true;
                dirtyChunks.push_back(&cm);
            }
        }
    };

    mark(cx, cy, cz);

    // Check borders to dirty neighbors
    if (x % CHUNK_SIZE == 0 && cx > 0) mark(cx - 1, cy, cz);
    if (x % CHUNK_SIZE == CHUNK_SIZE - 1 && cx < CHUNKS_PER_AXIS - 1) mark(cx + 1, cy, cz);
    if (y % CHUNK_SIZE == 0 && cy > 0) mark(cx, cy - 1, cz);
    if (y % CHUNK_SIZE == CHUNK_SIZE - 1 && cy < CHUNKS_PER_AXIS - 1) mark(cx, cy + 1, cz);
    if (z % CHUNK_SIZE == 0 && cz > 0) mark(cx, cy, cz - 1);
    if (z % CHUNK_SIZE == CHUNK_SIZE - 1 && cz < CHUNKS_PER_AXIS - 1) mark(cx, cy, cz + 1);
}

void rebuildChunkSync(int cx, int cy, int cz) {
    if (cx < 0 || cx >= CHUNKS_PER_AXIS || cy < 0 || cy >= CHUNKS_PER_AXIS || cz < 0 || cz >= CHUNKS_PER_AXIS) return;
    ChunkMesh& cm = chunkMeshes[cx][cy][cz];
    
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

void updateStaticMesh(glm::vec3 cameraPos, float maxRenderDistance) {

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
        
        const int MAX_DISPATCHES = isInitialLoading ? 4096 : 64;
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
        const int MAX_UPLOADS = isInitialLoading ? 512 : 8; // Prevent massive frame stalls
        while (!resultQueue.empty() && uploadsThisFrame < MAX_UPLOADS) {
            completedMeshes.push_back(std::move(resultQueue.front()));
            resultQueue.pop();
            uploadsThisFrame++;
        }
    }

    for (auto& res : completedMeshes) {
        ChunkMesh& cm = chunkMeshes[res.cx][res.cy][res.cz];
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
