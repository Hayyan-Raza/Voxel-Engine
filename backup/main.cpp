#define GLM_ENABLE_EXPERIMENTAL
#include <glad/gl.h>
#include <GLFW/glfw3.h>

// Bullet Physics
#include <btBulletDynamicsCommon.h>

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- ImGui ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm> // NEW: Added algorithm for std::clamp, etc.
#include <string>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// --- Constants & Global Data ---
#define GRID_SIZE 128
uint8_t voxelGrid[GRID_SIZE][GRID_SIZE][GRID_SIZE];
float voxelSize = 0.01f; // 1cm micro-voxels

struct Debris {
    glm::vec3 position;
    glm::vec3 velocity;
    uint8_t type;
    float life; // Time until it despawns
    bool shape[3][3][3]; // A small 3x3x3 cluster layout
};
std::vector<Debris> activeDebris;

// --- Camera Variables ---
glm::vec3 cameraPos   = glm::vec3(0.8f, 2.0f,  2.0f); // Spawn high up to drop in
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// --- Player Physics ---
float playerVelocityY = 0.0f;
float gravity = 6.0f; // Scaled down for tiny voxel world
float jumpForce = 2.2f;
bool isGrounded = false;
float swingTimer = 0.0f;

// --- Physics & Connectivity ---
struct VoxelPos { int x, y, z; };

btDefaultCollisionConfiguration* collisionConfiguration;
btCollisionDispatcher* dispatcher;
btBroadphaseInterface* overlappingPairCache;
btSequentialImpulseConstraintSolver* solver;
btDiscreteDynamicsWorld* dynamicsWorld;

void initPhysics() {
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    overlappingPairCache = new btDbvtBroadphase();
    solver = new btSequentialImpulseConstraintSolver();
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));

    // Static Ground
    btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
    btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0.25f, 0)));
    btRigidBody::btRigidBodyConstructionInfo groundCI(0.0f, groundMotionState, groundShape, btVector3(0,0,0));
    btRigidBody* groundBody = new btRigidBody(groundCI);
    dynamicsWorld->addRigidBody(groundBody);
}

void detectIslands(int startX, int startY, int startZ) {
    if (startY <= 12) return; // Don't detach ground
    if (voxelGrid[startX][startY][startZ] == 0) return;

    std::vector<VoxelPos> cluster;
    std::vector<VoxelPos> q;
    q.push_back({startX, startY, startZ});
    
    // Use a heap-allocated bitmask to avoid stack overflow (Windows stack is 1MB, this array is 2MB)
    std::vector<uint8_t> visited(GRID_SIZE * GRID_SIZE * GRID_SIZE, 0);
    visited[startX * GRID_SIZE * GRID_SIZE + startY * GRID_SIZE + startZ] = 1;

    bool touchesGround = false;
    size_t head = 0;
    while(head < q.size()) {
        VoxelPos p = q[head++];
        cluster.push_back(p);
        if (p.y <= 12) touchesGround = true;

        // Check 6 neighbors
        VoxelPos neighbors[6] = {
            {p.x+1, p.y, p.z}, {p.x-1, p.y, p.z},
            {p.x, p.y+1, p.z}, {p.x, p.y-1, p.z},
            {p.x, p.y, p.z+1}, {p.x, p.y, p.z-1}
        };

        for(auto& n : neighbors) {
            if (n.x >= 0 && n.x < GRID_SIZE && n.y >= 0 && n.y < GRID_SIZE && n.z >= 0 && n.z < GRID_SIZE) {
                int idx = n.x * GRID_SIZE * GRID_SIZE + n.y * GRID_SIZE + n.z;
                if (voxelGrid[n.x][n.y][n.z] > 0 && !visited[idx]) {
                    visited[idx] = 1;
                    q.push_back(n);
                }
            }
        }
    }

    if (!touchesGround && cluster.size() > 0) {
        // DETACHED! Convert to btRigidBody
        // (Simplified: for now, just turn into debris to avoid complex mesh collision hulls)
        for(auto& v : cluster) {
            Debris d;
            d.position = glm::vec3(v.x * voxelSize, v.y * voxelSize, v.z * voxelSize);
            d.type = voxelGrid[v.x][v.y][v.z];
            d.velocity = glm::vec3(0, -1, 0);
            d.life = 5.0f;
            activeDebris.push_back(d);
            voxelGrid[v.x][v.y][v.z] = 0;
        }
    }
}

