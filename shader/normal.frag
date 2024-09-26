#version 450

layout(set = 0, binding = 1) uniform sampler texSampler;
// keep this count in sync with common.h
layout(set = 0, binding = 2) uniform texture2D textures[2];

layout(location = 0) in flat vec3 fragColor;
layout(location = 1) in flat uint isTextured;
layout(location = 2) in flat uint textureID;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in flat vec2 texOffset;
layout(location = 5) in flat vec2 texSize;


layout(location = 0) out vec4 outColor;

void main() {
    if (isTextured == 1) {
        vec2 windowCoord = fract(texCoord / texSize) * texSize + texOffset;
        outColor = texture(sampler2D(textures[textureID], texSampler), windowCoord);
        if (outColor.rgb == vec3(0.0, 0.0, 0.0)) {
            discard;
        }
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}
