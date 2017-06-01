#version 450 core

#include "shaderDefines.h"


layout(binding = TEXUNIT_COLOR) uniform sampler2D colorTex;


layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 outColor;


void main(void)
{
    outColor = texture(colorTex, texcoord);
}
