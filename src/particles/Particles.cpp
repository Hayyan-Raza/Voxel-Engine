#include "Particles.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include <iostream>

extern float particleLifespan;
#include <cstdlib>
#include <algorithm>

// Realistic neutral smoke tones
static const glm::vec3 kDustColors[4] = {
    {0.50f, 0.49f, 0.47f},  // Warm mid grey
    {0.40f, 0.39f, 0.38f},  // Warm dark grey
    {0.58f, 0.56f, 0.53f},  // Soft light grey
    {0.32f, 0.31f, 0.30f}   // Charcoal
};

// Grass / foliage: emits dry chaff/beige-grey dust (NOT bright green!)
static const glm::vec3 kGrassColors[3] = {
    {0.52f, 0.51f, 0.47f},  // Dried grass grey-beige
    {0.58f, 0.56f, 0.52f},  // Light straw-grey
    {0.45f, 0.44f, 0.40f}   // Shadows grey-beige
};

// Dirt: emits dusty warm-grey-brown
static const glm::vec3 kDirtColors[3] = {
    {0.45f, 0.41f, 0.37f},  // Warm dusty brown
    {0.50f, 0.46f, 0.42f},  // Light dirt grey-brown
    {0.38f, 0.34f, 0.30f}   // Shadow dirt grey-brown
};

// Stone: concrete grey dust
static const glm::vec3 kStoneColors[3] = {
    {0.55f, 0.55f, 0.55f},  // Mid grey
    {0.65f, 0.65f, 0.65f},  // Concrete light grey
    {0.42f, 0.42f, 0.42f}   // Dark stone grey
};

// Wood: beige sawdust
static const glm::vec3 kWoodColors[3] = {
    {0.60f, 0.56f, 0.50f},  // Light beige wood dust
    {0.52f, 0.48f, 0.42f},  // Medium beige wood dust
    {0.42f, 0.38f, 0.32f}   // Bark-grey wood dust
};

// Explosion charcoal smoke
static const glm::vec3 kExplosionColors[3] = {
    {0.24f, 0.24f, 0.24f},  // Dark charcoal soot
    {0.32f, 0.32f, 0.32f},  // Medium charcoal soot
    {0.18f, 0.18f, 0.18f}   // Near-black carbon soot
};

// SPH/Fluid dynamics vortex ring structure
struct VortexRing {
    glm::vec3 center;
    float radius;
    float strength;
    float life;
};
static std::vector<VortexRing> activeVortices;

