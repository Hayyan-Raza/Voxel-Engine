#include "RenderPipeline.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../core/Globals.h"
#include "../rendering/ShaderManager.h"
#include "../rendering/Renderer.h"
#include "../physics/Ragdoll.h"
#include "../physics/Mob.h"
#include "../world/World.h"
#include "../rendering/WeaponModels.h"
#include "../player/Weapons.h"
#include <algorithm>

extern void drawChunks(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLuint VAO);
extern void drawGhostBlock(GLint modelLoc, GLint colorLoc, GLuint cubeVAO);
extern void drawBuildingGizmo(GLint modelLoc, GLint colorLoc, GLuint cubeVAO);
extern void drawActiveWeapon(GLint modelLoc, GLint colorLoc, GLint shadowLoc, GLint projLoc, GLint viewLoc, GLuint VAO);

void RenderPipeline::init(int width, int height) {
    initShaders();
    initBuffers(width, height);
    pp.init(width, height);
}

void RenderPipeline::shutdown() {
    std::cout << "Deleting OpenGL buffers..." << std::endl;
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &partVAO);
    glDeleteBuffers(1, &partVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    
    glDeleteProgram(mainShader);
    glDeleteProgram(particleShader);
    glDeleteProgram(shadowShader);
    glDeleteProgram(skyShader);
}

