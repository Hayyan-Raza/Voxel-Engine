#pragma once
#include <glad/gl.h>
#include "core/Types.h"

// Unit cube vertex data (pos + white RGB + normal, 36 verts)
extern const float unitCubeVertices[36 * 9];

// Get voxel color from type
glm::vec3 getVoxelColor(uint8_t type);

// Setup the shared unit cube VAO/VBO used for chunks and hammer
void initCubeGeometry(GLuint& VAO, GLuint& VBO);

// Draw all active tumbling chunks
void drawChunks(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLuint cubeVAO);
void drawGhostBlock(GLint modelLoc, GLint colorLoc, GLuint cubeVAO);
void drawBuildingGizmo(GLint modelLoc, GLint colorLoc, GLuint cubeVAO);
void drawActiveWeapon(GLint modelLoc, GLint colorLoc, GLint shadowLoc,
                      GLint projLoc, GLint viewLoc, GLuint VAO);

// Chunk rendering optimizations
void initChunkMesh(VoxelChunk& chunk);
void cleanupChunkMesh(VoxelChunk& chunk);
