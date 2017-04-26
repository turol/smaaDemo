#version 450 core

#include "shaderDefines.h"

readonly restrict layout(std430, binding = 0) buffer cubeData {
    Cube cubes[];
};


flat in int instance;


layout (location = 0) out vec4 outColor;


void main(void)
{
    Cube cube = cubes[instance];

    vec4 color = vec4(float((cube.color & 0x000000FF) >>  0) / 255.0
                    , float((cube.color & 0x0000FF00) >>  8) / 255.0
                    , float((cube.color & 0x00FF0000) >> 16) / 255.0
                    , 0.0);

    color.w = dot(color.xyz, vec3(0.299, 0.587, 0.114));
    outColor = color;
}
