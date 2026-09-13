#pragma once
#include <glm/glm.hpp>
#include <cstdint>

void spawnDust(glm::vec3 pos, int count, uint8_t voxelType = 0);
void spawnSparks(glm::vec3 pos, int count);
void spawnBlood(glm::vec3 pos, int count);
void spawnFallingLeaves(glm::vec3 playerPos);
void updateParticles();
