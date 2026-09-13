#include "World.h"
#include <cmath>
#include <iostream>

uint8_t voxelGrid[GRID_SIZE][GRID_SIZE][GRID_SIZE] = {{{0}}};
float voxelSize = 0.01f;
VoxelMesh staticMesh = {0, 0, 0};

struct VoxelVertex {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
};

// --- Private Helpers ---

void generateMacroBlock(int mx, int my, int mz, int macroSize) {
    int macroType = 2; // Default Dirt
    if (my == 1) macroType = 1; // Top layer is Grass block (Assuming terrainHeightMacro = 2)

    for (int lx = 0; lx < macroSize; lx++) {
        for (int lz = 0; lz < macroSize; lz++) {
            for (int ly = 0; ly < macroSize; ly++) {
                int vx = (mx * macroSize) + lx;
                int vy = (my * macroSize) + ly;
                int vz = (mz * macroSize) + lz;

                uint8_t microType = macroType;
                
                if (macroType == 1) { // It's a Grass Block
                    int grassDepth = 2;
                    // Create overhang/bleed effect on macro-block borders
                    if (lx == 0 || lx == macroSize - 1 || lz == 0 || lz == macroSize - 1) {
                        // Generate a pseudo-random stagger using modulus math
                        grassDepth += (lx * 3 + lz * 7) % 3; 
                    }

                    if (ly >= macroSize - grassDepth) {
                        microType = 1; // Grass
                    } else {
                        microType = 2; // Dirt
                    }
                }
                voxelGrid[vx][vy][vz] = microType;
            }
        }
    }
}

void spawnTreeBranch(int sx, int sy, int sz, glm::vec3 dir, int length) {
    for (int i = 0; i < length; i++) {
        int bx = sx + static_cast<int>(dir.x * i);
        int by = sy + static_cast<int>(dir.y * i);
        int bz = sz + static_cast<int>(dir.z * i);
        
        if (bx >=0 && bx < GRID_SIZE && by >= 0 && by < GRID_SIZE && bz >= 0 && bz < GRID_SIZE) {
            voxelGrid[bx][by][bz] = 4; // Wood
            if (i == length - 1) {
                // Leaves cluster at branch end
                for(int lx=-2; lx<=2; lx++) {
                    for(int ly=-2; ly<=2; ly++) {
                        for(int lz=-2; lz<=2; lz++) {
                            if (sqrt(lx*lx + ly*ly + lz*lz) < 2.3f) {
                                int fx = bx + lx; int fy = by + ly; int fz = bz + lz;
                                if (fx >= 0 && fx < GRID_SIZE && fy >= 0 && fy < GRID_SIZE && fz >= 0 && fz < GRID_SIZE)
                                    if (voxelGrid[fx][fy][fz] == 0) voxelGrid[fx][fy][fz] = 5;
                            }
                        }
                    }
                }
            }
        }
    }
}

void generateTree(int tx, int ty, int tz) {
    int trunkHeight = 12;
    for (int y = 0; y < trunkHeight; y++) {
        if (y < 4) { // 3x3 base
            for(int ox=-1; ox<=1; ox++) for(int oz=-1; oz<=1; oz++)
                voxelGrid[tx+ox][ty+y][tz+oz] = 4;
        } else if (y < 9) { // 2x2 mid
            for(int ox=0; ox<=1; ox++) for(int oz=0; oz<=1; oz++)
                voxelGrid[tx+ox][ty+y][tz+oz] = 4;
        } else { // 1x1 top
            voxelGrid[tx][ty+y][tz] = 4;
        }
    }

    spawnTreeBranch(tx, ty + 6, tz, glm::vec3(1, 0.5f, 1), 4);
    spawnTreeBranch(tx, ty + 7, tz, glm::vec3(-1, 0.4f, 1), 5);
    spawnTreeBranch(tx, ty + 8, tz, glm::vec3(0, 0.6f, -1), 4);
    spawnTreeBranch(tx, ty + 10, tz, glm::vec3(1, 0.3f, -1), 3);
}

// --- Public API ---

