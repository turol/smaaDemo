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

#define FXAA_PC 1
#define FXAA_GLSL_130 1


#include "fxaa3_11.h"


layout(set = 0, binding = 1) uniform sampler2D colorTex;

layout (location = 0) in vec2 texcoord;

layout (location = 0) out vec4 outColor;

void main(void)
{
    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);
    outColor = FxaaPixelShader(texcoord, zero, colorTex, colorTex, colorTex, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);
}
