#include "WeaponModels.h"
#include "WeaponModelsData.h"

GLuint hammerHandleVAO = 0; GLuint hammerHandleVBO = 0; int hammerHandleVertexCount = 0;
GLuint ak47VAO = 0; GLuint ak47VBO = 0; int ak47VertexCount = 0;
GLuint glockVAO = 0; GLuint glockVBO = 0; int glockVertexCount = 0;
GLuint shotgunVAO = 0; GLuint shotgunVBO = 0; int shotgunVertexCount = 0;
GLuint dynVAO = 0; GLuint dynVBO = 0; int dynVertexCount = 0;

void uploadWeaponVAO(GLuint& vao, GLuint& vbo, int& count, const std::vector<VoxelVertex>& verts) {
    if(verts.empty()) return;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VoxelVertex), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx)); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao)); glEnableVertexAttribArray(3);
    count = (int)verts.size();
    glBindVertexArray(0);
}

void initHammerGeometry() {
    WeaponGrid grid;
    float scale = 0.0f;
    generateHammerGeometry(grid, scale);
    std::vector<VoxelVertex> verts;
    grid.buildMesh(scale, verts);
    uploadWeaponVAO(hammerHandleVAO, hammerHandleVBO, hammerHandleVertexCount, verts);
}

void cleanupHammerGeometry() {
    if (hammerHandleVAO) glDeleteVertexArrays(1, &hammerHandleVAO);
    if (hammerHandleVBO) glDeleteBuffers(1, &hammerHandleVBO);
    hammerHandleVAO = 0; hammerHandleVBO = 0; hammerHandleVertexCount = 0;
}

void initAK47Geometry() {
    WeaponGrid grid;
    float scale = 0.0f;
    generateAK47Geometry(grid, scale);
    std::vector<VoxelVertex> verts;
    grid.buildMesh(scale, verts);
    uploadWeaponVAO(ak47VAO, ak47VBO, ak47VertexCount, verts);
}

void cleanupAK47Geometry() {
    if(ak47VAO) glDeleteVertexArrays(1, &ak47VAO);
    if(ak47VBO) glDeleteBuffers(1, &ak47VBO);
    ak47VAO = 0; ak47VBO = 0; ak47VertexCount = 0;
}

void initGlockGeometry() {
    WeaponGrid grid;
    float scale = 0.0f;
    generateGlockGeometry(grid, scale);
    std::vector<VoxelVertex> verts;
    grid.buildMesh(scale, verts);
    uploadWeaponVAO(glockVAO, glockVBO, glockVertexCount, verts);
}

void cleanupGlockGeometry() {
    if(glockVAO) glDeleteVertexArrays(1, &glockVAO);
    if(glockVBO) glDeleteBuffers(1, &glockVBO);
    glockVAO = 0; glockVBO = 0; glockVertexCount = 0;
}

void initShotgunGeometry() {
    WeaponGrid grid;
    float scale = 0.0f;
    generateShotgunGeometry(grid, scale);
    std::vector<VoxelVertex> verts;
    grid.buildMesh(scale, verts);
    uploadWeaponVAO(shotgunVAO, shotgunVBO, shotgunVertexCount, verts);
}

void cleanupShotgunGeometry() {
    if(shotgunVAO) glDeleteVertexArrays(1, &shotgunVAO);
    if(shotgunVBO) glDeleteBuffers(1, &shotgunVBO);
    shotgunVAO = 0; shotgunVBO = 0; shotgunVertexCount = 0;
}

void initDynamiteGeometry() {
    WeaponGrid grid;
    float scale = 0.0f;
    generateDynamiteGeometry(grid, scale);
    std::vector<VoxelVertex> verts;
    grid.buildMesh(scale, verts);
    uploadWeaponVAO(dynVAO, dynVBO, dynVertexCount, verts);
}

void cleanupDynamiteGeometry() {
    if(dynVAO) glDeleteVertexArrays(1, &dynVAO);
    if(dynVBO) glDeleteBuffers(1, &dynVBO);
    dynVAO = 0; dynVBO = 0; dynVertexCount = 0;
}

void initWeaponModels() {
    initHammerGeometry();
    initAK47Geometry();
    initGlockGeometry();
    initShotgunGeometry();
    initDynamiteGeometry();
}

void cleanupWeaponModels() {
    cleanupHammerGeometry();
    cleanupAK47Geometry();
    cleanupGlockGeometry();
    cleanupShotgunGeometry();
    cleanupDynamiteGeometry();
}
