#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormal;
layout (location = 2) out vec3 FragEmission;

in vec2 TexCoords;
in vec4 ParticleColor;
uniform float uTime;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = mix(hash(i + vec2(0.0,0.0)), hash(i + vec2(1.0,0.0)), u.x);
    float b = mix(hash(i + vec2(0.0,1.0)), hash(i + vec2(1.0,1.0)), u.x);
    return mix(a, b, u.y);
}

void main() {
    vec2 coord = TexCoords - vec2(0.5);
    
    float distToCenter = length(coord);
    if (distToCenter >= 0.45) {
        discard;
    }
    
    float n = noise(coord * 12.0 + vec2(uTime * 0.45));
    
    float d0 = length(coord) + n * 0.08 - 0.04;
    float c0 = 1.0 - smoothstep(0.0, 0.42, d0);
    
    float r1 = 0.14;
    float c1 = 1.0 - smoothstep(0.0, 0.28, length(coord - vec2(cos(0.0) * r1, sin(0.0) * r1)) + n * 0.06 - 0.03);
    float c2 = 1.0 - smoothstep(0.0, 0.28, length(coord - vec2(cos(2.094) * r1, sin(2.094) * r1)) + n * 0.06 - 0.03);
    float c3 = 1.0 - smoothstep(0.0, 0.28, length(coord - vec2(cos(4.188) * r1, sin(4.188) * r1)) + n * 0.06 - 0.03);
    
    float smoke = c0 * 0.45 + c1 * 0.22 + c2 * 0.22 + c3 * 0.22;
    
    float falloff = 1.0 - smoothstep(0.18, 0.43, distToCenter);
    smoke *= falloff;
    
    float alpha = ParticleColor.a * smoke;
    if (alpha <= 0.02) {
        discard;
    }
    
    FragColor = vec4(ParticleColor.rgb, smoke * ParticleColor.a * falloff);
    FragNormal = vec3(0.0);
    FragEmission = vec3(0.0);
}
