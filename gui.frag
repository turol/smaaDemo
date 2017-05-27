#version 450 core

#include "shaderDefines.h"


layout(binding = TEXUNIT_COLOR) uniform sampler2D colorTex;


layout (location = 0) in Data {
    vec2 uv;
    vec4 color;
};


layout (location = 0) out vec4 outColor;


void main(void)
{
    outColor = color * texture2D(colorTex, uv);
}