// --- Greedy Meshing ---
struct VoxelVertex {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
};

struct VoxelMesh {
    GLuint VAO, VBO;
    int vertexCount;
};
VoxelMesh staticMesh;

void updateStaticMesh() {
    std::vector<VoxelVertex> v;
    
    // Arrays for face iterations: normal, and 4 vertices relative to voxel origin (0,0,0) to (1,1,1)
    struct FaceDef {
        int dx, dy, dz; // Check neighbor
        glm::vec3 n;    // Normal
        glm::vec3 v[6]; // Triangle vertices (2 quads)
    };
    
    FaceDef faces[6] = {
        // +X Right
        {1, 0, 0, {1,0,0}, { {1,0,1}, {1,0,0}, {1,1,0}, {1,0,1}, {1,1,0}, {1,1,1} }},
        // -X Left
        {-1, 0, 0, {-1,0,0}, { {0,0,0}, {0,0,1}, {0,1,1}, {0,0,0}, {0,1,1}, {0,1,0} }},
        // +Y Top
        {0, 1, 0, {0,1,0}, { {0,1,1}, {1,1,1}, {1,1,0}, {0,1,1}, {1,1,0}, {0,1,0} }},
        // -Y Bottom
        {0, -1, 0, {0,-1,0}, { {0,0,0}, {1,0,0}, {1,0,1}, {0,0,0}, {1,0,1}, {0,0,1} }},
        // +Z Front
        {0, 0, 1, {0,0,1}, { {0,0,1}, {1,0,1}, {1,1,1}, {0,0,1}, {1,1,1}, {0,1,1} }},
        // -Z Back
        {0, 0, -1, {0,0,-1}, { {1,0,0}, {0,0,0}, {0,1,0}, {1,0,0}, {0,1,0}, {1,1,0} }}
    };

    // Fast 6-face culling iteration
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int z = 0; z < GRID_SIZE; z++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                uint8_t type = voxelGrid[x][y][z];
                if (type == 0) continue;

                float r=0.5f, g=0.5f, b=0.5f;
                if (type == 1)      { r = 0.72f; g = 0.75f; b = 0.35f; } // Grass
                else if (type == 2) { r = 0.40f; g = 0.30f; b = 0.15f; } // Dirt
                else if (type == 3) { r = 0.55f; g = 0.55f; b = 0.55f; } // Stone
                else if (type == 4) { r = 0.35f; g = 0.25f; b = 0.15f; } // Wood
                else if (type == 5) { r = 0.75f; g = 0.85f; b = 0.45f; } // Leaves
                
                float px = x * voxelSize;
                float py = y * voxelSize;
                float pz = z * voxelSize;

                for (int f = 0; f < 6; f++) {
                    int nx = x + faces[f].dx;
                    int ny = y + faces[f].dy;
                    int nz = z + faces[f].dz;
                    
                    bool drawFace = false;
                    if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE || nz < 0 || nz >= GRID_SIZE) {
                        drawFace = true; // World boundary
                    } else if (voxelGrid[nx][ny][nz] == 0) {
                        drawFace = true; // Empty space adjacent
                    }

                    if (drawFace) {
                        for (int i = 0; i < 6; i++) {
                            v.push_back({
                                px + faces[f].v[i].x * voxelSize,
                                py + faces[f].v[i].y * voxelSize,
                                pz + faces[f].v[i].z * voxelSize,
                                r, g, b,
                                faces[f].n.x, faces[f].n.y, faces[f].n.z
                            });
                        }
                    }
                }
            }
        }
    }

    if (staticMesh.VAO == 0) glGenVertexArrays(1, &staticMesh.VAO);
    if (staticMesh.VBO == 0) glGenBuffers(1, &staticMesh.VBO);
    glBindVertexArray(staticMesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, staticMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(VoxelVertex), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    staticMesh.vertexCount = v.size();
}

// Game State
bool gamePaused = false;
bool escapeKeyWasPressed = false;
bool leftMouseWasPressed = false;

    // NPC System fully removed.

// --- Vehicle System Removed ---

// Mouse state
bool firstMouse = true;
float yaw   = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right
float pitch =  0.0f;
float lastX =  800.0f / 2.0;
float lastY =  600.0f / 2.0;

// Timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// --- Shaders ---
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
    "   gl_Position = projection * view * vec4(FragPos, 1.0);\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 Normal;\n"
    "in vec3 FragPos;\n"
    "uniform vec4 voxelColor;\n"
    "uniform vec3 lightDir;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec2 screenRes;\n"
    "uniform float shadowObscurance;\n"
    "uniform float neighborAO;\n"
    "uniform float fogDensity;\n"
    "\n"
    "vec3 ACESFilm(vec3 x) {\n"
    "    float a = 2.51; float b = 0.03; float c = 2.43; float d = 0.59; float e = 0.14;\n"
    "    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);\n"
    "}\n"
    "\n"
    "float hash(vec3 p) {\n"
    "    p = fract(p * 0.3183099 + 0.1);\n"
    "    p *= 17.0;\n"
    "    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec3 norm = normalize(Normal);\n"
    "   vec3 viewDir = normalize(viewPos - FragPos);\n"
    "   vec3 lightDirN = normalize(-lightDir);\n"
    "   \n"
    "   // 1. Procedural Voxel Detail (Grit) - Toned down\n"
    "   float noise = hash(round(FragPos * 128.0)) * 0.02;\n"
    "   vec3 baseColor = (voxelColor.rgb * voxelColor.rgb) - noise;\n" // Use vertex array color directly!
    "   \n"
    "   // 2. High-Fidelity Teardown Atmospherics - Natural Sunlight\n"
    "   vec3 sunColor = vec3(1.0, 0.9, 0.8) * 2.0; // Very soft yellow-white sun\n"
    "   vec3 skyColor = mix(vec3(0.8, 0.9, 1.0), vec3(0.2, 0.4, 0.8), clamp(norm.y * 0.5 + 0.5, 0.0, 1.0));\n"
    "   vec3 horizonColor = vec3(0.7, 0.8, 0.9);\n"
    "   \n"
    "   // Hemi Lighting (Sky + Ground bounce)\n"
    "   float hemi = 0.5 * (norm.y + 1.0);\n"
    "   vec3 ambient = mix(vec3(0.05, 0.04, 0.03), vec3(0.2, 0.3, 0.6), hemi) * 0.4;\n"
    "   \n"
    "   // 3. Directional Light + Specular Refraction\n"
    "   float diff = max(dot(norm, lightDirN), 0.0);\n"
    "   vec3 diffuse = diff * sunColor * (1.1 - shadowObscurance);\n"
    "   \n"
    "   // Sharp Specular (Wet/Plastic Voxel look) - Lowered intensity\n"
    "   vec3 halfDir = normalize(lightDirN + viewDir);\n"
    "   float specBase = max(dot(norm, halfDir), 0.0);\n"
    "   float spec = pow(specBase, 64.0);\n"
    "   vec3 specular = spec * sunColor * (1.1 - shadowObscurance) * 0.2;\n"
    "   \n"
    "   // 4. Subtle Volumetric Light Scatter (Bloom/Sun Glow) - Toned down\n"
    "   float sunGlow = pow(max(dot(viewDir, -lightDirN), 0.0), 12.0) * 0.5;\n"
    "   vec3 scattering = sunColor * sunGlow * 0.02;\n"
    "   \n"
    "   // 5. Total Lighting with Ambient Occlusion\n"
    "   float hAO = clamp(FragPos.y * 4.0 + 0.1, 0.3, 1.0);\n"
    "   float ao = neighborAO * hAO;\n"
    "   \n"
    "   vec3 result = (ambient + diffuse + specular + scattering) * baseColor * ao;\n"
    "   \n"
    "   // 6. Distance Fog (Softens the horizon)\n"
    "   float dist = length(viewPos - FragPos);\n"
    "   float fogFactor = 1.0 - exp(-dist * fogDensity); \n"
    "   result = mix(result, horizonColor * 0.8, clamp(fogFactor, 0.0, 1.0));\n"
    "   \n"
    "   // 7. Cinema Post-Processing (Tonemapping & Gamma)\n"
    "   result = ACESFilm(result * 1.0); // Standard exposure without blowout\n"
    "   result = pow(result, vec3(1.0/2.2));\n"
    "   \n"
    "   // Resolution-Independent Vignette\n"
    "   vec2 uv = gl_FragCoord.xy / screenRes;\n"
    "   float v = uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);\n"
    "   float vignette = clamp(pow(v * 16.0, 0.05), 0.0, 1.0);\n"
    "   \n"
    "   FragColor = vec4(result * vignette, voxelColor.a);\n"
    "}\n\0";

