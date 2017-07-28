#version 450 core

#include "shaderDefines.h"


layout(set = 1, binding = 0) uniform sampler2D colorTex;


layout (location = 0) in vec4 color;
layout (location = 1) in vec2 uv;


layout (location = 0) out vec4 outColor;


void main(void)
{
    outColor = color * texture(colorTex, uv);
}
