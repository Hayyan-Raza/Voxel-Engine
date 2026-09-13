#pragma once
#include <btBulletDynamicsCommon.h>
#include "../core/Types.h"
#include <glm/glm.hpp>

// Bullet physics world objects (extern for cleanup in main)
extern btDefaultCollisionConfiguration*     collisionConfiguration;
extern btCollisionDispatcher*                dispatcher;
extern btBroadphaseInterface*                overlappingPairCache;
extern btSequentialImpulseConstraintSolver* solver;
extern btDiscreteDynamicsWorld*             dynamicsWorld;
extern btRigidBody*                         playerRigidBody;
extern btCollisionShape*                    playerShape;

extern btRigidBody*                         hammerRigidBody;
extern btCollisionShape*                    hammerShape;

extern bool  hammerHitThisFrame;
extern glm::vec3 hammerHitWorldPos;
extern glm::vec3 hammerHitWorldNormal;

void initPhysics();
void updateChunkPhysics();

bool checkChunkCollision(const VoxelChunk& c, const glm::vec3& testCenter, const glm::quat& testRot, glm::vec3& outNormal, glm::vec3& outContactPt);
void shatterChunk(const VoxelChunk& parent, const glm::vec3& impactPos, std::vector<VoxelChunk>& outNewChunks);
void triggerExplosion(glm::vec3 pos);

void createPhysicsForChunk(VoxelChunk& chunk, glm::vec3 initialVelocity, glm::vec3 initialAngularVel);
void cleanupChunkPhysics(VoxelChunk& chunk);

