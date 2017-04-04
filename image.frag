#version 450

uniform sampler2D colorTex;

in vec2 texcoord;

out vec4 outColor;

void main(void)
{
    outColor = texture2D(colorTex, texcoord);
}
