#version 450 core

#include "shaderDefines.h"

#define FXAA_PC 1
#define FXAA_GLSL_130 1


#include "fxaa3_11.h"


layout(set = 1, binding = 0) uniform sampler2D colorTex;

layout (location = 0) in vec2 texcoord;

layout (location = 0) out vec4 outColor;

void main(void)
{
    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);
    outColor = FxaaPixelShader(texcoord, zero, colorTex, colorTex, colorTex, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);
}
