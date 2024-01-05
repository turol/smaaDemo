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


[[vk::binding(1, 1)]] StructuredBuffer <Shape> shapes;


struct VertexIn {
    float3  position : POSITION0;
    uint    instance : SV_InstanceID;
};


struct VertexOut {
    float4               position  : SV_POSITION;
    nointerpolation int  instance  : TEXCOORD0;
    float3               currPos   : TEXCOORD1;
    float3               prevPos   : TEXCOORD2;
};


struct FragmentOut {
    float4  color    : COLOR0;
    float2  velocity : COLOR1;
};


VertexOut vertexShader(VertexIn vin)
{
    Shape shape = shapes[vin.instance];

    // rotate
    // this is quaternion multiplication from glm
    float3 v = vin.position;
    float3 rotationQuat = shape.rotation.xyz;
    float qw = shape.rotation.w;
    float3 uv = cross(rotationQuat, v);
    float3 uuv = cross(rotationQuat, uv);
    uv *= (2.0 * qw);
    uuv *= 2.0;
    float3 rotatedPos = v + uv + uuv;
    float4 worldPos = float4(rotatedPos + shape.position, 1.0);

    VertexOut vout;
    vout.position = viewProj * worldPos;
    vout.currPos  = vout.position.xyw;
    vout.prevPos  = (prevViewProj * worldPos).xyw;
    // Positions in projection space are in [-1, 1] range, while texture
    // coordinates are in [0, 1] range. So, we divide by 2 to get velocities in
    // the scale (and flip the y axis):
    vout.currPos.xy *= float2(0.5, -0.5);
    vout.prevPos.xy *= float2(0.5, -0.5);

    vout.instance = vin.instance;
    return vout;
}


FragmentOut fragmentShader(VertexOut v)
{
    Shape shape = shapes[v.instance];

    FragmentOut f;
    float4 color = float4(shape.color, 0.0);

    color.w = dot(color.xyz, float3(0.299, 0.587, 0.114));
    f.color = color;

    // w stored in z
    float2 curr = v.currPos.xy / v.currPos.z;
    float2 prev = v.prevPos.xy / v.prevPos.z;
    f.velocity  = curr - prev;

    return f;
}
