// =============================================================================
// TearDown Clone Engine - main.cpp
// Refactored: Clean modular architecture, no dead code.
// =============================================================================

#define GLM_ENABLE_EXPERIMENTAL
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <btBulletDynamicsCommon.h>

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Voxel world data, terrain generation, mesh building
#include "World.h"

// =============================================================================
//  STRUCTS
// =============================================================================

// --- Teardown-style voxel chunk debris (maintains shape, tumbles) ---
struct VoxelChunk {
    glm::vec3 center;                             // world-space center
    glm::vec3 velocity;
    glm::vec3 angularVelocity;                    // rad/s on each axis
    glm::quat rotation    = glm::quat(1,0,0,0);  // current orientation
    float     life        = 4.0f;
    std::vector<std::pair<glm::ivec3,uint8_t>> voxels; // local-grid offset + type
};

// --- Dust / smoke particle ---
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float     life, maxLife;
    float     size;        // world-space radius
    float     alpha;
    glm::vec3 color;
};

// =============================================================================
//  GLOBAL STATE
// =============================================================================

// --- Active world objects ---
std::vector<VoxelChunk> activeChunks;
std::vector<Particle>   activeParticles;

// --- Camera ---
glm::vec3 cameraPos   = glm::vec3(0.8f, 2.0f,  2.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// --- Player physics ---
float playerVelocityY = 0.0f;
const float GRAVITY    = 6.0f;
const float JUMP_FORCE = 2.2f;
bool  isGrounded  = false;
float swingTimer  = 0.0f;

// --- Mouse ---
bool  firstMouse = true;
float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  400.0f;
float lastY =  300.0f;

// --- Timing ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Game state ---
bool gamePaused        = false;
bool escapeKeyWasPressed = false;
bool leftMouseWasPressed = false;

// =============================================================================
//  BULLET PHYSICS
// =============================================================================

struct VoxelPos { int x, y, z; };

btDefaultCollisionConfiguration*     collisionConfiguration = nullptr;
btCollisionDispatcher*                dispatcher             = nullptr;
btBroadphaseInterface*                overlappingPairCache   = nullptr;
btSequentialImpulseConstraintSolver*  solver                 = nullptr;
btDiscreteDynamicsWorld*              dynamicsWorld          = nullptr;

static void initPhysics() {
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher             = new btCollisionDispatcher(collisionConfiguration);
    overlappingPairCache   = new btDbvtBroadphase();
    solver                 = new btSequentialImpulseConstraintSolver();
    dynamicsWorld = new btDiscreteDynamicsWorld(
        dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));

    btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
    btDefaultMotionState* groundState = new btDefaultMotionState(
        btTransform(btQuaternion(0,0,0,1), btVector3(0, 0.25f, 0)));
    btRigidBody::btRigidBodyConstructionInfo ci(0.0f, groundState, groundShape, btVector3(0,0,0));
    dynamicsWorld->addRigidBody(new btRigidBody(ci));
}

// =============================================================================
//  ISLAND DETECTION (Detached voxel clusters become debris)
// =============================================================================



