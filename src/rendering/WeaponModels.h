#pragma once
#include <glad/gl.h>
#include <vector>
#include <glm/glm.hpp>
#include "../core/Types.h"

extern GLuint hammerHandleVAO;
extern GLuint hammerHandleVBO;
extern int hammerHandleVertexCount;

extern GLuint ak47VAO;
extern GLuint ak47VBO;
extern int ak47VertexCount;

extern GLuint glockVAO;
extern GLuint glockVBO;
extern int glockVertexCount;

extern GLuint shotgunVAO;
extern GLuint shotgunVBO;
extern int shotgunVertexCount;

extern GLuint dynVAO;
extern GLuint dynVBO;
extern int dynVertexCount;

void initHammerGeometry();
void cleanupHammerGeometry();

void initAK47Geometry();
void cleanupAK47Geometry();

void initGlockGeometry();
void cleanupGlockGeometry();

void initShotgunGeometry();
void cleanupShotgunGeometry();

void initDynamiteGeometry();
void cleanupDynamiteGeometry();

void initWeaponModels();
void cleanupWeaponModels();
