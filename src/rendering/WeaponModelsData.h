#pragma once
#include <vector>
#include <cstring>
#include <glm/glm.hpp>
#include "../core/Types.h"

struct WeaponGrid {
    static const int SIZE = 80;
    uint8_t grid[SIZE][SIZE][SIZE];
    glm::vec3 colors[256];
    
    WeaponGrid() {
        memset(grid, 0, sizeof(grid));
        for(int i=0; i<256; i++) colors[i] = glm::vec3(1.0f);
    }
    
    void setCol(uint8_t type, glm::vec3 col) { colors[type] = col; }
    void set(int x, int y, int z, uint8_t type) {
        int cx = x + SIZE/2, cy = y + SIZE/2, cz = z + SIZE/2;
        if(cx>=0 && cx<SIZE && cy>=0 && cy<SIZE && cz>=0 && cz<SIZE) grid[cx][cy][cz] = type;
    }
    void box(int x0, int x1, int y0, int y1, int z0, int z1, uint8_t type) {
        for(int x=x0; x<=x1; x++) for(int y=y0; y<=y1; y++) for(int z=z0; z<=z1; z++) set(x,y,z,type);
    }
    uint8_t get(int x, int y, int z) {
        if(x<0 || x>=SIZE || y<0 || y>=SIZE || z<0 || z>=SIZE) return 0;
        return grid[x][y][z];
    }
    
    int vertexAO(int x, int y, int z, int d1x, int d1y, int d1z, int d2x, int d2y, int d2z) {
        bool side1 = get(x+d1x, y+d1y, z+d1z) > 0;
        bool side2 = get(x+d2x, y+d2y, z+d2z) > 0;
        bool corner = get(x+d1x+d2x, y+d1y+d2y, z+d1z+d2z) > 0;
        if (side1 && side2) return 0;
        return 3 - (side1 + side2 + corner);
    }
    
