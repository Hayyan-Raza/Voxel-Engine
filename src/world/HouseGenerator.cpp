#include "World.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

inline void setSafeVoxel(int x, int y, int z, uint8_t type) {
    if (y >= 0 && y < WORLD_HEIGHT)
        setVoxel(x, y, z, type);
}

// Fills a volume. If hollow is true, only sets voxels on the boundaries.
static void fillBox(int ox, int oy, int oz, int w, int h, int d, uint8_t mat, bool hollow = false) {
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            for (int z = 0; z < d; z++) {
                if (hollow && x > 0 && x < w-1 && y > 0 && y < h-1 && z > 0 && z < d-1) continue;
                setSafeVoxel(ox + x, oy + y, oz + z, mat);
            }
        }
    }
}

static uint8_t getRandomStone() {
    int r = rand() % 10;
    if (r < 5) return 3;       // Grey stone
    if (r < 8) return 15;      // Dark stone
    return 16;                 // Light stone
}

static uint8_t getRandomThatch() {
    return (rand() % 2 == 0) ? 29 : 30;
}

static void buildThickStoneWalls(int ox, int oy, int oz, int W, int H, int D, int thickness, bool hasDoor) {
    int doorW = W > 100 ? 40 : 16;
    int doorH = H > 50 ? 50 : 24;
    int doorL = W/2 - doorW/2, doorR = W/2 + doorW/2 - 1;

    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) {
            for (int z = 0; z < D; z++) {
                bool isWall = (x < thickness || x >= W - thickness || z < thickness || z >= D - thickness);
                if (!isWall) {
                    setSafeVoxel(ox + x, oy + y, oz + z, 0); // Hollow inside
                    continue; 
                }

                // Door hole on z=0 wall
                if (hasDoor && z < thickness && x >= doorL && x <= doorR && y < doorH) {
                    // Frame
                    if (x == doorL || x == doorR || y == doorH - 1) {
                        setSafeVoxel(ox + x, oy + y, oz + z, 20); // Dark wood frame
                    } else {
                        setSafeVoxel(ox + x, oy + y, oz + z, 4); // Wood door
                    }
                    continue;
                }
                
                // Add window hole
                int winR = W > 100 ? 16 : 8;
                if (z >= D - thickness && x >= W/2 - winR && x <= W/2 + winR && y >= H/2 - winR && y <= H/2 + winR) {
                    if (x == W/2 - winR || x == W/2 + winR || y == H/2 - winR || y == H/2 + winR) {
                        setSafeVoxel(ox + x, oy + y, oz + z, 20);
                    } else {
                        setSafeVoxel(ox + x, oy + y, oz + z, 0);
                    }
                    continue;
                }


                setSafeVoxel(ox + x, oy + y, oz + z, getRandomStone());
            }
        }
    }
}

static void buildMacroTimberFrame(int ox, int oy, int oz, int W, int H, int D, int t) {
    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) {
            for (int z = 0; z < D; z++) {
                bool isWall = (x < t || x >= W - t || z < t || z >= D - t);
                if (!isWall) {
                    setSafeVoxel(ox + x, oy + y, oz + z, 0);
                    continue;
                }

                bool vPost = ((x < t || x >= W - t) && (z < t || z >= D - t)) ||
                             (x > 0 && x < W-1 && x % 40 < t) || 
                             (z > 0 && z < D-1 && z % 40 < t);
                bool hBeam = (y < t || y >= H - t || (y > 0 && y < H-1 && y % 30 < t));
                
                // simple diagonal pattern
                bool diag = false;
                if ((x+y)%40 < t || (z+y)%40 < t) diag = true;

                if (vPost || hBeam || diag) {
                    setSafeVoxel(ox + x, oy + y, oz + z, 4); // Wood
                } else {
                    setSafeVoxel(ox + x, oy + y, oz + z, (rand() % 3 == 0) ? 18 : 19); // Stucco
                }
            }
        }
    }
}