void RenderPipeline::initShaders() {
    mainShader = ShaderManager::LoadShader("assets/shaders/main.vert", "assets/shaders/main.frag");
    particleShader = ShaderManager::LoadShader("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    shadowShader = ShaderManager::LoadShader("assets/shaders/shadow.vert", "assets/shaders/shadow.frag");
    skyShader = ShaderManager::LoadShader("assets/shaders/sky.vert", "assets/shaders/sky.frag");
    cacheUniformLocations();
}

void RenderPipeline::cacheUniformLocations() {
    // Main shader
    mainLocs.view = glGetUniformLocation(mainShader, "view");
    mainLocs.projection = glGetUniformLocation(mainShader, "projection");
    mainLocs.model = glGetUniformLocation(mainShader, "model");
    mainLocs.lightSpaceMatrix = glGetUniformLocation(mainShader, "lightSpaceMatrix");
    mainLocs.voxelColor = glGetUniformLocation(mainShader, "voxelColor");
    mainLocs.shadowObscurance = glGetUniformLocation(mainShader, "shadowObscurance");
    mainLocs.lightDir = glGetUniformLocation(mainShader, "lightDir");
    mainLocs.viewPos = glGetUniformLocation(mainShader, "viewPos");
    mainLocs.screenRes = glGetUniformLocation(mainShader, "screenRes");
    mainLocs.uSkyColor = glGetUniformLocation(mainShader, "uSkyColor");
    mainLocs.fogDensity = glGetUniformLocation(mainShader, "fogDensity");
    mainLocs.uAOScale = glGetUniformLocation(mainShader, "uAOScale");
    mainLocs.neighborAO = glGetUniformLocation(mainShader, "neighborAO");
    mainLocs.iTime = glGetUniformLocation(mainShader, "iTime");
    mainLocs.uWaterShallowColor = glGetUniformLocation(mainShader, "uWaterShallowColor");
    mainLocs.uWaterDeepColor = glGetUniformLocation(mainShader, "uWaterDeepColor");
    mainLocs.uWaterSkyBlend = glGetUniformLocation(mainShader, "uWaterSkyBlend");
    mainLocs.uWaterWaveSpeed = glGetUniformLocation(mainShader, "uWaterWaveSpeed");
    mainLocs.uVegSwaySpeed = glGetUniformLocation(mainShader, "uVegSwaySpeed");
    mainLocs.uVegSwayIntensity = glGetUniformLocation(mainShader, "uVegSwayIntensity");
    mainLocs.uAmbientColor = glGetUniformLocation(mainShader, "uAmbientColor");
    mainLocs.uDiffuseColor = glGetUniformLocation(mainShader, "uDiffuseColor");
    mainLocs.uPointLightPos = glGetUniformLocation(mainShader, "uPointLightPos");
    mainLocs.uPointLightColor = glGetUniformLocation(mainShader, "uPointLightColor");

    // Shadow shader
    shadowLocs.lightSpaceMatrix = glGetUniformLocation(shadowShader, "lightSpaceMatrix");
    shadowLocs.model = glGetUniformLocation(shadowShader, "model");

    // Sky shader
    skyLocs.invProj = glGetUniformLocation(skyShader, "invProj");
    skyLocs.invView = glGetUniformLocation(skyShader, "invView");
    skyLocs.uSunDir = glGetUniformLocation(skyShader, "uSunDir");
    skyLocs.uSkyColor = glGetUniformLocation(skyShader, "uSkyColor");
    skyLocs.uEnableSunMoon = glGetUniformLocation(skyShader, "uEnableSunMoon");
    skyLocs.uCameraPos = glGetUniformLocation(skyShader, "uCameraPos");

    // Particle shader
    particleLocs.view = glGetUniformLocation(particleShader, "view");
    particleLocs.projection = glGetUniformLocation(particleShader, "projection");
    particleLocs.uTime = glGetUniformLocation(particleShader, "uTime");
}

void RenderPipeline::initBuffers(int width, int height) {
    // --- Shadow Map Setup ---
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    initCubeGeometry(cubeVAO, cubeVBO);

    // --- Particle Billboard VBO ---
    glGenVertexArrays(1, &partVAO);
    glGenBuffers(1, &partVBO);
    glBindVertexArray(partVAO);
    glBindBuffer(GL_ARRAY_BUFFER, partVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Fullscreen quad for sky
    float quadVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, 
        -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glUseProgram(mainShader);
    glUniform1i(glGetUniformLocation(mainShader, "shadowMap"), 1);
}

void RenderPipeline::renderFrame(GLFWwindow* window, float deltaTime, UIManager& uiManager) {
    int fbW_int, fbH_int;
    glfwGetFramebufferSize(window, &fbW_int, &fbH_int);
    
    float renderScale = (fbW_int > 1920) ? 0.6f : 1.0f;
    int renderW = (int)(fbW_int * renderScale);
    int renderH = (int)(fbH_int * renderScale);

    static int lastRenderW = 800, lastRenderH = 600;
    if (renderW > 0 && renderH > 0 && (renderW != lastRenderW || renderH != lastRenderH)) {
        pp.resize(renderW, renderH);
        lastRenderW = renderW;
        lastRenderH = renderH;
    }

    fbW = (float)fbW_int;
    fbH = (float)fbH_int;

    float aspect = (fbH > 0) ? (float)fbW / (float)fbH : 1.0f;
    glm::mat4 proj = glm::perspective(glm::radians(fieldOfView), aspect, 0.01f, 100.0f);
    glm::mat4 playerView = glm::lookAt(cameraPos + cameraShakeOffset, cameraPos + cameraShakeOffset + cameraFront, cameraUp);
    glm::mat4 view = spectatorMode ? glm::lookAt(spectatorPos, spectatorPos + spectatorFront, spectatorUp) : playerView;
    viewFrustum.update(proj * playerView);

    // Day/Night Cycle
    static float timeOfDay = 0.0f;
    timeOfDay += deltaTime * dayNightSpeed; 
    
    glm::vec3 sun = glm::normalize(glm::vec3(cosf(timeOfDay), sinf(timeOfDay), sinf(timeOfDay) * 0.5f));
    glm::vec3 lightDir = sun.y > 0.0f ? sun : -sun;

    static glm::vec3 daySky(0.40f, 0.60f, 0.85f);
    glm::vec3 sunsetSky(0.85f, 0.40f, 0.20f);
    glm::vec3 nightSky(0.02f, 0.02f, 0.08f);

    glm::vec3 dayAmbient(0.15f, 0.18f, 0.22f); // Darkened to make GI bounces more visible
    glm::vec3 nightAmbient(0.02f, 0.03f, 0.05f);

    glm::vec3 dayDiffuse(1.1f, 1.05f, 1.0f);
    glm::vec3 sunsetDiffuse(1.0f, 0.6f, 0.4f);
    glm::vec3 nightDiffuse(0.2f, 0.3f, 0.5f);

    glm::vec3 currentSky, currentAmbient, currentDiffuse;
    if (sun.y > 0.2f) {
        currentSky = daySky; currentAmbient = dayAmbient; currentDiffuse = dayDiffuse;
    } else if (sun.y > 0.0f) {
        float t = sun.y / 0.2f;
        currentSky = glm::mix(sunsetSky, daySky, t);
        currentAmbient = dayAmbient; 
        currentDiffuse = glm::mix(sunsetDiffuse, dayDiffuse, t);
    } else if (sun.y > -0.2f) {
        float t = (sun.y + 0.2f) / 0.2f;
        currentSky = glm::mix(nightSky, sunsetSky, t);
        currentAmbient = glm::mix(nightAmbient, dayAmbient, t);
        currentDiffuse = glm::mix(nightDiffuse, sunsetDiffuse, t);
    } else {
        currentSky = nightSky; currentAmbient = nightAmbient; currentDiffuse = nightDiffuse;
    }
    
    currentAmbient *= 0.48f;
    if (!enableDirLight) {
        currentDiffuse = glm::vec3(0.0f);
        currentAmbient = glm::vec3(0.9f);
    } else {
        currentDiffuse *= 0.85f;
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glm::mat4 lightProjection, lightView;
    glm::mat4 lightSpaceMatrix;
    float near_plane = 1.0f, far_plane = 350.0f; 
    float orthoSize = 60.0f;
    lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane, far_plane);
    
    // Snap light target to texel size to prevent shadow swimming
    float texelSize = (orthoSize * 2.0f) / 2048.0f;
    glm::vec3 lightTarget = cameraPos;
    lightTarget.x = std::floor(lightTarget.x / texelSize) * texelSize;
    lightTarget.z = std::floor(lightTarget.z / texelSize) * texelSize;
    lightTarget.y = std::floor(lightTarget.y / texelSize) * texelSize; // Quantize Y too instead of forcing 0.0f
    
    // Place light high enough so tall mountains aren't clipped behind the light
    glm::vec3 lightPos = lightTarget + lightDir * 180.0f; 
    
    lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;

    // --- Shadow Pass ---
    glViewport(0, 0, 2048, 2048);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(shadowShader);
    glUniformMatrix4fv(shadowLocs.lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    // Draw static chunks
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(shadowLocs.model, 1, GL_FALSE, glm::value_ptr(model));

    int pcx = (int)std::floor(cameraPos.x / (CHUNK_SIZE * voxelSize));
    int pcz = (int)std::floor(cameraPos.z / (CHUNK_SIZE * voxelSize));

    for (ChunkMesh* cmPtr : activeStaticMeshes) {
        ChunkMesh& cm = *cmPtr;
        bool isSpawn = (cm.cx == spawnChunkPos.x && cm.cz == spawnChunkPos.y);
        int dist = std::max(std::abs(cm.cx - pcx), std::abs(cm.cz - pcz));
        if (!isSpawn && dist > renderDistanceChunks) continue;

        glm::vec3 chunkCenter = cm.minAABB + glm::vec3((CHUNK_SIZE * voxelSize) / 2.0f);
        // Use squared distance to avoid sqrt
        glm::vec2 diff(chunkCenter.x - lightTarget.x, chunkCenter.z - lightTarget.z);
        float distSq = glm::dot(diff, diff);
        float cullDist = orthoSize + CHUNK_SIZE * voxelSize;
        if (distSq > cullDist * cullDist) continue; // Skip chunks outside shadow frustum

        glBindVertexArray(cm.VAO);
        glDrawArrays(GL_TRIANGLES, 0, cm.vertexCount);
    }
    
    // Some static geometry needs dummy uniforms if they use the same draw logic, 
    // but the actual `drawChunks` function takes shader locations. Let's provide them for shadowShader
    drawChunks(shadowLocs.model, -1, -1, cubeVAO);
    drawRagdolls(shadowLocs.model, -1, -1);
    drawMobs(shadowLocs.model, -1, -1);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Main Render Pass ---
    pp.begin();
    glViewport(0, 0, fbW_int, fbH_int);
    glClearColor(currentSky.r, currentSky.g, currentSky.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (enableWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glUseProgram(mainShader);
    glUniformMatrix4fv(mainLocs.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(mainLocs.projection, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(mainLocs.lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    
    glUniform3f(mainLocs.uSkyColor, currentSky.r, currentSky.g, currentSky.b);
    glUniform1f(mainLocs.fogDensity, g_fogDensity);
    glUniform3f(mainLocs.uWaterShallowColor, waterShallowColor.r, waterShallowColor.g, waterShallowColor.b);
    glUniform3f(mainLocs.uWaterDeepColor, waterDeepColor.r, waterDeepColor.g, waterDeepColor.b);
    glUniform1f(mainLocs.uWaterSkyBlend, waterSkyBlend);
    glUniform1f(mainLocs.uWaterWaveSpeed, waterWaveSpeed);
    glUniform1f(mainLocs.uVegSwaySpeed, vegetationSwaySpeed);
    glUniform1f(mainLocs.uVegSwayIntensity, vegetationSwayIntensity);
    glUniform3f(mainLocs.uAmbientColor, currentAmbient.r, currentAmbient.g, currentAmbient.b);
    glUniform3f(mainLocs.uDiffuseColor, currentDiffuse.r, currentDiffuse.g, currentDiffuse.b);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glActiveTexture(GL_TEXTURE0);

    glUniform3f(mainLocs.lightDir, lightDir.x, lightDir.y, lightDir.z);
    glUniform3fv(mainLocs.viewPos, 1, glm::value_ptr(cameraPos));
    glUniform2f(mainLocs.screenRes, (float)fbW, (float)fbH);
    glUniform1f(mainLocs.uAOScale, vAOScale);
    glUniform1f(mainLocs.neighborAO, 1.0f);
    glUniform1f(mainLocs.iTime, (float)glfwGetTime());

    if (muzzleFlashTimer > 0.0f) {
        glUniform3f(mainLocs.uPointLightPos, muzzleFlashPos.x, muzzleFlashPos.y, muzzleFlashPos.z);
        glUniform3f(mainLocs.uPointLightColor, 10.0f, 8.0f, 2.0f);
    } else {
        glUniform3f(mainLocs.uPointLightColor, 0.0f, 0.0f, 0.0f);
    }

    glm::mat4 I = glm::mat4(1.0f);
    glUniformMatrix4fv(mainLocs.model, 1, GL_FALSE, glm::value_ptr(I));
    glUniform4f(mainLocs.voxelColor, 1, 1, 1, 1);
    glUniform1f(mainLocs.shadowObscurance, 0.0f);

    // Front-to-back sorting for Early-Z
    std::sort(activeStaticMeshes.begin(), activeStaticMeshes.end(), [&viewPos = cameraPos](ChunkMesh* a, ChunkMesh* b) {
        glm::vec3 ca = a->minAABB + glm::vec3((CHUNK_SIZE * voxelSize) / 2.0f);
        glm::vec3 cb = b->minAABB + glm::vec3((CHUNK_SIZE * voxelSize) / 2.0f);
        glm::vec3 da = ca - viewPos;
        glm::vec3 db = cb - viewPos;
        return glm::dot(da, da) < glm::dot(db, db);
    });

    for (ChunkMesh* cmPtr : activeStaticMeshes) {
        ChunkMesh& cm = *cmPtr;
        bool isSpawn = (cm.cx == spawnChunkPos.x && cm.cz == spawnChunkPos.y);
        int dist = std::max(std::abs(cm.cx - pcx), std::abs(cm.cz - pcz));
        if (!isSpawn && dist > renderDistanceChunks) continue;

        if (viewFrustum.isBoxVisible(cm.minAABB, cm.maxAABB)) {
            glBindVertexArray(cm.VAO);
            glDrawArrays(GL_TRIANGLES, 0, cm.vertexCount);
        }
    }

    drawChunks(mainLocs.model, mainLocs.voxelColor, mainLocs.shadowObscurance, cubeVAO);
    drawRagdolls(mainLocs.model, mainLocs.voxelColor, mainLocs.shadowObscurance);
    drawMobs(mainLocs.model, mainLocs.voxelColor, mainLocs.shadowObscurance);
    drawGhostBlock(mainLocs.model, mainLocs.voxelColor, cubeVAO);
    
    // --- Transparent Water ---
    // DISABLED FOR NOW
    /*
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform4f(mainLocs.voxelColor, 1.0f, 1.0f, 1.0f, 0.7f);
    glUniformMatrix4fv(mainLocs.model, 1, GL_FALSE, glm::value_ptr(I));
    for (ChunkMesh* cmPtr : activeWaterMeshes) {
        ChunkMesh& cm = *cmPtr;
        glm::vec3 minAABB(cm.cx * CHUNK_SIZE * voxelSize, cm.cy * CHUNK_SIZE * voxelSize, cm.cz * CHUNK_SIZE * voxelSize);
        glm::vec3 maxAABB((cm.cx + 1) * CHUNK_SIZE * voxelSize, (cm.cy + 1) * CHUNK_SIZE * voxelSize, (cm.cz + 1) * CHUNK_SIZE * voxelSize);
        if (viewFrustum.isBoxVisible(minAABB, maxAABB)) {
            glBindVertexArray(cm.waterVAO);
            glDrawArrays(GL_TRIANGLES, 0, cm.waterVertexCount);
        }
    }
    
    // Infinite Water Plane
    glUniform4f(mainLocs.voxelColor, 0.15f, 0.45f, 0.85f, 0.7f);
    glm::mat4 infWaterModel = glm::translate(glm::mat4(1.0f), glm::vec3(cameraPos.x, 36.0f * voxelSize - 0.1f, cameraPos.z));
    infWaterModel = glm::scale(infWaterModel, glm::vec3(10000.0f, 0.0f, 10000.0f));
    glUniformMatrix4fv(mainLocs.model, 1, GL_FALSE, glm::value_ptr(infWaterModel));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisable(GL_BLEND);
    */

    // Draw Sky
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyShader);
    
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(skyLocs.invProj, 1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
    glUniformMatrix4fv(skyLocs.invView, 1, GL_FALSE, glm::value_ptr(glm::inverse(skyView)));
    glUniform3f(skyLocs.uSunDir, sun.x, sun.y, sun.z);
    glUniform3f(skyLocs.uSkyColor, currentSky.r, currentSky.g, currentSky.b);
    glUniform1i(skyLocs.uEnableSunMoon, enableSunMoon ? 1 : 0);
    glUniform3fv(skyLocs.uCameraPos, 1, glm::value_ptr(cameraPos));
    
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthFunc(GL_LESS);

    // Draw Gizmo OVER the skybox but interacting correctly with opaque depth
    glUseProgram(mainShader);
    glUniform1f(mainLocs.fogDensity, 0.0f);
    glUniform3f(mainLocs.lightDir, 0.5f, 0.8f, 0.3f);
    glUniform3f(mainLocs.uAmbientColor, 0.5f, 0.5f, 0.5f);
    glUniform3f(mainLocs.uDiffuseColor, 0.7f, 0.7f, 0.7f);
    drawBuildingGizmo(mainLocs.model, mainLocs.voxelColor, cubeVAO);

    // Weapons
    glUseProgram(mainShader);
    glUniform1f(mainLocs.fogDensity, 0.0f);
    glUniform3f(mainLocs.lightDir, 0.5f, 0.8f, 0.3f);
    glUniform3f(mainLocs.uAmbientColor, 0.5f, 0.5f, 0.5f);
    glUniform3f(mainLocs.uDiffuseColor, 0.7f, 0.7f, 0.7f);

    drawActiveWeapon(mainLocs.model, mainLocs.voxelColor, mainLocs.shadowObscurance, mainLocs.projection, mainLocs.view, cubeVAO);
    
    // --- Particles ---
    if (!activeParticles.empty()) {
        glm::mat4 invView = glm::inverse(view);
        glm::vec3 camRight(invView[0][0], invView[0][1], invView[0][2]);
        glm::vec3 camUp(invView[1][0], invView[1][1], invView[1][2]);

        glm::vec2 corners[4] = {{-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f}};
        glm::vec2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

        auto collectVertices = [&](bool wantAdditive) -> std::vector<float> {
            std::vector<float> data;
            for (const auto& p : activeParticles) {
                bool isAdditive = (p.growthRate > 0.4f || p.growthRate == 0.0f);
                if (isAdditive != wantAdditive) continue;

                float cosA = cosf(p.rotation);
                float sinA = sinf(p.rotation);
                bool isSpark = (p.growthRate == 0.0f);
                float pSize = p.size;
                
                glm::vec3 v[4];
                if (isSpark && glm::length(p.velocity) > 0.1f) {
                    glm::vec3 velDir = glm::normalize(p.velocity);
                    float len = glm::length(p.velocity) * 0.035f;
                    glm::vec3 rightDir = glm::normalize(glm::cross(velDir, glm::vec3(0.0f, 1.0f, 0.0f)));
                    if (glm::length(rightDir) < 0.01f) rightDir = camRight;
                    
                    v[0] = p.position - velDir * len - rightDir * pSize;
                    v[1] = p.position - velDir * len + rightDir * pSize;
                    v[2] = p.position + velDir * len + rightDir * pSize;
                    v[3] = p.position + velDir * len - rightDir * pSize;
                } else {
                    for (int cIndex = 0; cIndex < 4; cIndex++) {
                        float rx = corners[cIndex].x * cosA - corners[cIndex].y * sinA;
                        float ry = corners[cIndex].x * sinA + corners[cIndex].y * cosA;
                        v[cIndex] = p.position + (camRight * rx + camUp * ry) * (pSize * 2.2f);
                    }
                }

                int indices[6] = { 0, 1, 2, 0, 2, 3 };
                for (int i = 0; i < 6; i++) {
                    int idx = indices[i];
                    data.push_back(v[idx].x); data.push_back(v[idx].y); data.push_back(v[idx].z);
                    data.push_back(uvs[idx].x); data.push_back(uvs[idx].y);
                    data.push_back(p.color.r); data.push_back(p.color.g); data.push_back(p.color.b);
                    data.push_back(p.alpha);
                }
            }
            return data;
        };

        std::vector<float> smokeData = collectVertices(false);
        glUseProgram(particleShader);
        glUniformMatrix4fv(particleLocs.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(particleLocs.projection, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(particleLocs.uTime, (float)glfwGetTime());

        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBindVertexArray(partVAO);

        if (!smokeData.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindBuffer(GL_ARRAY_BUFFER, partVBO);
            glBufferData(GL_ARRAY_BUFFER, smokeData.size() * sizeof(float), smokeData.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(smokeData.size() / 9));
        }

        std::vector<float> fireData = collectVertices(true);
        if (!fireData.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive
            glBindBuffer(GL_ARRAY_BUFFER, partVBO);
            glBufferData(GL_ARRAY_BUFFER, fireData.size() * sizeof(float), fireData.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(fireData.size() / 9));
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    pp.end();
    pp.render(fbW_int, fbH_int, (float)glfwGetTime(), vBloom, vChromAb, vGrain, vExposure, view, proj, sun);

    // Call UI Manager
    uiManager.renderUI(window, deltaTime, cameraPos, cameraFront, view, proj);

    glfwSwapBuffers(window);
}
