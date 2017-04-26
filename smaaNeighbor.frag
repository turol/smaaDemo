#version 450 core

#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "smaa.h"


layout (location = 0) out vec4 outColor;

layout(binding = TEXUNIT_BLEND) uniform sampler2D blendTex;
layout(binding = TEXUNIT_COLOR) uniform sampler2D colorTex;

in vec2 texcoord;
in vec4 offset;

void main(void)
{
    outColor = SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
