/*
Copyright (c) 2015-2018 Alternative Games Ltd / Turo Lamminen

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


#include <cassert>
#include <cfloat>
#include <cinttypes>
#include <cstdio>

#include <thread>
#include <chrono>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tclap/CmdLine.h>

#include <pcg_random.hpp>

#include "renderer/Renderer.h"
#include "utils/Utils.h"

#include "AreaTex.h"
#include "SearchTex.h"

// AFTER Renderer.h because it sets GLM_FORCE_* macros which affect these
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// mingw fuckery...
#if defined(__GNUC__) && defined(_WIN32)

#include <mingw.thread.h>

#undef  PRIu64
#define PRIu64 "I64u"

#endif  // defined(__GNUC__) && defined(_WIN32)


namespace ShaderDefines {

using namespace glm;

#include "../shaderDefines.h"

}  // namespace ShaderDefines


using namespace renderer;


class SMAADemo;


enum class AAMethod : uint8_t {
	  MSAA
	, FXAA
	, SMAA
	, SMAA2X
	, LAST = SMAA2X
};


const char *name(AAMethod m) {
	switch (m) {
	case AAMethod::MSAA:
		return "MSAA";
		break;

	case AAMethod::FXAA:
		return "FXAA";
		break;

	case AAMethod::SMAA:
		return "SMAA";
		break;

	case AAMethod::SMAA2X:
		return "SMAA2X";
		break;
	}

	UNREACHABLE();
}


const char *smaaDebugModes[3] = { "None", "Edges", "Weights" };

static const unsigned int inputTextBufferSize = 1024;


const char* GetClipboardText(void* user_data) {
	char *clipboard = SDL_GetClipboardText();
	if (clipboard) {
		char *text = static_cast<char*>(user_data);
		size_t length = strnlen(clipboard, inputTextBufferSize - 1);
		strncpy(text, clipboard, length);
		text[length] = '\0';
		SDL_free(clipboard);
		return text;
	} else {
		return nullptr;
	}
}


void SetClipboardText(void* /* user_data */, const char* text) {
	SDL_SetClipboardText(text);
}


class RandomGen {
	pcg32 rng;

	RandomGen(const RandomGen &) = delete;
	RandomGen &operator=(const RandomGen &) = delete;
	RandomGen(RandomGen &&) = delete;
	RandomGen &operator=(RandomGen &&) = delete;

public:

	explicit RandomGen(uint64_t seed)
	: rng(seed)
	{
	}


	float randFloat() {
		uint32_t u = randU32();
		// because 24 bits mantissa
		u &= 0x00FFFFFFU;
		return float(u) / 0x00FFFFFFU;
	}


	uint32_t randU32() {
		return rng();
	}


	// min inclusive
	// max exclusive
	uint32_t range(uint32_t min, uint32_t max) {
		uint32_t range = max - min;
		uint32_t size = std::numeric_limits<uint32_t>::max() / range;
		uint32_t discard = size * range;

		uint32_t r;
		do {
			r = rng();
		} while (r >= discard);

		return min + r / size;
	}
};


static const char *const msaaQualityLevels[] =
{ "2x", "4x", "8x", "16x", "32x", "64x" };


static unsigned int msaaSamplesToQuality(unsigned int q) {
	assert(q > 1);
	assert(isPow2(q));

#ifdef __GNUC__

	return __builtin_ctz(q) - 1;

#else  // __GNUC__

	unsigned long bit = 0;
	_BitScanForward(&bit, q);
	return bit - 1;

#endif  // __GNUC__
}


static unsigned int msaaQualityToSamples(unsigned int n) {
	return (1 << (n + 1));
}


static const char *fxaaQualityLevels[] =
{ "10", "15", "20", "29", "39" };


static const unsigned int maxFXAAQuality = sizeof(fxaaQualityLevels) / sizeof(fxaaQualityLevels[0]);


static const char *smaaQualityLevels[] =
{ "CUSTOM", "LOW", "MEDIUM", "HIGH", "ULTRA" };


static const unsigned int maxSMAAQuality = sizeof(smaaQualityLevels) / sizeof(smaaQualityLevels[0]);


static const std::array<ShaderDefines::SMAAParameters, maxSMAAQuality> defaultSMAAParameters =
{ {
	  { 0.05f, 0.1f * 0.15f, 32u, 16u, 25u, 0u, 0u, 0u }  // custom
	, { 0.15f, 0.1f * 0.15f,  1u,  8u, 25u, 0u, 0u, 0u }  // low
	, { 0.10f, 0.1f * 0.10f,  1u,  8u, 25u, 0u, 0u, 0u }  // medium
	, { 0.10f, 0.1f * 0.10f, 16u,  8u, 25u, 0u, 0u, 0u }  // high
	, { 0.05f, 0.1f * 0.05f, 32u, 16u, 25u, 0u, 0u, 0u }  // ultra
} };


enum class SMAAEdgeMethod : uint8_t {
	  Color
	, Luma
	, Depth
};


struct Image {
	std::string    filename;
	std::string    shortName;
	TextureHandle  tex;
	unsigned int   width, height;


	Image()
	: width(0)
	, height(0)
	{
	}


	Image(const Image &)             = default;
	Image(Image &&)                  = default;

	Image &operator=(const Image &)  = default;
	Image &operator=(Image &&)       = default;

	~Image() {}
};


struct FXAAKey {
	unsigned int quality;
	// TODO: more options


	bool operator==(const FXAAKey &other) const {
		return this->quality == other.quality;
	}
};


struct SMAAKey {
	unsigned int quality;
	SMAAEdgeMethod  edgeMethod;
	bool            predication;
	// TODO: more options


	SMAAKey()
	: quality(0)
	, edgeMethod(SMAAEdgeMethod::Color)
    , predication(false)
	{
	}

	SMAAKey(const SMAAKey &)            = default;
	SMAAKey(SMAAKey &&)                 = default;

	SMAAKey &operator=(const SMAAKey &) = default;
	SMAAKey &operator=(SMAAKey &&)      = default;

	~SMAAKey() {}


	bool operator==(const SMAAKey &other) const {
		if (this->quality    != other.quality) {
			return false;
		}

		if (this->edgeMethod != other.edgeMethod) {
			return false;
		}

		if (this->predication != other.predication) {
			return false;
		}

		return true;
	}
};


struct SceneRPKey {
	uint8_t numSamples;
	Layout  layout;


	SceneRPKey()
	: numSamples(1)
	, layout(Layout::Undefined)
	{
	}

	SceneRPKey(const SceneRPKey &)            = default;
	SceneRPKey(SceneRPKey &&)                 = default;

	SceneRPKey &operator=(const SceneRPKey &) = default;
	SceneRPKey &operator=(SceneRPKey &&)      = default;

	~SceneRPKey() {}


	bool operator==(const SceneRPKey &other) const {
		if (this->numSamples != other.numSamples) {
			return false;
		}

		if (this->layout     != other.layout) {
			return false;
		}

		return true;
	}
};


namespace std {

	template <> struct hash<SMAAKey> {
		size_t operator()(const SMAAKey &k) const {
			uint64_t temp = 0;
			temp |= (static_cast<uint64_t>(k.quality)    <<  0);
			temp |= (static_cast<uint64_t>(k.edgeMethod) <<  8);
			temp |= (static_cast<uint64_t>(k.predication) <<  9);

			return hash<uint64_t>()(temp);
		}
	};

	template <> struct hash<FXAAKey> {
		size_t operator()(const FXAAKey &k) const {
			return hash<uint32_t>()(k.quality);
		}
	};


	template <> struct hash<SceneRPKey> {
		size_t operator()(const SceneRPKey &k) const {
			uint32_t temp = 0;
			temp |= (static_cast<uint32_t>(k.numSamples) << 0);
			temp |= (static_cast<uint32_t>(k.layout)     << 8);

			return hash<uint32_t>()(temp);
		}
	};

}  // namespace std


struct SMAAPipelines {
	PipelineHandle  edgePipeline;
	PipelineHandle  blendWeightPipeline;
	std::array<PipelineHandle, 2>  neighborPipelines;
};


class SMAADemo {
	// command line things
	bool            renderDebug;
	bool            tracing;
	bool            noShaderCache;
	bool            noShaderOpt;
	std::vector<std::string> imageFiles;

	// global window things
	unsigned int    windowWidth, windowHeight;
	bool            fullscreen;
	VSync           vsync;

	unsigned int    numFrames;
	bool            recreateSwapchain;
	bool            recreateFramebuffers;
	bool keepGoing;

	// aa things
	bool antialiasing;
	AAMethod aaMethod;
	bool          temporalAA;
	bool          temporalAAFirstFrame;
	unsigned int  temporalFrame;
	bool          temporalReproject;
	float         reprojectionWeightScale;
	// number of samples in current scene fb
	// 1 or 2 if SMAA
	// 2.. if MSAA
	unsigned int  numSamples;
	unsigned int  debugMode;
	unsigned int  colorMode;
	unsigned int  fxaaQuality;
	unsigned int  msaaQuality;
	unsigned int  maxMSAAQuality;
	SMAAKey       smaaKey;
	ShaderDefines::SMAAParameters  smaaParameters;

	float         predicationThreshold;
	float         predicationScale;
	float         predicationStrength;

	// timing things
	bool            fpsLimitActive;
	uint32_t        fpsLimit;
	uint64_t        sleepFudge;
	uint64_t      tickBase;
	uint64_t      lastTime;
	uint64_t      freqMult;
	uint64_t      freqDiv;

	// scene things
	// 0 for cubes
	// 1.. for images
	unsigned int  activeScene;
	unsigned int cubesPerSide;
	bool          rotateCubes;
	bool          visualizeCubeOrder;
	unsigned int  cubeOrderNum;
	float         cameraRotation;
	float         cameraDistance;
	uint64_t      rotationTime;
	unsigned int  rotationPeriodSeconds;
	RandomGen     random;
	std::vector<Image> images;
	std::vector<ShaderDefines::Cube> cubes;

	glm::mat4 currViewProj;
	glm::mat4 prevViewProj;

	Renderer        renderer;
	Format          depthFormat;

	VertexShaderHandle    cubeVertexShader;
	FragmentShaderHandle  cubeFragmentShader;

	std::unordered_map<uint32_t, PipelineHandle>  cubePipelines;
	PipelineHandle     imagePipeline;
	PipelineHandle     blitPipeline;
	PipelineHandle     guiPipeline;
	PipelineHandle                                separatePipeline;
	std::array<PipelineHandle, 2>                 temporalAAPipelines;

	RenderTargetHandle mainColorRT;
	RenderTargetHandle mainDepthRT;
	RenderTargetHandle velocityRT;
	RenderTargetHandle edgesRT;
	RenderTargetHandle blendWeightsRT;
	RenderTargetHandle finalRenderRT;
	std::array<RenderTargetHandle, 2>  resolveRTs;

	std::array<RenderTargetHandle, 2>  subsampleRTs;
	FramebufferHandle                  separateFB;

	std::unordered_map<SceneRPKey, RenderPassHandle>  sceneRenderPasses;
	FramebufferHandle  sceneFramebuffer;
	std::array<RenderPassHandle, 2>  fxaaRenderPass;
	RenderPassHandle   finalRenderPass;
	RenderPassHandle   separateRenderPass;
	RenderPassHandle   smaaBlendRenderPass;  // for temporal aa, otherwise it's part of final render pass
	std::array<RenderPassHandle, 2>   smaa2XBlendRenderPasses;
	RenderPassHandle   guiOnlyRenderPass;
	FramebufferHandle  finalFramebuffer;
	std::array<FramebufferHandle, 2>  resolveFBs;