static void detectIslands(int startX, int startY, int startZ) {
    if (startY <= 12) return;
    int bfsY = startY + 1;
    if (bfsY >= GRID_SIZE || voxelGrid[startX][bfsY][startZ] == 0) return;

    std::vector<VoxelPos> cluster;
    std::vector<VoxelPos> q;
    q.push_back({startX, bfsY, startZ});

    std::vector<uint8_t> visited(GRID_SIZE * GRID_SIZE * GRID_SIZE, 0);
    visited[startX * GRID_SIZE * GRID_SIZE + bfsY * GRID_SIZE + startZ] = 1;

    bool touchesGround = false;
    size_t head = 0;
    const int MAX_CLUSTER = 512;

    while (head < q.size() && (int)cluster.size() < MAX_CLUSTER) {
        VoxelPos p = q[head++];
        cluster.push_back(p);
        if (p.y <= 12) { touchesGround = true; break; }

        const VoxelPos nb[6] = {
            {p.x+1,p.y,p.z},{p.x-1,p.y,p.z},
            {p.x,p.y+1,p.z},{p.x,p.y-1,p.z},
            {p.x,p.y,p.z+1},{p.x,p.y,p.z-1}
        };
        for (const auto& n : nb) {
            if (n.x<0||n.x>=GRID_SIZE||n.y<0||n.y>=GRID_SIZE||n.z<0||n.z>=GRID_SIZE) continue;
            int idx = n.x*GRID_SIZE*GRID_SIZE + n.y*GRID_SIZE + n.z;
            if (voxelGrid[n.x][n.y][n.z] > 0 && !visited[idx]) {
                visited[idx] = 1;
                q.push_back(n);
            }
        }
    }

    if (touchesGround || cluster.empty()) return;

    glm::vec3 com(0.0f);
    for (const auto& v : cluster)
        com += glm::vec3(v.x, v.y, v.z);
    com /= (float)cluster.size();

    VoxelChunk chunk;
    chunk.center   = com * voxelSize;
    chunk.velocity = glm::vec3(
        ((rand()%100)/100.0f - 0.5f) * 0.5f,
        ((rand()%100)/100.0f) * 1.5f + 0.5f,
        ((rand()%100)/100.0f - 0.5f) * 0.5f);
    chunk.angularVelocity = glm::vec3(
        ((rand()%100)/100.0f - 0.5f) * 8.0f,
        ((rand()%100)/100.0f - 0.5f) * 8.0f,
        ((rand()%100)/100.0f - 0.5f) * 8.0f);
    chunk.life = 5.0f;

    for (const auto& v : cluster) {
        glm::ivec3 off(v.x-(int)com.x, v.y-(int)com.y, v.z-(int)com.z);
        chunk.voxels.push_back({off, voxelGrid[v.x][v.y][v.z]});
        voxelGrid[v.x][v.y][v.z] = 0;
    }

    if (!chunk.voxels.empty())
        activeChunks.push_back(std::move(chunk));
}

// =============================================================================
//  SHADERS
// =============================================================================

static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
out vec3 VertColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    FragPos   = vec3(model * vec4(aPos, 1.0));
    Normal    = mat3(transpose(inverse(model))) * aNormal;
    VertColor = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

static const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 FragPos;
in vec3 VertColor;
uniform vec4 voxelColor;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec2 screenRes;
uniform float shadowObscurance;
uniform float neighborAO;
uniform float fogDensity;

vec3 ACESFilm(vec3 x) {
    float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
void main() {
    vec3 norm     = normalize(Normal);
    vec3 viewDir  = normalize(viewPos - FragPos);
    vec3 lDir     = normalize(-lightDir);

    // Per-vertex color blended with uniform tint (world uses white tint, debris/hammer use color tint)
    float noise    = hash(round(FragPos * 128.0)) * 0.02;
    vec3 baseColor = max((VertColor * voxelColor.rgb) - noise, vec3(0.0));

    // Hemisphere lighting
    float hemi    = 0.5 * (norm.y + 1.0);
    vec3 ambient  = mix(vec3(0.05, 0.04, 0.03), vec3(0.2, 0.3, 0.6), hemi) * 0.4;

    // Sunlight
    vec3 sunColor = vec3(1.0, 0.9, 0.8) * 2.0;
    float diff    = max(dot(norm, lDir), 0.0);
    vec3 diffuse  = diff * sunColor * (1.1 - shadowObscurance);

    // Specular
    vec3  halfD  = normalize(lDir + viewDir);
    float spec   = pow(max(dot(norm, halfD), 0.0), 64.0);
    vec3 specular = spec * sunColor * (1.1 - shadowObscurance) * 0.2;

    // Sun glow scatter
    float sunGlow   = pow(max(dot(viewDir, -lDir), 0.0), 12.0) * 0.5;
    vec3 scattering = sunColor * sunGlow * 0.02;

    // AO
    float hAO = clamp(FragPos.y * 4.0 + 0.1, 0.3, 1.0);
    float ao  = neighborAO * hAO;

    vec3 result = (ambient + diffuse + specular + scattering) * baseColor * ao;

    // Distance fog
    float dist      = length(viewPos - FragPos);
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    vec3 horizonColor = vec3(0.7, 0.8, 0.9);
    result = mix(result, horizonColor * 0.8, clamp(fogFactor, 0.0, 1.0));

    // Tonemapping + gamma
    result = ACESFilm(result);
    result = pow(result, vec3(1.0/2.2));

    // Vignette
    vec2 uv = gl_FragCoord.xy / screenRes;
    float v = uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    float vignette = clamp(pow(v * 16.0, 0.05), 0.0, 1.0);

    FragColor = vec4(result * vignette, voxelColor.a);
}
)";

