#version 450 core

#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "smaa.h"


layout (location = 0) out vec4 outColor;


layout(set = 1, binding = 0) uniform sampler2D colorTex;


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
    outColor = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 0.0);
}
