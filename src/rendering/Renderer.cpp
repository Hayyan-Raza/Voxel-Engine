#include "Renderer.h"
#include "WeaponModels.h"
#include <iostream>
#include <iostream>
#include <unordered_set>
#include "../core/Globals.h"
#include "../world/World.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>
#include <cmath>
#include <unordered_set>
#include <vector>

// clang-format off
const float unitCubeVertices[36 * 9] = {
    // -Z face (reversed to be CCW)
    -0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,
    -0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,
    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,

    // +Z face (CCW)
    -0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
    -0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
    -0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,

    // -X face (CCW)
    -0.5f, 0.5f, 0.5f,  1,1,1, -1, 0, 0,
    -0.5f, 0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f, 0.5f,  1,1,1, -1, 0, 0,
    -0.5f, 0.5f, 0.5f,  1,1,1, -1, 0, 0,

    // +X face (reversed to be CCW)
     0.5f, 0.5f, 0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f, 0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  1, 0, 0,

    // -Y face (CCW)
    -0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
    -0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,

    // +Y face (reversed to be CCW)
    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0,
    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0,
    -0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0
};
// clang-format on

void initCubeGeometry(GLuint& VAO, GLuint& VBO) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unitCubeVertices), unitCubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

glm::vec3 getVoxelColor(uint8_t type) {
    switch (type) {
        case 1: return {0.32f, 0.65f, 0.15f}; // More vibrant Green Grass (darkened)
        case 2: return {0.48f, 0.35f, 0.20f}; // Rich Dirt
        case 3: return {0.55f, 0.55f, 0.55f}; // Stone
        case 4: return {0.38f, 0.27f, 0.16f}; // Bark Brown
        case 5: return {0.52f, 0.82f, 0.14f}; // Lush Green Leaves & Grass Stalks
        case 6: return {0.95f, 0.15f, 0.15f}; // Dynamite red
        case 7: return {0.90f, 0.90f, 0.90f}; // Dynamite white cap
        case 8: return {0.15f, 0.45f, 0.85f}; // Dark Water Surface
        case 9: return {0.92f, 0.15f, 0.18f}; // Red Poppy / Rose
        case 10: return {0.96f, 0.82f, 0.10f}; // Yellow Sunflower / Dandelion
        case 11: return {0.62f, 0.25f, 0.88f}; // Purple Allium / Lavender
        case 12: return {0.95f, 0.90f, 0.95f}; // White / Pink Daisy
        case 13: return {0.22f, 0.45f, 0.10f}; // Dark Olive Grass
        case 14: return {0.45f, 0.78f, 0.15f}; // Bright Lime Grass (darkened)
        case 15: return {0.50f, 0.50f, 0.50f}; // Darker Stone shade
        case 26: return {0.38f, 0.27f, 0.16f}; // Flower Center (Swaying Bark Brown)
        case 16: return {0.60f, 0.60f, 0.60f}; // Lighter Stone shade
        case 17: return {0.44f, 0.32f, 0.18f}; // Darker Dirt shade
        case 18: return {0.85f, 0.80f, 0.55f}; // Sand
        case 19: return {0.90f, 0.85f, 0.60f}; // Lighter Sand
        case 20: return {0.30f, 0.20f, 0.10f}; // Darker Bark Brown
        case 21: return {0.25f, 0.50f, 0.15f}; // Bush Dark
        case 22: return {0.55f, 0.85f, 0.18f}; // Bush Bright
        case 23: return {0.22f, 0.45f, 0.10f}; // Tree Dark
        case 24: return {0.45f, 0.78f, 0.15f}; // Tree Bright
        case 27: return {0.98f, 0.75f, 0.85f}; // Cherry Blossom Light
        case 28: return {0.9f, 0.6f, 0.75f};   // Cherry Blossom Dark
        case 29: return {0.85f, 0.75f, 0.35f}; // Light Thatch / Straw
        case 30: return {0.75f, 0.65f, 0.25f}; // Dark Thatch / Straw
        case 31: return {1.0f, 1.0f, 1.0f};    // Emissive White
        default: return {0.5f, 0.5f, 0.5f};
    }
}


