#version 450

#include "shaderDefines.h"


layout(location = ATTR_POS) in vec3 position;

readonly restrict layout(std430, binding = 0) buffer cubeData {
    Cube cubes[];
};

out vec3 colorFrag;

void main(void)
{
    Cube cube = cubes[gl_InstanceID];

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
    vec3 color = vec3(float((cube.color & 0x000000FF) >>  0) / 255.0
                    , float((cube.color & 0x0000FF00) >>  8) / 255.0
                    , float((cube.color & 0x00FF0000) >> 16) / 255.0);

    colorFrag = color;
}
