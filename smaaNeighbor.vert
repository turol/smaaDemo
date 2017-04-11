#version 450

#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

uniform vec4 screenSize;

#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1


#include "smaa.h"


in vec2 pos;

out vec2 texcoord;
out vec4 offset;

void main(void)
{
    texcoord = pos * 0.5 + 0.5;
    offset = vec4(0.0, 0.0, 0.0, 0.0);
    SMAANeighborhoodBlendingVS(texcoord, offset);
    gl_Position = vec4(pos, 1.0, 1.0);
}
