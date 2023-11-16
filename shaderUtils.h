// From http://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/

// NOTE: this is in direct3d texture coord space (origin upper left)
// remember to flip y coordinate

//By CeeJay.dk
//License : CC0 - http://creativecommons.org/publicdomain/zero/1.0/

//Basic Buffer/Layout-less fullscreen triangle vertex shader
vec2 triangleVertex(in int vertID, out vec2 texcoord)
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

    texcoord.x = (vertID == 2) ?  2.0 :  0.0;
    texcoord.y = (vertID == 1) ?  2.0 :  0.0;

	return texcoord * vec2(2.0, -2.0) + vec2(-1.0, 1.0);
}


vec2 flipTexCoord(in vec2 tc) {
    return tc * vec2(1.0, -1.0) + vec2(0.0, 1.0);
}


vec4 flipTexCoord(in vec4 tc) {
    return tc * vec4(1.0, -1.0, 1.0, -1.0) + vec4(0.0, 1.0, 0.0, 1.0);
}


float sRGB2linear(float v) {
    if (v <= 0.04045) {
        return v / 12.92;
    } else {
        return pow((v + 0.055) / 1.055, 2.4);
    }
}


vec3 sRGB2linear(vec3 v) {
    return vec3(sRGB2linear(v.x), sRGB2linear(v.y), sRGB2linear(v.z));
}


float linear2sRGB(float v) {
    if (v <= 0.00031308) {
        return 12.92 * v;
    } else {
        return 1.055 * pow(v, (1.0 / 2.4) ) - 0.055;
    }
}


vec3 linear2sRGB(vec3 v) {
    return vec3(linear2sRGB(v.x), linear2sRGB(v.y), linear2sRGB(v.z));
}
