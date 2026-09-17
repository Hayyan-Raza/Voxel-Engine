#include "Engine.h"
#include <iostream>
#include <vector>
#include <ctime>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Audio.h"
#include "Globals.h"
#include "../world/World.h"
#include "../world/TerrainGenerator.h"
#include "../physics/Physics.h"
#include "../physics/IslandDetection.h"
#include "../world/Lighting.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include "../particles/Particles.h"
#include "../rendering/ShaderManager.h"
#include "../rendering/Renderer.h"
#include "../rendering/WeaponModels.h"
#include "../player/Player.h"
#include "../player/WeaponSystem.h"
#include "../player/Weapons.h"
#include "../input/Input.h"
#include "../world/WaterSimulator.h"

bool Engine::init() {
    std::cout << "Engine Startup...\n";
    activeChunks.reserve(512);
    for(int i = 0; i < 256; i++) resourceInventory[i] = 1000;

    // --- GLFW ---
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return false; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    g_window = window = glfwCreateWindow(800, 600, "Teardown Clone Engine", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return false; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable VSync for maximum FPS

    // --- GLAD ---
    if (gladLoadGL(glfwGetProcAddress) == 0) { std::cerr << "Failed to init GLAD\n"; return false; }

    cameraPos = glm::vec3(0.0f, 40.0f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    if (enableWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    uiManager.init(window);

    initAudio();
    initIslandThread();
    initGenerationThreads();
    initSaveThread();
    initLightingThread();
    initMesherThread();
    
    // Terrain generation and initial loading screen moved to run() loop after GameMode is selected
    
    WaterSimulator::getInstance().init();
    initWeaponModels();
    InitWeapons();

    renderPipeline.init(800, 600);

    isInitialLoading = false;

    lastFrame = glfwGetTime();
    std::cout << "All systems initialized. Entering main loop.\n";
    return true;
}





void Engine::updateGameLogic() {
    updateActiveChunks(cameraPos);
    updateStaticMesh(cameraPos);
    processIslandResults();
    processInput(window); 
    
    static int frameCount=0; frameCount++; 
    if(frameCount==60) triggerTestHit();

    if (g_buildingSystem.GetCurrentState() != UI::BuildModeState::Inactive) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        
        int winW, winH;
        glfwGetWindowSize(window, &winW, &winH);
        g_buildingSystem.Update(deltaTime, glm::vec2((float)mx, (float)my), glm::vec2((float)winW, (float)winH));
        
        bool isLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool isRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        g_buildingSystem.HandleClick(isLeftDown, isRightDown, cameraPos, cameraFront);
    }

    if (!gamePaused) {
        updatePhysics();
        updateChunkPhysics();
        spawnFallingLeaves(cameraPos);
        updateParticles();
        WaterSimulator::getInstance().update();
        updateMobs(deltaTime);
        if (g_buildingSystem.GetCurrentState() == UI::BuildModeState::Inactive) {
            handleDestruction(window);
        }
    }
}

void Engine::run() {
    while (!glfwWindowShouldClose(window)) {
        if (g_gameMode == GameMode::MainMenu) {
            uiManager.renderStartupMenu(window);
            glfwPollEvents();
            
            if (g_gameMode != GameMode::MainMenu) {
                // User just selected a mode!
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                
                // Set spawn chunk position based on camera position
                spawnChunkPos = glm::ivec2(
                    (int)(cameraPos.x / (CHUNK_SIZE * voxelSize)),
                    (int)(cameraPos.z / (CHUNK_SIZE * voxelSize))
                );

                std::cout << "Generating terrain..." << std::endl;
                generateTerrain();

                updateActiveChunks(cameraPos);
                updateStaticMesh(cameraPos);

                float loadStartTime = glfwGetTime();
                while (isTerrainGenerating() && (glfwGetTime() - loadStartTime < 15.0f) && !glfwWindowShouldClose(window)) {
                    glfwPollEvents();
                    updateActiveChunks(cameraPos);
                    updateStaticMesh(cameraPos);
                    uiManager.renderLoadingScreenMeshes(window, activeStaticMeshes.size(), 0);
                }
                
                lastFrame = static_cast<float>(glfwGetTime());
            }
            continue;
        }

        if (enableWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        float now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastFrame;
        lastFrame = now;

        updateGameLogic();
        renderPipeline.renderFrame(window, deltaTime, uiManager);

        glfwPollEvents();
    }
}



void Engine::shutdown() {
    std::cout << "Cleaning up ragdolls..." << std::endl;
    clearAllRagdolls();
    std::cout << "Cleaning up mobs..." << std::endl;
    clearAllMobs();
    
    std::cout << "Shutting down UI..." << std::endl;
    uiManager.shutdown();
    renderPipeline.shutdown();
    
    cleanupAudio();
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    std::cout << "Cleaning up chunk meshes..." << std::endl;
    clearMeshes();

    std::cout << "Cleaning up weapon models..." << std::endl;
    cleanupWeaponModels();

    if (playerRigidBody) {
        std::cout << "Cleaning up physics..." << std::endl;
        dynamicsWorld->removeRigidBody(playerRigidBody);
        if (playerRigidBody->getMotionState()) delete playerRigidBody->getMotionState();
        delete playerRigidBody;
    }
    if (playerShape) delete playerShape;

    std::cout << "Stopping island thread..." << std::endl;
    stopIslandThread();
    std::cout << "Stopping lighting thread..." << std::endl;
    stopLightingThread();
    std::cout << "Stopping mesher thread..." << std::endl;
    stopMesherThread();
    
    std::cout << "Stopping generation thread..." << std::endl;
    stopGenerationThreads();
    
    std::cout << "Stopping save thread..." << std::endl;
    stopSaveThread();
    
    std::cout << "Terminating GLFW..." << std::endl;
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    std::cout << "Shutdown complete." << std::endl;
}