void spawnDust(glm::vec3 pos, int count, uint8_t voxelType) {

    if (activeParticles.size() >= 1000) return;
    count = std::min(count, (int)(1000 - activeParticles.size()));

    if (voxelType == 6 && count > 10) {
        VortexRing vr;
        vr.center = pos;
        vr.radius = 0.08f;
        vr.strength = 3.0f;
        vr.life = 1.4f * particleLifespan;
        activeVortices.push_back(vr);
    }


    const glm::vec3* palette = kDustColors;
    int paletteSize = 4;

    if (voxelType == 1 || voxelType == 5) { // Grass or Leaves
        palette = kGrassColors;
        paletteSize = 3;
    } else if (voxelType == 2) { // Dirt
        palette = kDirtColors;
        paletteSize = 3;
    } else if (voxelType == 3) { // Stone
        palette = kStoneColors;
        paletteSize = 3;
    } else if (voxelType == 4) { // Wood
        palette = kWoodColors;
        paletteSize = 3;
    } else if (voxelType == 6) { // Explosion / Dynamite
        palette = kExplosionColors;
        paletteSize = 3;
    }

    for (int k = 0; k < count; k++) {
        Particle p;
        
        if (voxelType == 6) {
            // Explosion smoke & fire: wide scatter (up to 0.45m)
            float rx = ((rand()%200)/100.0f - 1.0f) * 0.45f;
            float ry = ((rand()%200)/100.0f - 1.0f) * 0.35f;
            float rz = ((rand()%200)/100.0f - 1.0f) * 0.45f;
            p.position = pos + glm::vec3(rx, ry, rz);
            
            glm::vec3 direction = glm::vec3(rx, ry + 0.15f, rz);
            if (glm::length(direction) > 0.001f) {
                direction = glm::normalize(direction);
            } else {
                direction = glm::vec3(0, 1, 0);
            }

            bool isFire = (k < count * 0.45); // 45% fire particles, 55% smoke particles
            if (isFire) {
                float speed = ((rand()%100)/100.0f) * 1.2f + 0.4f; // fast fire burst
                p.velocity = direction * speed;
                p.maxLife = (((rand()%100)/100.0f) * 0.3f + 0.3f) * particleLifespan; // short-lived fire (0.3s to 0.6s)
                p.size = 0.10f * (((rand()%100)/100.0f) * 0.6f + 0.7f); // smaller fire
                p.growthRate = 0.38f;
                // White-hot transitioning to bright yellow and deep orange
                float rColor = (rand() % 100) / 100.0f;
                if (rColor < 0.40f) {
                    p.color = glm::vec3(1.8f, 1.7f, 1.4f); // White-hot
                } else if (rColor < 0.75f) {
                    p.color = glm::vec3(1.5f, 0.85f, 0.15f); // Orange-yellow
                } else {
                    p.color = glm::vec3(1.2f, 0.35f, 0.06f); // Red-orange
                }
                p.buoyancy = 0.25f;

            } else {
                float speed = ((rand()%100)/100.0f) * 0.30f + 0.10f; // slow smoke billow
                p.velocity = direction * speed;
                p.maxLife = (((rand()%100)/100.0f) * 1.3f + 1.5f) * particleLifespan; // shorter smoke life (1.5s to 2.8s)
                p.size = 0.08f * (((rand()%100)/100.0f) * 0.8f + 0.6f); // smaller initial smoke
                p.growthRate = 0.15f; // compact expansion
                p.color = palette[rand() % paletteSize];
                p.buoyancy = 0.12f;
            }
        } else {
            // Normal hit dust
            p.position = pos + glm::vec3(
                ((rand()%200)/100.0f - 1.0f) * 0.04f,
                ((rand()%100)/100.0f)        * 0.02f,
                ((rand()%200)/100.0f - 1.0f) * 0.04f);
            
            float vx = ((rand()%200)/100.0f - 1.0f) * 0.12f;
            float vy = ((rand()%100)/100.0f)        * 0.12f + 0.06f;
            float vz = ((rand()%200)/100.0f - 1.0f) * 0.12f;
            p.velocity = glm::vec3(vx, vy, vz);
            p.maxLife = (((rand()%100)/100.0f) * 1.5f + 2.0f) * particleLifespan;
            p.size = 0.024f * (((rand()%100)/100.0f) * 1.2f + 0.6f);
            p.growthRate = 0.016f;
            p.color = palette[rand() % paletteSize];
            p.buoyancy = ((rand()%100)/100.0f) * 0.08f - 0.02f;
        }

        p.life     = p.maxLife;
        p.alpha    = 0.0f;
        p.rotation      = ((rand()%100)/100.0f) * 6.28318f;
        p.rotationSpeed = ((rand()%100)/100.0f - 0.5f) * 1.8f;
        
        activeParticles.push_back(p);
    }
}