	BufferHandle       cubeVBO;
	BufferHandle       cubeIBO;

	SamplerHandle      linearSampler;
	SamplerHandle      nearestSampler;

	std::unordered_map<FXAAKey, PipelineHandle> fxaaPipelines;
	std::unordered_map<SMAAKey, SMAAPipelines>  smaaPipelines;
	FramebufferHandle                           smaaEdgesFramebuffer;
	FramebufferHandle                           smaaWeightsFramebuffer;
	RenderPassHandle                            smaaEdgesRenderPass;
	RenderPassHandle                            smaaWeightsRenderPass;
	TextureHandle                               areaTex;
	TextureHandle                               searchTex;

	// gui / input things
	TextureHandle imguiFontsTex;
	bool          textInputActive;
	bool          rightShift, leftShift;
	char          imageFileName[inputTextBufferSize];
	char          clipboardText[inputTextBufferSize];


	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) = delete;
	SMAADemo &operator=(SMAADemo &&) = delete;

	const SMAAPipelines &getSMAAPipelines(const SMAAKey &key);
	const PipelineHandle &getFXAAPipeline(unsigned int q);

	RenderPassHandle getSceneRenderPass(unsigned int n, Layout l);
	PipelineHandle getCubePipeline(unsigned int n);


public:

	SMAADemo();

	~SMAADemo();

	void parseCommandLine(int argc, char *argv[]);

	void initRender();

	void createFramebuffers();

	void deleteFramebuffers();

	void createCubes();

	void shuffleCubeRendering();

	void reorderCubeRendering();

	void colorCubes();

	void mainLoopIteration();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();

	void doSMAA(RenderTargetHandle input, RenderPassHandle renderPass, FramebufferHandle outputFB, int pass);

	void doTemporalAA();

	void drawGUI(uint64_t elapsed);

	void loadImage(const std::string &filename);

	uint64_t getNanoseconds() {
		return (SDL_GetPerformanceCounter() - tickBase) * freqMult / freqDiv;
	}

};


SMAADemo::SMAADemo()
: renderDebug(false)
, tracing(false)
, noShaderCache(false)
, noShaderOpt(false)

, windowWidth(1280)
, windowHeight(720)
, fullscreen(false)
, vsync(VSync::On)

, numFrames(3)
, recreateSwapchain(false)
, recreateFramebuffers(false)
, keepGoing(true)

, antialiasing(true)
, aaMethod(AAMethod::SMAA)
, temporalAA(false)
, temporalAAFirstFrame(false)
, temporalFrame(0)
, temporalReproject(true)
, reprojectionWeightScale(30.0f)
, numSamples(1)
, debugMode(0)
, colorMode(0)
, fxaaQuality(maxFXAAQuality - 1)
, msaaQuality(0)
, maxMSAAQuality(1)
, predicationThreshold(0.01f)
, predicationScale(2.0f)
, predicationStrength(0.4f)

, fpsLimitActive(true)
, fpsLimit(0)
, sleepFudge(0)
, tickBase(0)
, lastTime(0)
, freqMult(0)
, freqDiv(0)

, activeScene(0)
, cubesPerSide(8)
, rotateCubes(false)
, visualizeCubeOrder(false)
, cubeOrderNum(1)
, cameraRotation(0.0f)
, cameraDistance(25.0f)
, rotationTime(0)
, rotationPeriodSeconds(30)
, random(1)

, depthFormat(Format::Invalid)

, textInputActive(false)
, rightShift(false)
, leftShift(false)
{
	smaaKey.quality = maxSMAAQuality - 1;
	smaaParameters  = defaultSMAAParameters[smaaKey.quality];

	uint64_t freq = SDL_GetPerformanceFrequency();
	tickBase      = SDL_GetPerformanceCounter();

	freqMult   = 1000000000ULL;
	freqDiv    = freq;
	uint64_t g = gcd(freqMult, freqDiv);
	freqMult  /= g;
	freqDiv   /= g;
	LOG("freqMult: %" PRIu64 "\n", freqMult);
	LOG("freqDiv: %"  PRIu64 "\n", freqDiv);

	lastTime = getNanoseconds();

	// measure minimum sleep length and use it as fudge factor
	sleepFudge = 1000ULL * 1000ULL;
	for (unsigned int i = 0; i < 8; i++) {
		std::this_thread::sleep_for(std::chrono::nanoseconds(1));
		uint64_t ticks = getNanoseconds();
		uint64_t diff  = ticks - lastTime;
		sleepFudge     = std::min(sleepFudge, diff);
		lastTime       = ticks;
	}

	LOG("sleep fudge (nanoseconds): %" PRIu64 "\n", sleepFudge);

	memset(imageFileName, 0, inputTextBufferSize);
	memset(clipboardText, 0, inputTextBufferSize);
}


SMAADemo::~SMAADemo() {
	ImGui::Shutdown();

	if (sceneFramebuffer) {
		deleteFramebuffers();

		for (auto rp : sceneRenderPasses) {
			renderer.deleteRenderPass(rp.second);
		}
		sceneRenderPasses.clear();

		assert(finalRenderPass);
		renderer.deleteRenderPass(finalRenderPass);
		for (unsigned int i = 0; i < 2; i++) {
			assert(fxaaRenderPass[i]);
			renderer.deleteRenderPass(fxaaRenderPass[i]);
		}

		assert(smaaBlendRenderPass);
		renderer.deleteRenderPass(smaaBlendRenderPass);
		for (unsigned int i = 0; i < 2; i++) {
			assert(smaa2XBlendRenderPasses[i]);
			renderer.deleteRenderPass(smaa2XBlendRenderPasses[i]);
		}

		assert(guiOnlyRenderPass);
		renderer.deleteRenderPass(guiOnlyRenderPass);
		assert(smaaEdgesRenderPass);
		renderer.deleteRenderPass(smaaEdgesRenderPass);
		assert(smaaWeightsRenderPass);
		renderer.deleteRenderPass(smaaWeightsRenderPass);
		assert(separateRenderPass);
		renderer.deleteRenderPass(separateRenderPass);
	}

	if (cubeVBO) {
		renderer.deleteBuffer(cubeVBO);
		cubeVBO = BufferHandle();

		renderer.deleteBuffer(cubeIBO);
		cubeIBO = BufferHandle();
	}

	if (linearSampler) {
		renderer.deleteSampler(linearSampler);
		linearSampler = SamplerHandle();

		renderer.deleteSampler(nearestSampler);
		nearestSampler = SamplerHandle();
	}

	if (areaTex) {
		renderer.deleteTexture(areaTex);
		areaTex = TextureHandle();

		renderer.deleteTexture(searchTex);
		searchTex = TextureHandle();
	}
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


void SMAADemo::parseCommandLine(int argc, char *argv[]) {
	try {
		TCLAP::CmdLine cmd("SMAA demo", ' ', "1.0");

		TCLAP::SwitchArg                       debugSwitch("",        "debug",      "Enable renderer debugging",     cmd, false);
		TCLAP::SwitchArg                       tracingSwitch("",      "trace",      "Enable renderer tracing",       cmd, false);
		TCLAP::SwitchArg                       noCacheSwitch("",      "nocache",    "Don't load shaders from cache", cmd, false);
		TCLAP::SwitchArg                       noOptSwitch("",        "noopt",      "Don't optimize shaders",        cmd, false);
		TCLAP::SwitchArg                       fullscreenSwitch("f",  "fullscreen", "Start in fullscreen mode",      cmd, false);
		TCLAP::SwitchArg                       noVsyncSwitch("",      "novsync",    "Disable vsync",                 cmd, false);

		TCLAP::ValueArg<unsigned int>          windowWidthSwitch("",  "width",      "Window width",  false, windowWidth,  "width",  cmd);
		TCLAP::ValueArg<unsigned int>          windowHeightSwitch("", "height",     "Window height", false, windowHeight, "height", cmd);

		TCLAP::ValueArg<unsigned int>          rotateSwitch("",       "rotate",     "Rotation period", false, 0,          "seconds", cmd);

		TCLAP::ValueArg<std::string>           aaMethodSwitch("m",    "method",     "AA Method",     false, "SMAA",        "SMAA/FXAA/MSAA", cmd);
		TCLAP::ValueArg<std::string>           aaQualitySwitch("q",   "quality",    "AA Quality",    false, "",            "", cmd);

		TCLAP::UnlabeledMultiArg<std::string>  imagesArg("images",    "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		renderDebug   = debugSwitch.getValue();
		tracing       = tracingSwitch.getValue();
		noShaderCache = noCacheSwitch.getValue();
		noShaderOpt   = noOptSwitch.getValue();
		fullscreen    = fullscreenSwitch.getValue();
		windowWidth   = windowWidthSwitch.getValue();
		windowHeight  = windowHeightSwitch.getValue();
		vsync         = noVsyncSwitch.getValue() ? VSync::Off : VSync::On;

		unsigned int r = rotateSwitch.getValue();
		if (r != 0) {
			rotateCubes           = true;
			rotationPeriodSeconds = std::max(1U, std::min(r, 60U));
		}

		std::string aaMethodStr = aaMethodSwitch.getValue();
		std::transform(aaMethodStr.begin(), aaMethodStr.end(), aaMethodStr.begin(), ::toupper);
		std::string aaQualityStr = aaQualitySwitch.getValue();
		if (aaMethodStr == "SMAA") {
			aaMethod = AAMethod::SMAA;

			if (!aaQualityStr.empty()) {
				std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
				for (unsigned int i = 0; i < maxSMAAQuality; i++) {
					if (aaQualityStr == smaaQualityLevels[i]) {
						smaaKey.quality = i;
						break;
					}
				}
			}
		} else if (aaMethodStr == "SMAA2X") {
			aaMethod = AAMethod::SMAA2X;

			if (!aaQualityStr.empty()) {
				std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
				for (unsigned int i = 0; i < maxSMAAQuality; i++) {
					if (aaQualityStr == smaaQualityLevels[i]) {
						smaaKey.quality = i;
						break;
					}
				}
			}
		} else if (aaMethodStr == "FXAA") {
			aaMethod = AAMethod::FXAA;

			if (!aaQualityStr.empty()) {
				std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
				for (unsigned int i = 0; i < maxFXAAQuality; i++) {
					if (aaQualityStr == fxaaQualityLevels[i]) {
						fxaaQuality = i;
						break;
					}
				}
			}
		} else if (aaMethodStr == "MSAA") {
			aaMethod = AAMethod::MSAA;

			int n = atoi(aaQualityStr.c_str());
			if (n > 0) {
				if (!isPow2(n)) {
					n = nextPow2(n);
				}

				msaaQuality = msaaSamplesToQuality(n);
			}

		} else {
			LOG("Bad AA method \"%s\"\n", aaMethodStr.c_str());
			fprintf(stderr, "Bad AA method \"%s\"\n", aaMethodStr.c_str());
			exit(1);
		}

		imageFiles    = imagesArg.getValue();

	} catch (TCLAP::ArgException &e) {
		LOG("parseCommandLine exception: %s for arg %s\n", e.error().c_str(), e.argId().c_str());
	} catch (...) {
		LOG("parseCommandLine: unknown exception\n");
	}
}


struct GlobalDS {
	BufferHandle   globalUniforms;
	SamplerHandle  linearSampler;
	SamplerHandle  nearestSampler;


	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout GlobalDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(GlobalDS, globalUniforms) }
	, { DescriptorType::Sampler,        offsetof(GlobalDS, linearSampler ) }
	, { DescriptorType::Sampler,        offsetof(GlobalDS, nearestSampler) }
	, { DescriptorType::End,            0                                  }
};

DSLayoutHandle GlobalDS::layoutHandle;


struct CubeSceneDS {
    BufferHandle instances;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout CubeSceneDS::layout[] = {
	  { DescriptorType::StorageBuffer,  offsetof(CubeSceneDS, instances) }
	, { DescriptorType::End,            0                                }
};

DSLayoutHandle CubeSceneDS::layoutHandle;


struct ColorCombinedDS {
	CSampler color;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout ColorCombinedDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(ColorCombinedDS, color) }
	, { DescriptorType::End,              0,                          }
};

DSLayoutHandle ColorCombinedDS::layoutHandle;


struct ColorTexDS {
	TextureHandle color;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout ColorTexDS::layout[] = {
	  { DescriptorType::Texture,  offsetof(ColorTexDS, color) }
	, { DescriptorType::End,      0,                        }
};

DSLayoutHandle ColorTexDS::layoutHandle;


struct EdgeDetectionDS {
	CSampler color;
	CSampler predicationTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout EdgeDetectionDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(EdgeDetectionDS, color) }
	, { DescriptorType::CombinedSampler,  offsetof(EdgeDetectionDS, predicationTex) }
	, { DescriptorType::End,              0,                               }
};

DSLayoutHandle EdgeDetectionDS::layoutHandle;


struct BlendWeightDS {
	CSampler edgesTex;
	CSampler areaTex;
	CSampler searchTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout BlendWeightDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, edgesTex)       }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, areaTex)        }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, searchTex)      }
	, { DescriptorType::End,              0,                                      }
};