static void buildLayeredRoof(int ox, int oy, int oz, int W, int D, bool isThatch) {
    int stepW = 2; // roof thickness
    int stepH = 2; // roof height per step
    int peak = W/2 + 4; // Height

    for (int x = -10; x <= W + 10; x++) {
        int dx = std::abs(x - W/2);
        int plank = dx;
        int rY = oy + peak - plank;

        for (int z = -10; z <= D + 10; z++) {
            // Overhang limits
            if (dx > W/2 + 8) continue;
            
            bool isEaveX = (dx >= W/2 + 4);
            bool isEaveZ = (z < 0 || z > D);
            
            uint8_t mat;
            if (isEaveX || isEaveZ) {
                mat = 20; // Dark border
            } else {
                mat = isThatch ? getRandomThatch() : ((rand()%2==0)? 4 : 20);
            }

            for(int ty=0; ty < stepH; ty++) {
                setSafeVoxel(ox + x, rY - ty, oz + z, mat);
            }

            // Gable fill (stone)
            if (z >= 0 && z < D && dx <= W/2) {
                for (int fy = oy; fy < rY - stepH + 1; fy++) {
                    uint8_t gMat = isThatch ? getRandomStone() : ((rand() % 2 == 0) ? 18 : 19);
                    setSafeVoxel(ox + x, fy, oz + z, gMat);
                }
            }
        }
    }
}

static void buildWaterWell(int ox, int oy, int oz) {
    int W = 40, D = 40;
    
    // Base platform
    fillBox(ox - 10, oy, oz - 10, W + 20, 2, D + 20, 15);
    
    // Circular stone base
    int R = 18;
    int cx = ox + W/2, cz = oz + D/2;
    for (int x = 0; x < W; x++) {
        for (int z = 0; z < D; z++) {
            int dist = std::round(std::sqrt(std::pow(x - W/2, 2) + std::pow(z - D/2, 2)));
            if (dist <= R) {
                if (dist >= R - 4) {
                    fillBox(ox + x, oy + 2, oz + z, 1, 10, 1, getRandomStone());
                } else {
                    fillBox(ox + x, oy + 2, oz + z, 1, 8, 1, 8); // Water
                }
            }
        }
    }

    // Pillars
    fillBox(cx - 16, oy + 12, cz - 4, 8, 40, 8, 20);
    fillBox(cx + 8, oy + 12, cz - 4, 8, 40, 8, 20);
    
    // Cross beam
    fillBox(cx - 16, oy + 44, cz - 2, 32, 4, 4, 4);

    // Mini roof
    buildLayeredRoof(cx - 24, oy + 52, cz - 16, 48, 32, false);
}

// ── MAIN ENTRY ────────────────────────────────────────────────────────────────
void generateHouse(int startX, int startY, int startZ) {
    // We only generate the Colossal Manor
    int W = 300, D = 250, H1 = 100, H2 = 120, thickness = 8;
    
    // Adjust startX and startZ so it's centered around the coordinates passed
    startX -= W / 2;
    startZ -= D / 2;

    // Stone base
    buildThickStoneWalls(startX, startY, startZ, W, H1, D, thickness, true);
    
    // Internal wooden floor
    fillBox(startX + thickness, startY, startZ + thickness, W - thickness*2, 2, D - thickness*2, 4, false);
    
    // Timber upper floor, overhanging by 16
    buildMacroTimberFrame(startX - 16, startY + H1, startZ - 16, W + 32, H2, D + 32, thickness);
    
    // Roof
    buildLayeredRoof(startX - 16, startY + H1 + H2, startZ - 16, W + 32, D + 32, false);
    
    // Colossal Stacked Logs outside
    fillBox(startX + W + 20, startY + 2, startZ + 30, 20, 24, 20, 20);
    fillBox(startX + W + 20, startY + 2, startZ + 60, 20, 24, 20, 20);
    fillBox(startX + W + 20, startY + 26, startZ + 45, 20, 24, 20, 20);
}
