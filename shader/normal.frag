#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in flat vec3 fragColor;
layout(location = 1) in flat uint isTextured;
layout(location = 2) in flat uint textureID;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

void main() {
    if (isTextured == 1) {
        outColor = texture(texSampler, texCoord);
        if (outColor.rgb == vec3(0.0, 0.0, 0.0)) {
            discard;
        }
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}
