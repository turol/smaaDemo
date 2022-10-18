/*
Copyright (c) 2015-2022 Alternative Games Ltd / Turo Lamminen

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

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0

#ifdef VULKAN_FLIP
#define SMAA_FLIP_Y 0
#endif

#ifndef EDGEMETHOD
#define EDGEMETHOD 0
#endif


#define SMAA_PREDICATION_THRESHOLD  predicationThreshold
#define SMAA_PREDICATION_SCALE      predicationScale
#define SMAA_PREDICATION_STRENGTH   predicationStrength


#define LinearSampler linearSampler
#define PointSampler  nearestSampler


#include "smaa.h"


layout (location = 0) out vec4 outColor;


#if EDGEMETHOD == 2

layout(set = 1, binding = 1) uniform SMAATexture2D(depthTex);

#else  // EDGEMETHOD

layout(set = 1, binding = 1) uniform SMAATexture2D(colorTex);

#endif  // EDGEMETHOD


#if SMAA_PREDICATION

layout(set = 1, binding = 2) uniform SMAATexture2D(predicationTex);

#endif  // SMAA_PREDICATION


layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 offset0;
layout (location = 2) in vec4 offset1;
layout (location = 3) in vec4 offset2;


void main(void)
{
    vec4 offsets[3];
    offsets[0] = offset0;
    offsets[1] = offset1;
    offsets[2] = offset2;

#if EDGEMETHOD == 0

#if SMAA_PREDICATION

	outColor = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);

#else  // SMAA_PREDICATION

	outColor = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 0.0);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 1

#if SMAA_PREDICATION

    outColor = vec4(SMAALumaEdgeDetectionPS(texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);

#else  // SMAA_PREDICATION

    outColor = vec4(SMAALumaEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 0.0);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 2

    outColor = vec4(SMAADepthEdgeDetectionPS(texcoord, offsets, depthTex), 0.0, 0.0);

#else

#error Bad EDGEMETHOD

#endif

}
