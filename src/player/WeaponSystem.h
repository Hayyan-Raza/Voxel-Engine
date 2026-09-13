#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Raycast + crater destruction (called each frame when unpaused)
void handleDestruction(GLFWwindow* window);

void triggerTestHit();
