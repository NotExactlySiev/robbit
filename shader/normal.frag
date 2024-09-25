#version 450

layout(set = 0, binding = 1) uniform sampler texSampler;
// keep this count in sync with common.h
layout(set = 0, binding = 2) uniform texture2D textures[64];

layout(location = 0) in flat vec3 fragColor;
layout(location = 1) in flat uint isTextured;
layout(location = 2) in flat uint textureID;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in flat ivec2 texOffset;

layout(location = 0) out vec4 outColor;

void main() {
    if (isTextured == 1) {
        //outColor = texture(sampler2D(textures[textureID], texSampler), texCoord);
        outColor = texture(
            sampler2D(textures[0], texSampler),
            vec2(fract(texCoord.x)/1.0, (fract(texCoord.y*4.0)/4.0)/4.0) + vec2(texOffset)/256.0
        );
        if (outColor.rgb == vec3(0.0, 0.0, 0.0)) {
            discard;
        }
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}
