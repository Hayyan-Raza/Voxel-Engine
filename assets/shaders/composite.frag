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
    
    FragColor = vec4(hdrColor, 1.0);
}
