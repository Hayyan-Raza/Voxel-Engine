#include <glad/gl.h>
#include <GLFW/glfw3.h>
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

// Game State
bool gamePaused = false;
bool escapeKeyWasPressed = false;
bool leftMouseWasPressed = false;

// Voxel Data
#define GRID_SIZE 64
uint8_t voxelGrid[GRID_SIZE][GRID_SIZE][GRID_SIZE]; 
float voxelSize = 0.025f; // Reverted to 2.5cm voxels

// --- Debris System ---
struct Debris {
    glm::vec3 position;
    glm::vec3 velocity;
    uint8_t type;
    float life; // Time until it despawns
};
std::vector<Debris> activeDebris;

// --- Vehicle System ---
struct Vehicle {
    glm::vec3 position;
    float rotation; // yaw in degrees
    glm::vec3 velocity;
    bool isDriving;
    uint8_t design[16][6][8]; // High resolution for Sports Car
};
Vehicle playerCar;

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
    "uniform float roughness;\n"   // PBR: 0.0=Shiny, 1.0=Matte
    "uniform float metalness;\n"   // PBR: 1.0=Metallic
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
    "   // 1. Procedural Voxel Detail (Grit)\n"
    "   float noise = hash(round(FragPos * 80.0)) * 0.04;\n"
    "   vec3 baseColor = voxelColor.rgb - noise;\n"
    "   \n"
    "   // 2. High-Fidelity Teardown Atmospherics\n"
    "   vec3 sunColor = vec3(1.0, 0.95, 0.85) * 3.5; // Natural Bright Sun\n"
    "   vec3 skyColor = mix(vec3(0.8, 0.9, 1.0), vec3(0.2, 0.4, 0.8), clamp(norm.y * 0.5 + 0.5, 0.0, 1.0));\n"
    "   vec3 horizonColor = vec3(0.6, 0.7, 0.85);\n"
    "   \n"
    "   // Hemi Lighting (Sky + Ground bounce)\n"
    "   float hemi = 0.5 * (norm.y + 1.0);\n"
    "   vec3 ambient = mix(vec3(0.08, 0.07, 0.06), vec3(0.2, 0.4, 0.7), hemi) * 0.4;\n"
    "   \n"
    "   // 3. Directional Light + PBR Specular\n"
    "   float diff = max(dot(norm, lightDirN), 0.0);\n"
    "   vec3 diffuse = diff * sunColor * (1.1 - shadowObscurance);\n"
    "   \n"
    "   // Sharp Specular vs Rough Specular\n"
    "   vec3 halfDir = normalize(lightDirN + viewDir);\n"
    "   float specBase = max(dot(norm, halfDir), 0.0);\n"
    "   float specPower = 128.0 * (1.1 - roughness);\n"
    "   float spec = pow(specBase, specPower);\n"
    "   \n"
    "   // Metalness: Reflect base color\n"
    "   vec3 specCol = mix(vec3(1.0), baseColor, metalness);\n"
    "   vec3 specular = spec * sunColor * specCol * (1.1 - shadowObscurance) * (1.0 - roughness);\n"
    "   \n"
    "   // 4. Subtle Volumetric Light Scatter (Bloom/Sun Glow)\n"
    "   float sunGlow = pow(max(dot(viewDir, -lightDirN), 0.0), 12.0) * 0.3;\n"
    "   vec3 scattering = sunColor * sunGlow * 0.15;\n"
    "   \n"
    "   // 5. Total Lighting with Ambient Occlusion\n"
    "   float hAO = clamp(FragPos.y * 4.0 + 0.1, 0.3, 1.0);\n"
    "   float ao = neighborAO * hAO;\n"
    "   \n"
    "   vec3 result = (ambient + diffuse + specular + scattering) * baseColor * ao;\n"
    "   \n"
    "   // 6. CINEMATIC: Depth of Field (Miniature Look)\n"
    "   float dist = length(viewPos - FragPos);\n"
    "   float focusRange = 2.0;\n"
    "   float dof = clamp(abs(dist - focusRange) * 0.1, 0.0, 1.0);\n"
    "   \n"
    "   // 7. Distance Fog (Soft Blue)\n"
    "   float fogFactor = 1.0 - exp(-dist * fogDensity); \n"
    "   result = mix(result, horizonColor, clamp(max(fogFactor, dof * 0.4), 0.0, 1.0));\n"
    "   \n"
    "   // 8. Cinema Post-Processing\n"
    "   result = ACESFilm(result * 1.5);\n"
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
    // 1. Enter/Exit Vehicle Logic (E Key)
    static bool eKeyWasPressed = false;
    bool eKeyPressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
    if (eKeyPressed && !eKeyWasPressed && !gamePaused) {
        float dist = glm::distance(cameraPos, playerCar.position);
        if (!playerCar.isDriving && dist < 1.0f) {
            playerCar.isDriving = true;
        } else if (playerCar.isDriving) {
            playerCar.isDriving = false;
            cameraPos.y += 0.2f; 
        }
    }
    eKeyWasPressed = eKeyPressed;

    // Escape key handling for Pausing
    bool escapeKeyPressed = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escapeKeyPressed && !escapeKeyWasPressed) {
        gamePaused = !gamePaused;
        if (gamePaused) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Show cursor for UI
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide cursor for Game
            firstMouse = true; // reset mouse catch to prevent jumping
        }
    }
    escapeKeyWasPressed = escapeKeyPressed;

    if (gamePaused) return; // Stop movement if paused

    if (playerCar.isDriving) {
        float accel = 5.0f * deltaTime;
        float turnSpeed = 120.0f * deltaTime;
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            playerCar.velocity += glm::vec3(cos(glm::radians(playerCar.rotation)), 0, -sin(glm::radians(playerCar.rotation))) * accel;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            playerCar.velocity -= glm::vec3(cos(glm::radians(playerCar.rotation)), 0, -sin(glm::radians(playerCar.rotation))) * accel;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            playerCar.rotation += turnSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            playerCar.rotation -= turnSpeed;
            
        // Move car and dampen velocity (friction)
        playerCar.position += playerCar.velocity * deltaTime;
        playerCar.velocity *= 0.98f;
        
        // Lock player camera TO the car
        cameraPos = playerCar.position + glm::vec3(0, 0.5f, 0); 

        // Stable Car Gravity/Grounding
        float checkHeight = 0.05f;
        int carX = static_cast<int>(round(playerCar.position.x / voxelSize));
        int carY = static_cast<int>(floor((playerCar.position.y - checkHeight) / voxelSize));
        int carZ = static_cast<int>(round(playerCar.position.z / voxelSize));
        
        bool carGrounded = false;
        if (carX >= 0 && carX < GRID_SIZE && carZ >= 0 && carZ < GRID_SIZE && carY >= 0 && carY < GRID_SIZE) {
            if (voxelGrid[carX][carY][carZ] > 0) carGrounded = true;
        } else if (playerCar.position.y <= checkHeight) carGrounded = true;

        if (!carGrounded) {
            playerCar.velocity.y -= gravity * deltaTime;
        } else {
            // Soft Landing: Instead of hard-snapping, we kill velocity and lift just enough
            if (playerCar.velocity.y < 0) playerCar.velocity.y = 0;
            float targetY = (carY < 0) ? checkHeight : (carY * voxelSize) + voxelSize + checkHeight;
            playerCar.position.y = targetY; 
        }
    } else {
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
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

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
    glfwMakeContextCurrent(window);

    // Initialize Voxel Grid (Thick Teardown-style structures)
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                // Foundation (Thick dirt/stone)
                if (y < 4) voxelGrid[x][y][z] = 3; // stone
                else if (y >= 4 && y < 7) voxelGrid[x][y][z] = 2; // dirt
                else if (y == 7 || y == 8) voxelGrid[x][y][z] = 1; // grass
                else voxelGrid[x][y][z] = 0;       // empty air
                
                // Add a THICK brick-like wall to destroy (1 meter / 10 voxels thick)
                if (x >= 40 && x <= 50 && y >= 8 && y <= 35 && z >= 10 && z <= 54) {
                    voxelGrid[x][y][z] = 3; // stone wall
                }

                // Add some metal structural beams
                if (x == 45 && y >= 8 && y <= 35 && (z == 15 || z == 45)) {
                    for(int bx=-1; bx<=1; bx++) for(int bz=-1; bz<=1; bz++) {
                        voxelGrid[x+bx][y][z+bz] = 4; // Metal pillar
                    }
                }
            }
        }
    }

    // Initialize the Sleek Teardown Sports Car
    playerCar.position = glm::vec3(1.5f, 0.45f, 1.5f); 
    playerCar.rotation = 90.0f;
    playerCar.velocity = glm::vec3(0);
    playerCar.isDriving = false;
    for(int x=0; x<16; x++) for(int y=0; y<6; y++) for(int z=0; z<8; z++) {
        playerCar.design[x][y][z] = 0;
        
        // 1. CHASSIS AND WHEELS (Black)
        if (y == 0) {
            if ((x >= 2 && x <= 4 && (z == 0 || z == 7)) || (x >= 11 && x <= 13 && (z == 0 || z == 7))) {
                playerCar.design[x][y][z] = 3; // Wheels
            } else if (z > 0 && z < 7) {
                playerCar.design[x][y][z] = 4; // White door sills / chassis
                if (x < 14 && x > 2) playerCar.design[x][y][z] = 5; // White accent
            }
        }
        // 2. MAIN BODY (Red)
        else if (y >= 1 && y <= 2) {
            if (x >= 1 && x <= 14 && z >= 1 && z <= 6) {
                playerCar.design[x][y][z] = 1; // Basic Red Body
                
                // Black Hood / Grille
                if (x < 6 && (z == 2 || z == 3 || z == 4 || z == 5)) {
                    playerCar.design[x][y][z] = 2; 
                }
            }
        }
        // 3. ROOF AND CABIN (Red/Black)
        else if (y >= 3 && y <= 4) {
            if (x >= 6 && x <= 11 && z >= 1 && z <= 6) {
                if (x == 6 || x == 11) playerCar.design[x][y][z] = 1; // Pillars
                else playerCar.design[x][y][z] = 2; // Black Roof
            }
        }
        // 4. MASSIVE SPOILER (Black)
        else if (y == 5) {
             if (x >= 13 && x <= 14 && (z == 0 || z == 7)) playerCar.design[x][y][z] = 2; // Spoiler supports
             if (x == 14 && z >= 0 && z <= 7) playerCar.design[x][y][z] = 2; // Flat wing
        }
    }

    // Set callback functions
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    // Capture the mouse so it stays in the window and hides the cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 3. Load OpenGL functions using GLAD
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    // Enable depth testing so 3D objects hide things behind them
    glEnable(GL_DEPTH_TEST);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark(); // Teardown style dark UI
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

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

    // 4. Main Game/Render Loop
    while (!glfwWindowShouldClose(window)) {
        // PER-FRAME TIME LOGIC
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // HANDLE INPUTS
        processInput(window);

        // --- PLAYER PHYSICS ---
        if (!gamePaused && !playerCar.isDriving) {
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

        // RAYCASTING (Destruction mechanic)
        bool leftMousePressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if (leftMousePressed && !leftMouseWasPressed && !gamePaused) {
            // Sledgehammer Swing
            if (swingTimer <= 0.0f) swingTimer = 0.15f; 
            
            // Simple Raycast
            glm::vec3 rayOrigin = cameraPos;
            glm::vec3 rayDir = cameraFront;
            float maxDistance = 2.5f; // Sledgehammer reach
            float step = 0.01f;       // Move 1cm per step
            
            for (float d = 0.0f; d < maxDistance; d += step) {
                glm::vec3 p = rayOrigin + rayDir * d;
                
                // Convert world space coordinates back to grid indexes
                int gridX = static_cast<int>(round(p.x / voxelSize));
                int gridY = static_cast<int>(round(p.y / voxelSize));
                int gridZ = static_cast<int>(round(p.z / voxelSize));
                
                // If it hits an active block inside our grid, destroy it in a radius
                if (gridX >= 0 && gridX < GRID_SIZE && 
                    gridY >= 0 && gridY < GRID_SIZE && 
                    gridZ >= 0 && gridZ < GRID_SIZE) {
                    
                    if (voxelGrid[gridX][gridY][gridZ] > 0) {
                        // Teardown-style Multi-Block Destruction
                        int radius = 2; // Radius of destruction
                        for (int x = -radius; x <= radius; x++) {
                            for (int y = -radius; y <= radius; y++) {
                                for (int z = -radius; z <= radius; z++) {
                                    // Sphere-ish destruction check
                                    if (x*x + y*y + z*z <= radius*radius + 1) {
                                        int tx = gridX + x;
                                        int ty = gridY + y;
                                        int tz = gridZ + z;
                                        
                                        if (tx >= 0 && tx < GRID_SIZE && ty >= 0 && ty < GRID_SIZE && tz >= 0 && tz < GRID_SIZE) {
                                            if (voxelGrid[tx][ty][tz] > 0) {
                                                uint8_t blockType = voxelGrid[tx][ty][tz];
                                                voxelGrid[tx][ty][tz] = 0;
                                                
                                                // Optimized Debris: 1 particle per destroyed voxel
                                                Debris d;
                                                d.position = glm::vec3(tx * voxelSize, ty * voxelSize, tz * voxelSize);
                                                float rx = ((rand() % 100) / 100.0f) - 0.5f;
                                                float ry = ((rand() % 100) / 100.0f) + 0.2f;
                                                float rz = ((rand() % 100) / 100.0f) - 0.5f;
                                                d.velocity = rayDir * 3.0f + glm::vec3(rx, ry + 1.0f, rz) * 1.5f;
                                                d.type = blockType;
                                                d.life = 0.5f;
                                                activeDebris.push_back(d);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        break; // Stop raycasting after the area hit
                    }
                }
            }
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
            if (!playerCar.isDriving) {
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
            unsigned int roughLoc = glGetUniformLocation(shaderProgram, "roughness");
            unsigned int metalLoc = glGetUniformLocation(shaderProgram, "metalness");

            // Classic high-contrast Teardown sun angle
            glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
            glUniform3f(lightLoc, sunDirection.x, sunDirection.y, sunDirection.z);
            glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));

            // Pass Resolution and Fog settings
            GLuint resLoc = glGetUniformLocation(shaderProgram, "screenRes");
            glUniform2f(resLoc, (float)width, (float)height);
            GLuint fogDenLoc = glGetUniformLocation(shaderProgram, "fogDensity");
            glUniform1f(fogDenLoc, 0.2f); // Moderate world fog

            // DRAW THE CUBE FRAGMENTS
            glBindVertexArray(VAO);

            for (int x = 0; x < GRID_SIZE; x++) {
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int z = 0; z < GRID_SIZE; z++) {
                        // Only draw if the voxel is solid/active!
                        uint8_t vType = voxelGrid[x][y][z];
                        if (vType == 0) continue;

                        // OPTIMIZATION: Don't render voxels that are not facing air
                        bool isExposed = false;
                        if (x == 0 || voxelGrid[x-1][y][z] == 0) isExposed = true;
                        else if (x == GRID_SIZE-1 || voxelGrid[x+1][y][z] == 0) isExposed = true;
                        else if (y == 0 || voxelGrid[x][y-1][z] == 0) isExposed = true;
                        else if (y == GRID_SIZE-1 || voxelGrid[x][y+1][z] == 0) isExposed = true;
                        else if (z == 0 || voxelGrid[x][y][z-1] == 0) isExposed = true;
                        else if (z == GRID_SIZE-1 || voxelGrid[x][y][z+1] == 0) isExposed = true;

                        if (!isExposed) continue; // Skip rendering hidden voxels!

                        // NEIGHBOR-BASED AMBIENT OCCLUSION
                        // More solid neighbors = darker voxel. Check 6 sides.
                        int neighbors = 0;
                        if (x > 0 && voxelGrid[x-1][y][z] > 0) neighbors++;
                        if (x < GRID_SIZE-1 && voxelGrid[x+1][y][z] > 0) neighbors++;
                        if (y > 0 && voxelGrid[x][y-1][z] > 0) neighbors++;
                        if (y < GRID_SIZE-1 && voxelGrid[x][y+1][z] > 0) neighbors++;
                        if (z > 0 && voxelGrid[x][y][z-1] > 0) neighbors++;
                        if (z < GRID_SIZE-1 && voxelGrid[x][y][z+1] > 0) neighbors++;
                        
                        float finalNeighborAO = 1.0f - (neighbors * 0.12f);
                        glUniform1f(aoLoc, glm::clamp(finalNeighborAO, 0.4f, 1.0f));

                        // CPU REALTIME RAYTRACED SHADOWS
                        // Trace a ray backwards to the sun for this specific voxel
                        float shadow = 0.0f;
                        glm::vec3 rayDir = -sunDirection; // pointing towards the sun
                        float rx = x; float ry = y; float rz = z;
                        for (int s = 1; s < 30; s++) { // Trace 30 blocks into the sky
                            rx += rayDir.x; ry += rayDir.y; rz += rayDir.z;
                            int cx = static_cast<int>(round(rx));
                            int cy = static_cast<int>(round(ry));
                            int cz = static_cast<int>(round(rz));
                            
                            // Target left the grid entirely upwards towards sun, it is not in shadow
                            if (cy >= GRID_SIZE || cy < 0 || cx < 0 || cx >= GRID_SIZE || cz < 0 || cz >= GRID_SIZE) break;
                            
                            if (voxelGrid[cx][cy][cz] > 0) {
                                shadow = 0.95f; // Blocked by another solid voxel! Extremely dark shadow
                                break;
                            }
                        }
                        glUniform1f(shadowLoc, shadow);

                        glm::mat4 model = glm::mat4(1.0f);
                        // Move to correct grid position
                        model = glm::translate(model, glm::vec3(x * voxelSize, y * voxelSize, z * voxelSize));
                        // Scale down to voxel size
                        model = glm::scale(model, glm::vec3(voxelSize));

                        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                        float r = 0.5f, g = 0.5f, b = 0.5f;
                        float roughness = 0.8f;
                        float metalness = 0.0f;

                        if (vType == 1)      { r = 0.35f; g = 0.55f; b = 0.25f; roughness = 0.9f; } // Grass
                        else if (vType == 2) { r = 0.40f; g = 0.30f; b = 0.15f; roughness = 1.0f; } // Dirt
                        else if (vType == 3) { r = 0.55f; g = 0.55f; b = 0.55f; roughness = 0.7f; } // Stone
                        else if (vType == 4) { r = 0.80f; g = 0.80f; b = 0.85f; roughness = 0.1f; metalness = 1.0f; } // Metal!

                        glUniform1f(roughLoc, roughness);
                        glUniform1f(metalLoc, metalness);

                        // Slight variations to highlight grid
                        float colorVariation = ((x + y + z) % 3) * 0.02f;
                        glUniform4f(colorLoc, r + colorVariation, g + colorVariation, b + colorVariation, 1.0f);

                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }
            }

            // --- DRAW DEBRIS ---
            for (size_t i = 0; i < activeDebris.size(); i++) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, activeDebris[i].position);
                model = glm::scale(model, glm::vec3(voxelSize * 0.6f)); // Slightly smaller than voxel for chunks
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                float r = 0.5f, g = 0.5f, b = 0.5f;
                uint8_t vType = activeDebris[i].type;
                if (vType == 1)      { r = 0.35f; g = 0.55f; b = 0.25f; glUniform1f(roughLoc, 0.9f); glUniform1f(metalLoc, 0.0f); }
                else if (vType == 2) { r = 0.40f; g = 0.30f; b = 0.15f; glUniform1f(roughLoc, 1.0f); glUniform1f(metalLoc, 0.0f); }
                else if (vType == 3) { r = 0.55f; g = 0.55f; b = 0.55f; glUniform1f(roughLoc, 0.7f); glUniform1f(metalLoc, 0.0f); }
                else if (vType == 4) { r = 0.80f; g = 0.80f; b = 0.85f; glUniform1f(roughLoc, 0.1f); glUniform1f(metalLoc, 1.0f); }

                glUniform4f(colorLoc, r, g, b, 1.0f);
                glUniform1f(shadowLoc, 0.0f); // Fast simple rendering with no dynamic casting shadows

                glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // --- DRAW THE SLEEK SPORTS CAR ---
            for(int x=0; x<16; x++) for(int y=0; y<6; y++) for(int z=0; z<8; z++) {
                uint8_t type = playerCar.design[x][y][z];
                if (type == 0) continue;
                
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, playerCar.position);
                model = glm::rotate(model, glm::radians(playerCar.rotation), glm::vec3(0, 1, 0));
                
                // Scale for sports car proportions
                float cS = voxelSize * 1.5f;
                model = glm::translate(model, glm::vec3((x-8) * cS, (y-0.5f) * cS, (z-4) * cS));
                model = glm::scale(model, glm::vec3(cS));
                
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                
                if (type == 1)      { glUniform4f(colorLoc, 0.9f, 0.1f, 0.1f, 1.0f); glUniform1f(roughLoc, 0.2f); glUniform1f(metalLoc, 0.0f); } // Red Car Paint
                else if (type == 2) { glUniform4f(colorLoc, 0.05f, 0.05f, 0.05f, 1.0f); glUniform1f(roughLoc, 0.7f); glUniform1f(metalLoc, 0.0f); } // Matte Black
                else if (type == 3) { glUniform4f(colorLoc, 0.1f, 0.1f, 0.1f, 1.0f); glUniform1f(roughLoc, 0.9f); glUniform1f(metalLoc, 0.0f); } // Tires
                else if (type == 4) { glUniform4f(colorLoc, 0.8f, 0.8f, 0.8f, 1.0f); glUniform1f(roughLoc, 0.1f); glUniform1f(metalLoc, 0.8f); } // Chrome
                else if (type == 5) { glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); glUniform1f(roughLoc, 0.1f); glUniform1f(metalLoc, 0.0f); } // Polished White
                
                glUniform1f(shadowLoc, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // --- DRAW HAMMER UI ---
            // --- DRAW MASSIVE HAMMER UI (Teardown Style) ---
            if (!playerCar.isDriving) {
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
                            glUniform1f(roughLoc, 0.8f);
                            glUniform1f(metalLoc, 0.0f);
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
                            float hNoise = ((abs(x)+abs(y)+abs(z)) % 2 == 0) ? 0.05f : 0.0f;
                            glUniform4f(colorLoc, 0.12f+hNoise, 0.15f+hNoise, 0.18f+hNoise, 1.0f);
                            
                            glUniform1f(shadowLoc, 0.1f); 
                            glUniform1f(roughLoc, 0.2f);
                            glUniform1f(metalLoc, 1.0f); // Metallic hammer head
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

    glfwTerminate();
    return 0;
}
