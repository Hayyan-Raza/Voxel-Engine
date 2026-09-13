#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "../ui/UIManager.h"
#include "../rendering/RenderPipeline.h"

class Engine {
public:
    bool init();
    void run();
    void shutdown();

private:
    GLFWwindow* window = nullptr;
    UIManager uiManager;
    RenderPipeline renderPipeline;

    
    
    
    void processInputInternal();
    void initShaders();
    void initBuffers();
    void renderFrame();
    void updateGameLogic();
};
