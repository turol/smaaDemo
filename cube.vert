/*
Copyright (c) 2015-2018 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


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
    vec4 worldPos = vec4(rotatedPos + cube.position, 1.0);

    gl_Position = viewProj * worldPos;
    instance = gl_InstanceIndex;

#ifdef VULKAN_FLIP
    gl_Position.y = -gl_Position.y;
#endif
}
