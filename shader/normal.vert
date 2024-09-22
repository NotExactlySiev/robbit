#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec4 vertColor;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 texCoord;

layout(location=0) out flat vec3 fragColor;
layout(location=1) out vec2 fragTexCoord;


layout(set = 0, binding = 0) uniform UB {
    float angle;
};

layout(push_constant) uniform PC {
    vec3 trans;
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

    float ar = 720.0 / 1280.0;
    gl_Position = vec4(rot2*rot*(position+trans), 1.0);
    gl_Position.xy *= 5.;
    gl_Position.x *= ar;
    gl_Position.z *= 0.05;
    gl_Position.z += 0.4;
    //fragTexCoord = gl_Position.xy;
    fragTexCoord = texCoord;
    //fragColor = dot(rot*normal, vec3(0., 0., 1.)) * vertColor.rgb;
    fragColor = vertColor.rgb;

}