// --- Mouse callback function ---
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (gamePaused) return; // Don't move camera while paused

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; // change this value to your liking
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// --- Callback to resize viewport when window is resized ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// --- Function to process keyboard input ---
void processInput(GLFWwindow *window) {
    // 1. Enter/Exit Vehicle Logic (E Key) removed

    // Escape key handling for Pausing
    bool escapeKeyPressed = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escapeKeyPressed && !escapeKeyWasPressed) {
        gamePaused = !gamePaused;
        if (!gamePaused) {
            // Update Physics
            dynamicsWorld->stepSimulation(deltaTime, 10);
            
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide cursor for Game
            firstMouse = true; // reset mouse catch to prevent jumping
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Show cursor for UI
        }
    }
    escapeKeyWasPressed = escapeKeyPressed;

    if (gamePaused) return; // Stop movement if paused

    if (!gamePaused) {
        float cameraSpeed = static_cast<float>(1.0 * deltaTime); // Slow player walking speed down to match scale
        // Flatten cameraFront so W always moves across ground instead of flying into sky
        glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += cameraSpeed * flatFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= cameraSpeed * flatFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= glm::normalize(glm::cross(flatFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += glm::normalize(glm::cross(flatFront, cameraUp)) * cameraSpeed;

        // Jump
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
            playerVelocityY = jumpForce;
            isGrounded = false;
        }
    }
}

