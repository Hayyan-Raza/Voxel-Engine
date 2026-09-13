#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class UIManager {
public:
    void init(GLFWwindow* window);
    void shutdown();
    
    // Render the initial loading screen when starting
    void renderLoadingScreenInitial(GLFWwindow* window);
    
    // Render the loading screen while meshes are being loaded
    void renderLoadingScreenMeshes(GLFWwindow* window, int loadedMeshes, int maxMeshes);
    
    // Render the main UI
    void renderUI(GLFWwindow* window, float deltaTime, const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::mat4& view, const glm::mat4& proj);

private:
    GLuint woodTexture = 0;

    GLuint loadTexture(const char* path);

    void drawBlueprintSaveDialog(float fbW, float fbH);
};
