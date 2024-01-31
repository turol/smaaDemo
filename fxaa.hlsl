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
#include "shaderUtils.h"

#define FXAA_PC 1
#define FXAA_HLSL_5 1

#include "fxaa3_11.h"


[[vk::binding(1, 1)]] uniform Texture2D colorTex;
[[vk::binding(2, 1)]] SamplerState LinearSampler;
[[vk::binding(3, 1)]] uniform RWTexture2D<float4> outputImage;


struct VertexOut {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};


VertexOut vertexShader(uint gl_VertexIndex : SV_VertexID)
{
    VertexOut v;
    float2 pos = triangleVertex(gl_VertexIndex, v.texcoord);

#ifndef VULKAN_FLIP
    v.texcoord = flipTexCoord(v.texcoord);
#endif  // VULKAN_FLIP

    v.position = float4(pos, 1.0, 1.0);
    return v;
}


float4 fragmentShader(VertexOut v)
{
    float4 zero = float4(0.0, 0.0, 0.0, 0.0);
    FxaaTex t;
    t.smpl = LinearSampler;
    t.tex  = colorTex;
    return FxaaPixelShader(v.texcoord, zero, t, t, t, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);
}


[numthreads(FXAA_COMPUTE_GROUP_X, FXAA_COMPUTE_GROUP_Y, 1)]
void computeShader(int3 GlobalInvocationID : SV_DispatchThreadID)
{
    // don't write outside image in case its size is not exactly divisible by group size
    if (GlobalInvocationID.x < screenSize.z && GlobalInvocationID.y < screenSize.w) {
        // TODO: rearrange invocations for better cache locality
        float2 texcoord = GlobalInvocationID.xy;
        // account for pixel center TODO: pass precalculated value in UBO to avoid one op
        texcoord += float2(0.5, 0.5);
        texcoord *= screenSize.xy;

        float4 zero = vec4(0.0, 0.0, 0.0, 0.0);
        FxaaTex t;
        t.smpl = LinearSampler;
        t.tex  = colorTex;
        float4 pixel = FxaaPixelShader(texcoord, zero, t, t, t, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);

        outputImage[GlobalInvocationID.xy] = pixel;
    }
}
