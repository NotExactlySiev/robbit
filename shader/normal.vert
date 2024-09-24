#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 vertColor;
layout(location = 2) in vec3 normal;
layout(location = 3) in uvec2 texData;
layout(location = 4) in vec2 texCoord;

layout(location = 0) out flat vec3 fragColor;
layout(location = 1) out flat uint isTextured;
layout(location = 2) out flat uint textureID;
layout(location = 3) out vec2 fragTexCoord;


layout(set = 0, binding = 0) uniform UB {
    float angle;
    float zoom;
    float ratio;
};

layout(push_constant) uniform PC {
    vec3 trans;
};

void main()
{
    isTextured = texData[0];
    textureID = texData[1];

    float a = 0.1 * angle;
    mat3 rot = mat3(
        cos(a), 0., -sin(a),
        0., 1., 0.,
        sin(a), 0., cos(a)
    );

    float b = radians(-21.);
    mat3 rot2 = mat3(
        1., 0., 0.,
        0., cos(b), sin(b),
        0., -sin(b), cos(b)
    );

    gl_Position = vec4(rot2*rot*(position+trans), 1.0);
    gl_Position.xy *= zoom * zoom;
    gl_Position.x *= -ratio;
    gl_Position.z *= 0.15;
    gl_Position.z += 0.4;
    fragTexCoord = texCoord*256.0;
    fragColor = vertColor.rgb;
}
