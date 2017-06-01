#version 450 core

#include "shaderDefines.h"
#include "utils.h"


layout (location = 0) out vec2 texcoord;


void main(void)
{
    vec2 temp;
    vec2 pos = triangleVertex(gl_VertexIndex, temp);
    texcoord = flipTexCoord(temp);
    gl_Position = vec4(pos, 1.0, 1.0);
}