void spawnSparks(glm::vec3 pos, int count) {
    if (activeParticles.size() >= 1000) return;
    count = std::min(count, (int)(1000 - activeParticles.size()));

    for (int k = 0; k < count; k++) {
        Particle p;
        p.position = pos + glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * 0.02f,
            ((rand()%200)/100.0f - 1.0f) * 0.02f,
            ((rand()%200)/100.0f - 1.0f) * 0.02f);
        float spd  = ((rand()%100)/100.0f) * 0.6f + 0.2f;
        p.velocity = glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * spd,
            ((rand()%200)/100.0f - 1.0f) * spd + 0.3f,
            ((rand()%200)/100.0f - 1.0f) * spd);
        p.maxLife  = (((rand()%100)/100.0f) * 0.4f + 0.2f) * particleLifespan;
        p.life     = p.maxLife;
        p.size     = 0.008f * (((rand()%100)/100.0f) * 1.2f + 0.5f);
        p.alpha    = 1.0f;
        p.rotation = 0.0f;
        p.rotationSpeed = 0.0f;
        p.growthRate = 0.0f;
        p.buoyancy = -0.5f; // Sparks fall down due to gravity
        
        float colorRand = (rand() % 100) / 100.0f;
        if (colorRand < 0.4f) {
            p.color = glm::vec3(1.0f, 0.4f, 0.0f);
        } else if (colorRand < 0.8f) {
            p.color = glm::vec3(1.0f, 0.75f, 0.0f);
        } else {
            p.color = glm::vec3(1.0f, 1.0f, 0.6f);
        }
        activeParticles.push_back(p);
    }
}

// Deep crimson blood droplets & flesh splatters
static const glm::vec3 kBloodColors[3] = {
    {0.70f, 0.05f, 0.08f},  // Bright crimson arterial blood
    {0.50f, 0.02f, 0.04f},  // Deep dark blood
    {0.35f, 0.01f, 0.02f}   // Clotted dark red
};

void spawnBlood(glm::vec3 pos, int count) {
    if (activeParticles.size() >= 1000) return;
    count = std::min(count, (int)(1000 - activeParticles.size()));

    for (int k = 0; k < count; k++) {
        Particle p;
        p.position = pos + glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * 0.03f,
            ((rand()%200)/100.0f - 1.0f) * 0.03f,
            ((rand()%200)/100.0f - 1.0f) * 0.03f);
        
        float spd = ((rand()%100)/100.0f) * 0.45f + 0.15f;
        p.velocity = glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * spd,
            ((rand()%100)/100.0f) * spd * 0.5f + 0.10f,
            ((rand()%200)/100.0f - 1.0f) * spd);

        p.maxLife = (((rand()%100)/100.0f) * 0.6f + 0.4f) * particleLifespan;
        p.life = p.maxLife;
        p.size = 0.014f * (((rand()%100)/100.0f) * 1.2f + 0.6f);
        p.alpha = 0.95f;
        p.growthRate = -0.005f;
        p.rotation = ((rand()%100)/100.0f) * 6.28318f;
        p.rotationSpeed = ((rand()%100)/100.0f - 0.5f) * 4.0f;
        p.buoyancy = -0.65f; // Heavy gravity fall

        p.color = kBloodColors[rand() % 3];
        activeParticles.push_back(p);
    }
}

void spawnFallingLeaves(glm::vec3 playerPos) {
    if (leafParticleDensity <= 0.0f) return;
    if (activeParticles.size() >= 2000) return; // Allow more particles for leaves
    
    // Attempt to spawn a leaf every few frames based on density
    if (rand() % 100 > (leafParticleDensity * 20.0f)) return;

    // Pick a random block in a 30m radius around the player
    float rx = playerPos.x + ((rand() % 600) / 10.0f - 30.0f);
    float ry = playerPos.y + ((rand() % 300) / 10.0f - 5.0f);
    float rz = playerPos.z + ((rand() % 600) / 10.0f - 30.0f);
    
    int vx = (int)round(rx);
    int vy = (int)round(ry);
    int vz = (int)round(rz);
    
    if (vy >= 0 && vy < WORLD_HEIGHT) {
        uint8_t type = getVoxel(vx, vy, vz);
        if (type == 23 || type == 24 || type == 27 || type == 28) {
            Particle p;
            p.position = glm::vec3(rx, ry - 0.6f, rz); // Spawn slightly below the leaf block
            p.velocity = glm::vec3(0.0f, -0.5f * leafParticleSpeed, 0.0f); // Gentle fall
            p.maxLife = (((rand()%100)/100.0f) * 2.0f + 4.0f); // Long life
            p.life = p.maxLife;
            p.size = 0.04f * (((rand()%100)/100.0f) * 0.5f + 0.8f); // Noticeable leaf size
            p.alpha = 0.9f;
            p.growthRate = 0.0f;
            p.rotation = ((rand()%100)/100.0f) * 6.28318f;
            p.rotationSpeed = ((rand()%100)/100.0f - 0.5f) * 1.5f;
            p.buoyancy = -0.1f; // Slow fall
            
            // Assign color based on leaf type
            if (type == 23 || type == 24) {
                p.color = glm::vec3(0.45f, 0.78f, 0.15f); // Green
                if (rand() % 2 == 0) p.color *= 0.8f;
            } else {
                p.color = glm::vec3(0.98f, 0.75f, 0.85f); // Pink Cherry Blossom
                if (rand() % 2 == 0) p.color *= 0.9f;
            }
            activeParticles.push_back(p);
        }
    }
}

