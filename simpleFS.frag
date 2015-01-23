#version 330

uniform vec4 color;

in vec4 colorFrag;


void main(void)
{
	gl_FragColor = color;
}
