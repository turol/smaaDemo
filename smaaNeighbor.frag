#version 330
#extension GL_ARB_gpu_shader5 : enable
#extension GL_ARB_texture_gather : enable


#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_3 1
#define SMAA_PRESET_ULTRA 1

uniform vec4 screenSize;

#include "utils.h"

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0

#include "smaa.h"


uniform sampler2D blendTex;
uniform sampler2D colorTex;


in vec2 texcoord;
in vec4 offset;


void main(void)
{
	gl_FragColor = SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