DSLayoutHandle BlendWeightDS::layoutHandle;


struct NeighborBlendDS {
	CSampler color;
	CSampler blendweights;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout NeighborBlendDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, color)              }
	, { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, blendweights)       }
	, { DescriptorType::End        ,      0                                             }
};

DSLayoutHandle NeighborBlendDS::layoutHandle;


struct TemporalAADS {
	CSampler currentTex;
	CSampler previousTex;
	CSampler velocityTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout TemporalAADS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, currentTex)  }
	, { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, previousTex) }
	, { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, velocityTex) }
	, { DescriptorType::End        ,      0                                   }
};

DSLayoutHandle TemporalAADS::layoutHandle;



static const int numDepths = 5;
static const std::array<Format, numDepths> depths
  = { { Format::Depth24X8, Format::Depth24S8, Format::Depth32Float, Format::Depth16, Format::Depth16S8 } };


void SMAADemo::initRender() {
	RendererDesc desc;
	desc.debug                = renderDebug;
	desc.tracing              = tracing;
	desc.skipShaderCache      = noShaderCache;
	desc.optimizeShaders      = !noShaderOpt;
	desc.swapchain.fullscreen = fullscreen;
	desc.swapchain.width      = windowWidth;
	desc.swapchain.height     = windowHeight;
	desc.swapchain.vsync      = vsync;

	renderer = Renderer::createRenderer(desc);
	const auto &features = renderer.getFeatures();
	LOG("Max MSAA samples: %u\n",  features.maxMSAASamples);
	LOG("sRGB frame buffer: %s\n", features.sRGBFramebuffer ? "yes" : "no");
	LOG("SSBO support: %s\n",      features.SSBOSupported ? "yes" : "no");
	maxMSAAQuality = msaaSamplesToQuality(features.maxMSAASamples) + 1;
	if (msaaQuality >= maxMSAAQuality) {
		msaaQuality = maxMSAAQuality - 1;
	}

	unsigned int refreshRate = renderer.getCurrentRefreshRate();

	if (refreshRate == 0) {
		LOG("Failed to get current refresh rate, using max\n");
		refreshRate = renderer.getMaxRefreshRate();
	}

	if (refreshRate == 0) {
		LOG("Failed to get refresh rate, defaulting to 60\n");
		fpsLimit = 2 * 60;
	} else {
		fpsLimit = 2 * refreshRate;
	}

	for (auto depth : depths) {
		if (renderer.isRenderTargetFormatSupported(depth)) {
			depthFormat = depth;
			break;
		}
	}
	if (depthFormat == Format::Invalid) {
		throw std::runtime_error("no supported depth formats");
	}
	LOG("Using depth format %s\n", formatName(depthFormat));

	renderer.registerDescriptorSetLayout<GlobalDS>();
	renderer.registerDescriptorSetLayout<CubeSceneDS>();
	renderer.registerDescriptorSetLayout<ColorCombinedDS>();
	renderer.registerDescriptorSetLayout<ColorTexDS>();
	renderer.registerDescriptorSetLayout<EdgeDetectionDS>();
	renderer.registerDescriptorSetLayout<BlendWeightDS>();
	renderer.registerDescriptorSetLayout<NeighborBlendDS>();
	renderer.registerDescriptorSetLayout<TemporalAADS>();

	{
		RenderPassDesc rpDesc;
		// TODO: check this
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment);
		finalRenderPass       = renderer.createRenderPass(rpDesc.name("final"));
	}

	{
		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment);
		fxaaRenderPass[0]     = renderer.createRenderPass(rpDesc.name("FXAA no temporal"));
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead);
		fxaaRenderPass[1]     = renderer.createRenderPass(rpDesc.name("FXAA temporal"));
	}

	{
		RenderPassDesc rpDesc;
		// FIXME: should be RGBA since SMAA wants gamma space?
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead);
		smaaBlendRenderPass   = renderer.createRenderPass(rpDesc.name("SMAA blend"));
	}

	for (unsigned int i = 0; i < 2; i++) {
		RenderPassDesc rpDesc;
		if (i == 0) {
			rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment);
		} else {
			assert(i == 1);
			rpDesc.color(0, Format::sRGBA8, PassBegin::Keep, Layout::ColorAttachment, Layout::ColorAttachment);
		}
		smaa2XBlendRenderPasses[i] = renderer.createRenderPass(rpDesc.name("SMAA2x blend " + std::to_string(i)));
	}

	{
		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Keep, Layout::ColorAttachment, Layout::TransferSrc);
		guiOnlyRenderPass     = renderer.createRenderPass(rpDesc.name("GUI only"));
	}

	{
		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead);

		smaaEdgesRenderPass   = renderer.createRenderPass(rpDesc.name("SMAA edges"));
		smaaWeightsRenderPass = renderer.createRenderPass(rpDesc.name("SMAA weights"));
	}

	{
		RenderPassDesc rpDesc;
		rpDesc.name("Separate")
		      .color(0, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead)
		      .color(1, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead);
		separateRenderPass       = renderer.createRenderPass(rpDesc);
	}

	createFramebuffers();

	{
		ShaderMacros macros;

		auto vertexShader   = renderer.createVertexShader("image", macros);
		auto fragmentShader = renderer.createFragmentShader("image", macros);

		// image is always rendered with 1 sample so we ask for that renderpass
		// instead of numSamples
		PipelineDesc plDesc;
		plDesc.renderPass(getSceneRenderPass(1, Layout::ShaderRead))
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader(vertexShader)
		      .fragmentShader(fragmentShader)
		      .name("image");

		imagePipeline = renderer.createPipeline(plDesc);
	}

	{
		ShaderMacros macros;

		cubeVertexShader   = renderer.createVertexShader("cube", macros);
		cubeFragmentShader = renderer.createFragmentShader("cube", macros);
	}

	{
		ShaderMacros macros;

		auto vertexShader   = renderer.createVertexShader("blit", macros);
		auto fragmentShader = renderer.createFragmentShader("blit", macros);

		PipelineDesc plDesc;
		plDesc.renderPass(finalRenderPass)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader(vertexShader)
		      .fragmentShader(fragmentShader)
		      .name("blit");

		blitPipeline = renderer.createPipeline(plDesc);
	}

	{
		ShaderMacros macros;

		auto vertexShader   = renderer.createVertexShader("gui", macros);
		auto fragmentShader = renderer.createFragmentShader("gui", macros);

		PipelineDesc plDesc;
		plDesc.renderPass(guiOnlyRenderPass)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader(vertexShader)
		      .fragmentShader(fragmentShader)
		      .blending(true)
		      .sourceBlend(BlendFunc::SrcAlpha)
		      .destinationBlend(BlendFunc::OneMinusSrcAlpha)
		      .scissorTest(true)
		      .vertexAttrib(ATTR_POS,   0, 2, VtxFormat::Float,  offsetof(ImDrawVert, pos))
		      .vertexAttrib(ATTR_UV,    0, 2, VtxFormat::Float,  offsetof(ImDrawVert, uv))
		      .vertexAttrib(ATTR_COLOR, 0, 4, VtxFormat::UNorm8, offsetof(ImDrawVert, col))
		      .vertexBufferStride(ATTR_POS, sizeof(ImDrawVert))
		      .name("gui");

		guiPipeline = renderer.createPipeline(plDesc);
	}

	{
		ShaderMacros macros;
		auto vertexShader   = renderer.createVertexShader("temporal", macros);  // TODO: better naming, should be 2D

		for (unsigned int i = 0; i < 2; i++) {
			macros.emplace("SMAA_REPROJECTION", std::to_string(i));

			auto fragmentShader = renderer.createFragmentShader("temporal", macros);

			PipelineDesc plDesc;
			plDesc.renderPass(smaaBlendRenderPass)
				  .descriptorSetLayout<GlobalDS>(0)
				  .descriptorSetLayout<TemporalAADS>(1)
				  .vertexShader(vertexShader)
				  .fragmentShader(fragmentShader)
				  .name("temporal AA");

			temporalAAPipelines[i] = renderer.createPipeline(plDesc);
		}
	}

	{
		ShaderMacros macros;
		auto vertexShader   = renderer.createVertexShader("temporal", macros);  // TODO: better naming, should be 2D
		auto fragmentShader = renderer.createFragmentShader("separate", macros);

		PipelineDesc plDesc;
		plDesc.renderPass(separateRenderPass)
			  .descriptorSetLayout<GlobalDS>(0)
			  .descriptorSetLayout<ColorCombinedDS>(1)  // TODO: does this need its own DS?
			  .vertexShader(vertexShader)
			  .fragmentShader(fragmentShader)
			  .name("subsample separate");

		separatePipeline = renderer.createPipeline(plDesc);
	}

	linearSampler  = renderer.createSampler(SamplerDesc().minFilter(FilterMode::Linear). magFilter(FilterMode::Linear) .name("linear"));
	nearestSampler = renderer.createSampler(SamplerDesc().minFilter(FilterMode::Nearest).magFilter(FilterMode::Nearest).name("nearest"));

	cubeVBO = renderer.createBuffer(BufferType::Vertex, sizeof(vertices), &vertices[0]);
	cubeIBO = renderer.createBuffer(BufferType::Index, sizeof(indices), &indices[0]);

