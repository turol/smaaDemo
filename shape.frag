/*
Copyright (c) 2015-2024 Alternative Games Ltd / Turo Lamminen

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

readonly restrict layout(std430, set = 1, binding = 1) buffer shapeData {
    Shape shapes[];
};


layout(location = 0) flat in int instance;
layout(location = 1) in vec3 currPos;
layout(location = 2) in vec3 prevPos;


layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outVelocity;


void main(void)
{
    Shape shape = shapes[instance];

    vec4 color = vec4(shape.color, 0.0);

    color.w = dot(color.xyz, vec3(0.299, 0.587, 0.114));
    outColor = color;
    outVelocity = vec2(0.0, 0.0);

    // w stored in z
    vec2 curr   = currPos.xy / currPos.z;
    vec2 prev   = prevPos.xy / prevPos.z;
    outVelocity = curr - prev;
}
