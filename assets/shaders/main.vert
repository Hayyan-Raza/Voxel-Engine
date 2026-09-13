#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in float aAO;
layout (location = 4) in float aEmissive;
layout (location = 5) in float aLight;

out vec3 FragPos;
out vec3 Normal;
out vec3 VertColor;
out vec3 LocalPos;
out float VertAO;
out vec4 FragPosLightSpace;
out float Emissive;
out float VertLight;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float iTime;
uniform float uVegSwaySpeed;
uniform float uVegSwayIntensity;

void main() {
    vec3 animatedPos = aPos;
    vec3 worldPos = vec3(model * vec4(aPos, 1.0));
    
    // Decode vegetation sway from ambient occlusion value
    float rawAO = aAO;
    float swayFactor = 0.0;
    bool isVegetation = false;
    bool isTree = false;
    
    if (rawAO < -0.1) {
        rawAO = -rawAO;
        rawAO -= 10.0; // Remove base offset
        
        float typeFloat = floor(rawAO / 1000.0);
        int type = int(typeFloat + 0.5);
        
        float gY = floor((rawAO - typeFloat * 1000.0) / 10.0);
        
        // Recover the original AO value (between 0 and 1)
        // Ensure we don't precision error our way out of 1.0
        float aoVal = rawAO - typeFloat * 1000.0 - gY * 10.0;
        if (aoVal > 0.95) aoVal = 1.0;
        
        // Calculate height above ground for this vertex
        float height = max(0.0, aPos.y - gY * 0.05);
        
        if (type == 23 || type == 24 || type == 27 || type == 28) {
            isTree = false;
            swayFactor = 0.0; // Tree sway disabled by user request
        } else {
            isVegetation = true;
            if (type == 5) swayFactor = height * 0.2; // stem
            else if (type == 11 || type == 26) swayFactor = height * 0.4; // flower head & center
            else if (type == 21) swayFactor = height * 0.3; // bush leaves
            else if (type == 22) swayFactor = height * 0.15; // bush stem
            else swayFactor = height * 0.3; // grass (9, 10, 12)
        }
        
        rawAO = aoVal;
    }
    
    if (isVegetation) {
        float windX = sin(iTime * uVegSwaySpeed * 1.33 + worldPos.x * 0.2 + worldPos.z * 0.2);
        float windZ = cos(iTime * uVegSwaySpeed * 1.20 + worldPos.x * 0.2 + worldPos.z * 0.2);
        animatedPos.x += windX * uVegSwayIntensity * swayFactor;
        animatedPos.z += windZ * uVegSwayIntensity * swayFactor;
    } else if (isTree) {
        // Different, more subtle wave for tall trees
        float windX = sin(iTime * uVegSwaySpeed * 0.6 + worldPos.x * 0.1 + worldPos.z * 0.1 + worldPos.y * 0.2);
        float windZ = cos(iTime * uVegSwaySpeed * 0.7 + worldPos.x * 0.1 + worldPos.z * 0.1 + worldPos.y * 0.2);
        animatedPos.x += windX * (uVegSwayIntensity * 0.2) * swayFactor;
        animatedPos.z += windZ * (uVegSwayIntensity * 0.2) * swayFactor;
    }

    // Trick 1: The Liquid Look (Voxel Step Ripples)
    bool isWater = (abs(aColor.b - 0.85) < 0.01 && abs(aColor.r - 0.15) < 0.01);
    if (isWater) {
        // Removed physical vertex displacement to prevent gaps between voxels
        // Ripple effects will be handled entirely in the fragment shader
    }

    FragPos   = vec3(model * vec4(animatedPos, 1.0));
    Normal    = mat3(transpose(inverse(model))) * aNormal;
    VertColor = aColor;
    LocalPos  = animatedPos;
    VertAO    = rawAO;
    Emissive  = aEmissive;
    VertLight = aLight;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
