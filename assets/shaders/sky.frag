#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormal;
layout (location = 2) out vec3 FragEmission;

in vec2 TexCoords;

uniform mat4 invProj;
uniform mat4 invView;
uniform vec3 uSunDir;
uniform vec3 uSkyColor;

uniform int uEnableSunMoon;

uniform vec3 uCameraPos;

float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }



void main() {
    vec4 target = invProj * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize((invView * vec4(target.xyz / target.w, 0.0)).xyz);
    vec3 ro = uCameraPos;

    vec3 col = uSkyColor;

    // Sun
    float sunFactor = max(0.0, dot(rd, uSunDir));
    float sun = 0.0;
    if (uEnableSunMoon == 1) {
        // Crisp sun disk + slight outer glow
        sun = smoothstep(0.999, 0.9995, sunFactor) * 2.0 + pow(sunFactor, 200.0) * 0.3;
        col += vec3(1.0, 0.9, 0.7) * sun;
    }

    // Moon
    float moonFactor = max(0.0, dot(rd, -uSunDir));
    float moon = 0.0;
    if (uEnableSunMoon == 1) {
        // Crisp moon disk + slight outer glow
        moon = smoothstep(0.9995, 0.9998, moonFactor) * 1.5 + pow(moonFactor, 300.0) * 0.2;
        col += vec3(0.8, 0.9, 1.0) * moon;
    }



    // Dither to remove color banding
    float dither = fract(sin(dot(TexCoords.xy, vec2(12.9898, 78.233))) * 43758.5453);
    col += (dither - 0.5) / 255.0;

    FragColor = vec4(col, 1.0);
    FragNormal = vec3(0.0);
    FragEmission = vec3(0.0);
}
