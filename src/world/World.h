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

extern ChunkMesh chunkMeshes[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];

// Terrain operations
void clearWorld();
void markAllChunksDirty();
void generateTerrain(unsigned int seed = 2342342547);

// Mesh operations
void updateStaticMesh(glm::vec3 cameraPos, float maxRenderDistance);
void markChunkDirty(int x, int y, int z);
void rebuildChunkSync(int cx, int cy, int cz);

// Background Mesher
void initMesherThread();
void stopMesherThread();
