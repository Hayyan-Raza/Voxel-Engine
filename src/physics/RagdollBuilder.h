#pragma once
#include "Ragdoll.h"
#include <glm/glm.hpp>

void initRagdollPartMesh(RagdollPart& part);
void cleanupRagdollPartMesh(RagdollPart& part);
void spawnRagdoll(const glm::vec3& position, const glm::vec3& initialVelocity);
