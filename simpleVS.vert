#version 330

uniform mat4 modelViewProj;

in vec4 color;
in vec3 position;

out vec4 colorFrag;

void main(void)
{
	colorFrag = color;
	gl_Position = modelViewProj * vec4(position, 1.0);
}
