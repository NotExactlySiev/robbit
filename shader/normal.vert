#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec4 vertColor;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 texCoord;

layout(location=0) out flat vec3 fragColor;
layout(location=1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    float angle;
    float x, y, z;
};

void main()
{
    float a = 0.1*angle;
    mat3 rot = mat3(
        cos(a), 0., -sin(a),
        0., 1., 0.,
        sin(a), 0., cos(a)
    );

    float b = radians(-25.);
    mat3 rot2 = mat3(
        1., 0., 0.,
        0., cos(b), sin(b),
        0., -sin(b), cos(b)
    );

    vec3 trans = vec3(x, y, z);


    gl_Position = vec4(rot2*rot*(position+trans)*0.05f, 1.0);
    gl_Position.xy *= 80.;
    gl_Position.z *= 0.05;
    gl_Position.z += 0.4;
    //fragTexCoord = gl_Position.xy;
    fragTexCoord = texCoord;
    //fragColor = dot(rot*normal, vec3(0., 0., 1.)) * vertColor.rgb;
    fragColor = vertColor.rgb;

}
