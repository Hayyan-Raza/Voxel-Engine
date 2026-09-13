#pragma once
#include <cstdint>
#include <vector>
#include <glad/gl.h>
#include <glm/glm.hpp>

#include "ChunkManager.h"

extern float voxelSize;

struct ChunkMesh {
    GLuint VAO = 0;
    GLuint VBO = 0;
    int vertexCount = 0;
    GLuint waterVAO = 0;
    GLuint waterVBO = 0;
    int waterVertexCount = 0;
    bool isDirty = true;
    bool isMeshing = false;
    
    int cx = 0, cy = 0, cz = 0;
    glm::vec3 minAABB{0.0f};
    glm::vec3 maxAABB{0.0f};
    bool isActiveStatic = false;
    bool isActiveWater = false;
    bool isDirtyListed = false;
    bool isMeshedOnce = false;
};

#include <unordered_map>
extern std::unordered_map<glm::ivec3, ChunkMesh*, ivec3_hash> chunkMeshes;
extern std::mutex meshMapMutex;

// Terrain operations
void clearMeshes();
void clearWorld();
void markAllChunksDirty();
void generateTerrain(unsigned int seed = 2342342547);

// Mesh operations
ChunkMesh* getChunkMesh(int cx, int cy, int cz);
void updateActiveChunks(glm::vec3 cameraPos);
void updateStaticMesh(glm::vec3 cameraPos);
void markChunkDirty(int x, int y, int z);
void rebuildChunkSync(int cx, int cy, int cz);

// Background Mesher
void initMesherThread();
void stopMesherThread();