#ifdef RENDERER_OPENGL

	const bool flipSMAATextures = true;

#else  // RENDERER_OPENGL

	const bool flipSMAATextures = false;

#endif  // RENDERER_OPENGL

	TextureDesc texDesc;
	texDesc.width(AREATEX_WIDTH)
	       .height(AREATEX_HEIGHT)
	       .format(Format::RG8);
	texDesc.name("SMAA area texture");

	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(AREATEX_SIZE);
		for (unsigned int y = 0; y < AREATEX_HEIGHT; y++) {
			unsigned int srcY = AREATEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * AREATEX_PITCH], areaTexBytes + srcY * AREATEX_PITCH, AREATEX_PITCH);
		}
		texDesc.mipLevelData(0, &tempBuffer[0], AREATEX_SIZE);
		areaTex = renderer.createTexture(texDesc);
	} else {
		texDesc.mipLevelData(0, areaTexBytes, AREATEX_SIZE);
		areaTex = renderer.createTexture(texDesc);
	}

	texDesc.width(SEARCHTEX_WIDTH)
	       .height(SEARCHTEX_HEIGHT)
	       .format(Format::R8);
	texDesc.name("SMAA search texture");
	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(SEARCHTEX_SIZE);
		for (unsigned int y = 0; y < SEARCHTEX_HEIGHT; y++) {
			unsigned int srcY = SEARCHTEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * SEARCHTEX_PITCH], searchTexBytes + srcY * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
		}
		texDesc.mipLevelData(0, &tempBuffer[0], SEARCHTEX_SIZE);
		searchTex = renderer.createTexture(texDesc);
	} else {
		texDesc.mipLevelData(0, searchTexBytes, SEARCHTEX_SIZE);
		searchTex = renderer.createTexture(texDesc);
	}

	images.reserve(imageFiles.size());
	for (const auto &filename : imageFiles) {
		loadImage(filename);
	}

	// imgui setup
	{
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename                 = nullptr;
		io.KeyMap[ImGuiKey_Tab]        = SDL_SCANCODE_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
		io.KeyMap[ImGuiKey_LeftArrow]  = SDL_SCANCODE_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow]    = SDL_SCANCODE_UP;
		io.KeyMap[ImGuiKey_DownArrow]  = SDL_SCANCODE_DOWN;
		io.KeyMap[ImGuiKey_PageUp]     = SDL_SCANCODE_PAGEUP;
		io.KeyMap[ImGuiKey_PageDown]   = SDL_SCANCODE_PAGEDOWN;
		io.KeyMap[ImGuiKey_Home]       = SDL_SCANCODE_HOME;
		io.KeyMap[ImGuiKey_End]        = SDL_SCANCODE_END;
		io.KeyMap[ImGuiKey_Delete]     = SDL_SCANCODE_DELETE;
		io.KeyMap[ImGuiKey_Backspace]  = SDL_SCANCODE_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter]      = SDL_SCANCODE_RETURN;
		io.KeyMap[ImGuiKey_Escape]     = SDL_SCANCODE_ESCAPE;
		io.KeyMap[ImGuiKey_A]          = SDL_SCANCODE_A;
		io.KeyMap[ImGuiKey_C]          = SDL_SCANCODE_C;
		io.KeyMap[ImGuiKey_V]          = SDL_SCANCODE_V;
		io.KeyMap[ImGuiKey_X]          = SDL_SCANCODE_X;
		io.KeyMap[ImGuiKey_Y]          = SDL_SCANCODE_Y;
		io.KeyMap[ImGuiKey_Z]          = SDL_SCANCODE_Z;

		// TODO: clipboard
		io.SetClipboardTextFn = SetClipboardText;
		io.GetClipboardTextFn = GetClipboardText;
		io.ClipboardUserData  = clipboardText;

		// Build texture atlas
		unsigned char *pixels = nullptr;
		int width = 0, height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		texDesc.width(width)
		       .height(height)
		       .format(Format::sRGBA8)
		       .name("GUI")
		       .mipLevelData(0, pixels, width * height * 4);
		imguiFontsTex = renderer.createTexture(texDesc);
		io.Fonts->TexID = nullptr;
	}
}


RenderPassHandle SMAADemo::getSceneRenderPass(unsigned int n, Layout l) {
	SceneRPKey k;
	k.numSamples = n;
	k.layout     = l;
	auto it = sceneRenderPasses.find(k);

	if (it == sceneRenderPasses.end()) {
		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, l)
		      .color(1, Format::RG16Float, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
		      .depthStencil(depthFormat, PassBegin::Clear)
		      .clearDepth(1.0f)
		      .numSamples(n);

		std::string name = "scene ";
		if (n > 1) {
			name += " MSAA x" + std::to_string(n) + " ";
		}
		name += layoutName(l);

		RenderPassHandle rp = renderer.createRenderPass(rpDesc.name(name));
		bool inserted = false;
		std::tie(it, inserted) = sceneRenderPasses.emplace(k, rp);
		assert(inserted);
	}

	return it->second;
}


PipelineHandle SMAADemo::getCubePipeline(unsigned int n) {
	auto it = cubePipelines.find(n);

	if (it == cubePipelines.end()) {
		std::string name = "cubes";
		if (n > 1) {
			name += " MSAA x" + std::to_string(n);
		}

		/*
		 Vulkan spec says:
		 Two render passes are compatible if their corresponding color, input,
		 resolve, and depth/stencil attachment references are compatible and
		 if they are otherwise identical except for:
		 * Initial and final image layout in attachment descriptions
		 * Load and store operations in attachment descriptions
		 * Image layout in attachment references

		 so we can just use Layout::ShaderRead when creating
		 no matter which one is used when rendering */
		PipelineDesc plDesc;
		plDesc.name(name)
		      .vertexShader(cubeVertexShader)
		      .fragmentShader(cubeFragmentShader)
		      .renderPass(getSceneRenderPass(n, Layout::ShaderRead))
		      .numSamples(n)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<CubeSceneDS>(1)
		      .vertexAttrib(ATTR_POS, 0, 3, VtxFormat::Float, 0)
		      .vertexBufferStride(ATTR_POS, sizeof(Vertex))
		      .depthWrite(true)
		      .depthTest(true)
		      .cullFaces(true);
		bool inserted = false;
		std::tie(it, inserted) = cubePipelines.emplace(n, renderer.createPipeline(plDesc));
		assert(inserted);
	}

	return it->second;
}


