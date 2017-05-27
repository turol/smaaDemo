#version 450 core

#include "shaderDefines.h"
#include "utils.h"


layout(location = ATTR_POS)   in vec2 position;
layout(location = ATTR_UV)    in vec2 uv;
layout(location = ATTR_COLOR) in vec4 color;


layout (location = 0) out Data {
    vec2 uv;
    vec4 color;
} outputs;


void main(void)
{
    gl_Position   = guiOrtho * vec4(position, 0.0, 1.0);
    outputs.uv    = uv;
    outputs.color = color;
}
