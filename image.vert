#version 450 core

#include "shaderDefines.h"
#include "utils.h"

out vec2 texcoord;

void main(void)
{
    vec2 pos = triangleVertex(gl_VertexID, texcoord);
    gl_Position = vec4(pos, 1.0, 1.0);
}
