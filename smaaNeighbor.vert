#version 330


uniform vec4 screenSize;


out vec2 texcoord;
out vec4 offset;


#define SMAA_RT_METRICS screenSize
#define SMAA_GLSL_3 1
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1

#include "smaa.h"


// From http://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/

//By CeeJay.dk
//License : CC0 - http://creativecommons.org/publicdomain/zero/1.0/

//Basic Buffer/Layout-less fullscreen triangle vertex shader
void main(void)
{
    /*
    //See: https://web.archive.org/web/20140719063725/http://www.altdev.co/2011/08/08/interesting-vertex-shader-trick/

       1  
    ( 0, 2)
    [-1, 3]   [ 3, 3]
        .
        |`.
        |  `.  
        |    `.
        '------`
       0         2
    ( 0, 0)   ( 2, 0)
    [-1,-1]   [ 3,-1]

    ID=0 -> Pos=[-1,-1], Tex=(0,0)
    ID=1 -> Pos=[-1, 3], Tex=(0,2)
    ID=2 -> Pos=[ 3,-1], Tex=(2,0)
    */

    texcoord.x = (gl_VertexID == 2) ?  2.0 :  0.0;
    texcoord.y = (gl_VertexID == 1) ?  2.0 :  0.0;

	offset = vec4(0.0, 0.0, 0.0, 0.0);
    SMAANeighborhoodBlendingVS(texcoord, offset);
	gl_Position = vec4(texcoord * vec2(2.0, 2.0) + vec2(-1.0, -1.0), 1.0, 1.0);
}
