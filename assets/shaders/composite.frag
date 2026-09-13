#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float uBloomIntensity;

void main() {
    vec3 hdrColor = texture(scene, TexCoords).rgb;      
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    
    // Additive blending for bloom
    hdrColor += bloomColor * uBloomIntensity;
    
    // Simple tone mapping to bring HDR back to LDR
    vec3 result = vec3(1.0) - exp(-hdrColor * 1.0); // exposure = 1.0
    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));
    
    FragColor = vec4(result, 1.0);
}