void updateParticles() {
    // 1. Update active thermal vortex rings
    for (size_t i = 0; i < activeVortices.size(); ) {
        VortexRing& vr = activeVortices[i];
        vr.life -= deltaTime;
        if (vr.life <= 0.0f) {
            activeVortices[i] = activeVortices.back();
            activeVortices.pop_back();
            continue;
        }
        // Thermal columns rise and expand
        vr.center.y += 0.85f * deltaTime;
        vr.radius   += 0.28f * deltaTime;
        vr.strength *= (1.0f - 0.45f * deltaTime);
        i++;
    }

    const float DRAG      = 0.65f;
    const float TURB_FREQ = 2.0f;
    const float TURB_AMP  = 0.35f;

    for (size_t i = 0; i < activeParticles.size(); ) {
        Particle& p = activeParticles[i];
        p.life -= deltaTime;
        if (p.life <= 0.0f) { 
            activeParticles[i] = activeParticles.back();
            activeParticles.pop_back();
            continue; 
        }

        float progress = 1.0f - (p.life / p.maxLife);
        
        p.velocity *= (1.0f - DRAG * deltaTime);
        p.velocity.y += p.buoyancy * deltaTime;
        
        if (p.growthRate == 0.0f && p.buoyancy < 0.0f && p.buoyancy >= -0.2f) { // Leaf particle
            p.velocity.x += sinf(p.life * 2.0f + p.position.y) * 0.5f * leafParticleSpeed * deltaTime;
            p.velocity.z += cosf(p.life * 1.5f + p.position.x) * 0.5f * leafParticleSpeed * deltaTime;
        }
        
        float t = progress * 2.5f;
        float curlX = sinf(p.position.y * TURB_FREQ + t) * cosf(p.position.z * TURB_FREQ);
        float curlY = sinf(p.position.z * TURB_FREQ + t) * cosf(p.position.x * TURB_FREQ);
        float curlZ = sinf(p.position.x * TURB_FREQ + t) * cosf(p.position.y * TURB_FREQ);
        
        p.velocity.x += curlX * TURB_AMP * deltaTime;
        p.velocity.y += curlY * TURB_AMP * deltaTime;
        p.velocity.z += curlZ * TURB_AMP * deltaTime;

        // Particle-particle pressure removed for performance (was O(N²))

        p.position += p.velocity * deltaTime;
        p.rotation += p.rotationSpeed * deltaTime;
        p.size += (p.growthRate * deltaTime); 

        // Alpha handling: Fire (high growthRate) fades out directly, Smoke fades in and then out
        if (p.growthRate > 0.4f) {
            p.alpha = std::max(0.0f, (1.0f - progress) * 0.95f);
        } else {
            if (progress < 0.15f) {
                p.alpha = (progress / 0.15f) * 0.75f;
            } else {
                float fadeOut = (1.0f - progress) / 0.85f;
                p.alpha = std::max(0.0f, fadeOut * 0.75f);
            }
        }

        ++i;
    }
}


