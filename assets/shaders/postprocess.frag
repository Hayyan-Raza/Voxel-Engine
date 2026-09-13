#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D emissionTexture;

uniform float uTime;
uniform vec2 uRes;
uniform float uBloom;
uniform float uChromAb;
uniform float uGrain;
uniform float uExposure;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uInvView;
uniform mat4 uInvProj;

vec3 ACESFilm(vec3 x) {
    float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// Reconstruct View Space Position from Depth
vec3 getViewPos(vec2 uv) {
    float z = texture(depthTexture, uv).r;
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
    vec4 viewSpace = uInvProj * clipSpace;
    return viewSpace.xyz / viewSpace.w;
}

float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = TexCoords;
    
    // Simple SSGI
    vec3 scene = texture(screenTexture, uv).rgb;
    vec3 emission = texture(emissionTexture, uv).rgb;
    
    // Add emission directly to scene
    scene += emission;

    // Simple Bloom
    vec3 bloom = vec3(0.0);
    float bloomOff = 0.002;
    bloom += texture(screenTexture, uv + vec2(bloomOff, bloomOff)).rgb;
    bloom += texture(screenTexture, uv + vec2(-bloomOff, bloomOff)).rgb;
    bloom += texture(screenTexture, uv + vec2(bloomOff, -bloomOff)).rgb;
    bloom += texture(screenTexture, uv + vec2(-bloomOff, -bloomOff)).rgb;
    bloom *= 0.25;
    bloom = max(bloom - 0.7, 0.0) * uBloom; 
    
    scene += bloom;

    // Exposure & Tonemapping
    scene *= uExposure;
    scene = ACESFilm(scene);
    
    // Film Grain
    float fnoise = (fract(sin(dot(uv + uTime, vec2(12.9898,78.233))) * 43758.5453) - 0.5) * uGrain;
    scene += fnoise;

    // Final Gamma
    scene = pow(scene, vec3(1.0/2.2));

    // GI Debug view (temporarily uncomment this locally to see JUST the raytraced lighting)
    // scene = giColor;

    FragColor = vec4(scene, 1.0);
}