int main() {
    // 1. Initialize GLFW
    std::cout << "Engine Startup Initializing..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    std::cout << "GLFW Initialized." << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Create Window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Teardown Clone Engine", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Window created successfully." << std::endl;
    glfwMakeContextCurrent(window);

    // 3. Load OpenGL functions using GLAD
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << "GLAD Initialized with version: " << version << std::endl;
    
    // --- Generate Flat Minecraft-Style World ---
    // A Macro-Block is 8x8x8 micro-voxels.
    // This gives us a 16x16x16 Macro-Block world within the 128x128x128 grid.
    int macroSize = 8;
    for (int mx = 0; mx < GRID_SIZE / macroSize; mx++) {
        for (int mz = 0; mz < GRID_SIZE / macroSize; mz++) {
            
            // Perfectly flat 2-macro-block high floor
            int terrainHeightMacro = 2; 

            for (int my = 0; my < terrainHeightMacro; my++) {
                
                // Determine Macro-Block type
                int macroType = 2; // Default Dirt
                if (my == terrainHeightMacro - 1) macroType = 1; // Top layer is Grass block

                // Fill the 8x8x8 micro-voxels for this macro-block
                for (int lx = 0; lx < macroSize; lx++) {
                    for (int lz = 0; lz < macroSize; lz++) {
                        for (int ly = 0; ly < macroSize; ly++) {
                            
                            int vx = (mx * macroSize) + lx;
                            int vy = (my * macroSize) + ly;
                            int vz = (mz * macroSize) + lz;

                            // Texture layering within the macro-block
                            uint8_t microType = macroType;
                            
                            if (macroType == 1) { // It's a Grass Block
                                // Base grass thickness is 2-3 micro-voxels
                                int grassDepth = 2;
                                
                                // Create overhang/bleed effect on macro-block borders
                                if (lx == 0 || lx == macroSize - 1 || lz == 0 || lz == macroSize - 1) {
                                    // Generate a pseudo-random stagger using modulus math
                                    grassDepth += (lx * 3 + lz * 7) % 3; 
                                }

                                if (ly >= macroSize - grassDepth) {
                                    microType = 1; // Grass
                                } else {
                                    microType = 2; // Dirt
                                }
                            }

                            voxelGrid[vx][vy][vz] = microType;
                        }
                    }
                }
            }
        }
    }

    // --- Spawn High-Detail Teardown Tree ---
    int tx = GRID_SIZE / 2;
    int tz = GRID_SIZE / 2;
    int ty = 10; 
    
    int trunkHeight = 12;
    for (int y = 0; y < trunkHeight; y++) {
        if (y < 4) { // 3x3 base
            for(int ox=-1; ox<=1; ox++) for(int oz=-1; oz<=1; oz++)
                voxelGrid[tx+ox][ty+y][tz+oz] = 4;
        } else if (y < 9) { // 2x2 mid
            for(int ox=0; ox<=1; ox++) for(int oz=0; oz<=1; oz++)
                voxelGrid[tx+ox][ty+y][tz+oz] = 4;
        } else { // 1x1 top
            voxelGrid[tx][ty+y][tz] = 4;
        }
    }

    auto spawnBranch = [&](int sx, int sy, int sz, glm::vec3 dir, int length) {
        for (int i = 0; i < length; i++) {
            int bx = sx + static_cast<int>(dir.x * i);
            int by = sy + static_cast<int>(dir.y * i);
            int bz = sz + static_cast<int>(dir.z * i);
            if (bx >=0 && bx < GRID_SIZE && by >= 0 && by < GRID_SIZE && bz >= 0 && bz < GRID_SIZE) {
                voxelGrid[bx][by][bz] = 4;
                if (i == length - 1) {
                    for(int lx=-2; lx<=2; lx++) for(int ly=-2; ly<=2; ly++) for(int lz=-2; lz<=2; lz++) {
                        if (sqrt(lx*lx + ly*ly + lz*lz) < 2.3f) {
                            int fx = bx + lx; int fy = by + ly; int fz = bz + lz;
                            if (fx >= 0 && fx < GRID_SIZE && fy >= 0 && fy < GRID_SIZE && fz >= 0 && fz < GRID_SIZE)
                                if (voxelGrid[fx][fy][fz] == 0) voxelGrid[fx][fy][fz] = 5;
                        }
                    }
                }
            }
        }
    };

    spawnBranch(tx, ty + 6, tz, glm::vec3(1, 0.5f, 1), 4);
    spawnBranch(tx, ty + 7, tz, glm::vec3(-1, 0.4f, 1), 5);
    spawnBranch(tx, ty + 8, tz, glm::vec3(0, 0.6f, -1), 4);
    spawnBranch(tx, ty + 10, tz, glm::vec3(1, 0.3f, -1), 3);

    // Initial Mesh Generation
    updateStaticMesh();

    // --- Bullet Physics Init ---
    initPhysics();

    // Set callback functions
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    // Capture the mouse so it stays in the window and hides the cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Enable depth testing so 3D objects hide things behind them
    glEnable(GL_DEPTH_TEST);
    std::cout << "Depth testing enabled." << std::endl;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark(); // Teardown style dark UI
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    std::cout << "ImGui Initialized." << std::endl;

    // --- Create and compile Shaders ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    std::cout << "Shaders compiled and linked." << std::endl;

    // --- Define a single 3D Cube (Voxel) with Normals ---
    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    std::cout << "Static VBO/VAO setup complete. Entering Main Loop..." << std::endl;

    // 4. Main Game/Render Loop
    while (!glfwWindowShouldClose(window)) {
        // PER-FRAME TIME LOGIC
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // HANDLE INPUTS
        processInput(window);

        // --- PLAYER PHYSICS ---
        if (!gamePaused) {
            if (swingTimer > 0.0f) swingTimer -= deltaTime;
            
            playerVelocityY -= gravity * deltaTime;
            cameraPos.y += playerVelocityY * deltaTime;
            
            float playerHeight = 0.25f; // Player is now 25 centimeters tall (10 voxels) instead of 1.3 meters!
            
            // Tight Floor Collision checking exact block top boundary
            int gridX = static_cast<int>(round(cameraPos.x / voxelSize));
            int gridY = static_cast<int>(floor((cameraPos.y - playerHeight + (voxelSize * 0.5f)) / voxelSize));
            int gridZ = static_cast<int>(round(cameraPos.z / voxelSize));
            
            isGrounded = false;
            if (gridX >= 0 && gridX < GRID_SIZE && gridZ >= 0 && gridZ < GRID_SIZE) {
                if (gridY >= 0 && gridY < GRID_SIZE) {
                    if (voxelGrid[gridX][gridY][gridZ] > 0) {
                        isGrounded = true;
                        // Clamp camera exactly onto the top face of the hit voxel
                        cameraPos.y = (gridY * voxelSize) + (voxelSize * 0.5f) + playerHeight;
                        playerVelocityY = 0.0f;
                    }
                } else if (gridY < 0) { // Safety floor outside voxel bounds
                    isGrounded = true;
                    cameraPos.y = playerHeight;
                    playerVelocityY = 0.0f;
                }
            } else if (cameraPos.y < playerHeight) { // Free falling safety floor outside grid
                cameraPos.y = playerHeight;
                isGrounded = true;
                playerVelocityY = 0.0f;
            }

            // --- DEBRIS PHYSICS ---
            for (size_t i = 0; i < activeDebris.size(); i++) {
                activeDebris[i].life -= deltaTime;
                if (activeDebris[i].life <= 0.0f) {
                    activeDebris.erase(activeDebris.begin() + i);
                    i--; // adjust index since we erased
                    continue;
                }
                
                // Gravity on debris
                activeDebris[i].velocity.y -= gravity * deltaTime;
                activeDebris[i].position += activeDebris[i].velocity * deltaTime;
                
                // Ground Collision
                int dx = static_cast<int>(round(activeDebris[i].position.x / voxelSize));
                int dy = static_cast<int>(round(activeDebris[i].position.y / voxelSize));
                int dz = static_cast<int>(round(activeDebris[i].position.z / voxelSize));
                
                bool onGround = false;
                float groundY = 0.0f;

                if (dy < 0) {
                    onGround = true;
                    groundY = 0.0f;
                } else if (dx >= 0 && dx < GRID_SIZE && dz >= 0 && dz < GRID_SIZE && dy < GRID_SIZE) {
                    if (voxelGrid[dx][dy][dz] > 0) {
                        onGround = true;
                        groundY = (dy * voxelSize) + voxelSize;
                    }
                }

                if (onGround) {
                    activeDebris[i].position.y = groundY;
                    
                    // Bounce and Friction
                    if (abs(activeDebris[i].velocity.y) > 0.1f) {
                        activeDebris[i].velocity.y *= -0.3f; // Bounce
                    } else {
                        activeDebris[i].velocity.y = 0; // Stop vertical
                    }
                    
                    activeDebris[i].velocity.x *= 0.7f; // Friction
                    activeDebris[i].velocity.z *= 0.7f;

                    // If almost stationary, just stop all movement
                    if (length(activeDebris[i].velocity) < 0.01f) {
                        activeDebris[i].velocity = glm::vec3(0);
                    }
                }
            }
            }
        }

        // RAYCASTING (Destruction)
        bool leftMousePressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if (leftMousePressed && !leftMouseWasPressed && !gamePaused) {
            if (swingTimer <= 0.0f) swingTimer = 0.15f; 
            
            glm::vec3 rayOrigin = cameraPos;
            glm::vec3 rayDir = cameraFront;
            float maxDistance = 2.5f;
            float step = 0.01f;
            
            for (float d = 0.0f; d < maxDistance; d += step) {
                glm::vec3 p = rayOrigin + rayDir * d;
                
                int gridX = static_cast<int>(round(p.x / voxelSize));
                int gridY = static_cast<int>(round(p.y / voxelSize));
                int gridZ = static_cast<int>(round(p.z / voxelSize));
                
                if (gridX >= 0 && gridX < GRID_SIZE && gridY >= 0 && gridY < GRID_SIZE && gridZ >= 0 && gridZ < GRID_SIZE) {
                    if (voxelGrid[gridX][gridY][gridZ] > 0) {
                        int hitType = voxelGrid[gridX][gridY][gridZ];
                        int radius = 2;
                        
                        // Break voxel grid
                        for (int x = -radius; x <= radius; x++) {
                            for (int y = -radius; y <= radius; y++) {
                                for (int z = -radius; z <= radius; z++) {
                                    if (x*x + y*y + z*z <= radius*radius + 1) {
                                        int tx = gridX + x; int ty = gridY + y; int tz = gridZ + z;
                                        if (tx >= 0 && tx < GRID_SIZE && ty >= 0 && ty < GRID_SIZE && tz >= 0 && tz < GRID_SIZE) {
                                            voxelGrid[tx][ty][tz] = 0;
                                        }
                                    }
                                }
                            }
                        }

                        // Spawn clustered chunk debris (instead of 1-pixel debris)
                        for (int i = 0; i < 4; i++) { // Spawn a few large clusters
                            Debris deb;
                            deb.position = glm::vec3(gridX * voxelSize, gridY * voxelSize, gridZ * voxelSize) + glm::vec3(((rand()%100)/100.0f)-0.5f, ((rand()%100)/100.0f), ((rand()%100)/100.0f)-0.5f) * (voxelSize * 4.0f);
                            deb.velocity = rayDir * 4.0f + glm::vec3(((rand()%100)/100.0f)-0.5f, ((rand()%100)/100.0f)+1.0f, ((rand()%100)/100.0f)-0.5f) * 2.0f;
                            deb.type = hitType; 
                            deb.life = 1.0f; // Last slightly longer
                            
                            // Randomize cluster shape (3x3x3 max layout)
                            for(int cx=0; cx<3; cx++) for(int cy=0; cy<3; cy++) for(int cz=0; cz<3; cz++) {
                                deb.shape[cx][cy][cz] = (rand() % 100 > 30); // 70% chance to be solid
                            }
                            // Ensure center is solid so it's not totally empty
                            deb.shape[1][1][1] = true;
                            
                            activeDebris.push_back(deb);
                        }

                        updateStaticMesh();
                        detectIslands(gridX, gridY, gridZ);
                        goto end_ray;
                    }
                }
            }
            end_ray:;
        }
        leftMouseWasPressed = leftMousePressed;

        // RENDER GRAPHICS
        // Clear background to accurate Teardown Sky Color (Pale Blue)
        glClearColor(0.61f, 0.70f, 0.76f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Game Rendering
        if (!gamePaused) {
            // Draw Teardown-style Crosshair (Simple Dot)
            {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                ImGui::SetNextWindowPos(ImVec2(width/2.0f - 5, height/2.0f - 5));
                ImGui::Begin("Crosshair", NULL, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 center = ImVec2(width/2.0f, height/2.0f);
                drawList->AddCircleFilled(center, 2.0f, IM_COL32(255, 255, 255, 200)); // Inner dot
                drawList->AddCircle(center, 2.5f, IM_COL32(0, 0, 0, 150)); // Outline
                ImGui::End();
            }

            // Activate shader
            glUseProgram(shaderProgram);

            // CREATE MATRICES FOR CAMERA/3D PERSPECTIVE
            // Get precise aspect ratio to avoid squashed/stretched rendering when resizing window
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
            
            // Projection Matrix: 60 degrees FOV, 0.01 near plane so tiny 2.5cm voxels don't clip out!
            glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
            
            // View Matrix: Essentially the Camera
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            
            // Pass matrices to our Shader
            unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
            unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

            unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
            unsigned int colorLoc = glGetUniformLocation(shaderProgram, "voxelColor");
            unsigned int lightLoc = glGetUniformLocation(shaderProgram, "lightDir");
            unsigned int viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
            unsigned int shadowLoc = glGetUniformLocation(shaderProgram, "shadowObscurance");
            unsigned int aoLoc = glGetUniformLocation(shaderProgram, "neighborAO");

            // Classic high-contrast Teardown sun angle
            glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
            glUniform3f(lightLoc, sunDirection.x, sunDirection.y, sunDirection.z);
            glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));

            // Pass Resolution and Fog settings
            GLuint resLoc = glGetUniformLocation(shaderProgram, "screenRes");
            glUniform2f(resLoc, (float)width, (float)height);
            GLuint fogDenLoc = glGetUniformLocation(shaderProgram, "fogDensity");
            glUniform1f(fogDenLoc, 0.2f); // Moderate world fog

            // 1. Draw Static Optimized Mesh (Greedy)
            if (staticMesh.vertexCount > 0) {
                glBindVertexArray(staticMesh.VAO);
                glm::mat4 model = glm::mat4(1.0f);
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1f(shadowLoc, 0.0f); // Shadows pre-baked or simplified
                glUniform1f(aoLoc, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, staticMesh.vertexCount);
            }


            // --- DRAW CLUSTER DEBRIS ---
            for (size_t i = 0; i < activeDebris.size(); i++) {
                
                float r = 0.5f, g = 0.5f, b = 0.5f;
                uint8_t vType = activeDebris[i].type;
                if (vType == 1)      { r = 0.72f; g = 0.75f; b = 0.35f; } // Grass
                else if (vType == 2) { r = 0.40f; g = 0.30f; b = 0.15f; } // Dirt
                else if (vType == 3) { r = 0.55f; g = 0.55f; b = 0.55f; } // Stone
                else if (vType == 4) { r = 0.35f; g = 0.25f; b = 0.15f; } // Wood
                else if (vType == 5) { r = 0.75f; g = 0.85f; b = 0.45f; } // Leaves
                else if (vType == 6) { r = 0.95f; g = 0.65f; b = 0.85f; } // Flower
                else if (vType == 7) { r = 0.75f; g = 0.88f; b = 0.35f; } // Grass Tuft

                glUniform4f(colorLoc, r, g, b, 1.0f);
                glUniform1f(shadowLoc, 0.0f); // Fast simple rendering with no dynamic casting shadows

                // Draw the cluster array
                for(int cx=0; cx<3; cx++) {
                    for(int cy=0; cy<3; cy++) {
                        for(int cz=0; cz<3; cz++) {
                            if (activeDebris[i].shape[cx][cy][cz]) {
                                glm::mat4 model = glm::mat4(1.0f);
                                // Center the cluster drawing relative to the point
                                glm::vec3 offset = glm::vec3(cx - 1.0f, cy - 1.0f, cz - 1.0f) * voxelSize;
                                model = glm::translate(model, activeDebris[i].position + offset);
                                model = glm::scale(model, glm::vec3(voxelSize)); 
                                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                                glDrawArrays(GL_TRIANGLES, 0, 36);
                            }
                        }
                    }
                }
            }

            // Vehicle Draw Code Removed

            {
                glClear(GL_DEPTH_BUFFER_BIT); 
                
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
                // Weapon FOV
                glm::mat4 uiProj = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 10.0f);
                glm::mat4 uiView = glm::lookAt(glm::vec3(0, 0, 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
                glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
                glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(uiView));
                
                
                // Disable fog for UI
                GLuint fogDenLoc = glGetUniformLocation(shaderProgram, "fogDensity");
                glUniform1f(fogDenLoc, 0.0f); 
                GLuint resLoc = glGetUniformLocation(shaderProgram, "screenRes");
                glUniform2f(resLoc, (float)width, (float)height);
                
                float hVScale = 0.07f; // Even bigger for that FPV look
                float swing = 0.0f;
                if (swingTimer > 0.0f) {
                    swing = sin((swingTimer / 0.15f) * 3.14159f); 
                }

                // Hammer "Shoulder" point
                glm::vec3 hBase = glm::vec3(0.6f, -0.6f, 0.8f); 

                // 1. Draw Massive Wooden Handle
                for(int y=0; y<14; y++) {
                    for(int hx=0; hx<2; hx++) {
                        for(int hz=0; hz<2; hz++) {
                            glm::mat4 model = glm::mat4(1.0f);
                            model = glm::translate(model, hBase);
                            // Initial posture: Angled forward and center
                            model = glm::rotate(model, glm::radians(-35.0f - (swing * 60.0f)), glm::vec3(1, 0, 0)); 
                            model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(0, 1, 0)); 
                            
                            model = glm::translate(model, glm::vec3(hx * hVScale * 0.5f, y * hVScale * 0.8f, hz * hVScale * 0.5f));
                            model = glm::scale(model, glm::vec3(hVScale * 0.6f, hVScale * 0.8f, hVScale * 0.6f));
                            
                            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                            glUniform4f(colorLoc, 0.28f, 0.15f, 0.05f, 1.0f); 
                            glUniform1f(shadowLoc, 0.2f);
                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }
                }
                
                // 2. Draw Giant Sledge Head
                for(int x=-3; x<=3; x++) {
                    for(int y=-4; y<=4; y++) {
                        for(int z=-6; z<=6; z++) {
                            glm::mat4 model = glm::mat4(1.0f);
                            model = glm::translate(model, hBase);
                            model = glm::rotate(model, glm::radians(-35.0f - (swing * 60.0f)), glm::vec3(1, 0, 0));
                            model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(0, 1, 0));
                            
                            model = glm::translate(model, glm::vec3(x * hVScale * 0.4f, (14 * hVScale * 0.8f) + (y * hVScale * 0.4f), (z * hVScale * 0.4f)));
                            model = glm::scale(model, glm::vec3(hVScale * 0.5f));
                            
                            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                            // FIX: Use glm::vec3 instead of GLSL vec3
                            float hNoise = ((abs(x)+abs(y)+abs(z)) % 2 == 0) ? 0.05f : 0.0f;
                            glUniform4f(colorLoc, 0.2f+hNoise, 0.22f+hNoise, 0.25f+hNoise, 1.0f);
                            
                            glUniform1f(shadowLoc, 0.0f); 
                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }
                }
            }
        } else {
            // Draw Pause Menu UI
            ImGui::SetNextWindowPos(ImVec2(800/2.0f - 100, 600/2.0f - 75), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(200, 150));
            ImGui::Begin("Game Paused", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Resume Game", ImVec2(180, 40))) {
                gamePaused = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            ImGui::Spacing();
            if (ImGui::Button("Quit to Desktop", ImVec2(180, 40))) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::End();
        }

        // --- Main UI Overlay (FPS Counter) ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. Clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    return 0;
}
