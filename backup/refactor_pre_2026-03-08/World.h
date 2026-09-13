#pragma once
#include <cstdint>
#include <vector>
#include <glad/gl.h>
#include <glm/glm.hpp>

#define GRID_SIZE 128
#define MACRO_SIZE 8
extern uint8_t voxelGrid[GRID_SIZE][GRID_SIZE][GRID_SIZE];
extern float voxelSize;

struct VoxelMesh {
    GLuint VAO, VBO;
    int vertexCount;
};
extern VoxelMesh staticMesh;

// Terrain operations
void generateTerrain();

// Mesh operations
void updateStaticMesh();