// --- Dedicated particle shaders (GL_POINTS, no lighting) ---
static const char* particleVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 projection;
uniform float uPointSize;
void main() {
    vec4 viewPos = view * vec4(aPos, 1.0);
    gl_Position  = projection * viewPos;
    // Perspective: bigger when close, smaller when far
    gl_PointSize = max(uPointSize / max(-viewPos.z, 0.001), 2.0);
}
)";

static const char* particleFragSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uParticleColor;
void main() {
    vec2  coord = gl_PointCoord - vec2(0.5);
    float dist  = length(coord);
    if (dist > 0.5) discard;
    // Soft Gaussian: bright centre, fully transparent at edge
    float alpha = exp(-dist * dist * 8.0) * uParticleColor.a;
    FragColor = vec4(uParticleColor.rgb, alpha);
}
)";


static void framebuffer_size_callback(GLFWwindow* /*window*/, int w, int h) {
    glViewport(0, 0, w, h);
}

static void mouse_callback(GLFWwindow* /*window*/, double xposIn, double yposIn) {
    if (gamePaused) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoffset = (xpos - lastX) * 0.1f;
    float yoffset = (lastY - ypos) * 0.1f;
    lastX = xpos;
    lastY = ypos;

    yaw   += xoffset;
    pitch = std::clamp(pitch + yoffset, -89.0f, 89.0f);

    glm::vec3 front;
    front.x    = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y    = sin(glm::radians(pitch));
    front.z    = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// =============================================================================
//  INPUT
// =============================================================================

static void processInput(GLFWwindow* window) {
    // --- Pause toggle (rising edge only) ---
    bool escNow = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escNow && !escapeKeyWasPressed) {
        gamePaused = !gamePaused;
        if (gamePaused) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    escapeKeyWasPressed = escNow;

    if (gamePaused) return;

    // --- Movement ---
    float speed = 1.0f * deltaTime;
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right     = glm::normalize(glm::cross(flatFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= speed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += speed * right;

    // --- Jump ---
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
        playerVelocityY = JUMP_FORCE;
        isGrounded      = false;
    }
}

// =============================================================================
//  PHYSICS UPDATE  (called once per frame when unpaused)
// =============================================================================

static void updatePlayerPhysics() {
    if (swingTimer > 0.0f) swingTimer -= deltaTime;

    // Gravity
    playerVelocityY -= GRAVITY * deltaTime;
    cameraPos.y     += playerVelocityY * deltaTime;

    const float playerHeight = 0.25f;

    // Floor collision
    int gridX = static_cast<int>(round(cameraPos.x / voxelSize));
    int gridY = static_cast<int>(floor((cameraPos.y - playerHeight + voxelSize * 0.5f) / voxelSize));
    int gridZ = static_cast<int>(round(cameraPos.z / voxelSize));

    isGrounded = false;
    if (gridX >= 0 && gridX < GRID_SIZE && gridZ >= 0 && gridZ < GRID_SIZE) {
        if (gridY >= 0 && gridY < GRID_SIZE) {
            if (voxelGrid[gridX][gridY][gridZ] > 0) {
                isGrounded      = true;
                cameraPos.y     = gridY * voxelSize + voxelSize * 0.5f + playerHeight;
                playerVelocityY = 0.0f;
            }
        } else if (gridY < 0) {
            isGrounded      = true;
            cameraPos.y     = playerHeight;
            playerVelocityY = 0.0f;
        }
    } else if (cameraPos.y < playerHeight) {
        cameraPos.y     = playerHeight;
        isGrounded      = true;
        playerVelocityY = 0.0f;
    }
}

// --- Chunk physics: gravity + rotation + ground bounce ---
static void updateChunkPhysics() {
    for (size_t i = 0; i < activeChunks.size(); ) {
        VoxelChunk& c = activeChunks[i];
        c.life -= deltaTime;
        if (c.life <= 0.0f) { activeChunks.erase(activeChunks.begin()+i); continue; }

        // Gravity
        c.velocity.y -= GRAVITY * deltaTime;
        c.center     += c.velocity * deltaTime;

        // Angular momentum (slow drag over time)
        glm::vec3 avel = c.angularVelocity * deltaTime;
        float angle = glm::length(avel);
        if (angle > 1e-6f) {
            glm::quat dq = glm::angleAxis(angle, glm::normalize(avel));
            c.rotation   = glm::normalize(dq * c.rotation);
        }
        c.angularVelocity *= 0.995f; // very gentle drag

        // Simple floor collision (y=0 plane)
        if (c.center.y < 0.0f) {
            c.center.y = 0.0f;
            c.velocity.y = std::abs(c.velocity.y) * 0.25f; // soft bounce
            c.velocity.x *= 0.7f;
            c.velocity.z *= 0.7f;
            c.angularVelocity *= 0.5f;
        }
        ++i;
    }
}


// =============================================================================
//  PARTICLE SYSTEM
// =============================================================================

static void spawnDust(glm::vec3 pos, int count) {
    // Hard cap — no more than 60 particles total
    if (activeParticles.size() >= 60) return;
    count = std::min(count, (int)(60 - activeParticles.size()));

    // Grey smoke tones
    const glm::vec3 dustColors[4] = {
        {0.45f, 0.45f, 0.45f},  // mid grey
        {0.35f, 0.35f, 0.35f},  // dark grey
        {0.60f, 0.58f, 0.55f},  // warm light grey
        {0.30f, 0.28f, 0.28f},  // near black (burnt)
    };
    for (int k = 0; k < count; k++) {
        Particle p;
        p.position = pos + glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * voxelSize * 4.0f,
            ((rand()%100)/100.0f)        * voxelSize * 2.0f,
            ((rand()%200)/100.0f - 1.0f) * voxelSize * 4.0f);
        float spd  = ((rand()%100)/100.0f) * 0.3f + 0.08f;
        p.velocity = glm::vec3(
            ((rand()%200)/100.0f - 1.0f) * spd,
            ((rand()%100)/100.0f) * spd * 2.0f + 0.06f,  // mostly upward
            ((rand()%200)/100.0f - 1.0f) * spd);
        p.maxLife  = ((rand()%100)/100.0f) * 0.9f + 0.5f; // 0.5-1.4s
        p.life     = p.maxLife;
        p.size     = voxelSize * (((rand()%100)/100.0f) * 2.5f + 1.0f);
        p.alpha    = 0.55f;
        p.color    = dustColors[rand() % 4];
        activeParticles.push_back(p);
    }
}

static void updateParticles() {
    for (size_t i = 0; i < activeParticles.size(); ) {
        Particle& p = activeParticles[i];
        p.life -= deltaTime;
        if (p.life <= 0.0f) { activeParticles.erase(activeParticles.begin()+i); continue; }

        float t   = 1.0f - (p.life / p.maxLife); // 0=fresh, 1=dead
        p.velocity.y -= 0.3f * deltaTime;          // gentle settling
        p.velocity    *= 0.97f;                    // air drag
        p.position   += p.velocity * deltaTime;
        p.alpha       = std::max(0.0f, 1.0f - t * t); // quadratic fade-out
        p.size       *= (1.0f + deltaTime * 1.5f);     // expand like smoke
        ++i;
    }
}

// =============================================================================
//  RAYCASTING / DESTRUCTION
// =============================================================================

static void handleDestruction(GLFWwindow* window) {
    bool leftNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    if (leftNow && !leftMouseWasPressed) {
        if (swingTimer <= 0.0f) swingTimer = 0.2f;

        // --- Raycast to first solid voxel ---
        const float maxDist = 3.0f;
        int gx=-1, gy=-1, gz=-1;
        for (float d = 0.0f; d < maxDist; d += 0.01f) {
            glm::vec3 p = cameraPos + cameraFront * d;
            int cx = static_cast<int>(round(p.x / voxelSize));
            int cy = static_cast<int>(round(p.y / voxelSize));
            int cz = static_cast<int>(round(p.z / voxelSize));
            if (cx<0||cx>=GRID_SIZE||cy<0||cy>=GRID_SIZE||cz<0||cz>=GRID_SIZE) continue;
            if (voxelGrid[cx][cy][cz] > 0) { gx=cx; gy=cy; gz=cz; break; }
        }
        if (gx < 0) { leftMouseWasPressed = leftNow; return; }

        glm::vec3 hitWorld(gx * voxelSize, gy * voxelSize, gz * voxelSize);

        // --- Phase 1: erase inner crater (fully destroyed zone) ---
        const int innerR = 16; // 16 micro-voxels = 2 macro blocks
        const int outerR = 28; // 28 micro-voxels = rim scatter zone

        for (int ox=-innerR; ox<=innerR; ox++)
        for (int oy=-innerR; oy<=innerR; oy++)
        for (int oz=-innerR; oz<=innerR; oz++) {
            if (ox*ox + oy*oy + oz*oz > innerR*innerR) continue;
            int tx=gx+ox, ty=gy+oy, tz=gz+oz;
            if (tx>=0&&tx<GRID_SIZE&&ty>=0&&ty<GRID_SIZE&&tz>=0&&tz<GRID_SIZE)
                voxelGrid[tx][ty][tz] = 0;
        }

        // --- Phase 2: eject macro blocks in rim zone as tumbling chunks ---
        const int macroR = outerR / MACRO_SIZE + 2;
        int mGx = gx / MACRO_SIZE;
        int mGy = gy / MACRO_SIZE;
        int mGz = gz / MACRO_SIZE;

        for (int mmx = mGx-macroR; mmx <= mGx+macroR; mmx++)
        for (int mmy = mGy-macroR; mmy <= mGy+macroR; mmy++)
        for (int mmz = mGz-macroR; mmz <= mGz+macroR; mmz++) {
            if (mmx<0||mmx>=GRID_SIZE/MACRO_SIZE) continue;
            if (mmy<0||mmy>=GRID_SIZE/MACRO_SIZE) continue;
            if (mmz<0||mmz>=GRID_SIZE/MACRO_SIZE) continue;

            int bcx = mmx*MACRO_SIZE + MACRO_SIZE/2;
            int bcy = mmy*MACRO_SIZE + MACRO_SIZE/2;
            int bcz = mmz*MACRO_SIZE + MACRO_SIZE/2;

            float dx=(float)(bcx-gx), dy=(float)(bcy-gy), dz=(float)(bcz-gz);
            float dist2 = dx*dx + dy*dy + dz*dz;

            if (dist2 <= (float)(innerR*innerR)) continue; // already erased
            if (dist2 > (float)(outerR*outerR))  continue; // too far

            // Collect surviving voxels in this macro block
            VoxelChunk chunk;
            chunk.center = glm::vec3(bcx, bcy, bcz) * voxelSize;

            for (int vx=mmx*MACRO_SIZE; vx<(mmx+1)*MACRO_SIZE; vx++)
            for (int vy=mmy*MACRO_SIZE; vy<(mmy+1)*MACRO_SIZE; vy++)
            for (int vz=mmz*MACRO_SIZE; vz<(mmz+1)*MACRO_SIZE; vz++) {
                if (vx<0||vx>=GRID_SIZE||vy<0||vy>=GRID_SIZE||vz<0||vz>=GRID_SIZE) continue;
                if (voxelGrid[vx][vy][vz] == 0) continue;
                chunk.voxels.push_back({{vx-bcx, vy-bcy, vz-bcz}, voxelGrid[vx][vy][vz]});
                voxelGrid[vx][vy][vz] = 0;
            }
            if (chunk.voxels.empty()) continue;

            // Outward + upward ejection velocity
            glm::vec3 dir = chunk.center - hitWorld;
            if (glm::length(dir) < 0.001f) dir = glm::vec3(0,1,0);
            dir = glm::normalize(dir);
            float spd = 0.8f + ((rand()%100)/100.0f) * 1.4f;
            chunk.velocity = dir * spd + glm::vec3(0.0f, spd * 0.7f, 0.0f);
            chunk.angularVelocity = glm::vec3(
                ((rand()%200)/100.0f - 1.0f) * 8.0f,
                ((rand()%200)/100.0f - 1.0f) * 8.0f,
                ((rand()%200)/100.0f - 1.0f) * 8.0f);
            chunk.life = 5.0f;
            activeChunks.push_back(std::move(chunk));
        }

        // --- Grey smoke cloud rising from crater ---
        spawnDust(hitWorld, 20);

        updateStaticMesh();
        detectIslands(gx, gy, gz);
    }
    leftMouseWasPressed = leftNow;
}

// =============================================================================
//  RENDERING HELPERS
// =============================================================================

// --- Voxel type -> RGB helper ---
static glm::vec3 getVoxelColor(uint8_t type) {
    switch (type) {
        case 1: return {0.72f, 0.75f, 0.35f}; // Grass
        case 2: return {0.40f, 0.30f, 0.15f}; // Dirt
        case 3: return {0.55f, 0.55f, 0.55f}; // Stone
        case 4: return {0.35f, 0.25f, 0.15f}; // Wood
        case 5: return {0.75f, 0.85f, 0.45f}; // Leaves
        case 6: return {0.95f, 0.65f, 0.85f}; // Flower
        case 7: return {0.75f, 0.88f, 0.35f}; // Grass tuft
        default: return {0.5f, 0.5f, 0.5f};
    }
}

// --- Draw tumbling voxel chunks ---
static void drawChunks(GLint modelLoc, GLint colorLoc, GLint shadowLoc) {
    for (const auto& chunk : activeChunks) {
        glm::mat4 rotMat = glm::mat4_cast(chunk.rotation);
        glUniform1f(shadowLoc, 0.0f);

        for (const auto& [off, type] : chunk.voxels) {
            glm::vec3 c = getVoxelColor(type);
            glUniform4f(colorLoc, c.r, c.g, c.b, 1.0f);

            glm::vec3 localPos = glm::vec3(off) * voxelSize;
            glm::vec3 worldPos = chunk.center + glm::vec3(rotMat * glm::vec4(localPos, 0.0f));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos);
            model = model * rotMat;
            model = glm::scale(model, glm::vec3(voxelSize));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
}

static void drawHammer(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLint projLoc, GLint viewLoc) {
    // Switch to weapon-layer projection (clears depth first)
    glClear(GL_DEPTH_BUFFER_BIT);
    int w, h;
    // These will be refreshed each call
    GLFWwindow* cur = glfwGetCurrentContext();
    glfwGetFramebufferSize(cur, &w, &h);
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    glm::mat4 uiProj = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 10.0f);
    glm::mat4 uiView = glm::lookAt(glm::vec3(0,0,2.0f), glm::vec3(0,0,0), glm::vec3(0,1,0));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));

    const float vs    = 0.07f;
    float swing = (swingTimer > 0.0f) ? sin((swingTimer / 0.15f) * 3.14159f) : 0.0f;
    glm::vec3 hBase   = glm::vec3(0.6f, -0.6f, 0.8f);
    float swingAngle  = -35.0f - swing * 60.0f;

    // Handle (14 voxels tall, 2x2 cross section)
    for (int y=0;y<14;y++) for (int hx=0;hx<2;hx++) for (int hz=0;hz<2;hz++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, hBase);
        model = glm::rotate(model, glm::radians(swingAngle), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(-30.0f),     glm::vec3(0,1,0));
        model = glm::translate(model, glm::vec3(hx*vs*0.5f, y*vs*0.8f, hz*vs*0.5f));
        model = glm::scale(model, glm::vec3(vs*0.6f, vs*0.8f, vs*0.6f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4f(colorLoc, 0.28f, 0.15f, 0.05f, 1.0f);
        glUniform1f(shadowLoc, 0.2f);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Head (7x9x13 block)
    for (int x=-3;x<=3;x++) for (int y=-4;y<=4;y++) for (int z=-6;z<=6;z++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, hBase);
        model = glm::rotate(model, glm::radians(swingAngle), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(-30.0f),     glm::vec3(0,1,0));
        model = glm::translate(model, glm::vec3(x*vs*0.4f, (14*vs*0.8f) + y*vs*0.4f, z*vs*0.4f));
        model = glm::scale(model, glm::vec3(vs*0.5f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        float hNoise = ((std::abs(x)+std::abs(y)+std::abs(z)) % 2 == 0) ? 0.05f : 0.0f;
        glUniform4f(colorLoc, 0.2f+hNoise, 0.22f+hNoise, 0.25f+hNoise, 1.0f);
        glUniform1f(shadowLoc, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

// =============================================================================
//  SHADER COMPILATION
// =============================================================================

static GLuint compileShader(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    int ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(id, 512, nullptr, buf);
        std::cerr << "Shader error: " << buf << "\n";
    }
    return id;
}

static GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    int ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetProgramInfoLog(prog, 512, nullptr, buf);
        std::cerr << "Link error: " << buf << "\n";
    }
    return prog;
}

// =============================================================================
//  UNIT CUBE (for debris & hammer voxels)
// =============================================================================

// clang-format off
// Layout: pos(3), color(3 white), normal(3) = 9 floats — matches World.cpp VoxelVertex layout
static const float unitCubeVertices[] = {
    // pos               color(white)    normal
    -0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,
    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 0,-1,
    -0.5f,-0.5f,-0.5f,  1,1,1,  0, 0,-1,

    -0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
    -0.5f, 0.5f, 0.5f,  1,1,1,  0, 0, 1,
    -0.5f,-0.5f, 0.5f,  1,1,1,  0, 0, 1,

    -0.5f, 0.5f, 0.5f,  1,1,1, -1, 0, 0,
    -0.5f, 0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1, -1, 0, 0,
    -0.5f,-0.5f, 0.5f,  1,1,1, -1, 0, 0,
    -0.5f, 0.5f, 0.5f,  1,1,1, -1, 0, 0,

     0.5f, 0.5f, 0.5f,  1,1,1,  1, 0, 0,
     0.5f, 0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  1, 0, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  1, 0, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  1, 0, 0,

    -0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
     0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
    -0.5f,-0.5f, 0.5f,  1,1,1,  0,-1, 0,
    -0.5f,-0.5f,-0.5f,  1,1,1,  0,-1, 0,

    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0,
     0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0,
    -0.5f, 0.5f, 0.5f,  1,1,1,  0, 1, 0,
    -0.5f, 0.5f,-0.5f,  1,1,1,  0, 1, 0
};
// clang-format on

// =============================================================================
//  MAIN
// =============================================================================

int main() {
    std::cout << "Engine Startup Initializing..." << std::endl;

    // --- GLFW ---
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return -1; }
    std::cout << "GLFW Initialized.\n";

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Teardown Clone Engine", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    std::cout << "Window created successfully.\n";
    glfwMakeContextCurrent(window);

    // --- GLAD ---
    int glVersion = gladLoadGL(glfwGetProcAddress);
    if (glVersion == 0) { std::cerr << "Failed to init GLAD\n"; return -1; }
    std::cout << "GLAD Initialized with version: " << glVersion << "\n";

    // --- World ---
    generateTerrain();
    updateStaticMesh();

    // --- Bullet ---
    initPhysics();

    // --- Callbacks ---
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // --- OpenGL state ---
    glEnable(GL_DEPTH_TEST);
    std::cout << "Depth testing enabled.\n";

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    std::cout << "ImGui Initialized.\n";

    // --- Shaders ---
    GLuint vert   = compileShader(GL_VERTEX_SHADER,   vertexShaderSource);
    GLuint frag   = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shader = linkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    std::cout << "Shaders compiled and linked.\n";

    // --- Unit cube VAO (debris + hammer) ---
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unitCubeVertices), unitCubeVertices, GL_STATIC_DRAW);
    // pos=0, color=1, normal=2  (9 floats/vertex, matching World.cpp VoxelVertex layout)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    std::cout << "Static VBO/VAO setup complete. Entering Main Loop...\n";

    // Cache uniform locations
    glUseProgram(shader);
    GLint uModel   = glGetUniformLocation(shader, "model");
    GLint uView    = glGetUniformLocation(shader, "view");
    GLint uProj    = glGetUniformLocation(shader, "projection");
    GLint uColor   = glGetUniformLocation(shader, "voxelColor");
    GLint uLight   = glGetUniformLocation(shader, "lightDir");
    GLint uViewPos = glGetUniformLocation(shader, "viewPos");
    GLint uRes     = glGetUniformLocation(shader, "screenRes");
    GLint uFog     = glGetUniformLocation(shader, "fogDensity");
    GLint uShadow  = glGetUniformLocation(shader, "shadowObscurance");
    GLint uAO      = glGetUniformLocation(shader, "neighborAO");

    // --- Particle shader ---
    GLuint pVert    = compileShader(GL_VERTEX_SHADER,   particleVertSrc);
    GLuint pFrag    = compileShader(GL_FRAGMENT_SHADER, particleFragSrc);
    GLuint pShader  = linkProgram(pVert, pFrag);
    glDeleteShader(pVert); glDeleteShader(pFrag);
    GLint pUView    = glGetUniformLocation(pShader, "view");
    GLint pUProj    = glGetUniformLocation(pShader, "projection");
    GLint pUSize    = glGetUniformLocation(pShader, "uPointSize");
    GLint pUColor   = glGetUniformLocation(pShader, "uParticleColor");

    // Dynamic VBO for particle positions (updated each frame)
    GLuint partVAO, partVBO;
    glGenVertexArrays(1, &partVAO);
    glGenBuffers(1, &partVBO);
    glBindVertexArray(partVAO);
    glBindBuffer(GL_ARRAY_BUFFER, partVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE); // allow vertex shader to set gl_PointSize

    // ==========================================================================
    //  MAIN LOOP
    // ==========================================================================
    while (!glfwWindowShouldClose(window)) {
        // --- Per-frame timing ---
        float now   = static_cast<float>(glfwGetTime());
        deltaTime   = now - lastFrame;
        lastFrame   = now;

        // --- Input ---
        processInput(window);

        // --- Physics (only when unpaused) ---
        if (!gamePaused) {
            dynamicsWorld->stepSimulation(deltaTime, 10);
            updatePlayerPhysics();
            updateChunkPhysics();
            updateParticles();
        }

        // --- Destruction ---
        if (!gamePaused) handleDestruction(window);

        // --- Clear ---
        glClearColor(0.61f, 0.70f, 0.76f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- ImGui frame start ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!gamePaused) {
            // --- Camera matrices ---
            int fbW, fbH;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            float aspect = (fbH > 0) ? (float)fbW / (float)fbH : 1.0f;

            glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
            glm::mat4 view       = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            glUseProgram(shader);
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));

            glm::vec3 sunDir = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
            glUniform3f(uLight,   sunDir.x, sunDir.y, sunDir.z);
            glUniform3fv(uViewPos, 1, glm::value_ptr(cameraPos));
            glUniform2f(uRes,  (float)fbW, (float)fbH);
            glUniform1f(uFog,  0.2f);
            glUniform1f(uAO,   1.0f);

            // --- Draw world mesh (per-vertex colors baked in, tint=white) ---
            if (staticMesh.vertexCount > 0) {
                glBindVertexArray(staticMesh.VAO);
                glm::mat4 identity = glm::mat4(1.0f);
                glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(identity));
                glUniform4f(uColor, 1.0f, 1.0f, 1.0f, 1.0f); // white tint: shows pure vertex colors
                glUniform1f(uShadow, 0.0f);
                glUniform1f(uAO, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, staticMesh.vertexCount);
            }

            // --- Draw tumbling chunks ---
            glBindVertexArray(VAO);
            drawChunks(uModel, uColor, uShadow);

            // --- Draw hammer (weapon layer) ---
            drawHammer(uModel, uColor, uShadow, uProj, uView);

            // Restore world projection
            glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
            glUniform1f(uFog, 0.0f);

            // --- Draw dust particles (GL_POINTS, soft Gaussian circles) ---
            if (!activeParticles.empty()) {
                // Upload current particle positions to GPU
                std::vector<float> posBuf;
                posBuf.reserve(activeParticles.size() * 3);
                for (const auto& p : activeParticles) {
                    posBuf.push_back(p.position.x);
                    posBuf.push_back(p.position.y);
                    posBuf.push_back(p.position.z);
                }
                glBindBuffer(GL_ARRAY_BUFFER, partVBO);
                glBufferData(GL_ARRAY_BUFFER,
                    posBuf.size() * sizeof(float), posBuf.data(), GL_DYNAMIC_DRAW);

                glUseProgram(pShader);
                glUniformMatrix4fv(pUView, 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(pUProj, 1, GL_FALSE, glm::value_ptr(projection));
                // World-space diameter * screen pixel density
                glUniform1f(pUSize, 180.0f); // tuned for perspective falloff

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);

                glBindVertexArray(partVAO);
                // Draw each particle with its own color+alpha
                for (size_t pi = 0; pi < activeParticles.size(); pi++) {
                    const auto& par = activeParticles[pi];
                    glUniform4f(pUColor, par.color.r, par.color.g, par.color.b, par.alpha);
                    glDrawArrays(GL_POINTS, (GLint)pi, 1);
                }

                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glUseProgram(shader); // restore main shader
            }

            // --- Crosshair ---
            {
                ImGui::SetNextWindowPos(ImVec2(fbW/2.0f - 5, fbH/2.0f - 5));
                ImGui::Begin("Crosshair", nullptr,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 center  = ImVec2(fbW/2.0f, fbH/2.0f);
                dl->AddCircleFilled(center, 2.0f, IM_COL32(255,255,255,200));
                dl->AddCircle(center, 2.5f, IM_COL32(0,0,0,150));
                ImGui::End();
            }

        } else {
            // --- Pause Menu ---
            ImGui::SetNextWindowPos(ImVec2(350, 225), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(200, 150));
            ImGui::Begin("Game Paused", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Resume Game", ImVec2(180, 40))) {
                gamePaused = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            ImGui::Spacing();
            if (ImGui::Button("Quit to Desktop", ImVec2(180, 40)))
                glfwSetWindowShouldClose(window, true);
            ImGui::End();
        }

        // --- FPS counter ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::Begin("##fps", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.0f,1.0f,0.0f,1.0f), "FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        // --- ImGui render ---
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ==========================================================================
    //  CLEANUP
    // ==========================================================================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shader);
    glfwTerminate();

    return 0;
}
