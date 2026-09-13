#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;
uniform bool first_iteration;

// 5-tap Gaussian blur weights
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(image, 0);
    vec3 result = vec3(0.0);
    
    if (first_iteration) {
        // Extract brightness threshold during the very first blur pass
        vec3 color = texture(image, TexCoords).rgb;
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        if(brightness > 1.0)
            result = color;
        else
            result = vec3(0.0);
            
        result = result * weight[0];
        
        for(int i = 1; i < 5; ++i) {
            vec3 s1 = texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb;
            vec3 s2 = texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb;
            
            float b1 = dot(s1, vec3(0.2126, 0.7152, 0.0722));
            float b2 = dot(s2, vec3(0.2126, 0.7152, 0.0722));
            
            if (b1 > 1.0) result += s1 * weight[i];
            if (b2 > 1.0) result += s2 * weight[i];
        }
    } else {
        // Normal blur pass
        result = texture(image, TexCoords).rgb * weight[0];
        if(horizontal) {
            for(int i = 1; i < 5; ++i) {
                result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            }
        } else {
            for(int i = 1; i < 5; ++i) {
                result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            }
        }
    }
    
    FragColor = vec4(result, 1.0);
}
