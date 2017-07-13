#version 450 core

#include "shaderDefines.h"

#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1


#include "smaa.h"
#include "utils.h"


layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec4 offset0;
layout (location = 2) out vec4 offset1;
layout (location = 3) out vec4 offset2;


void main(void)
{
    vec2 pos = triangleVertex(gl_VertexIndex, texcoord);

#ifndef VULKAN_FLIP
    texcoord = flipTexCoord(texcoord);
#endif  // VULKAN_FLIP

    vec4 offsets[3];
    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);
    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);
    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);
    SMAAEdgeDetectionVS(texcoord, offsets);
    offset0 = offsets[0];
    offset1 = offsets[1];
    offset2 = offsets[2];
    gl_Position = vec4(pos, 1.0, 1.0);

#ifdef VULKAN_FLIP
    gl_Position.y = -gl_Position.y;
#endif  // VULKAN_FLIP
}
