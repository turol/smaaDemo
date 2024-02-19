#define ATTR_POS   0
#define ATTR_UV    1
#define ATTR_COLOR 2

#define FXAA_COMPUTE_GROUP_X 8
#define FXAA_COMPUTE_GROUP_Y 8

#define SMAA_EDGES_COMPUTE_GROUP_X 8
#define SMAA_EDGES_COMPUTE_GROUP_Y 8

#define SMAA_WEIGHTS_COMPUTE_GROUP_X 8
#define SMAA_WEIGHTS_COMPUTE_GROUP_Y 8

#define SMAA_BLEND_COMPUTE_GROUP_X 8
#define SMAA_BLEND_COMPUTE_GROUP_Y 8

#ifdef HLSL

#define mat4 float4x4
#define vec2 float2
#define vec3 float3
#define vec4 float4

#endif  // HLSL


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

#elif defined(HLSL)

[[vk::binding(0, 0)]] cbuffer Globals


#else  // __cplusplus

layout(set = 0, binding = 0, std140) uniform Globals

#endif  // __cplusplus
{
	vec4 screenSize;
	mat4 viewProj;
	mat4 prevViewProj;
	mat4 guiOrtho;

	SMAAParameters  smaaParameters;

	vec4 subsampleIndices;

	float predicationThreshold;
	float predicationScale;
	float predicationStrength;
	float reprojWeigthScale;

	vec2 offsetHax;
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
