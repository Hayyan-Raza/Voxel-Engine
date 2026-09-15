#include "Globals.h"

std::vector<VoxelChunk> activeChunks;
std::vector<Particle>   activeParticles;
std::vector<ChunkMesh*> activeStaticMeshes;
std::vector<ChunkMesh*> activeWaterMeshes;
std::vector<ChunkMesh*> dirtyChunks;
std::mutex dirtyChunksMutex;

uint32_t nextChunkId = 1;
uint32_t heldChunkId = 0;
bool isLookingAtGrabbable = false;

VoxelChunk* getHeldChunk() {
    if (heldChunkId == 0) return nullptr;
    for (auto& c : activeChunks) {
        if (c.id == heldChunkId) return &c;
    }
    return nullptr;
}

glm::vec3 cameraPos   = glm::vec3(1.92f, 2.5f, 1.92f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f,  1.0f,  0.0f);

bool spectatorMode = false;
glm::vec3 spectatorPos = glm::vec3(1.92f, 2.5f, 1.92f);
glm::vec3 spectatorFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 spectatorUp = glm::vec3(0.0f, 1.0f, 0.0f);
float spectatorYaw = -90.0f;
float spectatorPitch = 0.0f;

float playerVelocityY    = 0.0f;
glm::vec3 playerVelocityH = glm::vec3(0.0f);
const float GRAVITY      = 6.0f;
const float JUMP_FORCE   = 2.2f;
bool  isGrounded         = false;
float swingTimer         = 0.0f;

glm::vec2 hammerOffset    = glm::vec2(0.0f);
float     hammerSwayTimer = 0.0f;
float     hammerTilt      = 0.0f;
float     fieldOfView     = 60.0f;

float swingX = 0.0f;
float swingY = 0.0f;
float swingZ = 0.0f;
float swingRotX = 0.0f;
float swingRotY = 0.0f;
glm::vec3 hammerHeadWorldPos = glm::vec3(0.0f);
glm::vec3 hammerHeadWorldPrevPos = glm::vec3(0.0f);

glm::vec3 hammerBasePos   = glm::vec3(0.749f, 0.199f, 1.072f);
glm::vec2 hammerBaseRot   = glm::vec2(28.659f, 12.011f);

bool  firstMouse = true;
float yaw        = -90.0f;
float pitch      =   0.0f;
float lastX      =  400.0f;
float lastY      =  300.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float g_fogDensity = 0.05f;
float dayNightSpeed = 0.01f; // Slowed down from 0.5f to prevent rapidly moving shadow patterns
Frustum viewFrustum;
float timeScale = 0.05f;

// Lighting globals
float orthoSize = 60.0f;

float leafParticleDensity = 0.5f;
float leafParticleSpeed = 1.0f;
bool enableSunMoon = true;
bool enableDirLight = false;
bool enableWireframe = false;
float chunkLifeMultiplier = 1.0f;
int renderDistanceChunks = 1;
glm::ivec2 spawnChunkPos(16, 16);
std::atomic<int> playerCurrentChunkX(16);
std::atomic<int> playerCurrentChunkZ(16);

glm::vec3 waterShallowColor = glm::vec3(0.15f, 0.70f, 0.85f);
glm::vec3 waterDeepColor = glm::vec3(0.02f, 0.20f, 0.60f);
float waterSkyBlend = 0.5f;
float waterWaveSpeed = 1.0f;
float vegetationSwaySpeed = 1.5f;
float vegetationSwayIntensity = 0.05f;

// ---- Post Processing Globals ----
float vAOScale = 1.0f;
float vBloom = 0.5f;
float vChromAb = 0.02f;
float vGrain = 0.02f;
float vExposure = 1.0f;
float particleLifespan = 2.0f;
bool enableVolumetricLighting = true;
float volumetricIntensity = 0.5f;

bool gamePaused          = false;
bool configMode          = false;
bool inspectorMode       = false;
bool escapeKeyWasPressed = false;
bool cKeyWasPressed      = false;
bool leftMouseWasPressed = false;
bool rightMouseWasPressed = false;
bool hammerHitThisFrame = false;
glm::ivec3 hammerHitVox(0,0,0);
bool destructionPending  = false;
bool f5KeyWasPressed     = false;
float fbW = 800.0f, fbH = 600.0f;

int currentBuildMaterial = 3; // Default to stone
glm::ivec3 ghostVox = glm::ivec3(-1);
bool showGhostVox = false;
bool isDragging = false;
glm::ivec3 dragStartVox = glm::ivec3(0);
UI::BuildingSystem g_buildingSystem;

bool blueprintMode = false;
bool hasCornerA = false;
bool hasCornerB = false;
glm::ivec3 blueprintCornerA = glm::ivec3(0);
glm::ivec3 blueprintCornerB = glm::ivec3(0);
bool showBlueprintSaveDialog = false;
std::vector<std::string> availableStructures;

glm::vec3 hammerRecoilPos = glm::vec3(0.0f);
glm::vec3 hammerRecoilRot = glm::vec3(0.0f);
glm::vec3 hammerRecoilVelPos = glm::vec3(0.0f);
glm::vec3 hammerRecoilVelRot = glm::vec3(0.0f);
glm::vec3 cameraShakeOffset = glm::vec3(0.0f);
glm::vec3 cameraShakeVel = glm::vec3(0.0f);

// ---- Inventory ----
int currentWeapon = 0;
int selectedSlot = 0;
int inventory[9] = {0, 1, 2, 3, 5, -1, -1, -1, -1};
bool showCreativeInventory = false;
int resourceInventory[256] = {0};
int currentSchematic = 0; // Default to Freeform
bool oneKeyWasPressed = false;
bool twoKeyWasPressed = false;
bool threeKeyWasPressed = false;
bool fourKeyWasPressed = false;
bool fiveKeyWasPressed = false;
bool rKeyWasPressed = false;
float shootTimer = 0.0f;
int dynamiteCount = 999; // Unlimited dynamites

// ---- Weapon FX ----
float muzzleFlashTimer = 0.0f;
glm::vec3 muzzleFlashPos = glm::vec3(0.0f);

bool isInitialLoading = true;

GLFWwindow* g_window = nullptr;
