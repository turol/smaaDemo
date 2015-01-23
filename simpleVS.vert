#version 330

uniform mat4 viewProj;
uniform vec4 rotationQuat;
uniform vec3 cubePos;

in vec3 position;


void main(void)
{
    // rotate
    // this is quaternion multiplication from glm
    vec3 v = position;
    vec3 uv = cross(rotationQuat.xyz, v);
    vec3 uuv = cross(rotationQuat.xyz, uv);
    uv *= (2.0 * rotationQuat.w);
    uuv *= 2.0;

    vec3 rotatedPos = v + uv + uuv;

    gl_Position = viewProj * vec4(rotatedPos + cubePos, 1.0);
}
