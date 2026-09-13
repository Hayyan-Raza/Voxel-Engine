#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace UI {
namespace Toolbar {

    extern GLuint handTexture;
    extern GLuint closedHandTexture;
    extern GLuint hammerIcon;
    extern GLuint ak47Icon;
    extern GLuint dynamiteIcon;
    extern GLuint glockIcon;
    extern GLuint shotgunIcon;
    extern GLuint placerIcon;

    void drawCrosshair(float fbW, float fbH);
    void drawWeaponBar(float fbW, float fbH);

}
}