const SMAAPipelines &SMAADemo::getSMAAPipelines(const SMAAKey &key) {
	auto it = smaaPipelines.find(key);
	// create lazily if missing
	// TODO: final blend pass appears to not be affected by quality
	// verify that and if so, share it
	if (it == smaaPipelines.end()) {
		PipelineDesc plDesc;
		plDesc.depthWrite(false)
			  .depthTest(false)
			  .cullFaces(true);
		plDesc.descriptorSetLayout<GlobalDS>(0);

		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[key.quality]);
		macros.emplace(qualityString, "1");
		if (key.edgeMethod != SMAAEdgeMethod::Color) {
			// TODO: edge detection method only affects the first pass, share others
			// TODO: also doesn't affect vertex shader
			macros.emplace("EDGEMETHOD", std::to_string(static_cast<uint8_t>(key.edgeMethod)));
		}

		if (key.predication && key.edgeMethod != SMAAEdgeMethod::Depth) {
			// TODO: predication only affects the first pass, share others
			// TODO: also doesn't affect vertex shader
			macros.emplace("SMAA_PREDICATION", "1");
		}

		auto vertexShader   = renderer.createVertexShader("smaaEdge", macros);
		auto fragmentShader = renderer.createFragmentShader("smaaEdge", macros);

		plDesc.renderPass(smaaEdgesRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<EdgeDetectionDS>(1);
		std::string passName = std::string("SMAA edges ") + std::to_string(key.quality);
		plDesc.name(passName.c_str());

		SMAAPipelines pipelines;
		pipelines.edgePipeline      = renderer.createPipeline(plDesc);

		vertexShader                = renderer.createVertexShader("smaaBlendWeight", macros);
		fragmentShader              = renderer.createFragmentShader("smaaBlendWeight", macros);
		plDesc.renderPass(smaaWeightsRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<BlendWeightDS>(1);
		passName = std::string("SMAA weights ") + std::to_string(key.quality);
		plDesc.name(passName.c_str());
		pipelines.blendWeightPipeline = renderer.createPipeline(plDesc);

		vertexShader                = renderer.createVertexShader("smaaNeighbor", macros);
		fragmentShader              = renderer.createFragmentShader("smaaNeighbor", macros);
		plDesc.renderPass(finalRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<NeighborBlendDS>(1);
		passName = std::string("SMAA blend ") + std::to_string(key.quality);
		plDesc.name(passName.c_str());
		pipelines.neighborPipelines[0] = renderer.createPipeline(plDesc);
		plDesc.blending(true)
		      .sourceBlend(BlendFunc::Constant)
		      .destinationBlend(BlendFunc::Constant);
		pipelines.neighborPipelines[1] = renderer.createPipeline(plDesc);

		bool inserted = false;
		std::tie(it, inserted) = smaaPipelines.emplace(std::move(key), std::move(pipelines));
		assert(inserted);
	}

	return it->second;
}


const PipelineHandle &SMAADemo::getFXAAPipeline(unsigned int q) {
	FXAAKey key;
	key.quality = q;

	auto it = fxaaPipelines.find(key);
	// create lazily if missing
	if (it == fxaaPipelines.end()) {
		// TODO: vertex shader not affected by quality, share it
		PipelineDesc plDesc;
		plDesc.depthWrite(false)
			  .depthTest(false)
			  .cullFaces(true);
		plDesc.descriptorSetLayout<GlobalDS>(0);

		std::string qualityString(fxaaQualityLevels[q]);

		ShaderMacros macros;
		macros.emplace("FXAA_QUALITY_PRESET", qualityString);
		auto vertexShader   = renderer.createVertexShader("fxaa", macros);
		auto fragmentShader = renderer.createFragmentShader("fxaa", macros);
		plDesc.renderPass(finalRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<ColorCombinedDS>(1);
		std::string passName = std::string("FXAA ") + std::to_string(q);
		plDesc.name(passName.c_str());

		bool inserted = false;
		std::tie(it, inserted) = fxaaPipelines.emplace(std::move(key), renderer.createPipeline(plDesc));
		assert(inserted);
	}


	return it->second;
}


void SMAADemo::loadImage(const std::string &filename) {
	int width = 0, height = 0;
	unsigned char *imageData = stbi_load(filename.c_str(), &width, &height, NULL, 4);
	LOG(" %s : %p  %dx%d\n", filename.c_str(), imageData, width, height);
	if (!imageData) {
		LOG("Bad image: %s\n", stbi_failure_reason());
		return;
	}

	images.push_back(Image());
	auto &img      = images.back();
	img.filename   = filename;
	auto lastSlash = filename.rfind('/');
	if (lastSlash != std::string::npos) {
		img.shortName = filename.substr(lastSlash + 1);
	}
	else {
		img.shortName = filename;
	}

	TextureDesc texDesc;
	texDesc.width(width)
	       .height(height)
	       .name(img.shortName)
	       .format(Format::sRGBA8);

	texDesc.mipLevelData(0, imageData, width * height * 4);
	img.width  = width;
	img.height = height;
	img.tex    = renderer.createTexture(texDesc);

	stbi_image_free(imageData);

	activeScene = static_cast<unsigned int>(images.size());
}


void SMAADemo::createFramebuffers() {
	if (sceneFramebuffer) {
		deleteFramebuffers();
	}

	if (antialiasing && aaMethod == AAMethod::MSAA) {
		numSamples = msaaQualityToSamples(msaaQuality);
		assert(numSamples > 1);
	} else if (antialiasing && aaMethod == AAMethod::SMAA2X) {
		numSamples = 2;
	} else {
		numSamples = 1;
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("main color")
		      .numSamples(numSamples)
		      .format(Format::sRGBA8)
		      .additionalViewFormat(Format::RGBA8)
		      .width(windowWidth)
		      .height(windowHeight);
		mainColorRT = renderer.createRenderTarget(rtDesc);
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("velocity")
		      .numSamples(numSamples)
		      .format(Format::RG16Float)
		      .width(windowWidth)
		      .height(windowHeight);
		velocityRT = renderer.createRenderTarget(rtDesc);
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("final")
		      .format(Format::sRGBA8)
		      .width(windowWidth)
		      .height(windowHeight);
		finalRenderRT = renderer.createRenderTarget(rtDesc);
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("main depth")
		      .numSamples(numSamples)
		      .format(depthFormat)
		      .width(windowWidth)
		      .height(windowHeight);
		mainDepthRT = renderer.createRenderTarget(rtDesc);
	}

	{
		FramebufferDesc fbDesc;
		fbDesc.name("scene")
		      .renderPass(getSceneRenderPass(numSamples, Layout::ShaderRead))
		      .color(0, mainColorRT)
		      .color(1, velocityRT)
		      .depthStencil(mainDepthRT);
		sceneFramebuffer = renderer.createFramebuffer(fbDesc);
	}

	{
		FramebufferDesc fbDesc;
		fbDesc.name("final")
		      .renderPass(finalRenderPass)
		      .color(0, finalRenderRT);
		finalFramebuffer = renderer.createFramebuffer(fbDesc);
	}

	// SMAA edges texture and FBO
	{
		RenderTargetDesc rtDesc;
		rtDesc.name("SMAA edges")
		      .format(Format::RGBA8)
		      .width(windowWidth)
		      .height(windowHeight);
		edgesRT = renderer.createRenderTarget(rtDesc);

		FramebufferDesc fbDesc;
		fbDesc.name("SMAA edges")
		      .renderPass(smaaEdgesRenderPass)
		      .color(0, edgesRT);
		smaaEdgesFramebuffer = renderer.createFramebuffer(fbDesc);
	}

	// SMAA blending weights texture and FBO
	{
		RenderTargetDesc rtDesc;
		rtDesc.name("SMAA weights")
		      .format(Format::RGBA8)
		      .width(windowWidth)
		      .height(windowHeight);
		blendWeightsRT = renderer.createRenderTarget(rtDesc);

		FramebufferDesc fbDesc;
		fbDesc.name("SMAA weights")
		      .renderPass(smaaWeightsRenderPass)
		      .color(0, blendWeightsRT);
		smaaWeightsFramebuffer = renderer.createFramebuffer(fbDesc);
	}

	if (temporalAA) {
		temporalAAFirstFrame = true;
		RenderTargetDesc rtDesc;
		rtDesc.name("Temporal resolve 0")
		      .format(Format::sRGBA8)  // TODO: not right?
		      .width(windowWidth)
		      .height(windowHeight);
		resolveRTs[0] = renderer.createRenderTarget(rtDesc);

		FramebufferDesc fbDesc;
		fbDesc.name("Temporal resolve 0")
		      .renderPass(smaaBlendRenderPass)
		      .color(0, resolveRTs[0]);
		resolveFBs[0] = renderer.createFramebuffer(fbDesc);

		rtDesc.name("Temporal resolve 1");
		resolveRTs[1] = renderer.createRenderTarget(rtDesc);

		fbDesc.color(0, resolveRTs[1])
		      .name("Temporal resolve 1");
		resolveFBs[1] = renderer.createFramebuffer(fbDesc);
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.format(Format::sRGBA8)
		      .additionalViewFormat(Format::RGBA8)
		      .width(windowWidth)
		      .height(windowHeight);

		for (unsigned int i = 0; i < 2; i++) {
			rtDesc.name("Temporal resolve" + std::to_string(i));
			subsampleRTs[i] = renderer.createRenderTarget(rtDesc);
		}

		FramebufferDesc fbDesc;
		fbDesc.name("Separate")
		      .renderPass(separateRenderPass)
		      .color(0, subsampleRTs[0])
		      .color(1, subsampleRTs[1]);
		separateFB = renderer.createFramebuffer(fbDesc);
	}
}


void SMAADemo::deleteFramebuffers() {
	assert(sceneFramebuffer);
	renderer.deleteFramebuffer(sceneFramebuffer);

	assert(finalFramebuffer);
	renderer.deleteFramebuffer(finalFramebuffer);

	assert(smaaEdgesFramebuffer);
	renderer.deleteFramebuffer(smaaEdgesFramebuffer);

	assert(smaaWeightsFramebuffer);
	renderer.deleteFramebuffer(smaaWeightsFramebuffer);

	assert(mainColorRT);
	renderer.deleteRenderTarget(mainColorRT);

	assert(velocityRT);
	renderer.deleteRenderTarget(velocityRT);

	assert(mainDepthRT);
	renderer.deleteRenderTarget(mainDepthRT);

	assert(edgesRT);
	renderer.deleteRenderTarget(edgesRT);

	assert(blendWeightsRT);
	renderer.deleteRenderTarget(blendWeightsRT);

	assert(finalRenderRT);
	renderer.deleteRenderTarget(finalRenderRT);

	if (resolveRTs[0]) {
		assert(resolveRTs[1]);
		renderer.deleteRenderTarget(resolveRTs[0]);
		resolveRTs[0] = RenderTargetHandle();
		renderer.deleteRenderTarget(resolveRTs[1]);
		resolveRTs[1] = RenderTargetHandle();

		assert(resolveFBs[0]);
		resolveFBs[0] = FramebufferHandle();
		assert(resolveFBs[1]);
		resolveFBs[1] = FramebufferHandle();
	} else {
		assert(!resolveRTs[1]);

		assert(!resolveFBs[0]);
		assert(!resolveFBs[1]);
	}

	assert(separateFB);
	renderer.deleteFramebuffer(separateFB);

	for (unsigned int i = 0; i < 2; i++) {
		assert(subsampleRTs[i]);
		renderer.deleteRenderTarget(subsampleRTs[i]);
	}
}


void SMAADemo::createCubes() {
	// cube of cubes, n^3 cubes total
	const unsigned int numCubes = static_cast<unsigned int>(pow(cubesPerSide, 3));

	const float cubeDiameter = sqrtf(3.0f);
	const float cubeDistance = cubeDiameter + 1.0f;

	const float bigCubeSide = cubeDistance * cubesPerSide;

	cubes.clear();
	cubes.reserve(numCubes);

	unsigned int order = 0;
	for (unsigned int x = 0; x < cubesPerSide; x++) {
		for (unsigned int y = 0; y < cubesPerSide; y++) {
			for (unsigned int z = 0; z < cubesPerSide; z++) {
				float qx = random.randFloat();
				float qy = random.randFloat();
				float qz = random.randFloat();
				float qw = random.randFloat();
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				ShaderDefines::Cube cube;
				cube.position = glm::vec3((x * cubeDistance) - (bigCubeSide / 2.0f)
				                        , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                        , (z * cubeDistance) - (bigCubeSide / 2.0f));

				cube.order    = order;
				order++;

				cube.rotation = glm::vec4(qx, qy, qz, qw);
				cube.color    = glm::vec3(1.0f, 1.0f, 1.0f);
				cubes.emplace_back(cube);
			}
		}
	}

	colorCubes();
}


void SMAADemo::shuffleCubeRendering() {
	const unsigned int numCubes = cubes.size();
	for (unsigned int i = 0; i < numCubes - 1; i++) {
		unsigned int victim = random.range(i, numCubes);
		std::swap(cubes[i], cubes[victim]);
	}
}


void SMAADemo::reorderCubeRendering() {
	auto cubeCompare = [] (const ShaderDefines::Cube &a, const ShaderDefines::Cube &b) {
		return a.order < b.order;
	};
	std::sort(cubes.begin(), cubes.end(), cubeCompare);
}


static float sRGB2linear(float v) {
    if (v <= 0.04045f) {
        return v / 12.92f;
    } else {
        return powf((v + 0.055f) / 1.055f, 2.4f);
    }
}


void SMAADemo::colorCubes() {
	if (colorMode == 0) {
		for (auto &cube : cubes) {
			// random RGB
			cube.color.x = sRGB2linear(random.randFloat());
			cube.color.y = sRGB2linear(random.randFloat());
			cube.color.z = sRGB2linear(random.randFloat());
		}
	} else {
		for (auto &cube : cubes) {
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			// TODO: use the same luma as shader

			float y = 0.3f;
			const float c_red   = 0.299f
			          , c_green = 0.587f
			          , c_blue  = 0.114f;
			float cb = random.randFloat() * 2.0f - 1.0f;
			float cr = random.randFloat() * 2.0f - 1.0f;

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			cube.color.x = sRGB2linear(r);
			cube.color.y = sRGB2linear(g);
			cube.color.z = sRGB2linear(b);
		}
	}
}


static void printHelp() {
	printf(" a                - toggle antialiasing on/off\n");
	printf(" c                - re-color cubes\n");
	printf(" d                - cycle through debug visualizations\n");
	printf(" f                - toggle fullscreen\n");
	printf(" h                - print help\n");
	printf(" m                - change antialiasing method\n");
	printf(" q                - cycle through AA quality levels\n");
	printf(" t                - toggle temporal antialiasing on/off\n");
	printf(" v                - toggle vsync\n");
	printf(" LEFT/RIGHT ARROW - cycle through scenes\n");
	printf(" SPACE            - toggle cube rotation\n");
	printf(" ESC              - quit\n");
}


void SMAADemo::mainLoopIteration() {
	ImGuiIO& io = ImGui::GetIO();

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
			io.KeysDown[event.key.keysym.scancode] = true;

			if (textInputActive) {
				break;
			}

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
				rotateCubes = !rotateCubes;
				break;

			case SDL_SCANCODE_A:
				antialiasing = !antialiasing;
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}
				if (temporalAA) {
					temporalAAFirstFrame = true;
				}
				break;

			case SDL_SCANCODE_C:
				if (rightShift || leftShift) {
					colorMode = (colorMode + 1) % 2;
				}
				colorCubes();
				break;

			case SDL_SCANCODE_D:
				if (antialiasing && (aaMethod == AAMethod::SMAA || aaMethod == AAMethod::SMAA2X)) {
					if (leftShift || rightShift) {
						debugMode = (debugMode + 3 - 1) % 3;
					} else {
						debugMode = (debugMode + 1) % 3;
					}
				}
				break;

			case SDL_SCANCODE_H:
				printHelp();
				break;

			case SDL_SCANCODE_M:
				// if moving either to or from MSAA or SMAA2X need to recreate framebuffers
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}

				if (leftShift || rightShift) {
					aaMethod = AAMethod((int(aaMethod) + int(AAMethod::LAST)) % (int(AAMethod::LAST) + 1));
				} else {
					aaMethod = AAMethod((int(aaMethod) + 1) % (int(AAMethod::LAST) + 1));
				}
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}
				break;

			case SDL_SCANCODE_Q:
				switch (aaMethod) {
				case AAMethod::MSAA:
					if (leftShift || rightShift) {
						msaaQuality = msaaQuality + maxSMAAQuality - 1;
					} else {
						msaaQuality = msaaQuality + 1;
					}
					msaaQuality = msaaQuality % maxMSAAQuality;
					recreateFramebuffers = true;
					break;

				case AAMethod::FXAA:
					if (leftShift || rightShift) {
						fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
					} else {
						fxaaQuality = fxaaQuality + 1;
					}
					fxaaQuality = fxaaQuality % maxFXAAQuality;
					break;

				case AAMethod::SMAA:
				case AAMethod::SMAA2X:
					if (leftShift || rightShift) {
						smaaKey.quality = smaaKey.quality + maxSMAAQuality - 1;
					} else {
						smaaKey.quality = smaaKey.quality + 1;
					}
					smaaKey.quality = smaaKey.quality % maxSMAAQuality;
					smaaParameters  = defaultSMAAParameters[smaaKey.quality];
					break;

				}
				break;

			case SDL_SCANCODE_T:
				if (aaMethod != AAMethod::MSAA) {
					temporalAA = !temporalAA;
					if (temporalAA) {
						recreateFramebuffers = true;
						temporalAAFirstFrame = true;
					}
				}
				break;

			case SDL_SCANCODE_V:
				switch (vsync) {
				case VSync::On:
					vsync = VSync::LateSwapTear;
					break;

				case VSync::LateSwapTear:
					vsync = VSync::Off;
					break;

				case VSync::Off:
					vsync = VSync::On;
					break;
				}
				recreateSwapchain = true;
				break;

			case SDL_SCANCODE_F:
				fullscreen = !fullscreen;
				recreateSwapchain = true;
				break;

			case SDL_SCANCODE_LEFT:
				sceneIncrement = -1;
				// fallthrough
			case SDL_SCANCODE_RIGHT:
				{
					// all images + cubes scene
					unsigned int numScenes = static_cast<unsigned int>(images.size()) + 1;
					activeScene = (activeScene + sceneIncrement + numScenes) % numScenes;
				}
				break;

			default:
				break;
			}

			break;

		case SDL_KEYUP:
			io.KeysDown[event.key.keysym.scancode] = false;

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

		case SDL_TEXTINPUT:
			io.AddInputCharactersUTF8(event.text.text);
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			case SDL_WINDOWEVENT_RESIZED:
				windowWidth  = event.window.data1;
				windowHeight = event.window.data2;
				recreateSwapchain = true;
				break;
			default:
				break;
			}
			break;

		case SDL_MOUSEMOTION:
			io.MousePos = ImVec2(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y));
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 6) {
				// SDL and imgui have left and middle in different order
				static const uint8_t SDLMouseLookup[5] = { 0, 2, 1, 3, 4 };
				io.MouseDown[SDLMouseLookup[event.button.button - 1]] = (event.button.state == SDL_PRESSED);
			}
			break;

		case SDL_MOUSEWHEEL:
			io.MouseWheel = static_cast<float>(event.wheel.y);
			break;

		case SDL_DROPFILE: {
				char* droppedFile = event.drop.file;
				std::string filestring(droppedFile);
				SDL_free(droppedFile);
				loadImage(filestring);
			} break;

		}
	}

	SDL_Keymod modstate = SDL_GetModState();
	if (modstate & KMOD_CTRL) {
		io.KeyCtrl = true;
	} else {
		io.KeyCtrl = false;
	}
	if (modstate & KMOD_SHIFT) {
		io.KeyShift = true;
	} else {
		io.KeyShift = false;
	}
	if (modstate & KMOD_ALT) {
		io.KeyAlt = true;
	} else {
		io.KeyAlt = false;
	}
	if (modstate & KMOD_GUI) {
		io.KeySuper = true;
	} else {
		io.KeySuper = false;
	}

	render();
}