void initChunkMesh(VoxelChunk& chunk) {
    if (chunk.voxels.empty()) return;
    std::vector<VoxelVertex> verts;
    
    std::unordered_set<int64_t> voxelMap;
    for (const auto& pair : chunk.voxels) {
        int64_t key = (int64_t)(pair.first.x + 1000) | ((int64_t)(pair.first.y + 1000) << 12) | ((int64_t)(pair.first.z + 1000) << 24);
        voxelMap.insert(key);
    }
    
    for (const auto& pair : chunk.voxels) {
        glm::ivec3 localPos = pair.first;
        uint8_t type = pair.second;
        glm::vec3 col = getVoxelColor(type);
        uint8_t emissiveVal = (type == 31) ? 255 : 0;
        
        for (int f = 0; f < 6; f++) {
            float nx = unitCubeVertices[f * 54 + 6];
            float ny = unitCubeVertices[f * 54 + 7];
            float nz = unitCubeVertices[f * 54 + 8];
            
            glm::ivec3 neighborPos = localPos + glm::ivec3(std::round(nx), std::round(ny), std::round(nz));
            int64_t nKey = (int64_t)(neighborPos.x + 1000) | ((int64_t)(neighborPos.y + 1000) << 12) | ((int64_t)(neighborPos.z + 1000) << 24);
            
            if (voxelMap.find(nKey) == voxelMap.end()) {
                for (int v = 0; v < 6; v++) {
                    int i = f * 6 + v;
                    float vx = unitCubeVertices[i * 9 + 0];
                    float vy = unitCubeVertices[i * 9 + 1];
                    float vz = unitCubeVertices[i * 9 + 2];
                    verts.push_back({ 
                        (localPos.x + vx) * voxelSize, (localPos.y + vy) * voxelSize, (localPos.z + vz) * voxelSize, 
                        (uint8_t)(col.r * 255.0f), (uint8_t)(col.g * 255.0f), (uint8_t)(col.b * 255.0f), 
                        (int8_t)(nx * 127.0f), (int8_t)(ny * 127.0f), (int8_t)(nz * 127.0f), 
                        emissiveVal, (uint8_t)0, 1.0f 
                    });
                }
            }
        }
    }
    
    chunk.vertexCount = (int)verts.size();
    glGenVertexArrays(1, &chunk.VAO);
    glGenBuffers(1, &chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VoxelVertex), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, r)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx)); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao)); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, emissive)); glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, light)); glEnableVertexAttribArray(5);
    glBindVertexArray(0);
}

void cleanupChunkMesh(VoxelChunk& chunk) {
    if (chunk.VAO > 0) { glDeleteVertexArrays(1, &chunk.VAO); chunk.VAO = 0; }
    if (chunk.VBO > 0) { glDeleteBuffers(1, &chunk.VBO); chunk.VBO = 0; }
    chunk.vertexCount = 0;
}

