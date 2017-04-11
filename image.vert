#version 450

#include "shaderDefines.h"

layout(location = ATTR_POS) in vec2 pos;

out vec2 texcoord;

void main(void)
{
    texcoord = pos * vec2(0.5, -0.5) + vec2(0.5, 0.5);
    gl_Position = vec4(pos, 1.0, 1.0);
}