void SMAADemo::render() {
	if (recreateSwapchain) {
		SwapchainDesc desc;
		desc.fullscreen = fullscreen;
		desc.numFrames  = numFrames;
		desc.width      = windowWidth;
		desc.height     = windowHeight;
		desc.vsync      = vsync;

		renderer.setSwapchainDesc(desc);
	}

	uint64_t ticks   = getNanoseconds();
	uint64_t elapsed = ticks - lastTime;

	if (fpsLimitActive) {
		uint64_t nsLimit = 1000000000ULL / fpsLimit;
		while (elapsed + sleepFudge < nsLimit) {
			// limit reached, throttle
			uint64_t nsWait = nsLimit - (elapsed + sleepFudge);
			std::this_thread::sleep_for(std::chrono::nanoseconds(nsWait));
			ticks   = getNanoseconds();
			elapsed = ticks - lastTime;
		}
	}

	lastTime = ticks;

	renderer.beginFrame();
	if (recreateSwapchain || recreateFramebuffers) {
		recreateSwapchain = false;
		recreateFramebuffers = false;

		glm::uvec2 size = renderer.getDrawableSize();
		LOG("drawable size: %ux%u\n", size.x, size.y);
		windowWidth  = size.x;
		windowHeight = size.y;

		createFramebuffers();
	}

	ShaderDefines::Globals globals;
	globals.screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

#ifdef RENDERER_VULKAN
	globals.guiOrtho   = glm::ortho(0.0f, float(windowWidth), 0.0f, float(windowHeight));
#else
	globals.guiOrtho   = glm::ortho(0.0f, float(windowWidth), float(windowHeight), 0.0f);
#endif

	globals.smaaParameters       = smaaParameters;

	globals.predicationThreshold = predicationThreshold;
	globals.predicationScale     = predicationScale;
	globals.predicationStrength  = predicationStrength;
	globals.reprojWeigthScale    = reprojectionWeightScale;

	glm::vec4 subsampleIndices[2];

	if (temporalAA) {
		temporalFrame = (temporalFrame + 1) % 2;
		if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
			if (temporalFrame == 0) {
				subsampleIndices[0] = glm::vec4(5.0f, 3.0f, 1.0f, 3.0f);
			} else {
				assert(temporalFrame == 1);
				subsampleIndices[0] = glm::vec4(3.0f, 5.0f, 1.0f, 4.0f);
			}
		} else {
			float v       = float(temporalFrame + 1);
			subsampleIndices[0] = glm::vec4(v, v, v, 0.0f);
		}
	} else {
		if (aaMethod == AAMethod::SMAA2X) {
			subsampleIndices[0] = glm::vec4(1.0, 1.0, 1.0, 0.0f);
		} else {
			subsampleIndices[0] = glm::vec4(0.0f);
		}
	}
	if (temporalAA) {
		if (temporalFrame == 0) {
			subsampleIndices[1] = glm::vec4(4.0f, 6.0f, 2.0f, 3.0f);
		} else {
			assert(temporalFrame == 1);
			subsampleIndices[1] = glm::vec4(6.0f, 4.0f, 2.0f, 4.0f);
		}
	} else {
		subsampleIndices[1] = glm::vec4(2.0f, 2.0f, 2.0f, 0.0f);
	}

	globals.subsampleIndices = subsampleIndices[0];

	Layout l = Layout::ShaderRead;
	if (!antialiasing || aaMethod == AAMethod::MSAA) {
		l = Layout::TransferSrc;
	}
	renderer.beginRenderPass(getSceneRenderPass(numSamples, l), sceneFramebuffer);

	if (activeScene == 0) {
		renderer.bindPipeline(getCubePipeline(numSamples));

		if (rotateCubes) {
			rotationTime += elapsed;

			// TODO: increasing rotation period can make cubes spin backwards
			const uint64_t rotationPeriod = rotationPeriodSeconds * 1000000000ULL;
			rotationTime   = rotationTime % rotationPeriod;
			cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
		}

		// TODO: better calculation, and check cube size (side is sqrt(3) currently)
		const float cubeDiameter = sqrtf(3.0f);
		const float cubeDistance = cubeDiameter + 1.0f;

		float farPlane  = cameraDistance + cubeDistance * float(cubesPerSide + 1);
		float nearPlane = std::max(0.1f, cameraDistance - cubeDistance * float(cubesPerSide + 1));

		glm::mat4 model  = glm::rotate(glm::mat4(1.0f), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 view   = glm::lookAt(glm::vec3(cameraDistance, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj   = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, nearPlane, farPlane);
		glm::mat4 viewProj = proj * view * model;

		// temporal jitter
		if (temporalAA) {
			glm::vec2 jitter;
			if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
				const glm::vec2 jitters[2] = {
					  {  0.125f,  0.125f }
					, { -0.125f, -0.125f }
				};
				jitter = jitters[temporalFrame];
			} else {
				const glm::vec2 jitters[2] = {
					  { -0.25f,  0.25f }
					, { 0.25f,  -0.25f }
				};
				jitter = jitters[temporalFrame];
			}

			jitter = jitter * 2.0f * glm::vec2(globals.screenSize.x, globals.screenSize.y);
			glm::mat4 jitterMatrix = glm::translate(glm::mat4(), glm::vec3(jitter, 0.0f));
			viewProj = jitterMatrix * viewProj;
		}

		prevViewProj         = currViewProj;
		currViewProj         = viewProj;
		globals.viewProj     = currViewProj;
		globals.prevViewProj = prevViewProj;

		renderer.setViewport(0, 0, windowWidth, windowHeight);

		GlobalDS globalDS;
		globalDS.globalUniforms = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::Globals), &globals);
		globalDS.linearSampler  = linearSampler;
		globalDS.nearestSampler = nearestSampler;
		renderer.bindDescriptorSet(0, globalDS);

		renderer.bindVertexBuffer(0, cubeVBO);
		renderer.bindIndexBuffer(cubeIBO, false);

		CubeSceneDS cubeDS;
		cubeDS.instances = renderer.createEphemeralBuffer(BufferType::Storage, static_cast<uint32_t>(sizeof(ShaderDefines::Cube) * cubes.size()), &cubes[0]);
		renderer.bindDescriptorSet(1, cubeDS);

		unsigned int numCubes = static_cast<unsigned int>(cubes.size());
		if (visualizeCubeOrder) {
			cubeOrderNum = cubeOrderNum % numCubes;
			cubeOrderNum++;
			numCubes     = cubeOrderNum;
		}

		renderer.drawIndexedInstanced(3 * 2 * 6, numCubes);
	} else {
		renderer.bindPipeline(imagePipeline);

		const auto &image = images.at(activeScene - 1);

		renderer.setViewport(0, 0, windowWidth, windowHeight);

		GlobalDS globalDS;
		globalDS.globalUniforms = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::Globals), &globals);
		globalDS.linearSampler = linearSampler;
		globalDS.nearestSampler = nearestSampler;
		renderer.bindDescriptorSet(0, globalDS);

		assert(activeScene - 1 < images.size());
		ColorTexDS colorDS;
		colorDS.color = image.tex;
		renderer.bindDescriptorSet(1, colorDS);
		renderer.draw(0, 3);
	}
	renderer.endRenderPass();

	if (antialiasing) {
		switch (aaMethod) {
		case AAMethod::MSAA: {
			if (false) {
				renderer.layoutTransition(resolveRTs[temporalFrame], Layout::Undefined, Layout::TransferDst);
				renderer.resolveMSAA(sceneFramebuffer, resolveFBs[temporalFrame]);
				// TODO: do this transition as part of renderpass?
				renderer.layoutTransition(resolveRTs[temporalFrame], Layout::TransferDst, Layout::ColorAttachment);

				doTemporalAA();
			} else {
				renderer.layoutTransition(finalRenderRT, Layout::Undefined, Layout::TransferDst);
				renderer.resolveMSAA(sceneFramebuffer, finalFramebuffer);
				renderer.layoutTransition(finalRenderRT, Layout::TransferDst, Layout::ColorAttachment);
			}
		} break;

		case AAMethod::FXAA: {
			renderer.beginRenderPass(fxaaRenderPass[temporalAA], temporalAA ? resolveFBs[temporalFrame] : finalFramebuffer);
			renderer.bindPipeline(getFXAAPipeline(fxaaQuality));
			ColorCombinedDS colorDS;
			colorDS.color.tex     = renderer.getRenderTargetTexture(mainColorRT);
			colorDS.color.sampler = linearSampler;
			renderer.bindDescriptorSet(1, colorDS);
			renderer.draw(0, 3);
			renderer.endRenderPass();

			if (temporalAA) {
				doTemporalAA();
			}
		} break;

		case AAMethod::SMAA: {
			if (temporalAA) {
				doSMAA(mainColorRT, smaaBlendRenderPass, resolveFBs[temporalFrame], 0);
			} else {
				doSMAA(mainColorRT, finalRenderPass, finalFramebuffer, 0);
			}

			if (temporalAA) {
				doTemporalAA();
			}
		} break;

		case AAMethod::SMAA2X: {
			// separate
			renderer.beginRenderPass(separateRenderPass, separateFB);
			renderer.bindPipeline(separatePipeline);
			ColorCombinedDS separateDS;
			separateDS.color.tex     = renderer.getRenderTargetTexture(mainColorRT);
			separateDS.color.sampler = nearestSampler;
			renderer.bindDescriptorSet(1, separateDS);
			renderer.draw(0, 3);
			renderer.endRenderPass();

			// TODO: clean up the renderpass mess
			if (temporalAA) {
				doSMAA(subsampleRTs[0], smaa2XBlendRenderPasses[0], resolveFBs[temporalFrame], 0);
				// TODO: this is ugly, subsample indices should be in their own UBO
				// or push constants
				globals.subsampleIndices = subsampleIndices[1];
				GlobalDS globalDS;
				globalDS.globalUniforms = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::Globals), &globals);
				globalDS.linearSampler  = linearSampler;
				globalDS.nearestSampler = nearestSampler;
				renderer.bindDescriptorSet(0, globalDS);
				doSMAA(subsampleRTs[1], smaa2XBlendRenderPasses[1], resolveFBs[temporalFrame], 1);
			} else {
				doSMAA(subsampleRTs[0], smaa2XBlendRenderPasses[0], finalFramebuffer, 0);
				globals.subsampleIndices = subsampleIndices[1];
				GlobalDS globalDS;
				globalDS.globalUniforms = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::Globals), &globals);
				globalDS.linearSampler  = linearSampler;
				globalDS.nearestSampler = nearestSampler;
				renderer.bindDescriptorSet(0, globalDS);
				doSMAA(subsampleRTs[1], smaa2XBlendRenderPasses[1], finalFramebuffer, 1);
			}

			if (temporalAA) {
				// FIXME: move to renderpass
				renderer.layoutTransition(resolveRTs[temporalFrame], Layout::ColorAttachment, Layout::ShaderRead);
				doTemporalAA();
			}
		} break;
		}

	} else {
		renderer.layoutTransition(finalRenderRT, Layout::Undefined, Layout::TransferDst);
		renderer.blit(sceneFramebuffer, finalFramebuffer);
		renderer.layoutTransition(finalRenderRT, Layout::TransferDst, Layout::ColorAttachment);
	}

	drawGUI(elapsed);

	renderer.presentFrame(finalRenderRT);

}


