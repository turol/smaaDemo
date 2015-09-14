#version 330
#extension GL_ARB_gpu_shader5 : enable


uniform sampler2D blendTex;
uniform sampler2D colorTex;
uniform vec4 screenSize;


in vec2 texcoord;
in vec4 offset;


#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_3 1
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0


#include "utils.h"
#include "smaa.h"


void main(void)
{
	gl_FragColor = SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
