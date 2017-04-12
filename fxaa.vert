#version 450

#include "shaderDefines.h"
#include "utils.h"

#define FXAA_PC 1
#define FXAA_GLSL_130 1

out vec2 texcoord;

void main(void)
{
    vec2 pos = triangleVertex(gl_VertexID, texcoord);
    texcoord = flipTexCoord(texcoord);
    gl_Position = vec4(pos, 1.0, 1.0);
}