void SMAADemo::doSMAA(RenderTargetHandle input, RenderPassHandle renderPass, FramebufferHandle outputFB, int pass) {
	// edges pass
	const SMAAPipelines &pipelines = getSMAAPipelines(smaaKey);
	renderer.beginRenderPass(smaaEdgesRenderPass, smaaEdgesFramebuffer);
	renderer.bindPipeline(pipelines.edgePipeline);

	EdgeDetectionDS edgeDS;
	if (smaaKey.edgeMethod == SMAAEdgeMethod::Depth) {
		edgeDS.color.tex     = renderer.getRenderTargetTexture(mainDepthRT);
	} else {
		edgeDS.color.tex     = renderer.getRenderTargetView(input, Format::RGBA8);
	}
	edgeDS.color.sampler = nearestSampler;
	edgeDS.predicationTex.tex     = renderer.getRenderTargetTexture(mainDepthRT);
	edgeDS.predicationTex.sampler = nearestSampler;
	renderer.bindDescriptorSet(1, edgeDS);
	renderer.draw(0, 3);
	renderer.endRenderPass();

	// blendweights pass
	renderer.beginRenderPass(smaaWeightsRenderPass, smaaWeightsFramebuffer);
	renderer.bindPipeline(pipelines.blendWeightPipeline);
	BlendWeightDS blendWeightDS;
	blendWeightDS.edgesTex.tex      = renderer.getRenderTargetTexture(edgesRT);
	blendWeightDS.edgesTex.sampler  = linearSampler;
	blendWeightDS.areaTex.tex       = areaTex;
	blendWeightDS.areaTex.sampler   = linearSampler;
	blendWeightDS.searchTex.tex     = searchTex;
	blendWeightDS.searchTex.sampler = linearSampler;
	renderer.bindDescriptorSet(1, blendWeightDS);

	renderer.draw(0, 3);
	renderer.endRenderPass();

	// final blending pass/debug pass
	renderer.beginRenderPass(renderPass, outputFB);

	switch (debugMode) {
	case 0: {
		// full effect
		renderer.bindPipeline(pipelines.neighborPipelines[pass]);

		NeighborBlendDS neighborBlendDS;
		neighborBlendDS.color.tex            = renderer.getRenderTargetTexture(input);
		neighborBlendDS.color.sampler        = linearSampler;
		neighborBlendDS.blendweights.tex     = renderer.getRenderTargetTexture(blendWeightsRT);
		neighborBlendDS.blendweights.sampler = linearSampler;
		renderer.bindDescriptorSet(1, neighborBlendDS);
	} break;

	case 1: {
		// visualize edges
		ColorTexDS blitDS;
		renderer.bindPipeline(blitPipeline);
		blitDS.color   = renderer.getRenderTargetTexture(edgesRT);
		renderer.bindDescriptorSet(1, blitDS);
	} break;

	case 2: {
		// visualize blend weights
		ColorTexDS blitDS;
		renderer.bindPipeline(blitPipeline);
		blitDS.color   = renderer.getRenderTargetTexture(blendWeightsRT);
		renderer.bindDescriptorSet(1, blitDS);
	} break;

	}
	renderer.draw(0, 3);
	renderer.endRenderPass();
}


void SMAADemo::doTemporalAA() {
	renderer.beginRenderPass(finalRenderPass, finalFramebuffer);
	renderer.bindPipeline(temporalAAPipelines[temporalReproject]);
	TemporalAADS temporalDS;
	temporalDS.currentTex.tex      = renderer.getRenderTargetTexture(resolveRTs[temporalFrame]);
	temporalDS.currentTex.sampler  = nearestSampler;
	if (temporalAAFirstFrame) {
		// to prevent flicker on first frame after enabling
		temporalDS.previousTex.tex     = renderer.getRenderTargetTexture(resolveRTs[temporalFrame]);
		temporalDS.previousTex.sampler = nearestSampler;
		temporalAAFirstFrame = false;
	} else {
		temporalDS.previousTex.tex     = renderer.getRenderTargetTexture(resolveRTs[1 - temporalFrame]);
		temporalDS.previousTex.sampler = nearestSampler;
	}
	temporalDS.velocityTex.tex         = renderer.getRenderTargetTexture(velocityRT);
	temporalDS.velocityTex.sampler     = nearestSampler;

	renderer.bindDescriptorSet(1, temporalDS);
	renderer.draw(0, 3);
	renderer.endRenderPass();
}


