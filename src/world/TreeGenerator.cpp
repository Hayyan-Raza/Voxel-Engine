#include "TreeGenerator.h"
#include "World.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Helper: draw thick line (Trunks and Branches)
static void drawThickLine(glm::vec3 start, glm::vec3 end, float thicknessStart, float thicknessEnd) {
    glm::vec3 dir = end - start;
    float len = glm::length(dir);
    if (len == 0) return;
    dir = glm::normalize(dir);
    
    for (float t = 0; t <= len; t += 0.25f) { 
        glm::vec3 pos = start + dir * t;
        int px = (int)round(pos.x);
        int py = (int)round(pos.y);
        int pz = (int)round(pos.z);
        
        float currentThickness = thicknessStart + (thicknessEnd - thicknessStart) * (t / len);
        int thickInt = (int)ceil(currentThickness);

        for (int tx = -thickInt; tx <= thickInt; tx++) {
            for (int tz = -thickInt; tz <= thickInt; tz++) {
                for (int ty = -thickInt; ty <= thickInt; ty++) {
                    
                    float distSq = (tx*tx) + (ty*ty) + (tz*tz);
                    
                    float noise = ((rand() % 100) - 50) / 100.0f;
                    float effectiveSq = (currentThickness * currentThickness) + (noise * currentThickness * 0.3f);

                    if (distSq <= effectiveSq) {
                        int fx = px + tx;
                        int fy = py + ty;
                        int fz = pz + tz;
                        if (fy >= 0 && fy < WORLD_HEIGHT) {
                            uint8_t barkType = 4;
                            if ((fx * 37 + fy * 19 + fz * 17) % 4 == 0) {
                                barkType = 20; 
                            }
                            setVoxel(fx, fy, fz, barkType);
                        }
                    }
                }
            }
        }
    }
}

// TEARDOWN STYLE LEAVES: scattered, airy, single-voxel clumps
static void placeTeardownLeaves(int cx, int cy, int cz, float radius, bool isCherry) {
    // Density scaling. User requested to decrease leaves a little. Factor reduced from 1.5f to 1.2f.
    int numLeaves = (int)(radius * radius * radius * 1.2f); 
    
    for (int i = 0; i < numLeaves; i++) {
        // Random point inside the sphere using spherical coordinates
        float u = (rand() % 1000) / 1000.0f;
        float v = (rand() % 1000) / 1000.0f;
        float theta = u * 2.0f * glm::pi<float>();
        float phi = acos(2.0f * v - 1.0f);
        float r = std::cbrt((rand() % 1000) / 1000.0f) * radius;

        int fx = cx + (int)(r * sin(phi) * cos(theta));
        int fy = cy + (int)(r * cos(phi));
        int fz = cz + (int)(r * sin(phi) * sin(theta));

        // Place a SINGLE leaf voxel (maximum airiness)
        if (fy >= 0 && fy < WORLD_HEIGHT) {
            if (getVoxel(fx, fy, fz) == 0) { 
                if (isCherry) {
                    setVoxel(fx, fy, fz, (rand() % 3 == 0) ? 28 : 27);
                } else {
                    setVoxel(fx, fy, fz, (rand() % 3 == 0) ? 24 : 23);
                }
            }
        }
        
        // Randomly attach ONE extra leaf next to it 30% of the time
        if (rand() % 100 < 30) {
            int nx = fx + (rand() % 3 - 1);
            int ny = fy + (rand() % 3 - 1);
            int nz = fz + (rand() % 3 - 1);
            if (ny >= 0 && ny < WORLD_HEIGHT) {
                if (getVoxel(nx, ny, nz) == 0) {
                    setVoxel(nx, ny, nz, isCherry ? 28 : 23);
                }
            }
        }
    }
}

