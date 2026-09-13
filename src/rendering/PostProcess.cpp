#include "PostProcess.h"
#include "ShaderManager.h"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

PostProcess::PostProcess() : fbo(0), texture(0), normalTexture(0), depthTexture(0), rbo(0), quadVAO(0), quadVBO(0), shader(0), width(0), height(0) {}

PostProcess::~PostProcess() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (texture) glDeleteTextures(1, &texture);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (emissionTexture) glDeleteTextures(1, &emissionTexture);
    if (depthTexture) glDeleteTextures(1, &depthTexture);
    glDeleteFramebuffers(3, pingpongFBO);
    glDeleteTextures(3, pingpongColorbuffers);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void PostProcess::init(int width, int height) {
    this->width = width;
    this->height = height;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color Texture (Attachment 0)
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Normal Texture (Attachment 1)
    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTexture, 0);

    // Emission Texture (Attachment 2)
    glGenTextures(1, &emissionTexture);
    glBindTexture(GL_TEXTURE_2D, emissionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, emissionTexture, 0);

    // Tell OpenGL which color attachments we'll use
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    // Depth Texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Ping-pong framebuffers for blur
    glGenFramebuffers(3, pingpongFBO);
    glGenTextures(3, pingpongColorbuffers);
    for (unsigned int i = 0; i < 3; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR::FRAMEBUFFER:: PingPong FBO not complete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    setupQuad();
    
    shader = ShaderManager::LoadShader("assets/shaders/postprocess.vert", "assets/shaders/postprocess.frag");
    blurShader = ShaderManager::LoadShader("assets/shaders/postprocess.vert", "assets/shaders/blur.frag");
    compositeShader = ShaderManager::LoadShader("assets/shaders/postprocess.vert", "assets/shaders/composite.frag");

    uTime = glGetUniformLocation(shader, "uTime");
    uRes = glGetUniformLocation(shader, "uRes");
    uBloom = glGetUniformLocation(shader, "uBloom");
    uChromAb = glGetUniformLocation(shader, "uChromAb");
    uGrain = glGetUniformLocation(shader, "uGrain");
    uExposure = glGetUniformLocation(shader, "uExposure");
    
    uView = glGetUniformLocation(shader, "uView");
    uProj = glGetUniformLocation(shader, "uProj");
    uInvView = glGetUniformLocation(shader, "uInvView");
    uInvProj = glGetUniformLocation(shader, "uInvProj");

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "screenTexture"), 0);
    glUniform1i(glGetUniformLocation(shader, "depthTexture"), 1);
    glUniform1i(glGetUniformLocation(shader, "normalTexture"), 2);
    glUniform1i(glGetUniformLocation(shader, "emissionTexture"), 3);
    
    glUseProgram(blurShader);
    glUniform1i(glGetUniformLocation(blurShader, "image"), 0);
    
    glUseProgram(compositeShader);
    glUniform1i(glGetUniformLocation(compositeShader, "scene"), 0);
    glUniform1i(glGetUniformLocation(compositeShader, "bloomBlur"), 1);
}

void PostProcess::resize(int width, int height) {
    this->width = width;
    this->height = height;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    
    glBindTexture(GL_TEXTURE_2D, emissionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    
    for (unsigned int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    }
}

void PostProcess::begin() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void PostProcess::end() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::render(int width, int height, float time, float bloom, float chromAb, float grain, float exposure, const glm::mat4& view, const glm::mat4& proj) {
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(shader);
    
    glUniform1f(uTime, time);
    glUniform2f(uRes, (float)width, (float)height);
    
    // Vibe Pack Uniforms passed from UI
    glUniform1f(uBloom, bloom);
    glUniform1f(uChromAb, chromAb);
    glUniform1f(uGrain, grain);
    glUniform1f(uExposure, exposure);
    
    glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(uInvView, 1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
    glUniformMatrix4fv(uInvProj, 1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, emissionTexture);

    glBindVertexArray(quadVAO);
    // Draw postprocess into pingpong 0 (to extract bloom/composite base)
    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Blur Pass
    bool horizontal = true, first_iteration = true;
    int amount = 4;
    glUseProgram(blurShader);
    for (unsigned int i = 0; i < amount; i++) {
        // Indices 1 and 2 for ping-pong
        int bindIndex = horizontal ? 1 : 2;
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[bindIndex]); 
        glUniform1i(glGetUniformLocation(blurShader, "horizontal"), horizontal);
        
        glActiveTexture(GL_TEXTURE0);
        // On first iteration, bind from scene (0), else from the other pingpong
        int textureIndex = first_iteration ? 0 : (horizontal ? 2 : 1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[textureIndex]); 
        
        glUniform1i(glGetUniformLocation(blurShader, "first_iteration"), first_iteration);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to default framebuffer
    
    // Composite Pass
    glUseProgram(compositeShader);
    glUniform1f(glGetUniformLocation(compositeShader, "uBloomIntensity"), bloom);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[0]); // Scene
    glActiveTexture(GL_TEXTURE1);
    // The last render was in !horizontal pingpong buffer, which is (horizontal ? 2 : 1)
    int finalBlurIndex = horizontal ? 2 : 1;
    glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[finalBlurIndex]); // Blurred Bloom
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glEnable(GL_DEPTH_TEST);
}

void PostProcess::setupQuad() {
    float quad[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), &quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}
