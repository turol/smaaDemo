#version 330
#extension GL_ARB_gpu_shader5 : enable


uniform sampler2D color;
uniform vec4 screenSize;


in vec2 texcoord;


#define FXAA_PC 1
#define FXAA_GLSL_130 1
#define FXAA_QUALITY__PRESET 39
#define FXAA_GREEN_AS_LUMA 1


#include "fxaa3_11.h"


void main(void)
{
    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);
    gl_FragColor = FxaaPixelShader(texcoord, zero, color, color, color, screenSize.zw, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);
}
