#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace UI {
namespace Debug {

    void drawFPSCounter();
    void drawVibePackMenu();
    void drawConfigMenu(GLFWwindow* window);
    void drawPauseMenu(GLFWwindow* window, float fbW, float fbH);
    void drawRagdollMenu(const glm::vec3& cameraPos, const glm::vec3& cameraFront);

}
}
