#version 450

in vec3 colorFrag;

out vec4 outColor;

void main(void)
{
    vec4 temp;
    temp.xyz = colorFrag;
    temp.w   = dot(colorFrag, vec3(0.299, 0.587, 0.114));
    outColor = temp;
}
