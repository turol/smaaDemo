#define ATTR_POS   0
#define ATTR_UV    1
#define ATTR_COLOR 2


struct SMAAParameters {
	float threshold;
	float depthThreshold;
	uint  maxSearchSteps;
	uint  maxSearchStepsDiag;

	uint  cornerRounding;
	uint  pad0;
	uint  pad1;
	uint  pad2;
};


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
	mat4 prevViewProj;
	mat4 guiOrtho;

};


#ifdef __cplusplus

struct SMAAUBO

#else  // __cplusplus

layout(set = 1, binding = 0, std140) uniform SMAAUBO

#endif  // __cplusplus
{
	SMAAParameters  smaaParameters;

	vec4 subsampleIndices;

	float predicationThreshold;
	float predicationScale;
	float predicationStrength;
	float reprojWeigthScale;
};


#ifdef SMAA_PRESET_CUSTOM

#define SMAA_THRESHOLD                 smaaParameters.threshold
#define SMAA_DEPTH_THRESHOLD           smaaParameters.depthThreshold
#define SMAA_MAX_SEARCH_STEPS          smaaParameters.maxSearchSteps
#define SMAA_MAX_SEARCH_STEPS_DIAG     smaaParameters.maxSearchStepsDiag
#define SMAA_CORNER_ROUNDING           smaaParameters.cornerRounding

#endif  // SMAA_PRESET_CUSTOM


struct Shape {
	vec4   rotation;
	vec3   position;
	uint   order;
	vec3   color;
	float  pad1;
};
