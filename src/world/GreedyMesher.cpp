#include "GreedyMesher.h"
#include "World.h"
#include "../rendering/Renderer.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

void performGreedyMeshing(std::vector<VoxelVertex>& v, std::vector<VoxelVertex>& wv, int x0, int x1, int y0, int y1, int z0, int z1) {
    {
        std::shared_lock<std::shared_mutex> lock(chunkMutex);
        int cx = x0 / CHUNK_SIZE;
        int cy = y0 / CHUNK_SIZE;
        int cz = z0 / CHUNK_SIZE;
        if (chunkManager.find(glm::ivec3(cx, cy, cz)) == chunkManager.end()) {
            return; // Chunk is completely air, no need to mesh
        }
    }



    auto isSolid = [&](int x, int y, int z) -> bool {
        uint8_t t = getVoxel(x, y, z);
        return t > 0 && t != 8;
    };

    auto isOpaque = [](uint8_t t) -> bool {
        return t > 0 && t != 8 && t != 5 && t != 9 && t != 10 && t != 11 && t != 12 && t != 21 && t != 22 && t != 26 && t != 27 && t != 28;
    };

    auto getAOScore = [&](bool s1, bool s2, bool corner) -> uint8_t {
        if (s1 && s2) return 0;
        int count = (s1 ? 1 : 0) + (s2 ? 1 : 0) + (corner ? 1 : 0);
        if (count == 3) return 0;
        if (count == 2) return 1;
        if (count == 1) return 2;
        return 3;
    };

    auto getVertexLight = [&](int bx, int by, int bz, int dx1, int dy1, int dz1, int dx2, int dy2, int dz2, int dx3, int dy3, int dz3) -> uint8_t {
        bool s1 = isSolid(bx + dx1, by + dy1, bz + dz1);
        bool s2 = isSolid(bx + dx2, by + dy2, bz + dz2);
        int sum = getLight(bx, by, bz);
        sum += s1 ? 0 : getLight(bx + dx1, by + dy1, bz + dz1);
        sum += s2 ? 0 : getLight(bx + dx2, by + dy2, bz + dz2);
        sum += (s1 && s2) ? 0 : getLight(bx + dx3, by + dy3, bz + dz3);
        return (uint8_t)(sum / 4);
    };

    const float floatAO[4] = { 0.35f, 0.56f, 0.78f, 1.00f };

    auto getVoxelGroundY = [&](int vx, int vy, int vz) -> int {
        int gy = vy;
        while (gy > 0) {
            uint8_t t = getVoxel(vx, gy - 1, vz);
            // Treat air (0) and vegetation as non-ground, keep searching downwards
            bool isSolidGround = (t != 0 && t != 5 && t != 9 && t != 10 && t != 11 && t != 12 && t != 21 && t != 22 && t != 26);
            if (isSolidGround) return gy;
            gy--;
        }
        return 0;
    };

    auto pushQuad = [&](uint8_t type, uint8_t aoKey, uint16_t lightKey, uint8_t gY, float r, float g, float b, const glm::vec3& n, const glm::vec3 corners[6]) {
        float f0 = floatAO[(aoKey >> 6) & 3];
        float f1 = floatAO[(aoKey >> 4) & 3];
        float f2 = floatAO[(aoKey >> 2) & 3];
        float f3 = floatAO[aoKey & 3];
        float ao[6] = { f0, f1, f2, f0, f2, f3 };
        uint8_t l0 = (lightKey >> 12) & 0xF;
        uint8_t l1 = (lightKey >> 8) & 0xF;
        uint8_t l2 = (lightKey >> 4) & 0xF;
        uint8_t l3 = lightKey & 0xF;
        uint8_t vLight[6] = { l0, l1, l2, l0, l2, l3 };
        bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
        for (int i = 0; i < 6; i++) {
            float finalAO = ao[i];
            if (isVeg || type == 26) {
                float groundY = (float)gY;
                finalAO = -(ao[i] + (float)type * 1000.0f + groundY * 10.0f + 10.0f);
            }
            uint8_t cr = (uint8_t)(r * 255.0f);
            uint8_t cg = (uint8_t)(g * 255.0f);
            uint8_t cb = (uint8_t)(b * 255.0f);
            int8_t cnx = (int8_t)(n.x * 127.0f);
            int8_t cny = (int8_t)(n.y * 127.0f);
            int8_t cnz = (int8_t)(n.z * 127.0f);
            uint8_t isEm = (type == 31) ? 255 : 0;
            uint8_t lightLevel = vLight[i];
            if (type == 8) wv.push_back({corners[i].x, corners[i].y, corners[i].z, cr, cg, cb, cnx, cny, cnz, isEm, lightLevel, finalAO}); else v.push_back({corners[i].x, corners[i].y, corners[i].z, cr, cg, cb, cnx, cny, cnz, isEm, lightLevel, finalAO});
        }
    };

    // --- Face 0: +X Right ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int x = x0; x < x1; x++) {
            // fill removed
            for (int y = y0; y < y1; y++) {
                for (int z = z0; z < z1; z++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x + 1, y, z);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x+1, y-1, z), isSolid(x+1, y, z+1), isSolid(x+1, y-1, z+1));
                            uint8_t s1 = getAOScore(isSolid(x+1, y-1, z), isSolid(x+1, y, z-1), isSolid(x+1, y-1, z-1));
                            uint8_t s2 = getAOScore(isSolid(x+1, y+1, z), isSolid(x+1, y, z-1), isSolid(x+1, y+1, z-1));
                            uint8_t s3 = getAOScore(isSolid(x+1, y+1, z), isSolid(x+1, y, z+1), isSolid(x+1, y+1, z+1));
                            
                            uint8_t l0 = getVertexLight(x+1, y, z, 0, -1, 0, 0, 0, 1, 0, -1, 1);
                            uint8_t l1 = getVertexLight(x+1, y, z, 0, -1, 0, 0, 0, -1, 0, -1, -1);
                            uint8_t l2 = getVertexLight(x+1, y, z, 0, 1, 0, 0, 0, -1, 0, 1, -1);
                            uint8_t l3 = getVertexLight(x+1, y, z, 0, 1, 0, 0, 0, 1, 0, 1, 1);
                            
                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(y - y0) * CHUNK_SIZE + (z - z0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int y = y0; y < y1; y++) {
                for (int z = z0; z < z1; z++) {
                    uint64_t maskVal = mask[(y - y0) * CHUNK_SIZE + (z - z0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) { while (z + w < z1 && mask[(y - y0) * CHUNK_SIZE + (z + w - z0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) { while (y + h < y1 && canGrow) {
                            for (int k = 0; k < w; k++) {
                                if (mask[(y + h - y0) * CHUNK_SIZE + (z + k - z0)] != maskVal) { canGrow = false; break; }
                            }
                            if (canGrow) h++;
                        } }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;
                        
                        

                        float px = x * voxelSize;
                        float py = y * voxelSize;
                        float pz = z * voxelSize;

                        glm::vec3 n(1, 0, 0);
                        glm::vec3 corners[6] = {
                            {px + voxelSize, py, pz + w * voxelSize},
                            {px + voxelSize, py, pz},
                            {px + voxelSize, py + h * voxelSize, pz},
                            {px + voxelSize, py, pz + w * voxelSize},
                            {px + voxelSize, py + h * voxelSize, pz},
                            {px + voxelSize, py + h * voxelSize, pz + w * voxelSize}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int ry = 0; ry < h; ry++) {
                            for (int rz = 0; rz < w; rz++) {
                                mask[(y + ry - y0) * CHUNK_SIZE + (z + rz - z0)] = 0;
                            }
                        }
                        z += w - 1;
                    }
                }
            }
        }
    }

    // --- Face 1: -X Left ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int x = x0; x < x1; x++) {
            // fill removed
            for (int y = y0; y < y1; y++) {
                for (int z = z0; z < z1; z++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x - 1, y, z);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x-1, y-1, z), isSolid(x-1, y, z-1), isSolid(x-1, y-1, z-1));
                            uint8_t s1 = getAOScore(isSolid(x-1, y-1, z), isSolid(x-1, y, z+1), isSolid(x-1, y-1, z+1));
                            uint8_t s2 = getAOScore(isSolid(x-1, y+1, z), isSolid(x-1, y, z+1), isSolid(x-1, y+1, z+1));
                            uint8_t s3 = getAOScore(isSolid(x-1, y+1, z), isSolid(x-1, y, z-1), isSolid(x-1, y+1, z-1));

                            uint8_t l0 = getVertexLight(x-1, y, z, 0, -1, 0, 0, 0, -1, 0, -1, -1);
                            uint8_t l1 = getVertexLight(x-1, y, z, 0, -1, 0, 0, 0, 1, 0, -1, 1);
                            uint8_t l2 = getVertexLight(x-1, y, z, 0, 1, 0, 0, 0, 1, 0, 1, 1);
                            uint8_t l3 = getVertexLight(x-1, y, z, 0, 1, 0, 0, 0, -1, 0, 1, -1);

                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(y - y0) * CHUNK_SIZE + (z - z0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int y = y0; y < y1; y++) {
                for (int z = z0; z < z1; z++) {
                    uint64_t maskVal = mask[(y - y0) * CHUNK_SIZE + (z - z0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) { while (z + w < z1 && mask[(y - y0) * CHUNK_SIZE + (z + w - z0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) { while (y + h < y1 && canGrow) {
                            for (int k = 0; k < w; k++) {
                                if (mask[(y + h - y0) * CHUNK_SIZE + (z + k - z0)] != maskVal) { canGrow = false; break; }
                            }
                            if (canGrow) h++;
                        } }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;
                        
                        float px = x * voxelSize;
                        float py = y * voxelSize;
                        float pz = z * voxelSize;
                        float pz_w = (z + w) * voxelSize;
                        float py_h = (y + h) * voxelSize;

                        glm::vec3 n(-1, 0, 0);
                        glm::vec3 corners[6] = {
                            {px, py, pz},
                            {px, py, pz_w},
                            {px, py_h, pz_w},
                            {px, py, pz},
                            {px, py_h, pz_w},
                            {px, py_h, pz}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int ry = 0; ry < h; ry++) {
                            for (int rz = 0; rz < w; rz++) {
                                mask[(y + ry - y0) * CHUNK_SIZE + (z + rz - z0)] = 0;
                            }
                        }
                        z += w - 1;
                    }
                }
            }
        }
    }

    // --- Face 2: +Y Top ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int y = y0; y < y1; y++) {
            // fill removed
            for (int x = x0; x < x1; x++) {
                for (int z = z0; z < z1; z++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x, y + 1, z);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x-1, y+1, z), isSolid(x, y+1, z+1), isSolid(x-1, y+1, z+1));
                            uint8_t s1 = getAOScore(isSolid(x+1, y+1, z), isSolid(x, y+1, z+1), isSolid(x+1, y+1, z+1));
                            uint8_t s2 = getAOScore(isSolid(x+1, y+1, z), isSolid(x, y+1, z-1), isSolid(x+1, y+1, z-1));
                            uint8_t s3 = getAOScore(isSolid(x-1, y+1, z), isSolid(x, y+1, z-1), isSolid(x-1, y+1, z-1));

                            uint8_t l0 = getVertexLight(x, y+1, z, -1, 0, 0, 0, 0, 1, -1, 0, 1);
                            uint8_t l1 = getVertexLight(x, y+1, z, 1, 0, 0, 0, 0, 1, 1, 0, 1);
                            uint8_t l2 = getVertexLight(x, y+1, z, 1, 0, 0, 0, 0, -1, 1, 0, -1);
                            uint8_t l3 = getVertexLight(x, y+1, z, -1, 0, 0, 0, 0, -1, -1, 0, -1);

                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(x - x0) * CHUNK_SIZE + (z - z0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int x = x0; x < x1; x++) {
                for (int z = z0; z < z1; z++) {
                    uint64_t maskVal = mask[(x - x0) * CHUNK_SIZE + (z - z0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) {
                            while (z + w < z1 && mask[(x - x0) * CHUNK_SIZE + (z + w - z0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) {
                            while (x + h < x1 && canGrow) {
                                for (int k = 0; k < w; k++) {
                                    if (mask[(x + h - x0) * CHUNK_SIZE + (z + k - z0)] != maskVal) { canGrow = false; break; }
                                }
                                if (canGrow) h++;
                            }
                        }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;
                        
                        float px = x * voxelSize;
                        float py = (y + 1) * voxelSize;
                        float pz = z * voxelSize;
                        float px_h = (x + h) * voxelSize;
                        float pz_w = (z + w) * voxelSize;

                        glm::vec3 n(0, 1, 0);
                        glm::vec3 corners[6] = {
                            {px, py, pz_w},
                            {px_h, py, pz_w},
                            {px_h, py, pz},
                            {px, py, pz_w},
                            {px_h, py, pz},
                            {px, py, pz}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int rx = 0; rx < h; rx++) {
                            for (int rz = 0; rz < w; rz++) {
                                mask[(x + rx - x0) * CHUNK_SIZE + (z + rz - z0)] = 0;
                            }
                        }
                        z += w - 1;
                    }
                }
            }
        }
    }

    // --- Face 3: -Y Bottom ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int y = y0; y < y1; y++) {
            // fill removed
            for (int x = x0; x < x1; x++) {
                for (int z = z0; z < z1; z++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x, y - 1, z);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x-1, y-1, z), isSolid(x, y-1, z-1), isSolid(x-1, y-1, z-1));
                            uint8_t s1 = getAOScore(isSolid(x+1, y-1, z), isSolid(x, y-1, z-1), isSolid(x+1, y-1, z-1));
                            uint8_t s2 = getAOScore(isSolid(x+1, y-1, z), isSolid(x, y-1, z+1), isSolid(x+1, y-1, z+1));
                            uint8_t s3 = getAOScore(isSolid(x-1, y-1, z), isSolid(x, y-1, z+1), isSolid(x-1, y-1, z+1));

                            uint8_t l0 = getVertexLight(x, y-1, z, -1, 0, 0, 0, 0, -1, -1, 0, -1);
                            uint8_t l1 = getVertexLight(x, y-1, z, 1, 0, 0, 0, 0, -1, 1, 0, -1);
                            uint8_t l2 = getVertexLight(x, y-1, z, 1, 0, 0, 0, 0, 1, 1, 0, 1);
                            uint8_t l3 = getVertexLight(x, y-1, z, -1, 0, 0, 0, 0, 1, -1, 0, 1);

                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(x - x0) * CHUNK_SIZE + (z - z0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int x = x0; x < x1; x++) {
                for (int z = z0; z < z1; z++) {
                    uint64_t maskVal = mask[(x - x0) * CHUNK_SIZE + (z - z0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) {
                            while (z + w < z1 && mask[(x - x0) * CHUNK_SIZE + (z + w - z0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) {
                            while (x + h < x1 && canGrow) {
                                for (int k = 0; k < w; k++) {
                                    if (mask[(x + h - x0) * CHUNK_SIZE + (z + k - z0)] != maskVal) { canGrow = false; break; }
                                }
                                if (canGrow) h++;
                            }
                        }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;
                        
                        float px = x * voxelSize;
                        float py = y * voxelSize;
                        float pz = z * voxelSize;
                        float px_h = (x + h) * voxelSize;
                        float pz_w = (z + w) * voxelSize;

                        glm::vec3 n(0, -1, 0);
                        glm::vec3 corners[6] = {
                            {px, py, pz},
                            {px_h, py, pz},
                            {px_h, py, pz_w},
                            {px, py, pz},
                            {px_h, py, pz_w},
                            {px, py, pz_w}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int rx = 0; rx < h; rx++) {
                            for (int rz = 0; rz < w; rz++) {
                                mask[(x + rx - x0) * CHUNK_SIZE + (z + rz - z0)] = 0;
                            }
                        }
                        z += w - 1;
                    }
                }
            }
        }
    }

    // --- Face 4: +Z Front ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int z = z0; z < z1; z++) {
            // fill removed
            for (int x = x0; x < x1; x++) {
                for (int y = y0; y < y1; y++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x, y, z + 1);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x+1, y, z+1), isSolid(x, y-1, z+1), isSolid(x+1, y-1, z+1));
                            uint8_t s1 = getAOScore(isSolid(x-1, y, z+1), isSolid(x, y-1, z+1), isSolid(x-1, y-1, z+1));
                            uint8_t s2 = getAOScore(isSolid(x-1, y, z+1), isSolid(x, y+1, z+1), isSolid(x-1, y+1, z+1));
                            uint8_t s3 = getAOScore(isSolid(x+1, y, z+1), isSolid(x, y+1, z+1), isSolid(x+1, y+1, z+1));

                            uint8_t l0 = getVertexLight(x, y, z+1, 1, 0, 0, 0, -1, 0, 1, -1, 0);
                            uint8_t l1 = getVertexLight(x, y, z+1, -1, 0, 0, 0, -1, 0, -1, -1, 0);
                            uint8_t l2 = getVertexLight(x, y, z+1, -1, 0, 0, 0, 1, 0, -1, 1, 0);
                            uint8_t l3 = getVertexLight(x, y, z+1, 1, 0, 0, 0, 1, 0, 1, 1, 0);

                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(x - x0) * CHUNK_SIZE + (y - y0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int x = x0; x < x1; x++) {
                for (int y = y0; y < y1; y++) {
                    uint64_t maskVal = mask[(x - x0) * CHUNK_SIZE + (y - y0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) {
                            while (y + w < y1 && mask[(x - x0) * CHUNK_SIZE + (y + w - y0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) {
                            while (x + h < x1 && canGrow) {
                                for (int k = 0; k < w; k++) {
                                    if (mask[(x + h - x0) * CHUNK_SIZE + (y + k - y0)] != maskVal) { canGrow = false; break; }
                                }
                                if (canGrow) h++;
                            }
                        }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;

                        float px = x * voxelSize;
                        float py = y * voxelSize;
                        float pz = (z + 1) * voxelSize;
                        float px_h = (x + h) * voxelSize;
                        float py_w = (y + w) * voxelSize;

                        glm::vec3 n(0, 0, 1);
                        glm::vec3 corners[6] = {
                            {px, py, pz},
                            {px_h, py, pz},
                            {px_h, py_w, pz},
                            {px, py, pz},
                            {px_h, py_w, pz},
                            {px, py_w, pz}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int rx = 0; rx < h; rx++) {
                            for (int ry = 0; ry < w; ry++) {
                                mask[(x + rx - x0) * CHUNK_SIZE + (y + ry - y0)] = 0;
                            }
                        }
                        y += w - 1;
                    }
                }
            }
        }
    }

    // --- Face 5: -Z Back ---
    {
        std::vector<uint64_t> mask(CHUNK_SIZE * CHUNK_SIZE, 0);
        for (int z = z0; z < z1; z++) {
            // fill removed
            for (int x = x0; x < x1; x++) {
                for (int y = y0; y < y1; y++) {
                    uint8_t type = getVoxel(x, y, z);
                    if (type > 0) {
                        uint8_t nType = getVoxel(x, y, z - 1);
                        bool shouldDraw = false;
                        if (type == 8) shouldDraw = (nType == 0);
                        else if (!isOpaque(type)) shouldDraw = (nType == 0 || nType != type);
                        else shouldDraw = (nType == 0 || !isOpaque(nType));

                        if (shouldDraw) {
                            uint8_t s0 = getAOScore(isSolid(x-1, y, z-1), isSolid(x, y-1, z-1), isSolid(x-1, y-1, z-1));
                            uint8_t s1 = getAOScore(isSolid(x+1, y, z-1), isSolid(x, y-1, z-1), isSolid(x+1, y-1, z-1));
                            uint8_t s2 = getAOScore(isSolid(x+1, y, z-1), isSolid(x, y+1, z-1), isSolid(x+1, y+1, z-1));
                            uint8_t s3 = getAOScore(isSolid(x-1, y, z-1), isSolid(x, y+1, z-1), isSolid(x-1, y+1, z-1));

                            uint8_t l0 = getVertexLight(x, y, z-1, -1, 0, 0, 0, -1, 0, -1, -1, 0);
                            uint8_t l1 = getVertexLight(x, y, z-1, 1, 0, 0, 0, -1, 0, 1, -1, 0);
                            uint8_t l2 = getVertexLight(x, y, z-1, 1, 0, 0, 0, 1, 0, 1, 1, 0);
                            uint8_t l3 = getVertexLight(x, y, z-1, -1, 0, 0, 0, 1, 0, -1, 1, 0);

                            uint64_t lightKey = (l0 << 12) | (l1 << 8) | (l2 << 4) | l3;
                            uint8_t aoKey = (s0 << 6) | (s1 << 4) | (s2 << 2) | s3;
                            bool isVeg = (type == 5 || type == 9 || type == 10 || type == 11 || type == 12 || type == 21 || type == 22 || type == 26);
                            uint64_t gY = isVeg ? getVoxelGroundY(x, y, z) : 0;
                            mask[(x - x0) * CHUNK_SIZE + (y - y0)] = (lightKey << 24) | (gY << 16) | ((uint64_t)type << 8) | aoKey;
                        }
                    }
                }
            }
            for (int x = x0; x < x1; x++) {
                for (int y = y0; y < y1; y++) {
                    uint64_t maskVal = mask[(x - x0) * CHUNK_SIZE + (y - y0)];
                    if (maskVal > 0) {
                        uint8_t type = (maskVal >> 8) & 0xFF;
                        uint8_t aoKey = maskVal & 0xFF;
                        uint8_t gY = (maskVal >> 16) & 0xFF;
                        uint16_t lightKey = (maskVal >> 24) & 0xFFFF;
                        int w = 1;
                        if (type != 8) {
                            while (y + w < y1 && mask[(x - x0) * CHUNK_SIZE + (y + w - y0)] == maskVal) w++;
                        }
                        int h = 1;
                        bool canGrow = true;
                        if (type != 8) {
                            while (x + h < x1 && canGrow) {
                                for (int k = 0; k < w; k++) {
                                    if (mask[(x + h - x0) * CHUNK_SIZE + (y + k - y0)] != maskVal) { canGrow = false; break; }
                                }
                                if (canGrow) h++;
                            }
                        }
                        glm::vec3 color = getVoxelColor(type);
                        float r = color.r, g = color.g, b = color.b;

                        float px = x * voxelSize;
                        float py = y * voxelSize;
                        float pz = z * voxelSize;
                        float px_h = (x + h) * voxelSize;
                        float py_w = (y + w) * voxelSize;

                        glm::vec3 n(0, 0, -1);
                        glm::vec3 corners[6] = {
                            {px_h, py, pz},
                            {px, py, pz},
                            {px, py_w, pz},
                            {px_h, py, pz},
                            {px, py_w, pz},
                            {px_h, py_w, pz}
                        };
                        pushQuad(type, aoKey, lightKey, gY, r, g, b, n, corners);
                        for (int rx = 0; rx < h; rx++) {
                            for (int ry = 0; ry < w; ry++) {
                                mask[(x + rx - x0) * CHUNK_SIZE + (y + ry - y0)] = 0;
                            }
                        }
                        y += w - 1;
                    }
                }
            }
        }
    }
}