    void buildMesh(float vs, std::vector<VoxelVertex>& verts) {
        for(int x=0; x<SIZE; x++) for(int y=0; y<SIZE; y++) for(int z=0; z<SIZE; z++) {
            uint8_t type = grid[x][y][z];
            if(!type) continue;
            glm::vec3 c = colors[type];
            float wx = (x - SIZE/2) * vs, wy = (y - SIZE/2) * vs, wz = (z - SIZE/2) * vs;
            
            // X- (Left)
            if(!get(x-1,y,z)) {
                float a00 = vertexAO(x-1,y,z, 0,-1,0, 0,0,-1) / 3.0f;
                float a01 = vertexAO(x-1,y,z, 0,-1,0, 0,0,1) / 3.0f;
                float a10 = vertexAO(x-1,y,z, 0,1,0, 0,0,-1) / 3.0f;
                float a11 = vertexAO(x-1,y,z, 0,1,0, 0,0,1) / 3.0f;
                float nx=-1, ny=0, nz=0;
                verts.push_back({wx, wy, wz, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz+vs, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
            // X+ (Right)
            if(!get(x+1,y,z)) {
                float a00 = vertexAO(x+1,y,z, 0,-1,0, 0,0,-1) / 3.0f;
                float a01 = vertexAO(x+1,y,z, 0,-1,0, 0,0,1) / 3.0f;
                float a10 = vertexAO(x+1,y,z, 0,1,0, 0,0,-1) / 3.0f;
                float a11 = vertexAO(x+1,y,z, 0,1,0, 0,0,1) / 3.0f;
                float nx=1, ny=0, nz=0;
                verts.push_back({wx+vs, wy, wz, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz+vs, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
            // Y- (Bottom)
            if(!get(x,y-1,z)) {
                float a00 = vertexAO(x,y-1,z, -1,0,0, 0,0,-1) / 3.0f;
                float a01 = vertexAO(x,y-1,z, -1,0,0, 0,0,1) / 3.0f;
                float a10 = vertexAO(x,y-1,z, 1,0,0, 0,0,-1) / 3.0f;
                float a11 = vertexAO(x,y-1,z, 1,0,0, 0,0,1) / 3.0f;
                float nx=0, ny=-1, nz=0;
                verts.push_back({wx, wy, wz, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz+vs, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
            // Y+ (Top)
            if(!get(x,y+1,z)) {
                float a00 = vertexAO(x,y+1,z, -1,0,0, 0,0,-1) / 3.0f;
                float a01 = vertexAO(x,y+1,z, -1,0,0, 0,0,1) / 3.0f;
                float a10 = vertexAO(x,y+1,z, 1,0,0, 0,0,-1) / 3.0f;
                float a11 = vertexAO(x,y+1,z, 1,0,0, 0,0,1) / 3.0f;
                float nx=0, ny=1, nz=0;
                verts.push_back({wx, wy+vs, wz, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz+vs, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
            // Z- (Back)
            if(!get(x,y,z-1)) {
                float a00 = vertexAO(x,y,z-1, -1,0,0, 0,-1,0) / 3.0f;
                float a01 = vertexAO(x,y,z-1, 1,0,0, 0,-1,0) / 3.0f;
                float a10 = vertexAO(x,y,z-1, -1,0,0, 0,1,0) / 3.0f;
                float a11 = vertexAO(x,y,z-1, 1,0,0, 0,1,0) / 3.0f;
                float nx=0, ny=0, nz=-1;
                verts.push_back({wx+vs, wy, wz, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy, wz, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
            // Z+ (Front)
            if(!get(x,y,z+1)) {
                float a00 = vertexAO(x,y,z+1, -1,0,0, 0,-1,0) / 3.0f;
                float a01 = vertexAO(x,y,z+1, -1,0,0, 0,1,0) / 3.0f;
                float a10 = vertexAO(x,y,z+1, 1,0,0, 0,-1,0) / 3.0f;
                float a11 = vertexAO(x,y,z+1, 1,0,0, 0,1,0) / 3.0f;
                float nx=0, ny=0, nz=1;
                verts.push_back({wx, wy, wz+vs, (uint8_t)(c.r*a00*255.0f), (uint8_t)(c.g*a00*255.0f), (uint8_t)(c.b*a00*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz+vs, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy, wz+vs, (uint8_t)(c.r*a10*255.0f), (uint8_t)(c.g*a10*255.0f), (uint8_t)(c.b*a10*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx+vs, wy+vs, wz+vs, (uint8_t)(c.r*a11*255.0f), (uint8_t)(c.g*a11*255.0f), (uint8_t)(c.b*a11*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
                verts.push_back({wx, wy+vs, wz+vs, (uint8_t)(c.r*a01*255.0f), (uint8_t)(c.g*a01*255.0f), (uint8_t)(c.b*a01*255.0f), (int8_t)(nx*127.0f), (int8_t)(ny*127.0f), (int8_t)(nz*127.0f), 0, 0, 1.0f});
            }
        }
    }
};

inline void generateHammerGeometry(WeaponGrid& grid, float& scale) {
    scale = 0.0225f;
    grid.setCol(1, glm::vec3(0.15f, 0.15f, 0.15f));
    grid.setCol(2, glm::vec3(0.9f, 0.7f, 0.1f));   
    grid.setCol(3, glm::vec3(0.4f, 0.4f, 0.45f));  
    grid.setCol(4, glm::vec3(0.6f, 0.6f, 0.65f));  
    
    grid.box(-2, 2, -30, 15, -1, 1, 2); 
    grid.box(-1, 1, -30, 15, -2, 2, 2); 
    
    grid.box(-3, 3, -30, -5, -2, 2, 1);
    grid.box(-2, 2, -30, -5, -3, 3, 1);
    grid.box(-3, 3, -32, -30, -3, 3, 1);
    
    grid.box(-3, 3, 13, 19, -12, 12, 3);
    grid.box(-4, 4, 14, 18, -11, 11, 3); 
    
    grid.box(-3, 3, 14, 18, -13, -12, 4); 
    grid.box(-3, 3, 14, 18, 12, 13, 4);  
}

inline void generateAK47Geometry(WeaponGrid& grid, float& scale) {
    scale = 0.012f;
    grid.setCol(1, glm::vec3(0.6f, 0.3f, 0.1f)); 
    grid.setCol(2, glm::vec3(0.35f, 0.35f, 0.35f)); 
    grid.setCol(3, glm::vec3(0.45f, 0.45f, 0.45f)); 
    grid.setCol(4, glm::vec3(0.1f, 0.1f, 0.1f)); 
    
    grid.box(-2, 2, -2, 4, -8, 8, 2);
    grid.box(-1, 1, 4, 6, -8, 8, 2);
    
    for(int i=0; i<16; i++) {
        grid.box(-1, 1, -3 - i/2, 3 - i/3, -9 - i, -8 - i, 1);
    }
    grid.box(-1, 1, -3 - 15/2, 3 - 15/3, -25, -24, 4);
    
    for(int i=0; i<8; i++) {
        grid.box(-1, 1, -10 + i, -3 + i, -6 - i/2, -4 - i/2, 1);
    }
    
    for(int i=0; i<15; i++) {
        grid.box(-1, 1, -15 + i, -2, 2 + i/3, 6 + i/3, 3);
    }
    
    grid.box(-2, 2, -1, 3, 9, 20, 1);
    grid.box(-1, 1, 4, 6, 10, 18, 1);
    
    grid.box(-1, 1, 0, 2, 21, 36, 3);
    grid.box(-1, 1, 3, 5, 21, 28, 2);
    grid.box(-1, 1, 2, 5, 28, 30, 2);
    
    grid.box(-1, 1, 2, 7, 34, 35, 4);
    grid.box(-1, 1, 6, 8, 6, 8, 4);
    
    grid.box(-1.5f, 1.5f, -0.5f, 2.5f, 36, 38, 4);
}

inline void generateGlockGeometry(WeaponGrid& grid, float& scale) {
    scale = 0.012f;
    grid.setCol(1, glm::vec3(0.2f, 0.2f, 0.2f)); 
    grid.setCol(2, glm::vec3(0.6f, 0.6f, 0.65f)); 
    grid.setCol(3, glm::vec3(0.1f, 0.1f, 0.1f)); 
    
    grid.box(-2, 2, -3, 1, -6, 9, 1);
    grid.box(-2, 2, 2, 6, -7, 10, 2);
    
    for(int i=0; i<10; i++) {
        grid.box(-2, 2, -13 + i, -4, -7 - i/3, -1 - i/3, 1);
    }
    
    grid.box(-1, 1, -7, -4, 2, 3, 1);
    grid.box(-1, 1, -7, -6, 2, 6, 1);
    
    grid.box(0, 0, -4, -3, 4, 5, 3);
    
    grid.box(-1, 1, 6, 7, -6, -5, 3); 
    grid.box(0, 0, 6, 7, 8, 9, 3);   
    
    grid.setCol(4, glm::vec3(0.05f));
    grid.box(-1, 1, 3, 5, 10, 10, 4);
}

inline void generateShotgunGeometry(WeaponGrid& grid, float& scale) {
    scale = 0.012f;
    grid.setCol(1, glm::vec3(0.6f, 0.3f, 0.1f)); 
    grid.setCol(2, glm::vec3(0.5f, 0.5f, 0.55f)); 
    grid.setCol(3, glm::vec3(0.2f, 0.2f, 0.2f)); 
    
    grid.box(-2, 2, -2, 4, -6, 7, 2);
    
    for(int i=0; i<16; i++) {
        grid.box(-1, 1, -5 - i/2, 3 - i/4, -7 - i, -6 - i, 1);
    }
    
    grid.box(-1, 1, -7, -3, -5, -1, 1);
    
    grid.box(-2, -1, 0, 2, 8, 38, 2); 
    grid.box( 1,  2, 0, 2, 8, 38, 2); 
    grid.box(-1, 1, 0, 1, 8, 38, 2);  
    
    grid.box(-3, 3, -4, -1, 10, 24, 1);
    for(int z=11; z<=23; z+=3) {
        grid.box(-3.5f, 3.5f, -4.5f, -0.5f, z, z+1, 3);
    }
}

inline void generateDynamiteGeometry(WeaponGrid& grid, float& scale) {
    scale = 0.012f;
    grid.setCol(1, glm::vec3(0.8f, 0.1f, 0.1f)); 
    grid.setCol(2, glm::vec3(0.9f, 0.8f, 0.5f)); 
    grid.setCol(3, glm::vec3(0.2f, 0.2f, 0.2f)); 
    grid.setCol(4, glm::vec3(0.1f, 0.1f, 0.1f)); 
    
    grid.box(-4, 0, -3, 1, -10, 10, 1); 
    grid.box(1, 5, -3, 1, -10, 10, 1);  
    grid.box(-1, 3, 2, 6, -10, 10, 1);  
    
    grid.box(-3, -1, -2, 0, -11, -11, 4);
    grid.box(2, 4, -2, 0, -11, -11, 4);
    grid.box(0, 2, 3, 5, -11, -11, 4);
    
    grid.box(-3, -1, -2, 0, 11, 11, 4);
    grid.box(2, 4, -2, 0, 11, 11, 4);
    grid.box(0, 2, 3, 5, 11, 11, 4);
    
    grid.box(-5, 6, -4, 7, -5, -3, 2);
    grid.box(-5, 6, -4, 7, 3, 5, 2);
    
    grid.box(1, 1, 4, 6, 12, 12, 3);
    grid.box(1, 1, 6, 6, 11, 11, 3);
    
    grid.box(1, 1, 6, 6, -4, -2, 1); 
    grid.box(-1, -1, 6, 6, -4, -2, 2); 
}
