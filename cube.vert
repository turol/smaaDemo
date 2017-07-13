#version 450 core

#include "shaderDefines.h"


layout(location = ATTR_POS) in vec3 position;


readonly restrict layout(std430, set = 1, binding = 0) buffer cubeData {
    Cube cubes[];
};


layout(location = 0) flat out int instance;


void main(void)
{
    Cube cube = cubes[gl_InstanceIndex];

    // rotate
    // this is quaternion multiplication from glm
    vec3 v = position;
    vec3 rotationQuat = cube.rotation.xyz;
    float qw = cube.rotation.w;
    vec3 uv = cross(rotationQuat, v);
    vec3 uuv = cross(rotationQuat, uv);
    uv *= (2.0 * qw);
    uuv *= 2.0;
    vec3 rotatedPos = v + uv + uuv;

    gl_Position = viewProj * vec4(rotatedPos + cube.position, 1.0);
    instance = gl_InstanceIndex;

#ifdef VULKAN_FLIP
    gl_Position.y = -gl_Position.y;
#endif
}
