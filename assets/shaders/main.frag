#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormal;
layout (location = 2) out vec3 FragEmission;
in vec3 Normal;
in vec3 FragPos;
in vec3 VertColor;
in vec3 LocalPos;
in float VertAO;
in vec4 FragPosLightSpace;
in float Emissive;
in float VertLight;
uniform vec4 voxelColor;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform vec3 uSkyColor;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec2 screenRes;
uniform float shadowObscurance;
uniform float neighborAO;
uniform float fogDensity;
uniform float iTime;
uniform float uAOScale;
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;

uniform vec3 uWaterShallowColor;
uniform vec3 uWaterDeepColor;
uniform float uWaterSkyBlend;
uniform float uWaterWaveSpeed;

// Simple 2D noise for natural wave patterns
float hash2d(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise2d(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float res = mix(
        mix(hash2d(i + vec2(0.0, 0.0)), hash2d(i + vec2(1.0, 0.0)), f.x),
        mix(hash2d(i + vec2(0.0, 1.0)), hash2d(i + vec2(1.0, 1.0)), f.x), f.y);
    return res * 2.0 - 1.0;
}

float hash3d(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z + p.x * p.y);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float bias = max(0.02 * (1.0 - dot(normal, lightDir)), 0.005);

    // Fast Soft Shadows (4 samples for performance)
    vec2 poissonDisk[4] = vec2[]( 
       vec2( -0.94201624, -0.39906216 ), vec2( 0.94558609, -0.76890725 ), 
       vec2( -0.094184101, -0.92938870 ), vec2( 0.34495938, 0.29387760 )
    );
    
    float shadow = 0.0;
    float filterRadius = 4.0 / 2048.0; 
    for(int i = 0; i < 4; i++) {
        float pcfDepth = texture(shadowMap, projCoords.xy + poissonDisk[i] * filterRadius).r; 
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
    }
    shadow /= 4.0;
    
    // Keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        shadow = 0.0;
        
    // Softly fade the shadow out at the edges of the shadow map to prevent hard square boundaries
    float fadeX = smoothstep(0.0, 0.1, projCoords.x) * (1.0 - smoothstep(0.9, 1.0, projCoords.x));
    float fadeY = smoothstep(0.0, 0.1, projCoords.y) * (1.0 - smoothstep(0.9, 1.0, projCoords.y));
    shadow *= fadeX * fadeY;
        
    return shadow;
}

void main() {
    vec3 norm    = normalize(Normal);
    vec3 lDir    = normalize(lightDir);

    vec3 baseColor = VertColor * voxelColor.rgb;

    // Apply depth darkening (simulating ambient occlusion from terrain depth)
    // We don't darken exact gray (Type 15: 0.5, 0.5, 0.5) to keep deep caves visible
    float depthFactor = 0.65 + (LocalPos.y / 64.0) * 0.35;
    if (abs(VertColor.r - 0.5) > 0.01 || abs(VertColor.g - 0.5) > 0.01 || abs(VertColor.b - 0.5) > 0.01) {
        baseColor *= depthFactor;
    }

    // Procedural noise removed as per user request to keep solid colors

    // Directional light
    float shadow = ShadowCalculation(FragPosLightSpace, norm, lDir);
    float diff = max(dot(norm, lDir), 0.0);
    
    // Scale by voxel lighting
    // VertLight is 0..15 from the attribute. Map it to 0.0 .. 1.0
    float blockLight = clamp(VertLight / 15.0, 0.0, 1.0);
    // Let's add blockLight as a baseline to ambient
    vec3 effectiveAmbient = uAmbientColor + vec3(blockLight * 0.8);
    
    vec3 diffuse = uDiffuseColor * diff * baseColor * (1.0 - shadow);
    vec3 result = (effectiveAmbient * baseColor) + diffuse;
    
    // Emissive boost
    if (Emissive > 0.0) {
        result = baseColor * 3.0; // Glow (HDR but not extreme, so GI bounce stays visible)
        FragEmission = result; 
    } else {
        FragEmission = vec3(0.0);
    }
    
    // Baked Voxel Ambient Occlusion scaled by uAOScale
    float aoFactor = mix(1.0, VertAO, uAOScale);
    if (Emissive > 0.0) aoFactor = 1.0;
    result *= aoFactor;

    // Dynamic Point Light (held glowing voxel)
    if (dot(uPointLightColor, uPointLightColor) > 0.01) {
        vec3 pLightDir = uPointLightPos - FragPos;
        float dist = length(pLightDir);
        pLightDir = normalize(pLightDir);
        float pDiff = max(dot(norm, pLightDir), 0.0);
        float attenuation = 1.0 / (1.0 + 0.5 * dist + 1.0 * dist * dist);
        
        vec3 pLightContrib = pDiff * uPointLightColor * attenuation * baseColor;
        result += pLightContrib;
    }

    // Atmospheric fog (exponential squared for a solid horizon)
    float dist      = length(viewPos - FragPos);
    float fogFactor = 1.0 - exp(-pow(dist * fogDensity * 1.5, 2.5));
    result = mix(result, uSkyColor, clamp(fogFactor, 0.0, 1.0));

    // Transparent water look
    float alpha = voxelColor.a;
    if (abs(VertColor.b - 0.85) < 0.01 && abs(VertColor.r - 0.15) < 0.01) {
        // "much smaller normal voxelsize": Use a finer quantization grid for the ripples
        // Physical voxelSize is 0.5 (multiplier 2.0). We use 8.0 to make ripples look like tiny 0.125 voxels
        vec2 qPos = floor(LocalPos.xz * 8.0) / 8.0;
        
        // More natural noise-based wave derivatives
        float t = iTime * uWaterWaveSpeed;
        
        // Base low-frequency wave
        float dx = noise2d(qPos * 0.5 + vec2(t * 0.5, t * 0.2)) * 1.5;
        float dy = noise2d(qPos * 0.5 + vec2(-t * 0.3, t * 0.4)) * 1.5;
        
        // High-frequency detail
        dx += noise2d(qPos * 1.5 - vec2(t * 1.2, 0.0)) * 0.7;
        dy += noise2d(qPos * 1.5 + vec2(0.0, t * 1.1)) * 0.7;
        
        // Quantize the slopes to create "voxel shaped" flat reflective steps
        float qdx = floor(dx * 3.0) / 3.0;
        float qdy = floor(dy * 3.0) / 3.0;
        
        vec3 waterNorm = normalize(norm + vec3(qdx * 0.15, 0.0, qdy * 0.15));
        
        // Only apply waves on the top surface
        if (norm.y > 0.9) {
            norm = waterNorm;
        }

        // Specular highlight from directional light (sun) using physical view dir
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfDir = normalize(lDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), 64.0); 

        // Fresnel effect for transparency and reflection
        float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0);
        
        // Make water less transparent so colors are highly visible
        alpha = mix(0.85, 1.0, fresnel);
        
        // Blend shallow and deep colors based on view angle (Fresnel)
        vec3 waterBase = mix(uWaterDeepColor, uWaterShallowColor, fresnel);
        
        // Blend in sky reflection but keep the base color strong
        result = mix(waterBase, uSkyColor, fresnel * uWaterSkyBlend);
        
        // Add specular highlight on top
        result += vec3(1.0, 1.0, 1.0) * spec * 0.8;
    }

    FragColor = vec4(result, alpha);
    FragNormal = norm * 0.5 + 0.5;
}
