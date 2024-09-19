#version 450

//layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in float fragColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vec3(fragColor), 1.0);
    //outColor = texture(texSampler, texCoord/5.0) * fragColor;
}
