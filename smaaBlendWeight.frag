#version 450

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

uniform vec4 screenSize;

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "smaa.h"


out vec4 outColor;

uniform sampler2D edgesTex;
uniform sampler2D areaTex;
uniform sampler2D searchTex;

in vec2 texcoord;
in vec2 pixcoord;
in vec4 offset0;
in vec4 offset1;
in vec4 offset2;

void main(void)
{
    vec4 offsets[3];
    offsets[0] = offset0;
    offsets[1] = offset1;
    offsets[2] = offset2;
    outColor = SMAABlendingWeightCalculationPS(texcoord, pixcoord, offsets, edgesTex, areaTex, searchTex, vec4(0.0, 0.0, 0.0, 0.0));
}