void SMAADemo::drawGUI(uint64_t elapsed) {
	ImGuiIO& io    = ImGui::GetIO();
	io.DeltaTime   = float(double(elapsed) / double(1000000000ULL));
	io.DisplaySize = ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

	ImGui::NewFrame();

	if (io.WantTextInput != textInputActive) {
		textInputActive = io.WantTextInput;
		if (textInputActive) {
			SDL_StartTextInput();
		} else {
			SDL_StopTextInput();
		}
	}

	bool windowVisible = true;
	int flags = 0;
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_AlwaysAutoResize;

	if (ImGui::Begin("SMAA", &windowVisible, flags)) {
		if (ImGui::CollapsingHeader("Antialiasing properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool aaChanged = ImGui::Checkbox("Antialiasing", &antialiasing);
			if (aaChanged && temporalAA) {
				temporalAAFirstFrame = true;
			}

			int aa = static_cast<int>(aaMethod);
			ImGui::RadioButton("MSAA", &aa, static_cast<int>(AAMethod::MSAA)); ImGui::SameLine();
			ImGui::RadioButton("FXAA", &aa, static_cast<int>(AAMethod::FXAA)); ImGui::SameLine();
			ImGui::RadioButton("SMAA", &aa, static_cast<int>(AAMethod::SMAA)); ImGui::SameLine();
			ImGui::RadioButton("SMAA2X", &aa, static_cast<int>(AAMethod::SMAA2X));

			{
				if (aaMethod == AAMethod::MSAA) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				if (ImGui::Checkbox("Temporal AA", &temporalAA)) {
					recreateFramebuffers = true;
				}

				// temporal reprojection only enabled when temporal AA is
				if (!temporalAA) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				ImGui::Checkbox("Temporal reprojection", &temporalReproject);
				if (!temporalAA) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}

				if (aaMethod == AAMethod::MSAA) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}

			float w = reprojectionWeightScale;
			ImGui::SliderFloat("Reprojection weight scale", &w, 0.0f, 80.0f);
			reprojectionWeightScale = w;

			ImGui::Separator();
			int msaaq = msaaQuality;
			bool msaaChanged = ImGui::Combo("MSAA quality", &msaaq, msaaQualityLevels, maxMSAAQuality);
			if (aaChanged || aa != static_cast<int>(aaMethod)) {
				// if moving either to or from MSAA or SMAA2X need to recreate framebuffers
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}
				aaMethod = static_cast<AAMethod>(aa);
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}
			}

			if (msaaChanged && aaMethod == AAMethod::MSAA) {
				assert(msaaq >= 0);
				msaaQuality = static_cast<unsigned int>(msaaq);
				recreateFramebuffers = true;
			}

			ImGui::Separator();
			int sq = smaaKey.quality;
			ImGui::Combo("SMAA quality", &sq, smaaQualityLevels, maxSMAAQuality);
			assert(sq >= 0);
			assert(sq < int(maxSMAAQuality));
			if (int(smaaKey.quality) != sq) {
				smaaKey.quality = sq;
				if (sq != 0) {
					smaaParameters  = defaultSMAAParameters[sq];
				}
			}

			if (ImGui::CollapsingHeader("SMAA custom properties")) {
				// parameters can only be changed in custom mode
				// https://github.com/ocornut/imgui/issues/211
				if (smaaKey.quality != 0) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				ImGui::SliderFloat("SMAA color/luma edge threshold", &smaaParameters.threshold,      0.0f, 0.5f);
				ImGui::SliderFloat("SMAA depth edge threshold",      &smaaParameters.depthThreshold, 0.0f, 1.0f);

				int s = smaaParameters.maxSearchSteps;
				ImGui::SliderInt("Max search steps",  &s, 0, 112);
				smaaParameters.maxSearchSteps = s;

				s = smaaParameters.maxSearchStepsDiag;
				ImGui::SliderInt("Max diagonal search steps", &s, 0, 20);
				smaaParameters.maxSearchStepsDiag = s;

				s = smaaParameters.cornerRounding;
				ImGui::SliderInt("Corner rounding",   &s, 0, 100);
				smaaParameters.cornerRounding = s;

				if (smaaKey.quality != 0) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}

			ImGui::Checkbox("Predicated thresholding", &smaaKey.predication);

			if (!smaaKey.predication) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			ImGui::SliderFloat("Predication threshold", &predicationThreshold, 0.0f, 1.0f, "%.4f", 3.0f);
			ImGui::SliderFloat("Predication scale",     &predicationScale,     1.0f, 5.0f);
			ImGui::SliderFloat("Predication strength",  &predicationStrength,  0.0f, 1.0f);
			if (ImGui::Button("Reset predication values")) {
				predicationThreshold = 0.01f;
				predicationScale     = 2.0f;
				predicationStrength  = 0.4f;
			}

			if (!smaaKey.predication) {
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}

			int em = static_cast<int>(smaaKey.edgeMethod);
			ImGui::Text("SMAA edge detection");
			ImGui::RadioButton("Color", &em, static_cast<int>(SMAAEdgeMethod::Color));
			ImGui::RadioButton("Luma",  &em, static_cast<int>(SMAAEdgeMethod::Luma));
			ImGui::RadioButton("Depth", &em, static_cast<int>(SMAAEdgeMethod::Depth));
			smaaKey.edgeMethod = static_cast<SMAAEdgeMethod>(em);

			int d = debugMode;
			ImGui::Combo("SMAA debug", &d, smaaDebugModes, 3);
			assert(d >= 0);
			assert(d < 3);
			debugMode = d;

			int fq = fxaaQuality;
			ImGui::Separator();
			ImGui::Combo("FXAA quality", &fq, fxaaQualityLevels, maxFXAAQuality);
			assert(fq >= 0);
			assert(fq < int(maxFXAAQuality));
			fxaaQuality = fq;
		}

		if (ImGui::CollapsingHeader("Scene properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			// TODO: don't regenerate this on every frame
			std::vector<const char *> scenes;
			scenes.reserve(images.size() + 1);
			scenes.push_back("Cubes");
			for (const auto &img : images) {
				scenes.push_back(img.shortName.c_str());
			}
			assert(activeScene < scenes.size());
			int s = activeScene;
			ImGui::Combo("Scene", &s, &scenes[0], static_cast<int>(scenes.size()));
			assert(s >= 0);
			assert(s < int(scenes.size()));
			activeScene = s;

			ImGui::InputText("Load image", imageFileName, inputTextBufferSize);

			ImGui::Columns(2);

			if (ImGui::Button("Paste")) {
				char *clipboard = SDL_GetClipboardText();
				if (clipboard) {
					size_t length = strnlen(clipboard, inputTextBufferSize - 1);
					strncpy(imageFileName, clipboard, length);
					imageFileName[length] = '\0';
					SDL_free(clipboard);
				}
			}
			ImGui::NextColumn();
			if (ImGui::Button("Load")) {
				std::string filename(imageFileName);
				loadImage(filename);
			}

			ImGui::Columns(1);

			int m = cubesPerSide;
			bool changed = ImGui::InputInt("Cubes per side", &m);
			if (changed && m > 0 && m < 55) {
				cubesPerSide = m;
				createCubes();
			}

			float l = cameraDistance;
			if (ImGui::SliderFloat("Camera distance", &l, 1.0f, 256.0f, "%.1f")) {
				cameraDistance = l;
			}

			ImGui::Checkbox("Rotate cubes", &rotateCubes);
			int p = rotationPeriodSeconds;
			ImGui::SliderInt("Rotation period (sec)", &p, 1, 60);
			assert(p >= 1);
			assert(p <= 60);
			rotationPeriodSeconds = p;

			ImGui::Separator();
			ImGui::Text("Cube coloring mode");
			int newColorMode = colorMode;
			ImGui::RadioButton("RGB",   &newColorMode, 0);
			ImGui::RadioButton("YCbCr", &newColorMode, 1);

			if (int(colorMode) != newColorMode) {
				colorMode = newColorMode;
				colorCubes();
			}

			if (ImGui::Button("Re-color cubes")) {
				colorCubes();
			}

			if (ImGui::Button("Shuffle cube rendering order")) {
				shuffleCubeRendering();
				cubeOrderNum = 1;
			}

			if (ImGui::Button("Reorder cube rendering order")) {
				reorderCubeRendering();
				cubeOrderNum = 1;
			}

			ImGui::Checkbox("Visualize cube order", &visualizeCubeOrder);
		}

		if (ImGui::CollapsingHeader("Swapchain properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			recreateSwapchain = ImGui::Checkbox("Fullscreen", &fullscreen);

			int vsyncTemp = static_cast<int>(vsync);
			ImGui::Text("V-Sync");
			ImGui::RadioButton("Off",            &vsyncTemp, 0);
			ImGui::RadioButton("On",             &vsyncTemp, 1);
			ImGui::RadioButton("Late swap tear", &vsyncTemp, 2);

			if (vsyncTemp != static_cast<int>(vsync)) {
				recreateSwapchain = true;
				vsync             = static_cast<VSync>(vsyncTemp);
			}

			int n = numFrames;
			// TODO: ask Renderer for the limits
			if (ImGui::SliderInt("frames ahead", &n, 1, 16)) {
				numFrames = n;
				recreateSwapchain = true;
			}

			ImGui::Checkbox("FPS limit", &fpsLimitActive);

			int f   = fpsLimit;
			bool changed = ImGui::InputInt("Max FPS", &f);
			if (changed && f > 0) {
				fpsLimit = f;
			}

			ImGui::Separator();
			// TODO: measure actual GPU time
			ImGui::LabelText("FPS", "%.1f", io.Framerate);
			ImGui::LabelText("Frame time ms", "%.1f", 1000.0f / io.Framerate);

#ifdef RENDERER_VULKAN
			ImGui::Separator();
			// VMA memory allocation stats
			MemoryStats stats = renderer.getMemStats();
			float usedMegabytes = static_cast<float>(stats.usedBytes) / (1024.0f * 1024.0f);
			float totalMegabytes = static_cast<float>(stats.usedBytes + stats.unusedBytes) / (1024.0f * 1024.0f);
			ImGui::LabelText("Allocation count", "%u", stats.allocationCount);
			ImGui::LabelText("Suballocation count", "%u", stats.subAllocationCount);
			ImGui::LabelText("Used memory (MB)", "%.2f", usedMegabytes);
			ImGui::LabelText("Total memory (MB)", "%.2f", totalMegabytes);
#endif
		}

		if (ImGui::Button("Quit")) {
			keepGoing = false;
		}
	}

	// move the window to right edge of screen
	ImVec2 pos;
	pos.x =  windowWidth  - ImGui::GetWindowWidth();
	pos.y = (windowHeight - ImGui::GetWindowHeight()) / 2.0f;
	ImGui::SetWindowPos(pos);

	ImGui::End();

#if 0
	bool demoVisible = true;
	ImGui::ShowDemoWindow(&demoVisible);
#endif  // 0

	ImGui::Render();

	auto drawData = ImGui::GetDrawData();
	assert(drawData->Valid);
	if (drawData->CmdListsCount > 0) {
		renderer.beginRenderPass(guiOnlyRenderPass, finalFramebuffer);

		assert(drawData->CmdLists      != nullptr);
		assert(drawData->TotalVtxCount >  0);
		assert(drawData->TotalIdxCount >  0);

		renderer.bindPipeline(guiPipeline);
		ColorTexDS colorDS;
		colorDS.color = imguiFontsTex;
		renderer.bindDescriptorSet(1, colorDS);
		// TODO: upload all buffers first, render after
		// and one buffer each vertex/index

		for (int n = 0; n < drawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = drawData->CmdLists[n];

			BufferHandle vtxBuf = renderer.createEphemeralBuffer(BufferType::Vertex, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data);
			BufferHandle idxBuf = renderer.createEphemeralBuffer(BufferType::Index, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data);
			renderer.bindIndexBuffer(idxBuf, true);
			renderer.bindVertexBuffer(0, vtxBuf);

			unsigned int idx_buffer_offset = 0;
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback) {
					// TODO: this probably does nothing useful for us
					assert(false);

					pcmd->UserCallback(cmd_list, pcmd);
				} else {
					assert(pcmd->TextureId == 0);
					renderer.setScissorRect(static_cast<unsigned int>(pcmd->ClipRect.x), static_cast<unsigned int>(pcmd->ClipRect.y),
						static_cast<unsigned int>(pcmd->ClipRect.z - pcmd->ClipRect.x), static_cast<unsigned int>(pcmd->ClipRect.w - pcmd->ClipRect.y));
					renderer.drawIndexedOffset(pcmd->ElemCount, idx_buffer_offset);
				}
				idx_buffer_offset += pcmd->ElemCount;
			}
		}
#if 0
		LOG("CmdListsCount: %d\n", drawData->CmdListsCount);
		LOG("TotalVtxCount: %d\n", drawData->TotalVtxCount);
		LOG("TotalIdxCount: %d\n", drawData->TotalIdxCount);
#endif // 0

		renderer.endRenderPass();
	} else {
		assert(drawData->CmdLists      == nullptr);
		assert(drawData->TotalVtxCount == 0);
		assert(drawData->TotalIdxCount == 0);
	}
}


int main(int argc, char *argv[]) {
	try {
		logInit();

		auto demo = std::make_unique<SMAADemo>();

		demo->parseCommandLine(argc, argv);

		demo->initRender();
		demo->createCubes();
		printHelp();

		while (demo->shouldKeepGoing()) {
			try {
				demo->mainLoopIteration();
			} catch (std::exception &e) {
				LOG("caught std::exception: \"%s\"\n", e.what());
				break;
			} catch (...) {
				LOG("caught unknown exception\n");
				break;
			}
		}
	} catch (std::exception &e) {
		LOG("caught std::exception \"%s\"\n", e.what());
#ifndef _MSC_VER
		logShutdown();
		// so native dumps core
		throw;
#endif
	} catch (...) {
		LOG("unknown exception\n");
#ifndef _MSC_VER
		logShutdown();
		// so native dumps core
		throw;
#endif
	}
	logShutdown();
	return 0;
}
