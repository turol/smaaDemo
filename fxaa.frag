#version 450

#define FXAA_PC 1
#define FXAA_GLSL_130 1


#include "fxaa3_11.h"


uniform sampler2D colorTex;
uniform vec4 screenSize;

in vec2 texcoord;

out vec4 outColor;

void main(void)
{
    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);
    outColor = FxaaPixelShader(texcoord, zero, colorTex, colorTex, colorTex, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);
}
