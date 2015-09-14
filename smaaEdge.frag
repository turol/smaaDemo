#version 330
#extension GL_ARB_gpu_shader5 : enable


uniform sampler2D color;
uniform vec4 screenSize;


in vec2 texcoord;
in vec4 offset0;
in vec4 offset1;
in vec4 offset2;


#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_3 1
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "utils.h"
#include "smaa.h"


void main(void)
{
	vec4 offsets[3];
	offsets[0] = offset0;
	offsets[1] = offset1;
	offsets[2] = offset2;
	gl_FragColor = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, color), 0.0, 0.0);
}
