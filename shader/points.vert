#version 450

layout(location=0) in vec3 position;

layout(location=0) out float fragColor;
layout(location=1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    float color;
    vec3 trans;
};

void main() {
    float alpha = radians(30);
    mat4 rotation = mat4(1.0);
    rotation[1][1] = cos(alpha);
    rotation[1][2] = sin(alpha);
    rotation[2][2] = cos(alpha);
    rotation[2][1] = -sin(alpha);
    rotation[3][3] = 1.0;

    //gl_Position = rotation*vec4(position*0.05, 1.0);
    texCoord = position.xy;
    gl_Position = vec4((position + trans*20.f)*20.0f, 1.0);
    
    //fragColor = vec3(color);
    //fragColor = color;
    fragColor = 1.0f;
    gl_PointSize = 4.0f;
}
