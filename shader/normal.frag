#version 450

layout(set = 0, binding = 1) uniform sampler texSampler;
layout(set = 0, binding = 2) uniform texture2D textures[64];

layout(location = 0) in flat vec3 fragColor;
layout(location = 1) in flat uint isTextured;
layout(location = 2) in flat uint textureID;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

vec3 colors[8] = {
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0),
    vec3(0.5, 0.5, 0.5),
    vec3(1.0, 1.0, 1.0),
};

void main() {
    if (isTextured == 1) {
        outColor = texture(sampler2D(textures[textureID], texSampler), texCoord);
        if (outColor.rgb == vec3(0.0, 0.0, 0.0)) {
            discard;
        }
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}
