#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>

void updatePhysics();

// Raycast + crater destruction (called each frame when unpaused)

void triggerTestHit();
