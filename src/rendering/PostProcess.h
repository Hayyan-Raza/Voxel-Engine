#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

class PostProcess {
public:
    GLuint fbo;
    GLuint texture;
    GLuint normalTexture;
    GLuint emissionTexture; // Attachment 2 for G-Buffer
    GLuint depthTexture;
    GLuint rbo; // keeping rbo just in case, or we can remove it since depthTexture replaces it
    
    // Ping-pong for blur
    GLuint pingpongFBO[3];
    GLuint pingpongColorbuffers[3];
    
    GLuint quadVAO, quadVBO;
    GLuint shader;
    GLuint blurShader;
    GLuint compositeShader;
    GLint uTime, uRes, uBloom, uChromAb, uGrain, uExposure;
    GLint uView, uProj, uInvView, uInvProj;
    int width, height;

    PostProcess();
    ~PostProcess();

    void init(int width, int height);
    void resize(int width, int height);
    void begin();
    void render(int width, int height, float time, float bloom, float chromAb, float grain, float exposure, const glm::mat4& view, const glm::mat4& proj);
    void end();

private:
    void setupQuad();
};
