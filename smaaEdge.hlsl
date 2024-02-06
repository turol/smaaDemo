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

#define SMAA_INCLUDE_CS 1

#ifdef VULKAN_FLIP
#define SMAA_FLIP_Y 0
#else  // VULKAN_FLIP
#define SMAA_FLIP_Y 1
#endif  // VULKAN_FLIP


[[vk::binding(1, 0)]] SamplerState LinearSampler;
[[vk::binding(2, 0)]] SamplerState PointSampler;


#include "smaa.h"
#include "shaderUtils.h"


#if EDGEMETHOD == 2

[[vk::binding(3, 0)]] uniform SMAATexture2D(depthTex);

#else  // EDGEMETHOD

[[vk::binding(3, 0)]] uniform SMAATexture2D(colorTex);
#endif  // EDGEMETHOD


#if SMAA_PREDICATION

[[vk::binding(4, 0)]]uniform SMAATexture2D(predicationTex);

#endif  // SMAA_PREDICATION


[[vk::binding(5, 0)]] uniform RWTexture2D<float4> outputImage;


struct VertexOut {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 offset0  : TEXCOORD1;
    float4 offset1  : TEXCOORD2;
    float4 offset2  : TEXCOORD3;
};


VertexOut vertexShader(uint gl_VertexIndex : SV_VertexID)
{
    VertexOut v;
    float2 pos = triangleVertex(gl_VertexIndex, v.texcoord);

#ifndef VULKAN_FLIP
    v.texcoord = flipTexCoord(v.texcoord);
#endif  // VULKAN_FLIP

    float4 offsets[3];
    offsets[0] = float4(0.0, 0.0, 0.0, 0.0);
    offsets[1] = float4(0.0, 0.0, 0.0, 0.0);
    offsets[2] = float4(0.0, 0.0, 0.0, 0.0);
    SMAAEdgeDetectionVS(v.texcoord, offsets);
    v.offset0 = offsets[0];
    v.offset1 = offsets[1];
    v.offset2 = offsets[2];
    v.position = float4(pos, 1.0, 1.0);

    return v;
}


float4 fragmentShader(VertexOut v)
{
    float4 offsets[3];
    offsets[0] = v.offset0;
    offsets[1] = v.offset1;
    offsets[2] = v.offset2;

#if EDGEMETHOD == 0

#if SMAA_PREDICATION

    return float4(SMAAColorEdgeDetectionPS(v.texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);

#else  // SMAA_PREDICATION

    return float4(SMAAColorEdgeDetectionPS(v.texcoord, offsets, colorTex), 0.0, 0.0);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 1

#if SMAA_PREDICATION

    return float4(SMAALumaEdgeDetectionPS(v.texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);

#else  // SMAA_PREDICATION

    return float4(SMAALumaEdgeDetectionPS(v.texcoord, offsets, colorTex), 0.0, 0.0);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 2

    return float4(SMAADepthEdgeDetectionPS(v.texcoord, offsets, depthTex), 0.0, 0.0);

#else

#error Bad EDGEMETHOD

#endif

}


[numthreads(SMAA_EDGES_COMPUTE_GROUP_X, SMAA_EDGES_COMPUTE_GROUP_Y, 1)]
void computeShader(int3 GlobalInvocationID : SV_DispatchThreadID)
{
    // TODO: rearrange invocations for better cache locality
    int2 coord = int2(GlobalInvocationID.xy);

    // don't write outside image in case its size is not exactly divisible by group size
    if (coord.x < screenSize.z && coord.y < screenSize.w) {
        float2 texcoord = coord.xy;
        // account for pixel center TODO: pass precalculated value in UBO to avoid one op
        texcoord += float2(0.5, 0.5);
        texcoord *= screenSize.xy;

#if EDGEMETHOD == 0

#if SMAA_PREDICATION

        float2 pixel = SMAAColorEdgeDetectionCS(texcoord, colorTex, predicationTex);

#else  // SMAA_PREDICATION

        float2 pixel = SMAAColorEdgeDetectionCS(texcoord, colorTex);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 1

#if SMAA_PREDICATION

        float2 pixel = SMAALumaEdgeDetectionCS(texcoord, colorTex, predicationTex);

#else  // SMAA_PREDICATION

        float2 pixel = SMAALumaEdgeDetectionCS(texcoord, colorTex);

#endif  // SMAA_PREDICATION

#elif EDGEMETHOD == 2

        float2 pixel = SMAADepthEdgeDetectionCS(texcoord, depthTex);

#else

#error Bad EDGEMETHOD

#endif

        outputImage[coord] = float4(pixel, 0.0, 0.0);
    }
}
