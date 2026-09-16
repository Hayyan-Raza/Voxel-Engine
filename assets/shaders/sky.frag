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
uniform float uTime;
uniform int uEnableClouds;
uniform float uCloudDensity;
uniform float uCloudCoverage;
uniform int uRenderDist;
uniform float uCloudSpeed;

// 3D Value Noise for clouds
float hash3(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    
    return mix(mix(mix(hash3(p + vec3(0,0,0)), hash3(p + vec3(1,0,0)), f.x),
                   mix(hash3(p + vec3(0,1,0)), hash3(p + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash3(p + vec3(0,0,1)), hash3(p + vec3(1,0,1)), f.x),
                   mix(hash3(p + vec3(0,1,1)), hash3(p + vec3(1,1,1)), f.x), f.y), f.z);
}

// Fractional Brownian Motion
float fbm(vec3 p) {
    float f = 0.0;
    float w = 0.5;
    for (int i = 0; i < 4; i++) {
        f += w * noise(p);
        p *= 2.02;
        w *= 0.5;
    }
    return f;
}

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



    // Volumetric Clouds
    if (uEnableClouds == 1 && rd.y > 0.0) {
        float cloudHeightMin = 80.0;
        float cloudHeightMax = 180.0; // Taller clouds to be less flat
        
        // Ray intersect with cloud layer
        float tMin = (cloudHeightMin - ro.y) / rd.y;
        float tMax = (cloudHeightMax - ro.y) / rd.y;
        
        if (tMax > 0.0) {
            tMin = max(tMin, 0.0);
            
            // Limit render distance of clouds based on game render distance
            float maxRayDist = float(max(uRenderDist, 2)) * 32.0 * 0.1 * 8.0; 
            if (tMax - tMin > maxRayDist) {
                tMax = tMin + maxRayDist;
            }

            int steps = 10 + clamp(uRenderDist, 2, 10); // Optimize steps for FPS
            float stepSize = (tMax - tMin) / float(steps);
            
            // Dither starting position to hide banding with low step count
            float dither = fract(sin(dot(TexCoords.xy, vec2(12.9898, 78.233))) * 43758.5453);
            float t = tMin + stepSize * dither;
            
            float density = 0.0;
            vec3 p;
            
            // Wind movement
            vec3 wind = vec3(uTime * 2.0 * uCloudSpeed, 0.0, uTime * 1.5 * uCloudSpeed);
            
            for (int i = 0; i < steps; i++) {
                p = ro + rd * t;
                
                // Sample cloud density
                vec3 samplePos = p * 0.02 + wind;
                float d = fbm(samplePos) - uCloudCoverage; // Use coverage slider
                
                // Shape it vertically
                float h = (p.y - cloudHeightMin) / (cloudHeightMax - cloudHeightMin);
                d -= abs(h - 0.5) * 1.0; // Gentler feathering allows them to be taller
                
                if (d > 0.0) {
                    // Accumulate density (remove the 0.1 multiplier so they can get very thick)
                    density += d * stepSize * uCloudDensity;
                    if (density >= 1.0) {
                        density = 1.0;
                        break;
                    }
                }
                t += stepSize;
            }
            
            if (density > 0.0) {
                vec3 cloudColor = mix(vec3(1.0), vec3(0.6, 0.65, 0.7), density); // Self-shadowing
                
                // Sun scattering
                if (uEnableSunMoon == 1) {
                    float scatter = max(0.0, dot(rd, uSunDir));
                    cloudColor += vec3(1.0, 0.9, 0.7) * pow(scatter, 8.0) * 0.5;
                }
                
                // Fade out into distance (atmospheric scattering/fog)
                float fog = exp(-tMin * 0.0025);
                
                col = mix(col, cloudColor, density * fog);
            }
        }
    }

    // Dither to remove color banding
    float dither = fract(sin(dot(TexCoords.xy, vec2(12.9898, 78.233))) * 43758.5453);
    col += (dither - 0.5) / 255.0;

    FragColor = vec4(col, 1.0);
    FragNormal = vec3(0.0);
    FragEmission = vec3(0.0);
}
