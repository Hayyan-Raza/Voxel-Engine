
#pragma once
#include "Types.h"
#include "../ui/BuildingSystem.h"
#include <glm/glm.hpp>
#include <vector>

// ---- Active world objects ----
struct ChunkMesh;
extern std::vector<VoxelChunk> activeChunks;
extern std::vector<Particle>   activeParticles;
extern std::vector<ChunkMesh*> activeStaticMeshes;
extern std::vector<ChunkMesh*> activeWaterMeshes;
#include <mutex>
extern std::vector<ChunkMesh*> dirtyChunks;
extern std::mutex dirtyChunksMutex;

extern uint32_t nextChunkId;
extern uint32_t heldChunkId;
extern VoxelChunk* getHeldChunk();
extern bool isLookingAtGrabbable;

// ---- Camera ----
extern glm::vec3 cameraPos;
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;
extern bool spectatorMode;
extern glm::vec3 spectatorPos;
extern glm::vec3 spectatorFront;
extern glm::vec3 spectatorUp;
extern float spectatorYaw;
extern float spectatorPitch;

// ---- Player physics ----
extern float playerVelocityY;
extern glm::vec3 playerVelocityH; // Horizontal movement (velocity)
extern const float GRAVITY;
extern const float JUMP_FORCE;
extern bool  isGrounded;
extern float swingTimer;

// ---- Hammer Animation ----
extern glm::vec2 hammerOffset;    // Lag/inertia offset from mouse
extern float     hammerSwayTimer; // For idle breathing
extern float     hammerTilt;      // Left/right tilt when moving
extern float     fieldOfView;

extern float swingX;
extern float swingY;
extern float swingZ;
extern float swingRotX;
extern float swingRotY;
extern glm::vec3 hammerHeadWorldPos;
extern glm::vec3 hammerHeadWorldPrevPos;

extern glm::vec3 hammerBasePos;
extern glm::vec2 hammerBaseRot;

// ---- Mouse ----
extern bool  firstMouse;
extern float yaw;
extern float pitch;
extern float lastX;
extern float lastY;

void initializeGlobals();

// ---- Timing ----
extern float deltaTime;
extern float lastFrame;

// ---- Settings ----
extern float g_fogDensity;
extern float dayNightSpeed;
extern float leafParticleDensity;
extern float leafParticleSpeed;
extern bool enableSunMoon;
extern bool enableDirLight;
extern bool enableWireframe;
extern bool enableWireframe;
extern float chunkLifeMultiplier;
extern glm::vec3 waterShallowColor;
extern glm::vec3 waterDeepColor;
extern float waterSkyBlend;
extern float waterWaveSpeed;
extern float vegetationSwaySpeed;
extern float vegetationSwayIntensity;
extern int renderDistanceChunks;
extern glm::ivec2 spawnChunkPos;

#include <atomic>
extern std::atomic<int> playerCurrentChunkX;
extern std::atomic<int> playerCurrentChunkZ;

// ---- Post Processing Globals ----
extern float vAOScale;
extern float vBloom;
extern float vChromAb;
extern float vGrain;
extern float vExposure;
extern float particleLifespan;
extern bool enableVolumetricLighting;
extern bool enableVolumetricClouds;
extern float cloudDensityMult;
extern float cloudCoverage;
extern float cloudSpeedMult;
extern float volumetricIntensity;
extern bool enableSoftShadows;

// ---- Inventory ----
extern int currentWeapon; // -1 for none, 0-4 for weapons, 5 for voxel placer
extern int selectedSlot;  // 0-5
extern int inventory[9];
extern bool showCreativeInventory;
extern int currentBuildMaterial;
extern int resourceInventory[256];

// ---- Game state ----
extern bool gamePaused;
extern bool configMode;
extern bool inspectorMode;
extern bool escapeKeyWasPressed;
extern bool cKeyWasPressed;
extern bool leftMouseWasPressed;
extern bool rightMouseWasPressed;
extern bool hammerHitThisFrame;
extern glm::ivec3 hammerHitVox;
extern bool destructionPending; // True if hammer is mid-swing and impact hasn't happened yet
extern bool f5KeyWasPressed;

// ---- Building System ----
extern int currentBuildMaterial; // Selected material to place
extern glm::ivec3 ghostVox;      // Where the ghost block is
extern bool showGhostVox;        // Should the ghost block render?
extern bool isDragging;          // Are we currently dragging a box?
extern glm::ivec3 dragStartVox;  // The starting voxel of the drag volume
extern UI::BuildingSystem g_buildingSystem;

// ---- Blueprint System ----
#include <string>
extern bool blueprintMode;
extern bool hasCornerA;
extern bool hasCornerB;
extern glm::ivec3 blueprintCornerA;
extern glm::ivec3 blueprintCornerB;
extern bool showBlueprintSaveDialog;
extern std::vector<std::string> availableStructures;

// ---- Window ----
extern struct GLFWwindow* g_window;
extern float fbW, fbH;

// ---- Procedural Recoil and Camera Shake ----
extern glm::vec3 hammerRecoilPos;
extern glm::vec3 hammerRecoilRot;
extern glm::vec3 hammerRecoilVelPos;
extern glm::vec3 hammerRecoilVelRot;
extern glm::vec3 cameraShakeOffset;
extern glm::vec3 cameraShakeVel;

// ---- Weapon system ----
extern bool hasWeapon[6]; // Tracks if the player has unlocked/holds a weapon
extern int currentWeapon; // 0 = Hammer, 1 = AK-47, 2 = Dynamite, 3 = Glock, 4 = Shotgun, 5 = Voxel Placer
extern int currentSchematic; // 0 = Freeform, 1 = Wall, 2 = Pillar, 3 = Roof, 4 = Stairs
extern bool oneKeyWasPressed;
extern bool twoKeyWasPressed;
extern bool threeKeyWasPressed;
extern bool fourKeyWasPressed;
extern bool fiveKeyWasPressed;
extern bool rKeyWasPressed;
extern float shootTimer;
extern int dynamiteCount;

// ---- Weapon FX ----
extern float muzzleFlashTimer;
extern glm::vec3 muzzleFlashPos;

extern bool isInitialLoading;
