#version 330
#extension GL_ARB_gpu_shader5 : enable

#define FXAA_PC 1
#define FXAA_GLSL_130 1
#define FXAA_QUALITY__PRESET 39

#include "utils.h"

out vec2 texcoord;

void main(void)
{
	vec2 pos = triangleVertex(gl_VertexID, texcoord);
    texcoord = flipTexCoord(texcoord);

	gl_Position = vec4(pos, 1.0, 1.0);
}
