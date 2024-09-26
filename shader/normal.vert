#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 vertColor;
layout(location = 2) in vec3 normal;
layout(location = 3) in uvec2 texData;
layout(location = 4) in vec2 texCoord;
layout(location = 5) in vec4 texWindow;


layout(location = 0) out flat vec3 fragColor;
layout(location = 1) out flat uint isTextured;
layout(location = 2) out flat uint textureID;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out flat vec2 fragTexOffset;
layout(location = 5) out flat vec2 fragTexSize;


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
    //texWindow *= 256.0;
    fragTexOffset = 256.0 * texWindow.xy;
    fragTexSize = 256.0 * texWindow.zw;
    // TODO: move matrix generation to the application

    // write the matrices the way you normally do, just multiply in the other direction

    mat4 squeeze = mat4(
        1.0,    0.0,    0.0,    0.0,
        0.0,    1.0,    0.0,    0.0,
        0.0,    0.0,    0.5,    0.5,
        0.0,    0.0,    0.0,    1.0
    );

    mat4 tranmat = mat4(
        1.0,    0.0,    0.0,    trans.x,
        0.0,    1.0,    0.0,    trans.y,
        0.0,    0.0,    1.0,    trans.z,
        0.0,    0.0,    0.0,    1.0
    );

    float s = zoom * zoom;
    mat4 scale = mat4(
        s,      0.0,    0.0,    0.0,
        0.0,    s,      0.0,    0.0,
        0.0,    0.0,    1.0,    0.0,
        0.0,    0.0,    0.0,    1.0
    );

    mat4 ratiom = mat4(
        ratio,  0.0,    0.0,    0.0,
        0.0,    1.0,    0.0,    0.0,
        0.0,    0.0,    1.0,    0.0,
        0.0,    0.0,    0.0,    1.0
    );

    float a = 0.1 * angle;
    mat4 rot = mat4(
        cos(a), 0.0,    sin(a), 0.0,
        0.0,    1.0,    0.0,    0.0,
        -sin(a),0.0,    cos(a), 0.0,
        0.0,    0.0,    0.0,    1.0
    );

    float b = radians(21.);
    mat4 rot2 = mat4(
        1.0,    0.0,    0.0,    0.0,
        0.0,    cos(b), -sin(b),0.0,
        0.0,    sin(b), cos(b), 0.0,
        0.0,    0.0,    0.0,    1.0
    );

    mat4 matr = (tranmat) * (rot * rot2) * (scale * ratiom * squeeze);
    gl_Position = vec4(position, 1.0) * matr;
    fragTexCoord = texCoord;
    fragColor = vertColor.rgb;
}
