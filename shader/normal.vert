#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec4 vertColor;

layout(location=0) out flat vec3 fragColor;
layout(location=1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    float angle;
};

void main()
{
    float a = 0.1*angle;
    mat3 rotation = mat3(
        cos(a), 0., -sin(a),
        0., 1., 0.,
        sin(a), 0., cos(a)
    );

    texCoord = position.xy;
    gl_Position = vec4(rotation*position*1.0f, 1.0);
    gl_Position.xy *= 120.;
    gl_Position.z *= 0.2;
    gl_Position.z += 0.5;
    fragColor = vertColor.rgb;
}
