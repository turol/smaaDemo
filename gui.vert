#version 450 core

#include "shaderDefines.h"
#include "utils.h"


layout(location = ATTR_POS)   in vec2 position;
layout(location = ATTR_UV)    in vec2 uv;
layout(location = ATTR_COLOR) in vec4 color;


layout (location = 0) out vec4 out_color;
layout (location = 1) out vec2 out_uv;


void main(void)
{
    gl_Position   = guiOrtho * vec4(position, 0.0, 1.0);
    out_uv        = uv;
    out_color     = color;
}
