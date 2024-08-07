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


#define HLSL
#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_HLSL_4_1 1
#define SMAA_HLSL_NO_SAMPLERS 1

#define SMAA_REPROJECTION_WEIGHT_SCALE reprojWeigthScale

#ifdef VULKAN_FLIP
#define SMAA_FLIP_Y 0
#else  // VULKAN_FLIP
#define SMAA_FLIP_Y 1
#endif  // VULKAN_FLIP


[[vk::binding(1, 0)]] SamplerState LinearSampler;
[[vk::binding(2, 0)]] SamplerState PointSampler;


#include "smaa.h"
#include "shaderUtils.h"


[[vk::binding(3, 0)]] uniform SMAATexture2D(currentTex);
[[vk::binding(4, 0)]] uniform SMAATexture2D(previousTex);
#if SMAA_REPROJECTION
[[vk::binding(5, 0)]] uniform SMAATexture2D(velocityTex);
#endif  // SMAA_REPROJECTION


struct VertexOut {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};


VertexOut vertexShader(uint gl_VertexIndex : SV_VertexID)
{
    VertexOut v;
    vec2 pos = triangleVertex(gl_VertexIndex, v.texcoord);

#ifndef VULKAN_FLIP
    v.texcoord = flipTexCoord(v.texcoord);
#endif  // VULKAN_FLIP

    v.position = vec4(pos, 1.0, 1.0);
    return v;
}


float4 fragmentShader(VertexOut v)
{
#if SMAA_REPROJECTION
    return SMAAResolvePS(v.texcoord, currentTex, previousTex, velocityTex);
#else  // SMAA_REPROJECTION
    return SMAAResolvePS(v.texcoord, currentTex, previousTex);
#endif  // SMAA_REPROJECTION
}
