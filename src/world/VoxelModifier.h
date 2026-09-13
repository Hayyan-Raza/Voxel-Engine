#pragma once
#include <glm/glm.hpp>

// Modifies the terrain by applying spherical destruction at hitVox, creating debris chunks
void applyTerrainDestruction(const glm::ivec3& hitVox, const glm::vec3& hitWorldPos, int radius, const glm::vec3& ejectBaseDir);

// Extracts a single voxel from terrain to create a grabbable loose chunk
void extractTerrainToChunk(const glm::ivec3& hitVox, const glm::vec3& pullDir);
