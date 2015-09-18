#version 330

uniform mat4 viewProj;

in vec3 rotationQuat;
in vec3 cubePos;
in vec3 color;
in vec3 position;


out vec3 colorFrag;


void main(void)
{
    // our quaternions are normalized and have w > 0.0
    float qw = sqrt(1.0 - dot(rotationQuat, rotationQuat));
    // rotate
    // this is quaternion multiplication from glm
    vec3 v = position;
    vec3 uv = cross(rotationQuat, v);
    vec3 uuv = cross(rotationQuat, uv);
    uv *= (2.0 * qw);
    uuv *= 2.0;

    vec3 rotatedPos = v + uv + uuv;

    gl_Position = viewProj * vec4(rotatedPos + cubePos, 1.0);
    colorFrag = color;
}
