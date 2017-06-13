#define ATTR_POS   0
#define ATTR_UV    1
#define ATTR_COLOR 2


#ifdef __cplusplus

struct Globals

#else  // __cplusplus

layout(binding = 0, std140) uniform Globals

#endif  // __cplusplus
{
	vec4 screenSize;
	mat4 viewProj;
	mat4 guiOrtho;
};


struct Cube {
	vec4 rotation;
	vec3 position;
	uint color;
};
