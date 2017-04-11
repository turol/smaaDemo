#version 450

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

uniform vec4 screenSize;

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "smaa.h"


out vec4 outColor;

uniform sampler2D blendTex;
uniform sampler2D colorTex;

in vec2 texcoord;
in vec4 offset;

void main(void)
{
    outColor = SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