void drawChunks(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLuint VAO) {
    glUniform1f(shadowLoc, 0.0f);
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Default multiplier is 1.0
    for (const auto& chunk : activeChunks) {
        if (chunk.weaponType != -1) {
            GLuint wVAO = 0;
            int wCount = 0;
            // Matches drawActiveWeapon offsets roughly
            if (chunk.weaponType == 0) { wVAO = hammerHandleVAO; wCount = hammerHandleVertexCount; }
            else if (chunk.weaponType == 1) { wVAO = ak47VAO; wCount = ak47VertexCount; }
            else if (chunk.weaponType == 2) { wVAO = dynVAO; wCount = dynVertexCount; }
            else if (chunk.weaponType == 3) { wVAO = glockVAO; wCount = glockVertexCount; }
            else if (chunk.weaponType == 4) { wVAO = shotgunVAO; wCount = shotgunVertexCount; }

            if (wVAO > 0) {
                glBindVertexArray(wVAO);
                glm::mat4 rotMat = glm::mat4_cast(chunk.rotation);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), chunk.center + glm::vec3(0.0f, 0.1f, 0.0f));
                model = model * rotMat;
                model = glm::scale(model, glm::vec3(0.45f)); 
                
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glDrawArrays(GL_TRIANGLES, 0, wCount);
            }
            continue; // Skip normal chunk rendering
        }

        if (chunk.vertexCount > 0 && chunk.VAO > 0) {
            // Frustum Culling for dynamic chunks
            float radius = std::min(10.0f, chunk.vertexCount * 0.05f + 1.0f);
            if (!viewFrustum.isSphereVisible(chunk.center, radius)) continue;

            glBindVertexArray(chunk.VAO);
            glm::mat4 rotMat = glm::mat4_cast(chunk.rotation);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), chunk.center);
            model = model * rotMat;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, chunk.vertexCount);
        }
    }
}





