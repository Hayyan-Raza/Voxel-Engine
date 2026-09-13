#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "../rendering/PostProcess.h"
#include "../ui/UIManager.h"

class RenderPipeline {
public:
    void init(int width, int height);
    void renderFrame(GLFWwindow* window, float deltaTime, UIManager& uiManager);
    void shutdown();

private:
    void initShaders();
    void initBuffers(int width, int height);

    PostProcess pp;

    // Shader IDs
    GLuint mainShader = 0;
    GLuint particleShader = 0;
    GLuint shadowShader = 0;
    GLuint skyShader = 0;

    // Global VAO/VBOs
    GLuint cubeVAO = 0;
    GLuint cubeVBO = 0;
    GLuint partVAO = 0;
    GLuint partVBO = 0;
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    // Shadow Map FBO
    GLuint depthMapFBO = 0;
    GLuint depthMap = 0;

    // Cached uniform locations (resolved once, used every frame)
    struct {
        GLint view, projection, model, lightSpaceMatrix;
        GLint voxelColor, shadowObscurance, lightDir, viewPos, screenRes;
        GLint uSkyColor, fogDensity, uAOScale, neighborAO, iTime;
        GLint uWaterShallowColor, uWaterDeepColor, uWaterSkyBlend, uWaterWaveSpeed;
        GLint uVegSwaySpeed, uVegSwayIntensity;
        GLint uAmbientColor, uDiffuseColor;
        GLint uPointLightPos, uPointLightColor;
    } mainLocs;

    struct {
        GLint lightSpaceMatrix, model;
    } shadowLocs;

    struct {
        GLint invProj, invView, uSunDir, uSkyColor;
        GLint uEnableSunMoon, uCameraPos;
    } skyLocs;

    struct {
        GLint view, projection, uTime;
    } particleLocs;

    void cacheUniformLocations();
};
