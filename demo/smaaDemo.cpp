/*
Copyright (c) 2015 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tclap/CmdLine.h>

#include "Renderer.h"
#include "Utils.h"

#include "AreaTex.h"
#include "SearchTex.h"


#ifdef _MSC_VER
#define fileno _fileno
#define __builtin_unreachable() assert(false)
#endif


union Color {
	uint32_t val;
	struct {
		uint8_t r, g, b, a;
	};
};


static const Color white = { 0xFFFFFFFF };


class SMAADemo;


namespace AAMethod {

enum AAMethod {
	  FXAA
	, SMAA
	, LAST = SMAA
};


const char *name(AAMethod m) {
	switch (m) {
	case FXAA:
		return "FXAA";
		break;

	case SMAA:
		return "SMAA";
		break;
	}

	__builtin_unreachable();
}


}  // namespace AAMethod


static const char *smaaDebugModeStr(unsigned int mode) {
	switch (mode) {
	case 0:
		return "none";

	case 1:
		return "edges";

	case 2:
		return "blend";
	}

	__builtin_unreachable();
}


// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

static uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}


class RandomGen {
	pcg32_random_t rng;

	RandomGen(const RandomGen &) = delete;
	RandomGen &operator=(const RandomGen &) = delete;
	RandomGen(RandomGen &&) = delete;
	RandomGen &operator=(RandomGen &&) = delete;

public:

	explicit RandomGen(uint64_t seed)
	{
		rng.state = seed;
		rng.inc = 1;
		// spin it once for proper initialization
		pcg32_random_r(&rng);
	}


	float randFloat() {
		uint32_t u = randU32();
		// because 24 bits mantissa
		u &= 0x00FFFFFFU;
		return float(u) / 0x00FFFFFFU;
	}


	uint32_t randU32() {
		return pcg32_random_r(&rng);
	}
};


static const char *fxaaQualityLevels[] =
{ "10", "15", "20", "29", "39" };


static const unsigned int maxFXAAQuality = sizeof(fxaaQualityLevels) / sizeof(fxaaQualityLevels[0]);


static const char *smaaQualityLevels[] =
{ "LOW", "MEDIUM", "HIGH", "ULTRA" };


static const unsigned int maxSMAAQuality = sizeof(smaaQualityLevels) / sizeof(smaaQualityLevels[0]);


class SMAADemo {
	unsigned int windowWidth, windowHeight;
	unsigned int resizeWidth, resizeHeight;
	bool vsync;
	bool fullscreen;
	SDL_Window *window;
	SDL_GLContext context;
	bool glDebug;
	unsigned int glMajor;
	unsigned int glMinor;

	std::unique_ptr<Shader> cubeShader;
	std::unique_ptr<Shader> imageShader;

	// TODO: create helper classes for these
	GLuint cubeVAO;
	GLuint cubeVBO, cubeIBO;
	GLuint fullscreenVAO;
	GLuint fullscreenVBO;
	GLuint instanceVBO;

	GLuint linearSampler;
	GLuint nearestSampler;

	unsigned int cubePower;

	std::unique_ptr<Framebuffer> builtinFBO;
	std::unique_ptr<Framebuffer> renderFBO;
	std::unique_ptr<Framebuffer> edgesFBO;
	std::unique_ptr<Framebuffer> blendFBO;

	bool antialiasing;
	AAMethod::AAMethod aaMethod;
	std::unique_ptr<Shader> fxaaShader;
	std::unique_ptr<Shader> smaaEdgeShader;
	std::unique_ptr<Shader> smaaBlendWeightShader;
	std::unique_ptr<Shader> smaaNeighborShader;
	GLuint areaTex;
	GLuint searchTex;

	bool rotateCamera;
	float cameraRotation;
	uint64_t lastTime;
	uint64_t freq;
	uint64_t rotationTime;
	unsigned int debugMode;
	unsigned int colorMode;
	bool rightShift, leftShift;
	RandomGen random;
	unsigned int fxaaQuality;
	unsigned int smaaQuality;
	bool keepGoing;
	// 0 for cubes
	// 1.. for images
	unsigned int activeScene;


	struct Image {
		std::string filename;
		GLuint tex;
	};

	std::vector<Image> images;


	struct Cube {
		glm::vec3 pos;
		glm::quat orient;
		Color col;


		Cube(float x_, float y_, float z_, glm::quat orient_, Color col_)
		: pos(x_, y_, z_), orient(orient_), col(col_)
		{
		}
	};

	std::vector<Cube> cubes;


	struct InstanceData {
		float x, y, z;
		float qx, qy, qz;
		Color col;


		InstanceData(glm::quat rot, glm::vec3 pos, Color col_)
			: x(pos.x), y(pos.y), z(pos.z)
			, qx(rot.x), qy(rot.y), qz(rot.z)
			, col(col_)
		{
			// shader assumes this and uses it to calculate w from other components
			assert(rot.w >= 0.0);
		}
	};

	std::vector<InstanceData> instances;

	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) = delete;
	SMAADemo &operator=(SMAADemo &&) = delete;

public:

	SMAADemo();

	~SMAADemo();

	void parseCommandLine(int argc, char *argv[]);

	void initRender();

	void createFramebuffers();

	void applyVSync();

	void applyFullscreen();

	void buildImageShader();

	void buildFXAAShader();

	void buildSMAAShaders();

	void createCubes();

	void colorCubes();

	void setCubeVBO();

	void setFullscreenVBO();

	void mainLoopIteration();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, vsync(true)
, fullscreen(false)
, window(NULL)
, context(NULL)
, glDebug(false)
, glMajor(3)
, glMinor(1)
, cubeVAO(0)
, cubeVBO(0)
, cubeIBO(0)
, fullscreenVAO(0)
, fullscreenVBO(0)
, instanceVBO(0)
, linearSampler(0)
, nearestSampler(0)
, cubePower(3)
, antialiasing(true)
, aaMethod(AAMethod::SMAA)
, areaTex(0)
, searchTex(0)
, rotateCamera(false)
, cameraRotation(0.0f)
, lastTime(0)
, freq(0)
, rotationTime(0)
, debugMode(0)
, colorMode(0)
, rightShift(false)
, leftShift(false)
, random(1)
, fxaaQuality(maxFXAAQuality - 1)
, smaaQuality(maxSMAAQuality - 1)
, keepGoing(true)
, activeScene(0)
{
	resizeWidth = windowWidth;
	resizeHeight = windowHeight;

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	freq = SDL_GetPerformanceFrequency();
	lastTime = SDL_GetPerformanceCounter();

	// TODO: detect screens, log interesting display parameters etc
	// TODO: initialize random using external source
}


SMAADemo::~SMAADemo() {
	if (context != NULL) {
		SDL_GL_DeleteContext(context);
		context = NULL;
	}

	if (window != NULL) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

		glDeleteVertexArrays(1, &cubeVAO);
		glDeleteVertexArrays(1, &fullscreenVAO);
	
	glDeleteBuffers(1, &cubeVBO);
	glDeleteBuffers(1, &cubeIBO);
	glDeleteBuffers(1, &fullscreenVBO);
	glDeleteBuffers(1, &instanceVBO);

		glDeleteSamplers(1, &linearSampler);
		glDeleteSamplers(1, &nearestSampler);

	glDeleteTextures(1, &areaTex);
	glDeleteTextures(1, &searchTex);

	SDL_Quit();
}


struct Vertex {
	float x, y, z;
};


const float coord = sqrtf(3.0f) / 2.0f;


static const Vertex vertices[] =
{
	  { -coord , -coord, -coord }
	, { -coord ,  coord, -coord }
	, {  coord , -coord, -coord }
	, {  coord ,  coord, -coord }
	, { -coord , -coord,  coord }
	, { -coord ,  coord,  coord }
	, {  coord , -coord,  coord }
	, {  coord ,  coord,  coord }
};


static const float fullscreenVertices[] =
{
	  -1.0f, -1.0f
	,  3.0f, -1.0f
	, -1.0f,  3.0f
};


static const uint32_t indices[] =
{
	// top
	  1, 3, 5
	, 5, 3, 7

	// front
	, 0, 2, 1
	, 1, 2, 3

	// back
	, 7, 6, 5
	, 5, 6, 4

	// left
	, 0, 1, 4
	, 4, 1, 5

	// right
	, 2, 6, 3
	, 3, 6, 7

	// bottom
	, 2, 0, 6
	, 6, 0, 4
};


#define VBO_OFFSETOF(st, member) reinterpret_cast<GLvoid *>(offsetof(st, member))


void SMAADemo::buildImageShader() {
	VertexShader vShader("image.vert");
	FragmentShader fShader("image.frag");

	imageShader = std::make_unique<Shader>(vShader, fShader);
}


void SMAADemo::buildFXAAShader() {
	glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

	ShaderBuilder s;

	s.pushLine("#define FXAA_PC 1");

		s.pushLine("#define FXAA_GLSL_130 1");

	// TODO: cache shader based on quality level
	s.pushLine("#define FXAA_QUALITY_PRESET " + std::string(fxaaQualityLevels[fxaaQuality]));

	ShaderBuilder vert(s);
	vert.pushVertexAttr("vec2 pos;");
	vert.pushVertexVarying("vec2 texcoord;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
	vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
	vert.pushLine("}");

	VertexShader vShader("fxaa.vert", vert);

	// fragment
	ShaderBuilder frag(s);
	frag.pushFile("fxaa3_11.h");
	frag.pushLine("uniform sampler2D colorTex;");
	frag.pushLine("uniform vec4 screenSize;");
	frag.pushFragmentVarying("vec2 texcoord;");
	frag.pushFragmentOutputDecl();
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushLine("    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);");
	frag.pushFragmentOutput("FxaaPixelShader(texcoord, zero, colorTex, colorTex, colorTex, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);");
	frag.pushLine("}");

	FragmentShader fShader("fxaa.frag", frag);

	fxaaShader = std::make_unique<Shader>(vShader, fShader);
	glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
}


void SMAADemo::buildSMAAShaders() {
		ShaderBuilder s;

		s.pushLine("#define SMAA_RT_METRICS screenSize");
		s.pushLine("#define SMAA_GLSL_3 1");
		// TODO: cache shader based on quality level
		s.pushLine("#define SMAA_PRESET_"  + std::string(smaaQualityLevels[smaaQuality]) + " 1");

		s.pushLine("uniform vec4 screenSize;");

		ShaderBuilder commonVert(s);

		commonVert.pushLine("#define SMAA_INCLUDE_PS 0");
		commonVert.pushLine("#define SMAA_INCLUDE_VS 1");

		commonVert.pushFile("smaa.h");

		ShaderBuilder commonFrag(s);

		commonFrag.pushLine("#define SMAA_INCLUDE_PS 1");
		commonFrag.pushLine("#define SMAA_INCLUDE_VS 0");

		commonFrag.pushFile("smaa.h");
		commonFrag.pushFragmentOutputDecl();

		glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec4 offset0;");
			vert.pushVertexVarying("vec4 offset1;");
			vert.pushVertexVarying("vec4 offset2;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    vec4 offsets[3];");
			vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    SMAAEdgeDetectionVS(texcoord, offsets);");
			vert.pushLine("    offset0 = offsets[0];");
			vert.pushLine("    offset1 = offsets[1];");
			vert.pushLine("    offset2 = offsets[2];");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaEdge.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D colorTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec4 offset0;");
			frag.pushFragmentVarying("vec4 offset1;");
			frag.pushFragmentVarying("vec4 offset2;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushLine("    vec4 offsets[3];");
			frag.pushLine("    offsets[0] = offset0;");
			frag.pushLine("    offsets[1] = offset1;");
			frag.pushLine("    offsets[2] = offset2;");
			frag.pushFragmentOutput("vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 0.0);");
			frag.pushLine("}");

			FragmentShader fShader("smaaEdge.frag", frag);

			smaaEdgeShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}

		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec2 pixcoord;");
			vert.pushVertexVarying("vec4 offset0;");
			vert.pushVertexVarying("vec4 offset1;");
			vert.pushVertexVarying("vec4 offset2;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    vec4 offsets[3];");
			vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    pixcoord = vec2(0.0, 0.0);");
			vert.pushLine("    SMAABlendingWeightCalculationVS(texcoord, pixcoord, offsets);");
			vert.pushLine("    offset0 = offsets[0];");
			vert.pushLine("    offset1 = offsets[1];");
			vert.pushLine("    offset2 = offsets[2];");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaBlendWeight.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D edgesTex;");
			frag.pushLine("uniform sampler2D areaTex;");
			frag.pushLine("uniform sampler2D searchTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec2 pixcoord;");
			frag.pushFragmentVarying("vec4 offset0;");
			frag.pushFragmentVarying("vec4 offset1;");
			frag.pushFragmentVarying("vec4 offset2;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushLine("    vec4 offsets[3];");
			frag.pushLine("    offsets[0] = offset0;");
			frag.pushLine("    offsets[1] = offset1;");
			frag.pushLine("    offsets[2] = offset2;");
			frag.pushFragmentOutput("SMAABlendingWeightCalculationPS(texcoord, pixcoord, offsets, edgesTex, areaTex, searchTex, vec4(0.0, 0.0, 0.0, 0.0));");
			frag.pushLine("}");

			FragmentShader fShader("smaaBlendWeight.frag", frag);

			smaaBlendWeightShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}

		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec4 offset;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    offset = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    SMAANeighborhoodBlendingVS(texcoord, offset);");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaNeighbor.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D blendTex;");
			frag.pushLine("uniform sampler2D colorTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec4 offset;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushFragmentOutput("SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);");
			frag.pushLine("}");

			FragmentShader fShader("smaaNeighbor.frag", frag);

			smaaNeighborShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}
}


void SMAADemo::parseCommandLine(int argc, char *argv[]) {
	try {
		TCLAP::CmdLine cmd("SMAA demo", ' ', "1.0");

		TCLAP::SwitchArg glDebugSwitch("", "gldebug", "Enable OpenGL debugging", cmd, false);
		TCLAP::ValueArg<std::string> dsaSwitch("", "dsa", "Select DSA mode", false, "arb", "arb, ext or none", cmd);
		TCLAP::ValueArg<unsigned int> glMajorSwitch("", "glmajor", "OpenGL major version", false, glMajor, "version", cmd);
		TCLAP::ValueArg<unsigned int> glMinorSwitch("", "glminor", "OpenGL minor version", false, glMinor, "version", cmd);
		TCLAP::ValueArg<unsigned int> windowWidthSwitch("", "width", "Window width", false, windowWidth, "width", cmd);
		TCLAP::ValueArg<unsigned int> windowHeightSwitch("", "height", "Window height", false, windowHeight, "height", cmd);
		TCLAP::UnlabeledMultiArg<std::string> imagesArg("images", "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		glDebug = glDebugSwitch.getValue();
		glMajor = glMajorSwitch.getValue();
		glMinor = glMinorSwitch.getValue();
		windowWidth = windowWidthSwitch.getValue();
		windowHeight = windowHeightSwitch.getValue();
		resizeWidth = windowWidth;
		resizeHeight = windowHeight;

		const auto &imageFiles = imagesArg.getValue();
		images.reserve(imageFiles.size());
		for (const auto &filename : imageFiles) {
			images.push_back(Image());
			auto &img = images.back();
			img.tex = 0;
			img.filename = filename;
		}

	} catch (TCLAP::ArgException &e) {
		printf("parseCommandLine exception: %s for arg %s\n", e.error().c_str(), e.argId().c_str());
	} catch (...) {
		printf("parseCommandLine: unknown exception\n");
	}
}


void SMAADemo::initRender() {
	assert(window == NULL);
	assert(context == NULL);

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		if (glDebug) {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		}

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	printf("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		printf("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			printf("Display mode %i : width %i, height %i, BPP %i\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format));
		}
	}

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);

	context = SDL_GL_CreateContext(window);

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	applyVSync();

	glewExperimental = true;
	glewInit();

	// TODO: check extensions
	// at least direct state access, texture storage

	if (!GLEW_ARB_direct_state_access) {
		printf("ARB_direct_state_access not found\n");
		exit(1);
	}

	if (glDebug) {
		if (GLEW_KHR_debug) {
			printf("KHR_debug found\n");

			glDebugMessageCallback(glDebugCallback, NULL);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		} else {
			printf("KHR_debug not found\n");
		}
	}

	printf("GL vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("GL renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("GL version: \"%s\"\n", glGetString(GL_VERSION));
	printf("GLSL version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// swap once to get better traces
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	SDL_GL_SwapWindow(window);

	cubeShader = std::make_unique<Shader>(VertexShader("cube.vert"), FragmentShader("cube.frag"));
	buildImageShader();
	buildSMAAShaders();
	buildFXAAShader();

		glCreateSamplers(1, &linearSampler);
		glSamplerParameteri(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glCreateSamplers(1, &nearestSampler);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glCreateBuffers(1, &cubeVBO);
	glNamedBufferData(cubeVBO, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

	glCreateBuffers(1, &cubeIBO);
	glNamedBufferData(cubeIBO, sizeof(indices), &indices[0], GL_STATIC_DRAW);

	glCreateBuffers(1, &instanceVBO);
	glNamedBufferData(instanceVBO, sizeof(InstanceData), NULL, GL_STREAM_DRAW);

		glCreateVertexArrays(1, &cubeVAO);
		glVertexArrayElementBuffer(cubeVAO, cubeIBO);

		glBindVertexArray(cubeVAO);

		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

		glEnableVertexArrayAttrib(cubeVAO, ATTR_POS);

			glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
			glVertexAttribPointer(ATTR_CUBEPOS, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, x));
			glVertexAttribDivisor(ATTR_CUBEPOS, 1);

			glVertexAttribPointer(ATTR_ROT, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, qx));
			glVertexAttribDivisor(ATTR_ROT, 1);

			glVertexAttribPointer(ATTR_COLOR, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, col));
			glVertexAttribDivisor(ATTR_COLOR, 1);

			glEnableVertexArrayAttrib(cubeVAO, ATTR_CUBEPOS);
			glEnableVertexArrayAttrib(cubeVAO, ATTR_ROT);
			glEnableVertexArrayAttrib(cubeVAO, ATTR_COLOR);

	glCreateBuffers(1, &fullscreenVBO);
	glNamedBufferData(fullscreenVBO, sizeof(fullscreenVertices), &fullscreenVertices[0], GL_STATIC_DRAW);

		glCreateVertexArrays(1, &fullscreenVAO);
		glBindVertexArray(fullscreenVAO);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
		glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);

		glEnableVertexArrayAttrib(fullscreenVAO, ATTR_POS);

	const bool flipSMAATextures = true;

	glCreateTextures(GL_TEXTURE_2D, 1, &areaTex);
	glBindTextureUnit(TEXUNIT_AREATEX, areaTex);
	glTextureStorage2D(areaTex, 1, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT);
	glTextureParameteri(areaTex, GL_TEXTURE_MAX_LEVEL, 0);

	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(AREATEX_SIZE);
		for (unsigned int y = 0; y < AREATEX_HEIGHT; y++) {
			unsigned int srcY = AREATEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * AREATEX_PITCH], areaTexBytes + srcY * AREATEX_PITCH, AREATEX_PITCH);
		}
		glTextureSubImage2D(areaTex, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	} else {
		glTextureSubImage2D(areaTex, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);
	}
	glBindSampler(TEXUNIT_AREATEX, linearSampler);

	glCreateTextures(GL_TEXTURE_2D, 1, &searchTex);
	glBindTextureUnit(TEXUNIT_SEARCHTEX, searchTex);
	glTextureStorage2D(searchTex, 1, GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT);
	glTextureParameteri(searchTex, GL_TEXTURE_MAX_LEVEL, 0);
	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(SEARCHTEX_SIZE);
		for (unsigned int y = 0; y < SEARCHTEX_HEIGHT; y++) {
			unsigned int srcY = SEARCHTEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * SEARCHTEX_PITCH], searchTexBytes + srcY * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
		}
		glTextureSubImage2D(searchTex, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	} else {
		glTextureSubImage2D(searchTex, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);
	}

	glBindSampler(TEXUNIT_SEARCHTEX, linearSampler);

	builtinFBO = std::make_unique<Framebuffer>(0);
	builtinFBO->width = windowWidth;
	builtinFBO->height = windowHeight;

	createFramebuffers();

	for (auto &img : images) {
		const auto filename = img.filename.c_str();
		int width = 0, height = 0;
		unsigned char *imageData = stbi_load(filename, &width, &height, NULL, 3);
		printf(" %p  %dx%d\n", imageData, width, height);

		glCreateTextures(GL_TEXTURE_2D, 1, &img.tex);
		glTextureStorage2D(img.tex, 1, GL_RGB8, width, height);
		glTextureParameteri(img.tex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureSubImage2D(img.tex, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imageData);

		stbi_image_free(imageData);
	}

	// default scene to last image or cubes if none
	activeScene = images.size();
}


void SMAADemo::setCubeVBO() {
		glBindVertexArray(cubeVAO);
}


void SMAADemo::setFullscreenVBO() {
		glBindVertexArray(fullscreenVAO);
}


void SMAADemo::createFramebuffers()	{
	renderFBO.reset();
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	renderFBO = std::make_unique<Framebuffer>(fbo);
	renderFBO->width = windowWidth;
	renderFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	GLuint tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	renderFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_COLOR, renderFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	renderFBO->depthTex = tex;
	glBindTextureUnit(TEXUNIT_TEMP, renderFBO->depthTex);
	glTextureStorage2D(tex, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, tex, 0);

	// SMAA edges texture and FBO
	edgesFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	edgesFBO = std::make_unique<Framebuffer>(fbo);
	edgesFBO->width = windowWidth;
	edgesFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	edgesFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_EDGES, edgesFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glBindSampler(TEXUNIT_EDGES, linearSampler);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	// SMAA blending weights texture and FBO
	blendFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	blendFBO = std::make_unique<Framebuffer>(fbo);
	blendFBO->width = windowWidth;
	blendFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	blendFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_BLEND, blendFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glBindSampler(TEXUNIT_BLEND, linearSampler);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
}


void SMAADemo::applyVSync() {
	if (vsync) {
		// enable vsync, using late swap tearing if possible
		int retval = SDL_GL_SetSwapInterval(-1);
		if (retval != 0) {
			// TODO: check return val
			SDL_GL_SetSwapInterval(1);
		}
		printf("VSync is on\n");
	} else {
		// TODO: check return val
		SDL_GL_SetSwapInterval(0);
		printf("VSync is off\n");
	}
}


void SMAADemo::applyFullscreen() {
	if (fullscreen) {
		// TODO: check return val?
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		printf("Fullscreen\n");
	} else {
		SDL_SetWindowFullscreen(window, 0);
		printf("Windowed\n");
	}
}


void SMAADemo::createCubes() {
	// cubes on a side is some power of 2
	const unsigned int cubesSide = pow(2, cubePower);

	// cube of cubes, n^3 cubes total
	const unsigned int numCubes = pow(cubesSide, 3);

	const float cubeDiameter = sqrtf(3.0f);
	const float cubeDistance = cubeDiameter + 1.0f;

	const float bigCubeSide = cubeDistance * cubesSide;

	cubes.clear();
	cubes.reserve(numCubes);

	for (unsigned int x = 0; x < cubesSide; x++) {
		for (unsigned int y = 0; y < cubesSide; y++) {
			for (unsigned int z = 0; z < cubesSide; z++) {
				float qx = random.randFloat();
				float qy = random.randFloat();
				float qz = random.randFloat();
				float qw = random.randFloat();
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				cubes.emplace_back((x * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (z * cubeDistance) - (bigCubeSide / 2.0f)
				                 , glm::quat(qx, qy, qz, qw)
				                 , white);
			}
		}
	}

	// reallocate instance data buffer
	glNamedBufferData(instanceVBO, sizeof(InstanceData) * numCubes, NULL, GL_STREAM_DRAW);

	colorCubes();
}


void SMAADemo::colorCubes() {
	if (colorMode == 0) {
		for (auto &cube : cubes) {
			Color col;
			// random RGB, alpha = 1.0
			// FIXME: we're abusing little-endianness, make it portable
			col.val = random.randU32() | 0xFF000000;
			cube.col = col;
		}
	} else {
		for (auto &cube : cubes) {
			Color col;
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			// TODO: use the same luma as shader

			float y = 0.5f;
			const float c_red = 0.299
				, c_green = 0.587
			, c_blue = 0.114;
			float cb = random.randFloat();
			float cr = random.randFloat();

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			col.r = 255 * r;
			col.g = 255 * g;
			col.b = 255 * b;
			col.a = 0xFF;
			cube.col = col;
		}
	}
}


static void printHelp() {
	printf(" a     - toggle antialiasing on/off\n");
	printf(" c     - re-color cubes\n");
	printf(" d     - cycle through debug visualizations\n");
	printf(" f     - toggle fullscreen\n");
	printf(" h     - print help\n");
	printf(" m     - change antialiasing method\n");
	printf(" q     - cycle through AA quality levels\n");
	printf(" v     - toggle vsync\n");
	printf(" SPACE - toggle camera rotation\n");
	printf(" ESC   - quit\n");
}


void SMAADemo::mainLoopIteration() {
		// TODO: timing
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		while (SDL_PollEvent(&event)) {
			int sceneIncrement = 1;
			switch (event.type) {
			case SDL_QUIT:
				keepGoing = false;
				break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE:
					keepGoing = false;
					break;

				case SDL_SCANCODE_LSHIFT:
					leftShift = true;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = true;
					break;

				case SDL_SCANCODE_SPACE:
					rotateCamera = !rotateCamera;
					printf("camera rotation is %s\n", rotateCamera ? "on" : "off");
					break;

				case SDL_SCANCODE_A:
					antialiasing = !antialiasing;
					printf("antialiasing set to %s\n", antialiasing ? "on" : "off");
					break;

				case SDL_SCANCODE_C:
					if (rightShift || leftShift) {
						colorMode = (colorMode + 1) % 2;
						printf("color mode set to %s\n", colorMode ? "YCbCr" : "RGB");
					}
					colorCubes();
					break;

				case SDL_SCANCODE_D:
					if (antialiasing && aaMethod == AAMethod::SMAA) {
						if (leftShift || rightShift) {
							debugMode = (debugMode + 3 - 1) % 3;
						} else {
							debugMode = (debugMode + 1) % 3;
						}
						printf("Debug mode set to %s\n", smaaDebugModeStr(debugMode));
					}
					break;

				case SDL_SCANCODE_H:
					printHelp();
					break;

				case SDL_SCANCODE_M:
					aaMethod = AAMethod::AAMethod((int(aaMethod) + 1) % (int(AAMethod::LAST) + 1));
					printf("aa method set to %s\n", AAMethod::name(aaMethod));
					break;

				case SDL_SCANCODE_Q:
					switch (aaMethod) {
					case AAMethod::FXAA:
						if (leftShift || rightShift) {
							fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
						} else {
							fxaaQuality = fxaaQuality + 1;
						}
						fxaaQuality = fxaaQuality % maxFXAAQuality;
						buildFXAAShader();
						printf("FXAA quality set to %s (%u)\n", fxaaQualityLevels[fxaaQuality], fxaaQuality);
						break;

					case AAMethod::SMAA:
						if (leftShift || rightShift) {
							smaaQuality = smaaQuality + maxSMAAQuality - 1;
						} else {
							smaaQuality = smaaQuality + 1;
						}
						smaaQuality = smaaQuality % maxSMAAQuality;
						buildSMAAShaders();
						printf("SMAA quality set to %s (%u)\n", smaaQualityLevels[smaaQuality], smaaQuality);
						break;

					}
					break;

				case SDL_SCANCODE_V:
					vsync = !vsync;
					applyVSync();
					break;

				case SDL_SCANCODE_F:
					fullscreen = !fullscreen;
					applyFullscreen();
					break;

				case SDL_SCANCODE_LEFT:
					sceneIncrement = -1;
					// fallthrough
				case SDL_SCANCODE_RIGHT:
					{
						// all images + cubes scene
						unsigned int numScenes = images.size() + 1;
						activeScene = (activeScene + sceneIncrement + numScenes) % numScenes;
					}
					break;

				default:
					break;
				}
				break;

			case SDL_KEYUP:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_LSHIFT:
					leftShift = false;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = false;
					break;

				default:
					break;
				}
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					resizeWidth = event.window.data1;
					resizeHeight = event.window.data2;
					break;
				default:
					break;
				}
			}
		}

		render();
}


void SMAADemo::render() {
	if (resizeWidth != windowWidth || resizeHeight != windowHeight) {
		windowWidth = resizeWidth;
		windowHeight = resizeHeight;
		createFramebuffers();

		glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

		glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}

	uint64_t ticks = SDL_GetPerformanceCounter();
	uint64_t elapsed = ticks - lastTime;

	lastTime = ticks;

	// TODO: reset all relevant state in case some 3rd-party program fucked them up

	glViewport(0, 0, windowWidth, windowHeight);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	builtinFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	renderFBO->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (activeScene == 0) {
		if (rotateCamera) {
			rotationTime += elapsed;

			const uint64_t rotationPeriod = 30 * freq;
			rotationTime = rotationTime % rotationPeriod;
			cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
		}
		glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f)), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, 0.1f, 100.0f);
		glm::mat4 viewProj = proj * view;

			cubeShader->bind();
			GLint viewProjLoc = cubeShader->getUniformLocation("viewProj");
			glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

			instances.clear();
			instances.reserve(cubes.size());
			for (const auto &cube : cubes) {
				instances.emplace_back(cube.orient, cube.pos, cube.col);
			}

			setCubeVBO();
			glNamedBufferSubData(instanceVBO, 0, sizeof(InstanceData) * instances.size(), &instances[0]);

			glDrawElementsInstanced(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL, cubes.size());
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		assert(activeScene - 1 < images.size());
		const auto &image = images[activeScene - 1];
		imageShader->bind();
		glBindTextureUnit(TEXUNIT_COLOR, image.tex);
		glBindSampler(TEXUNIT_COLOR, nearestSampler);
		setFullscreenVBO();
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindTextureUnit(TEXUNIT_COLOR, renderFBO->colorTex);
		glBindSampler(TEXUNIT_COLOR, linearSampler);
	}

	setFullscreenVBO();

	if (antialiasing) {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		switch (aaMethod) {
		case AAMethod::FXAA:
			builtinFBO->bind();
			fxaaShader->bind();
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;

		case AAMethod::SMAA:
			smaaEdgeShader->bind();

			if (debugMode == 1) {
				// detect edges only
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				edgesFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			smaaBlendWeightShader->bind();
			if (debugMode == 2) {
				// show blending weights
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				blendFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			// full effect
			smaaNeighborShader->bind();
			builtinFBO->bind();
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;
		}

	} else {
		renderFBO->blitTo(*builtinFBO);
	}

	SDL_GL_SwapWindow(window);
}


int main(int argc, char *argv[]) {
	try {
	auto demo = std::make_unique<SMAADemo>();

	demo->parseCommandLine(argc, argv);

	demo->initRender();
	demo->createCubes();
	printHelp();

	while (demo->shouldKeepGoing()) {
		demo->mainLoopIteration();
	}

	} catch (std::exception &e) {
		printf("caught std::exception \"%s\"\n", e.what());
		// so native dumps core
		throw;
	} catch (...) {
		printf("unknown exception\n");
		// so native dumps core
		throw;
	}
	return 0;
}
