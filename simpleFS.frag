#version 330


in vec3 colorFrag;


void main(void)
{
    gl_FragColor.xyz = colorFrag;
    gl_FragColor.w = dot(colorFrag, vec3(0.299, 0.587, 0.114));
}