// RECURSIVE FRACTAL BRANCHING
static void buildBranchTree(glm::vec3 startPos, glm::vec3 direction, float length, float baseThick, int depth, bool isCherry) {
    if (depth > 3) return; 

    // Branches sag down under gravity
    if (depth > 0) {
        direction.y -= 0.15f * depth; 
        direction = glm::normalize(direction);
    }

    glm::vec3 endPos = startPos + direction * length;
    float endThick = baseThick * 0.5f;

    drawThickLine(startPos, endPos, baseThick, endThick);

    // --- LEAF PLACEMENT --- //
    // Depth 1+: Place foliage at the end of MAIN branches
    if (depth >= 1 && depth < 2) {
        // End of main branch
        placeTeardownLeaves(endPos.x, endPos.y, endPos.z, 5.0f + (rand() % 3), isCherry);
    }

    // Depth 2+: Twigs and heavy foliage clouds
    if (depth >= 2) {
        // End of twig
        placeTeardownLeaves(endPos.x, endPos.y, endPos.z, 7.0f + (rand() % 5), isCherry);
        
        // Midway on the twig: frequently place foliage for density
        if (rand() % 2 == 0) { 
            glm::vec3 mid = startPos + direction * (length * 0.5f);
            placeTeardownLeaves(mid.x, mid.y, mid.z, 5.0f + (rand() % 4), isCherry);
        }
        
        if (depth == 3) return; // twig ends, recursion stops here
    }

    // --- SUB-BRANCHING --- //
    // Slightly increase the number of branches at deeper levels
    int numBranches = (depth == 0) ? (4 + rand() % 3) : (3 + rand() % 3);
    
    for (int i = 0; i < numBranches; i++) {
        // Child branch origin
        float t = 0.3f + ((rand() % 70) / 100.0f); 
        if (depth == 0) t = 0.5f + ((rand() % 50) / 100.0f); 

        glm::vec3 childStart = startPos + direction * (length * t);
        
        // Calculate child branch direction
        float angle = (rand() % 360) * glm::pi<float>() / 180.0f;
        float pitch = (30 + rand() % 50) * glm::pi<float>() / 180.0f; 
        
        glm::vec3 childDir(cos(pitch) * cos(angle), sin(pitch), cos(pitch) * sin(angle));
        
        // Add some variation based on parent direction
        childDir = glm::normalize(childDir + direction * 0.5f);
        
        float childLength = length * (0.6f + (rand() % 20) / 100.0f);
        float childBaseThick = baseThick * (1.0f - t) + endThick * t; 

        buildBranchTree(childStart, childDir, childLength, childBaseThick * 0.7f, depth + 1, isCherry);
    }
}

void spawnTreeBranch(int sx, int sy, int sz, glm::vec3 dir, int length, bool isCherry) {
    // Left for signature compatibility
}

void generateOrganicTree(int cx, int cy, int cz) {
    bool isCherry = (rand() % 5 == 0); 
    
    // NORMALIZED SCALE (Short but proportionate)
    // User requested to increase height a little. Range subtly increased from (25.0f + rand%15) to (28.0f + rand%17).
    float trunkHeight = 28.0f + (rand() % 17); 
    
    // Yahan maine 6.0f ko 7.5f kar diya hai (Trunk bilkul thora sa patla ho jayega)
    float trunkThick = trunkHeight / 7.5f;     
    
    glm::vec3 rootStart(cx, cy, cz);
    glm::vec3 upDir = glm::normalize(glm::vec3((rand()%100-50)/300.0f, 1.0f, (rand()%100-50)/300.0f)); 
    
    // GENERATE ROOTS
    int numRoots = 4 + rand() % 4;
    for (int i = 0; i < numRoots; i++) {
        float angle = (i * (360.0f / numRoots)) + (rand() % 20);
        angle = angle * glm::pi<float>() / 180.0f;
        
        glm::vec3 rootDir(cos(angle), -0.6f, sin(angle));
        rootDir = glm::normalize(rootDir);
        
        float rootLen = trunkThick * 1.8f + (rand() % 4);
        glm::vec3 rStart = rootStart + glm::vec3(0, 2, 0); 
        
        drawThickLine(rStart, rStart + rootDir * rootLen, trunkThick * 0.7f, 0.5f);
    }
    
    // GENERATE TREE
    buildBranchTree(rootStart, upDir, trunkHeight, trunkThick, 0, isCherry);
}