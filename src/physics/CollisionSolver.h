#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../core/Globals.h"

// Check collision between a voxel chunk and the voxel world
bool checkChunkCollision(const VoxelChunk& c, const glm::vec3& testCenter, const glm::quat& testRot, glm::vec3& outNormal, glm::vec3& outContactPt);