void drawGhostBlock(GLint modelLoc, GLint colorLoc, GLuint cubeVAO) {
    if (!showGhostVox || currentWeapon != 5) return;
    if (g_buildingSystem.GetCurrentState() != UI::BuildModeState::Inactive) return; // Hide when using new building system
    
    float r = 1.0f, g = 1.0f, b = 1.0f;
    uint8_t type = currentBuildMaterial;
    if (type == 1 || type == 2) { r = 0.48f; g = 0.35f; b = 0.20f; }
    else if (type == 3) { r = 0.55f; g = 0.55f; b = 0.55f; }
    else if (type == 4) { r = 0.35f; g = 0.25f; b = 0.15f; }
    else if (type == 5) { r = 0.65f; g = 0.85f; b = 0.15f; }
    else if (type == 8) { r = 0.15f; g = 0.45f; b = 0.85f; }
    else if (type == 9)  { r = 0.92f; g = 0.15f; b = 0.18f; }
    else if (type == 10) { r = 0.96f; g = 0.82f; b = 0.10f; }
    else if (type == 11) { r = 0.62f; g = 0.25f; b = 0.88f; }
    else if (type == 12) { r = 0.95f; g = 0.90f; b = 0.95f; }
    else if (type == 13) { r = 0.22f; g = 0.45f; b = 0.10f; }
    else if (type == 14) { r = 0.55f; g = 0.88f; b = 0.18f; }
    
    glm::ivec3 minV = ghostVox;
    glm::ivec3 maxV = ghostVox;
    
    extern glm::vec3 cameraFront;
    if (currentSchematic != 0) {
        if (currentSchematic == 1) { // Wall
            if (std::abs(cameraFront.x) > std::abs(cameraFront.z)) maxV += glm::ivec3(0, 9, 9); // Z-wall
            else maxV += glm::ivec3(9, 9, 0); // X-wall
        } else if (currentSchematic == 2) { // Pillar
            maxV += glm::ivec3(1, 9, 1);
        } else if (currentSchematic == 3) { // Roof
            maxV += glm::ivec3(9, 0, 9);
        } else if (currentSchematic == 4) { // Stairs
            maxV += glm::ivec3(9, 9, 9); // Bounds of stairs
        }
    } else {
        if (isDragging) {
            minV = glm::min(dragStartVox, ghostVox);
            maxV = glm::max(dragStartVox, ghostVox);
        }
    }
    
    glm::vec3 center = glm::vec3(minV + maxV) * 0.5f + 0.5f;
    glm::vec3 scale = glm::vec3(maxV - minV + 1) * voxelSize * 1.02f;
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, center * voxelSize);
    model = glm::scale(model, scale);
    
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    
    // Solid translucent pass
    glUniform4f(colorLoc, r, g, b, 0.3f);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // Wireframe pass
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(colorLoc, r*1.5f, g*1.5f, b*1.5f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void drawBuildingGizmo(GLint modelLoc, GLint colorLoc, GLuint cubeVAO) {
    auto state = g_buildingSystem.GetCurrentState();
    if (state != UI::BuildModeState::GizmoPreview && state != UI::BuildModeState::GizmoAnchored) return;

    const auto& gizmo = g_buildingSystem.GetGizmo();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- BLUEPRINT DEPTH PRE-PASS ---
    glm::vec3 center = gizmo.position;
    glm::vec3 cubeScale = glm::vec3(voxelSize);
    
    static GLuint previewVAO = 0;
    static GLuint previewVBO = 0;
    static int previewVertexCount = 0;
    
    if (previewVAO == 0) {
        glGenVertexArrays(1, &previewVAO);
        glGenBuffers(1, &previewVBO);
    }
    
    if (gizmo.previewDirty) {
        std::vector<float> verts;
        // Find min/max to define grid bounds
        glm::ivec3 minP(1000000), maxP(-1000000);
        for (const auto& offset : gizmo.previewVoxels) {
            glm::ivec3 p = glm::ivec3(glm::round(offset / voxelSize));
            minP = glm::min(minP, p);
            maxP = glm::max(maxP, p);
        }
        
        if (!gizmo.previewVoxels.empty()) {
            glm::ivec3 size = maxP - minP + glm::ivec3(3); // +3 for padding
            std::vector<bool> grid(size.x * size.y * size.z, false);
            
            auto getIdx = [&](glm::ivec3 p) {
                glm::ivec3 lp = p - minP + glm::ivec3(1);
                return lp.x + lp.y * size.x + lp.z * size.x * size.y;
            };
            
            for (const auto& offset : gizmo.previewVoxels) {
                glm::ivec3 p = glm::ivec3(glm::round(offset / voxelSize));
                grid[getIdx(p)] = true;
            }
        
        for (const auto& offset : gizmo.previewVoxels) {
            glm::ivec3 p = glm::ivec3(glm::round(offset / voxelSize));
            for (int f = 0; f < 6; f++) {
                float nx = unitCubeVertices[f * 54 + 6];
                float ny = unitCubeVertices[f * 54 + 7];
                float nz = unitCubeVertices[f * 54 + 8];
                
                glm::ivec3 neighborPos = p + glm::ivec3(std::round(nx), std::round(ny), std::round(nz));
                
                if (!grid[getIdx(neighborPos)]) {
                    for (int v = 0; v < 6; v++) {
                        float vx = unitCubeVertices[f * 54 + v * 9 + 0];
                        float vy = unitCubeVertices[f * 54 + v * 9 + 1];
                        float vz = unitCubeVertices[f * 54 + v * 9 + 2];
                        verts.push_back((vx + p.x) * voxelSize);
                        verts.push_back((vy + p.y) * voxelSize);
                        verts.push_back((vz + p.z) * voxelSize);
                        verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f);
                        verts.push_back(nx); verts.push_back(ny); verts.push_back(nz);
                    }
                }
            }
        }
        } // close if (!empty)
        
        glBindVertexArray(previewVAO);
        glBindBuffer(GL_ARRAY_BUFFER, previewVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
        
        previewVertexCount = verts.size() / 9;
        g_buildingSystem.ClearPreviewDirty();
    }
    
    glBindVertexArray(previewVAO);

    // 1. Draw only to depth buffer
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // Disable unsupplied attributes and set defaults so main shader doesn't render them black
    glDisableVertexAttribArray(3);
    glVertexAttrib1f(3, 1.0f); // aAO (1.0 = no dark AO shadow)
    glDisableVertexAttribArray(4);
    glVertexAttrib1f(4, 0.0f); // aEmissive
    glDisableVertexAttribArray(5);
    glVertexAttrib1f(5, 15.0f); // aLight (15 = max sunlight)

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    if (previewVertexCount > 0) {
        glDrawArrays(GL_TRIANGLES, 0, previewVertexCount);
    }

    // 2. Draw color where depth equals what we just wrote
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    float r = 0.5f, g = 0.8f, b = 1.0f, a = 0.8f; // Increased opacity
    if (state == UI::BuildModeState::GizmoAnchored) {
        a = 1.0f;
        uint8_t type = currentBuildMaterial;
        if (type == 1 || type == 2) { r = 0.48f; g = 0.35f; b = 0.20f; }
        else if (type == 3) { r = 0.55f; g = 0.55f; b = 0.55f; }
        else if (type == 4) { r = 0.35f; g = 0.25f; b = 0.15f; }
        else if (type == 5) { r = 0.65f; g = 0.85f; b = 0.15f; }
        else if (type == 8) { r = 0.15f; g = 0.45f; b = 0.85f; }
        else if (type == 9)  { r = 0.92f; g = 0.15f; b = 0.18f; }
        else if (type == 10) { r = 0.96f; g = 0.82f; b = 0.10f; }
        else if (type == 11) { r = 0.62f; g = 0.25f; b = 0.88f; }
        else if (type == 12) { r = 0.95f; g = 0.90f; b = 0.95f; }
        else if (type == 13) { r = 0.22f; g = 0.45f; b = 0.10f; }
        else if (type == 14) { r = 0.55f; g = 0.88f; b = 0.18f; }
    }
    glUniform4f(colorLoc, r, g, b, a); 

    if (previewVertexCount > 0) {
        glDrawArrays(GL_TRIANGLES, 0, previewVertexCount);
    }

    // If anchored, draw the bounding box and voxel arrows!
    if (state == UI::BuildModeState::GizmoAnchored) {
        glBindVertexArray(cubeVAO);
        
        // Ensure defaults are set for cubeVAO as well (since it only has pos, col, norm)
        glDisableVertexAttribArray(3);
        glVertexAttrib1f(3, 1.0f);
        glDisableVertexAttribArray(4);
        glVertexAttrib1f(4, 0.0f);
        glDisableVertexAttribArray(5);
        glVertexAttrib1f(5, 15.0f);

        // Draw wireframe bounding box
        // We don't draw the wireframe bounding box anymore because the voxel preview already correctly shows the shape size
        
        // Clear depth so arrows draw over everything
        glClear(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        
        auto drawVoxelArrow = [&](glm::vec3 axis, glm::vec3 arrowCenter, bool isHovered, bool isColorPass, glm::vec3 color, float length) {
            float shaftLength = length;
            glm::vec3 arrowPos = arrowCenter + axis * (shaftLength * 0.5f);
            
            glm::vec3 arrowScale = glm::vec3(voxelSize * 0.8f); // Slimmer arrows
            if (std::abs(axis.x) > 0.5f) arrowScale.x = shaftLength;
            else if (std::abs(axis.y) > 0.5f) arrowScale.y = shaftLength;
            else arrowScale.z = shaftLength;

            glm::mat4 aModel = glm::mat4(1.0f);
            aModel = glm::translate(aModel, arrowPos);
            aModel = glm::scale(aModel, arrowScale);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(aModel));
            
            if (isColorPass) {
                float alpha = isHovered ? 0.95f : 0.5f;
                glUniform4f(colorLoc, color.x, color.y, color.z, alpha);
            }
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Arrow head
            glm::vec3 headPos = arrowCenter + axis * shaftLength;
            glm::mat4 hModel = glm::mat4(1.0f);
            hModel = glm::translate(hModel, headPos);
            hModel = glm::scale(hModel, glm::vec3(voxelSize * 2.5f)); // Slimmer arrow heads
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(hModel));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        };

        auto drawAllArrows = [&](bool isColorPass) {
            glm::vec3 cRed(0.7f, 0.1f, 0.1f);
            glm::vec3 cGreen(0.1f, 0.7f, 0.1f);
            glm::vec3 cYellow(0.7f, 0.7f, 0.1f);
            
            if (!isColorPass) {
                cRed *= 0.15f;
                cGreen *= 0.15f;
                cYellow *= 0.15f;
            } else {
                cRed *= 0.8f;
                cGreen *= 0.8f;
                cYellow *= 0.8f;
            }

            // Dynamic arrow lengths based on gizmo extents
            float maxExt = std::max(std::max(gizmo.extents.x, gizmo.extents.y), gizmo.extents.z);
            float moveArrowLen = maxExt + 4.0f * voxelSize; 
            float scaleArrowLen = 6.0f * voxelSize; 

            if (!gizmo.isScalingMode) {
                // Movement arrows at center
                drawVoxelArrow(glm::vec3(1,0,0), center, gizmo.activeDrag == UI::GizmoDragState::MoveX, isColorPass, cRed, moveArrowLen);
                drawVoxelArrow(glm::vec3(0,1,0), center, gizmo.activeDrag == UI::GizmoDragState::MoveY, isColorPass, cGreen, moveArrowLen);
                drawVoxelArrow(glm::vec3(0,0,1), center, gizmo.activeDrag == UI::GizmoDragState::MoveZ, isColorPass, cYellow, moveArrowLen);
            } else {
                // Scaling arrows at faces
                drawVoxelArrow(glm::vec3(1,0,0), center + glm::vec3(gizmo.extents.x, 0, 0), gizmo.activeDrag == UI::GizmoDragState::ScalePX, isColorPass, cRed, scaleArrowLen);
                drawVoxelArrow(glm::vec3(-1,0,0), center - glm::vec3(gizmo.extents.x, 0, 0), gizmo.activeDrag == UI::GizmoDragState::ScaleNX, isColorPass, cRed, scaleArrowLen);
                drawVoxelArrow(glm::vec3(0,1,0), center + glm::vec3(0, gizmo.extents.y, 0), gizmo.activeDrag == UI::GizmoDragState::ScalePY, isColorPass, cGreen, scaleArrowLen);
                drawVoxelArrow(glm::vec3(0,-1,0), center - glm::vec3(0, gizmo.extents.y, 0), gizmo.activeDrag == UI::GizmoDragState::ScaleNY, isColorPass, cGreen, scaleArrowLen);
                drawVoxelArrow(glm::vec3(0,0,1), center + glm::vec3(0, 0, gizmo.extents.z), gizmo.activeDrag == UI::GizmoDragState::ScalePZ, isColorPass, cYellow, scaleArrowLen);
                drawVoxelArrow(glm::vec3(0,0,-1), center - glm::vec3(0, 0, gizmo.extents.z), gizmo.activeDrag == UI::GizmoDragState::ScaleNZ, isColorPass, cYellow, scaleArrowLen);
            }
        };

        // --- ARROWS DEPTH PRE-PASS ---
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        drawAllArrows(false);
        
        // Color pass
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        drawAllArrows(true);

        // Reset state
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    
    // Restore standard states
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void drawActiveWeapon(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLint projLoc, GLint viewLoc, GLuint VAO) {
    glClear(GL_DEPTH_BUFFER_BIT);
    int w, h;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    // --- Procedural Animation ---
    float swayX = sinf(hammerSwayTimer * 1.5f) * 0.015f;
    float swayY = cosf(hammerSwayTimer * 2.0f) * 0.012f;
    
    float lagX = hammerOffset.x * 0.005f;
    float lagY = hammerOffset.y * 0.005f;
    float jumpSway = -playerVelocityY * 0.02f;

    glm::vec3 wBase;
    float baseRotationY, baseRotationX, tilt;

    if (currentWeapon == 0) {
        glm::mat4 uiProj = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,2.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3(hammerBasePos.x + swayX + lagX + swingX + hammerRecoilPos.x,
                          hammerBasePos.y + swayY + lagY + swingY + jumpSway + hammerRecoilPos.y - 0.5f, 
                          hammerBasePos.z + swingZ + hammerRecoilPos.z - 0.8f);
        baseRotationY = hammerBaseRot.x + hammerOffset.x * 0.5f + swingRotY + hammerRecoilRot.y;
        baseRotationX = hammerBaseRot.y + swingRotX + hammerRecoilRot.x;
        tilt = hammerTilt;
    } else if (currentWeapon == 1) {
        glm::mat4 uiProj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,1.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3( 0.20f + swayX + lagX + hammerRecoilPos.x,
                          -0.15f + swayY + lagY + jumpSway + hammerRecoilPos.y,
                          -0.05f + hammerRecoilPos.z);
        baseRotationY =  198.0f + hammerOffset.x * 0.3f + hammerRecoilRot.y;
        baseRotationX =   -6.0f + hammerOffset.y * 0.3f + hammerRecoilRot.x;
        tilt = -7.0f;
    } else if (currentWeapon == 2) {
        glm::mat4 uiProj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,1.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3(-0.18f + swayX + lagX,
                          -0.10f + swayY + lagY + jumpSway,
                          -0.10f);
        baseRotationY = 160.0f + hammerOffset.x * 0.2f;
        baseRotationX =  15.0f + hammerOffset.y * 0.2f;
        tilt = 10.0f;
    } else if (currentWeapon == 3) {
        glm::mat4 uiProj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,1.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3( 0.15f + swayX + lagX + hammerRecoilPos.x,
                          -0.15f + swayY + lagY + jumpSway + hammerRecoilPos.y,
                          -0.10f + hammerRecoilPos.z);
        baseRotationY = 195.0f + hammerOffset.x * 0.25f + hammerRecoilRot.y;
        baseRotationX =  -3.0f + hammerOffset.y * 0.25f + hammerRecoilRot.x;
        tilt = -5.0f;
    } else if (currentWeapon == 4) {
        glm::mat4 uiProj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,1.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3( 0.25f + swayX + lagX + hammerRecoilPos.x,
                          -0.15f + swayY + lagY + jumpSway + hammerRecoilPos.y,
                          -0.15f + hammerRecoilPos.z);
        baseRotationY = 197.0f + hammerOffset.x * 0.35f + hammerRecoilRot.y;
        baseRotationX =  -6.0f + hammerOffset.y * 0.35f + hammerRecoilRot.x;
        tilt = -6.0f;
    } else if (currentWeapon == 5) {
        glm::mat4 uiProj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 10.0f);
        glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,1.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

        wBase = glm::vec3( 0.18f + swayX + lagX,
                          -0.15f + swayY + lagY + jumpSway,
                          -0.10f);
        baseRotationY = 180.0f + hammerOffset.x * 0.2f;
        baseRotationX =  10.0f + hammerOffset.y * 0.2f;
        tilt = 0.0f;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, wBase);
    model = glm::rotate(model, glm::radians(baseRotationX), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(baseRotationY), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(tilt),          glm::vec3(0, 0, 1));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // Draw active weapon
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // AO is pre-multiplied in vertex colors!
    glUniform1f(shadowLoc, 0.0f); // No shadow block, pure AO!

    if (currentWeapon == 0) {
        if (hammerHandleVAO > 0) {
            glBindVertexArray(hammerHandleVAO);
            glDrawArrays(GL_TRIANGLES, 0, hammerHandleVertexCount);
        }
    } else if (currentWeapon == 1) {
        if (ak47VAO > 0) {
            glBindVertexArray(ak47VAO);
            glDrawArrays(GL_TRIANGLES, 0, ak47VertexCount);
        }
    } else if (currentWeapon == 2) {
        if (dynVAO > 0) {
            glBindVertexArray(dynVAO);
            glDrawArrays(GL_TRIANGLES, 0, dynVertexCount);
        }
    } else if (currentWeapon == 3) {
        if (glockVAO > 0) {
            glBindVertexArray(glockVAO);
            glDrawArrays(GL_TRIANGLES, 0, glockVertexCount);
        }
    } else if (currentWeapon == 4) {
        if (shotgunVAO > 0) {
            glBindVertexArray(shotgunVAO);
            glDrawArrays(GL_TRIANGLES, 0, shotgunVertexCount);
        }
    }
}




#include <iostream>
