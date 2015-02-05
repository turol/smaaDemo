#version 330


#include "tribuffer.h"


out vec2 texcoord;


void main(void)
{
	vec2 pos = triangleVertex(gl_VertexID, texcoord);
	texcoord.y = 1.0 - 1.0 * texcoord.y;

	gl_Position = vec4(pos, 1.0, 1.0);
}
