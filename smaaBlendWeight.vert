#version 330


uniform vec4 screenSize;


out vec2 texcoord;
out vec2 pixcoord;
out vec4 offset0;
out vec4 offset1;
out vec4 offset2;


#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_3 1
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1


#include "smaa.h"
#include "tribuffer.h"


void main(void)
{
	vec2 pos = triangleVertex(gl_VertexID, texcoord);
	texcoord.y = 1.0 - 1.0 * texcoord.y;

	vec4 offsets[3];
	offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);
	offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);
	offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);
	pixcoord = vec2(0.0, 0.0);
	SMAABlendingWeightCalculationVS(texcoord, pixcoord, offsets);
	offset0 = offsets[0];
	offset1 = offsets[1];
	offset2 = offsets[2];

	gl_Position = vec4(pos, 1.0, 1.0);
}
