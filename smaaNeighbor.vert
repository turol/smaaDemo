#version 450 core

#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1


#include "smaa.h"
#include "utils.h"


layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec4 offset;

void main(void)
{
    vec2 pos = triangleVertex(gl_VertexIndex, texcoord);

#ifndef VULKAN_FLIP
    texcoord = flipTexCoord(texcoord);
#endif  // VULKAN_FLIP

    offset = vec4(0.0, 0.0, 0.0, 0.0);
    SMAANeighborhoodBlendingVS(texcoord, offset);
    gl_Position = vec4(pos, 1.0, 1.0);

#ifdef VULKAN_FLIP
    gl_Position.y = -gl_Position.y;
#endif
}
