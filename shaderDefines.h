#define ATTR_POS   0
#define ATTR_UV    1
#define ATTR_COLOR 2


#ifdef __cplusplus

struct Globals

#else  // __cplusplus

layout(set = 0, binding = 1) uniform sampler linearSampler;
layout(set = 0, binding = 2) uniform sampler nearestSampler;

layout(set = 0, binding = 0, std140) uniform Globals

#endif  // __cplusplus
{
	vec4 screenSize;
	mat4 viewProj;
	mat4 guiOrtho;
};


struct Cube {
	vec4 rotation;
	vec3 position;
	float pad0;
	vec3  color;
	float pad1;
};