void generateTerrain() {
    int macroSize = 8;
    int terrainHeightMacro = 2; // Flat 2-macro-block high floor

    for (int mx = 0; mx < GRID_SIZE / macroSize; mx++) {
        for (int mz = 0; mz < GRID_SIZE / macroSize; mz++) {
            for (int my = 0; my < terrainHeightMacro; my++) {
                generateMacroBlock(mx, my, mz, macroSize);
            }
        }
    }

    // Spawn High-Detail Teardown Tree in center
    generateTree(GRID_SIZE / 2, 10, GRID_SIZE / 2);
}

void updateStaticMesh() {
    std::vector<VoxelVertex> v;
    
    // Arrays for face iterations: normal, and 4 vertices relative to voxel origin (0,0,0) to (1,1,1)
    struct FaceDef {
        int dx, dy, dz; // Check neighbor
        glm::vec3 n;    // Normal
        glm::vec3 vert[6]; // Triangle vertices (2 quads)
    };
    
    FaceDef faces[6] = {
        // +X Right
        {1, 0, 0, {1,0,0}, { {1,0,1}, {1,0,0}, {1,1,0}, {1,0,1}, {1,1,0}, {1,1,1} }},
        // -X Left
        {-1, 0, 0, {-1,0,0}, { {0,0,0}, {0,0,1}, {0,1,1}, {0,0,0}, {0,1,1}, {0,1,0} }},
        // +Y Top
        {0, 1, 0, {0,1,0}, { {0,1,1}, {1,1,1}, {1,1,0}, {0,1,1}, {1,1,0}, {0,1,0} }},
        // -Y Bottom
        {0, -1, 0, {0,-1,0}, { {0,0,0}, {1,0,0}, {1,0,1}, {0,0,0}, {1,0,1}, {0,0,1} }},
        // +Z Front
        {0, 0, 1, {0,0,1}, { {0,0,1}, {1,0,1}, {1,1,1}, {0,0,1}, {1,1,1}, {0,1,1} }},
        // -Z Back
        {0, 0, -1, {0,0,-1}, { {1,0,0}, {0,0,0}, {0,1,0}, {1,0,0}, {0,1,0}, {1,1,0} }}
    };

    // Fast 6-face culling iteration
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int z = 0; z < GRID_SIZE; z++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                uint8_t type = voxelGrid[x][y][z];
                if (type == 0) continue;

                float r=0.5f, g=0.5f, b=0.5f;
                if (type == 1)      { r = 0.72f; g = 0.75f; b = 0.35f; } // Grass
                else if (type == 2) { r = 0.40f; g = 0.30f; b = 0.15f; } // Dirt
                else if (type == 3) { r = 0.55f; g = 0.55f; b = 0.55f; } // Stone
                else if (type == 4) { r = 0.35f; g = 0.25f; b = 0.15f; } // Wood
                else if (type == 5) { r = 0.75f; g = 0.85f; b = 0.45f; } // Leaves
                
                float px = x * voxelSize;
                float py = y * voxelSize;
                float pz = z * voxelSize;

                for (int f = 0; f < 6; f++) {
                    int nx = x + faces[f].dx;
                    int ny = y + faces[f].dy;
                    int nz = z + faces[f].dz;
                    
                    bool drawFace = false;
                    if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE || nz < 0 || nz >= GRID_SIZE) {
                        drawFace = true; // World boundary
                    } else if (voxelGrid[nx][ny][nz] == 0) {
                        drawFace = true; // Empty space adjacent
                    }

                    if (drawFace) {
                        for (int i = 0; i < 6; i++) {
                            v.push_back({
                                px + faces[f].vert[i].x * voxelSize,
                                py + faces[f].vert[i].y * voxelSize,
                                pz + faces[f].vert[i].z * voxelSize,
                                r, g, b,
                                faces[f].n.x, faces[f].n.y, faces[f].n.z
                            });
                        }
                    }
                }
            }
        }
    }

    if (staticMesh.VAO == 0) glGenVertexArrays(1, &staticMesh.VAO);
    if (staticMesh.VBO == 0) glGenBuffers(1, &staticMesh.VBO);
    glBindVertexArray(staticMesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, staticMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(VoxelVertex), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    staticMesh.vertexCount = v.size();
}
