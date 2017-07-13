#version 450 core

#include "shaderDefines.h"
#include "utils.h"

layout (location = 0) out vec2 texcoord;

void main(void)
{
    vec2 pos = triangleVertex(gl_VertexIndex, texcoord);
    gl_Position = vec4(pos, 1.0, 1.0);

#ifdef VULKAN_FLIP
    gl_Position.y = -gl_Position.y;
#endif
}
