/*
Copyright (c) 2015-2019 Alternative Games Ltd / Turo Lamminen

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
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>

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

	// TODO: have a more specific function for this
	unsigned int retval = 0;
	unsigned int count = 0;
	forEachSetBit(q, [&] (uint32_t bit, uint32_t /* mask */) {
		assert(bit > 0);
		retval = bit - 1;
		count++;
	});

	assert(count == 1);
	return retval;
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
	Image(Image &&) noexcept            = default;

	Image &operator=(const Image &)  = default;
	Image &operator=(Image &&) noexcept = default;

	~Image() {}
};


namespace Rendertargets {


enum Rendertargets {
	  MainColor
	, MainDepth
	, Velocity
	, Edges
	, BlendWeights
	, TemporalPrevious
	, TemporalCurrent
	, Subsample1
	, Subsample2
	, FinalRender
	, Count
};


} // namespace Rendertargets



namespace Framebuffers {


enum Framebuffers {
	  Scene
	, Separate
	, Final
	, TemporalPrevious
	, TemporalCurrent
	, SMAAEdges
	, SMAAWeights
	, Count
};


} // namespace Framebuffers


namespace RenderPasses {


enum RenderPasses {
	  Scene
	, Final
	, GUI
	, FXAA
	, Separate
	, SMAAEdges
	, SMAAWeights
	, SMAABlend
	, SMAAEdges2
	, SMAAWeights2
	, SMAA2XBlend1
	, SMAA2XBlend2
	, Count
};


}  // namespace RenderPasses


namespace std {


template <> struct hash<RenderPasses::RenderPasses> {
	size_t operator()(const RenderPasses::RenderPasses &k) const {
		return hash<size_t>()(static_cast<size_t>(k));
	}

};


} // namespace std


struct SMAAPipelines {
	PipelineHandle  edgePipeline;
	PipelineHandle  blendWeightPipeline;
	std::array<PipelineHandle, 2>  neighborPipelines;
};


class RenderGraph {

	enum class State : uint8_t {
		  Invalid
		, Building
		, Ready
		, Rendering
	};


	typedef  std::function<void(RenderPasses::RenderPasses)>  RenderPassFunc;


	State                                          state;
	std::vector<std::function<void(Renderer &)> >  functions;
	Rendertargets::Rendertargets                   finalTarget;

	struct Pipeline {
		PipelineDesc    desc;
		PipelineHandle  handle;
	};

	// TODO: use hash map
	std::vector<Pipeline>                          pipelines;


	RenderGraph(const RenderGraph &)                = delete;
	RenderGraph(RenderGraph &&) noexcept            = delete;

	RenderGraph &operator=(const RenderGraph &)     = delete;
	RenderGraph &operator=(RenderGraph &&) noexcept = delete;


public:

	struct PassDesc {
		PassDesc()
		: depthStencil_(Rendertargets::Count)
		, numSamples_(1)
		, clearDepthAttachment(false)
		, depthClearValue(1.0f)
		{
			for (auto &rt : colorRTs_) {
				rt.id            = Rendertargets::Count;
				rt.passBegin     = PassBegin::DontCare;
				rt.clearValue    = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}

		~PassDesc() { }

		PassDesc(const PassDesc &)                = default;
		PassDesc(PassDesc &&) noexcept            = default;

		PassDesc &operator=(const PassDesc &)     = default;
		PassDesc &operator=(PassDesc &&) noexcept = default;

		PassDesc &depthStencil(Rendertargets::Rendertargets ds, PassBegin) {
			depthStencil_ = ds;
			return *this;
		}

		PassDesc &color(unsigned int index, Rendertargets::Rendertargets, PassBegin pb, glm::vec4 clear = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)) {
			assert(index < MAX_COLOR_RENDERTARGETS);
			colorRTs_[index].passBegin      = pb;
			if (pb == PassBegin::Clear) {
				colorRTs_[index].clearValue = clear;
			}
			return *this;
		}

		PassDesc &clearDepth(float v) {
			clearDepthAttachment  = true;
			depthClearValue       = v;
			return *this;
		}

		PassDesc &name(const std::string &str) {
			name_ = str;
			return *this;
		}

		PassDesc &numSamples(unsigned int n) {
			numSamples_ = n;
			return *this;
		}

	private:

		struct RTInfo {
			Rendertargets::Rendertargets  id;
			PassBegin                     passBegin;
			glm::vec4                     clearValue;
		};

		Rendertargets::Rendertargets                 depthStencil_;
		std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorRTs_;
		unsigned int                                 numSamples_;
		std::string                                  name_;
		bool                                         clearDepthAttachment;
		float                                        depthClearValue;
	};

	std::unordered_set<RenderPasses::RenderPasses> usedRenderPasses;

	// TODO: hide these
	RenderTargetHandle                                renderTargets[Rendertargets::Count];
	RenderPassHandle                                  renderPasses[RenderPasses::Count];
	FramebufferHandle                                 framebuffers[Framebuffers::Count];


	RenderGraph()
	: state(State::Invalid)
	{
	}


	~RenderGraph() {
	}


	void reset(Renderer &renderer);

	void renderTarget(Rendertargets::Rendertargets rt, const RenderTargetDesc &desc);

	// TODO: need layouts?
	void externalRenderTarget(Rendertargets::Rendertargets rt, Format format);

	void renderPass(RenderPasses::RenderPasses rp, PassDesc desc, RenderPassFunc f);

	void resolveMSAA(Rendertargets::Rendertargets source, Rendertargets::Rendertargets target);

	void blit(Rendertargets::Rendertargets source, Rendertargets::Rendertargets target);

	void presentRenderTarget(Rendertargets::Rendertargets rt);

	void build(Renderer &renderer);

	void bindExternalRT(Rendertargets::Rendertargets rt, RenderTargetHandle handle);

	void render(Renderer &renderer);

	PipelineHandle createPipeline(Renderer &renderer, RenderPasses::RenderPasses rp, PipelineDesc &desc);


	// TODO: remove these
	void renderPass(Renderer &renderer, RenderPasses::RenderPasses rp, const RenderPassDesc &rpDesc, Framebuffers::Framebuffers fb, const FramebufferDesc &fbDesc, RenderPassFunc f);

	void createRenderTarget(Renderer &renderer, Rendertargets::Rendertargets rt, const RenderTargetDesc &desc);

	void layoutTransition(Rendertargets::Rendertargets image, Layout src, Layout dest);
};


class SMAADemo {
	RendererDesc                                      rendererDesc;
	RenderGraph                                       renderGraph;

	// command line things
	std::vector<std::string>                          imageFiles;

	bool                                              recreateSwapchain;
	bool                                              recreateFramebuffers;
	bool                                              rebuildRG;
	bool                                              keepGoing;

	// aa things
	bool                                              antialiasing;
	AAMethod                                          aaMethod;
	bool                                              temporalAA;
	bool                                              temporalAAFirstFrame;
	unsigned int                                      temporalFrame;
	bool                                              temporalReproject;
	float                                             reprojectionWeightScale;
	// number of samples in current scene fb
	// 1 or 2 if SMAA
	// 2.. if MSAA
	unsigned int                                      numSamples;
	unsigned int                                      debugMode;
	unsigned int                                      fxaaQuality;
	unsigned int                                      msaaQuality;
	unsigned int                                      maxMSAAQuality;

	unsigned int                                      smaaQuality;
	SMAAEdgeMethod                                    smaaEdgeMethod;
	bool                                              smaaPredication;
	ShaderDefines::SMAAParameters                     smaaParameters;

	float                                             predicationThreshold;
	float                                             predicationScale;
	float                                             predicationStrength;

	// timing things
	bool                                              fpsLimitActive;
	uint32_t                                          fpsLimit;
	uint64_t                                          sleepFudge;
	uint64_t                                          tickBase;
	uint64_t                                          lastTime;
	uint64_t                                          freqMult;
	uint64_t                                          freqDiv;

	// scene things
	// 0 for cubes
	// 1.. for images
	unsigned int                                      activeScene;
	unsigned int                                      cubesPerSide;
	unsigned int                                      colorMode;
	bool                                              rotateCubes;
	bool                                              visualizeCubeOrder;
	unsigned int                                      cubeOrderNum;
	float                                             cameraRotation;
	float                                             cameraDistance;
	uint64_t                                          rotationTime;
	unsigned int                                      rotationPeriodSeconds;
	RandomGen                                         random;
	std::vector<Image>                                images;
	std::vector<ShaderDefines::Cube>                  cubes;

	glm::mat4                                         currViewProj;
	glm::mat4                                         prevViewProj;
	std::array<glm::vec4, 2>                          subsampleIndices;

	Renderer                                          renderer;
	Format                                            depthFormat;

	std::array<RenderTargetHandle, 2>                 temporalRTs;

	PipelineHandle                                    cubePipeline;
	PipelineHandle                                    imagePipeline;
	PipelineHandle                                    blitPipeline;
	PipelineHandle                                    guiPipeline;
	PipelineHandle                                    separatePipeline;
	std::array<PipelineHandle, 2>                     temporalAAPipelines;
	PipelineHandle                                    fxaaPipeline;

	BufferHandle                                      cubeVBO;
	BufferHandle                                      cubeIBO;

	SamplerHandle                                     linearSampler;
	SamplerHandle                                     nearestSampler;

	SMAAPipelines                                     smaaPipelines;
	TextureHandle                                     areaTex;
	TextureHandle                                     searchTex;

	// gui / input things
	TextureHandle                                     imguiFontsTex;
	ImGuiContext                                      *imGuiContext;
	bool                                              textInputActive;
	bool                                              rightShift, leftShift;
	bool                                              rightAlt,   leftAlt;
	bool                                              rightCtrl,  leftCtrl;
	char                                              imageFileName[inputTextBufferSize];
	char                                              clipboardText[inputTextBufferSize];


	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) = delete;
	SMAADemo &operator=(SMAADemo &&) = delete;

	void renderFXAA(RenderPasses::RenderPasses rp);

	void renderSeparate(RenderPasses::RenderPasses rp);

	void renderSMAAEdges(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets input, int pass);

	void renderSMAAWeights(RenderPasses::RenderPasses rp, int pass);

	void renderSMAABlend(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets input, int pass);

	void renderSMAADebug(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets rt);

	void renderTemporalAA(RenderPasses::RenderPasses rp);

	void updateGUI(uint64_t elapsed);

	void renderGUI(RenderPasses::RenderPasses rp);

	void renderCubeScene(RenderPasses::RenderPasses rp);

	void renderImageScene(RenderPasses::RenderPasses rp);

	void loadImage(const std::string &filename);

	uint64_t getNanoseconds() {
		return (SDL_GetPerformanceCounter() - tickBase) * freqMult / freqDiv;
	}

	void shuffleCubeRendering();

	void reorderCubeRendering();

	void colorCubes();


public:

	SMAADemo();

	~SMAADemo();

	void parseCommandLine(int argc, char *argv[]);

	void initRender();

	void rebuildRenderGraph();

	void createCubes();

	void mainLoopIteration();

	void processInput();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();
};


SMAADemo::SMAADemo()
: recreateSwapchain(false)
, recreateFramebuffers(false)
, rebuildRG(true)
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
, colorMode(0)
, rotateCubes(false)
, visualizeCubeOrder(false)
, cubeOrderNum(1)
, cameraRotation(0.0f)
, cameraDistance(25.0f)
, rotationTime(0)
, rotationPeriodSeconds(30)
, random(1)

, depthFormat(Format::Invalid)

, imGuiContext(nullptr)
, textInputActive(false)
, rightShift(false)
, leftShift(false)
, rightAlt(false)
, leftAlt(false)
, rightCtrl(false)
, leftCtrl(false)
{
	rendererDesc.swapchain.width  = 1280;
	rendererDesc.swapchain.height = 720;

	smaaQuality = maxSMAAQuality - 1;
	smaaEdgeMethod  = SMAAEdgeMethod::Color;
	smaaPredication = false;
	smaaParameters  = defaultSMAAParameters[smaaQuality];

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
	if (imGuiContext) {
		ImGui::DestroyContext(imGuiContext);
		imGuiContext = nullptr;
	}

	if (temporalRTs[0]) {
		assert(temporalRTs[1]);
		renderer.deleteRenderTarget(temporalRTs[0]);
		renderer.deleteRenderTarget(temporalRTs[1]);
	}

	renderGraph.reset(renderer);

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
		TCLAP::SwitchArg                       robustSwitch("",       "robust",     "Enable renderer robustness",    cmd, false);
		TCLAP::SwitchArg                       tracingSwitch("",      "trace",      "Enable renderer tracing",       cmd, false);
		TCLAP::SwitchArg                       noCacheSwitch("",      "nocache",    "Don't load shaders from cache", cmd, false);
		TCLAP::SwitchArg                       noOptSwitch("",        "noopt",      "Don't optimize shaders",        cmd, false);
		TCLAP::SwitchArg                       validateSwitch("",     "validate",   "Validate shader SPIR-V",        cmd, false);
		TCLAP::SwitchArg                       fullscreenSwitch("f",  "fullscreen", "Start in fullscreen mode",      cmd, false);
		TCLAP::SwitchArg                       noVsyncSwitch("",      "novsync",    "Disable vsync",                 cmd, false);
		TCLAP::SwitchArg                       noTransferQSwitch("",  "no-transfer-queue", "Disable transfer queue", cmd, false);

		TCLAP::ValueArg<unsigned int>          windowWidthSwitch("",  "width",      "Window width",  false, rendererDesc.swapchain.width,  "width",  cmd);
		TCLAP::ValueArg<unsigned int>          windowHeightSwitch("", "height",     "Window height", false, rendererDesc.swapchain.height, "height", cmd);

		TCLAP::ValueArg<unsigned int>          rotateSwitch("",       "rotate",     "Rotation period", false, 0,          "seconds", cmd);

		TCLAP::ValueArg<std::string>           aaMethodSwitch("m",    "method",     "AA Method",     false, "SMAA",        "SMAA/FXAA/MSAA", cmd);
		TCLAP::ValueArg<std::string>           aaQualitySwitch("q",   "quality",    "AA Quality",    false, "",            "", cmd);

		TCLAP::UnlabeledMultiArg<std::string>  imagesArg("images",    "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		rendererDesc.debug                 = debugSwitch.getValue();
		rendererDesc.robustness            = robustSwitch.getValue();
		rendererDesc.tracing               = tracingSwitch.getValue();
		rendererDesc.skipShaderCache       = noCacheSwitch.getValue();
		rendererDesc.optimizeShaders       = !noOptSwitch.getValue();
		rendererDesc.validateShaders       = validateSwitch.getValue();
		rendererDesc.transferQueue         = !noTransferQSwitch.getValue();
		rendererDesc.swapchain.fullscreen  = fullscreenSwitch.getValue();
		rendererDesc.swapchain.width       = windowWidthSwitch.getValue();
		rendererDesc.swapchain.height      = windowHeightSwitch.getValue();
		rendererDesc.swapchain.vsync       = noVsyncSwitch.getValue() ? VSync::Off : VSync::On;

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
						smaaQuality = i;
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
						smaaQuality = i;
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
	BufferHandle unused;
	BufferHandle instances;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout CubeSceneDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(CubeSceneDS, unused)    }
	, { DescriptorType::StorageBuffer,  offsetof(CubeSceneDS, instances) }
	, { DescriptorType::End,            0                                }
};

DSLayoutHandle CubeSceneDS::layoutHandle;


struct ColorCombinedDS {
	BufferHandle unused;
	CSampler color;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout ColorCombinedDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(ColorCombinedDS, unused) }
	, { DescriptorType::CombinedSampler,  offsetof(ColorCombinedDS, color)  }
	, { DescriptorType::End,              0,                                }
};

DSLayoutHandle ColorCombinedDS::layoutHandle;


struct ColorTexDS {
	BufferHandle unused;
	TextureHandle color;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout ColorTexDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(ColorTexDS, unused) }
	, { DescriptorType::Texture,        offsetof(ColorTexDS, color)  }
	, { DescriptorType::End,            0,                           }
};

DSLayoutHandle ColorTexDS::layoutHandle;


struct EdgeDetectionDS {
	BufferHandle  smaaUBO;
	CSampler color;
	CSampler predicationTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout EdgeDetectionDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(EdgeDetectionDS, smaaUBO)        }
	, { DescriptorType::CombinedSampler,  offsetof(EdgeDetectionDS, color)          }
	, { DescriptorType::CombinedSampler,  offsetof(EdgeDetectionDS, predicationTex) }
	, { DescriptorType::End,              0,                                        }
};

DSLayoutHandle EdgeDetectionDS::layoutHandle;


struct BlendWeightDS {
	BufferHandle  smaaUBO;
	CSampler edgesTex;
	CSampler areaTex;
	CSampler searchTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout BlendWeightDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(BlendWeightDS, smaaUBO)   }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, edgesTex)  }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, areaTex)   }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, searchTex) }
	, { DescriptorType::End,              0,                                 }
};

DSLayoutHandle BlendWeightDS::layoutHandle;


struct NeighborBlendDS {
	BufferHandle  smaaUBO;
	CSampler color;
	CSampler blendweights;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout NeighborBlendDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(NeighborBlendDS, smaaUBO)      }
	, { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, color)        }
	, { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, blendweights) }
	, { DescriptorType::End,              0                                       }
};

DSLayoutHandle NeighborBlendDS::layoutHandle;


struct TemporalAADS {
	BufferHandle  smaaUBO;
	CSampler currentTex;
	CSampler previousTex;
	CSampler velocityTex;

	static const DescriptorLayout layout[];
	static DSLayoutHandle layoutHandle;
};


const DescriptorLayout TemporalAADS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(TemporalAADS, smaaUBO)     }
	, { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, currentTex)  }
	, { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, previousTex) }
	, { DescriptorType::CombinedSampler,  offsetof(TemporalAADS, velocityTex) }
	, { DescriptorType::End,              0                                   }
};

DSLayoutHandle TemporalAADS::layoutHandle;



static const int numDepths = 5;
static const std::array<Format, numDepths> depths
  = { { Format::Depth24X8, Format::Depth24S8, Format::Depth32Float, Format::Depth16, Format::Depth16S8 } };


void SMAADemo::initRender() {
	renderer = Renderer::createRenderer(rendererDesc);
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
		imGuiContext = ImGui::CreateContext();
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


void SMAADemo::rebuildRenderGraph() {
	assert(rebuildRG);

	if (temporalRTs[0]) {
		assert(temporalRTs[1]);
		renderer.deleteRenderTarget(temporalRTs[0]);
		renderer.deleteRenderTarget(temporalRTs[1]);
	}

	renderGraph.reset(renderer);

	if (antialiasing && aaMethod == AAMethod::MSAA) {
		numSamples = msaaQualityToSamples(msaaQuality);
		assert(numSamples > 1);
	} else if (antialiasing && aaMethod == AAMethod::SMAA2X) {
		numSamples = 2;
	} else {
		numSamples = 1;
	}

	const unsigned int windowWidth  = rendererDesc.swapchain.width;
	const unsigned int windowHeight = rendererDesc.swapchain.height;

	LOG("create framebuffers at size %ux%u\n", windowWidth, windowHeight);

	if (activeScene == 0) {
		// cube scene

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main color")
				  .numSamples(numSamples)
				  .format(Format::sRGBA8)
				  .additionalViewFormat(Format::RGBA8)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::MainColor, rtDesc);
		}

		// TODO: only create velocity buffer when doing temporal AA
		{
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity")
				  .numSamples(numSamples)
				  .format(Format::RG16Float)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::Velocity, rtDesc);
		}

		Layout l = Layout::ShaderRead;
		if (!antialiasing || aaMethod == AAMethod::MSAA) {
			l = Layout::TransferSrc;
		}

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main depth")
				  .numSamples(numSamples)
				  .format(depthFormat)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::MainDepth, rtDesc);
		}

		std::string name = "scene ";
		if (numSamples > 1) {
			name += " MSAA x" + std::to_string(numSamples) + " ";
		}
		name += layoutName(l);

		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, l)
			  .color(1, Format::RG16Float, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
			  .depthStencil(depthFormat, PassBegin::Clear)
			  .clearDepth(1.0f)
			  .name(name)
			  .numSamples(numSamples);

		FramebufferDesc fbDesc;
		fbDesc.name("scene")
		      .color(0, renderGraph.renderTargets[Rendertargets::MainColor])
		      .color(1, renderGraph.renderTargets[Rendertargets::Velocity])
		      .depthStencil(renderGraph.renderTargets[Rendertargets::MainDepth]);

		renderGraph.renderPass(renderer, RenderPasses::Scene, rpDesc, Framebuffers::Scene, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderCubeScene(rp); } );
	} else {
		// image scene

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main color")
				  .numSamples(numSamples)
				  .format(Format::sRGBA8)
				  .additionalViewFormat(Format::RGBA8)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::MainColor, rtDesc);
		}

		// TODO: don't use velocity buffer
		{
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity")
				  .numSamples(numSamples)
				  .format(Format::RG16Float)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::Velocity, rtDesc);
		}

		Layout l = Layout::ShaderRead;
		if (!antialiasing || aaMethod == AAMethod::MSAA) {
			l = Layout::TransferSrc;
		}

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main depth")
				  .numSamples(numSamples)
				  .format(depthFormat)
				  .width(windowWidth)
				  .height(windowHeight);
			renderGraph.createRenderTarget(renderer, Rendertargets::MainDepth, rtDesc);
		}

		std::string name = "scene ";
		if (numSamples > 1) {
			name += " MSAA x" + std::to_string(numSamples) + " ";
		}
		name += layoutName(l);

		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, l)
			  .color(1, Format::RG16Float, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
			  .depthStencil(depthFormat, PassBegin::Clear)
			  .clearDepth(1.0f)
			  .name(name)
			  .numSamples(numSamples);

		FramebufferDesc fbDesc;
		fbDesc.name("scene")
		      .color(0, renderGraph.renderTargets[Rendertargets::MainColor])
		      .color(1, renderGraph.renderTargets[Rendertargets::Velocity])
		      .depthStencil(renderGraph.renderTargets[Rendertargets::MainDepth]);

		renderGraph.renderPass(renderer, RenderPasses::Scene, rpDesc, Framebuffers::Scene, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderImageScene(rp); } );
	}

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("final")
		      .format(Format::sRGBA8)
		      .width(windowWidth)
		      .height(windowHeight);
		renderGraph.createRenderTarget(renderer, Rendertargets::FinalRender, rtDesc);
	}

	if (antialiasing) {
		if (temporalAA) {
			// TODO: implement MSAA temporal AA
			if (aaMethod != AAMethod::MSAA) {
				RenderTargetDesc rtDesc;
				rtDesc.name("Temporal resolve 0")
				      .format(Format::sRGBA8)  // TODO: not right?
				      .width(windowWidth)
				      .height(windowHeight);
				temporalRTs[0] = renderer.createRenderTarget(rtDesc);

				rtDesc.name("Temporal resolve 1");
				temporalRTs[1] = renderer.createRenderTarget(rtDesc);

				renderGraph.externalRenderTarget(Rendertargets::TemporalPrevious, Format::sRGBA8);
				renderGraph.externalRenderTarget(Rendertargets::TemporalCurrent,  Format::sRGBA8);
			}

			switch (aaMethod) {
			case AAMethod::MSAA: {
				renderGraph.resolveMSAA(Rendertargets::MainColor, Rendertargets::FinalRender);
			} break;

			case AAMethod::FXAA: {
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("FXAA temporal");

					FramebufferDesc fbDesc;
					fbDesc.name("Temporal resolve 0")
						  .color(0, renderGraph.renderTargets[Rendertargets::TemporalCurrent]);

					renderGraph.renderPass(renderer, RenderPasses::FXAA, rpDesc, Framebuffers::TemporalCurrent, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderFXAA(rp); } );
				}

				{
					RenderPassDesc rpDesc;
					// TODO: check this
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
					      .name("final");

					FramebufferDesc fbDesc;
					fbDesc.name("final")
						  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
					renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderTemporalAA(rp); } );
				}

			} break;

			case AAMethod::SMAA: {
				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::Edges, rtDesc);
				}

				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::MainColor, 0); } );
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::BlendWeights, rtDesc);

					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA weights");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA weights")
						  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAWeights, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 0); } );
				}

				// full effect
				{
					RenderPassDesc rpDesc;
					// FIXME: should be RGBA since SMAA wants gamma space?
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA blend");

					FramebufferDesc fbDesc;
					fbDesc.name("Temporal resolve 0")
						  .color(0, renderGraph.renderTargets[Rendertargets::TemporalCurrent]);

					renderGraph.renderPass(renderer, RenderPasses::SMAABlend, rpDesc, Framebuffers::TemporalCurrent, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::MainColor, 0); } );
				}

				{
					RenderPassDesc rpDesc;
					// TODO: check this
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
					      .name("final");

					FramebufferDesc fbDesc;
					fbDesc.name("final")
						  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
					renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderTemporalAA(rp); } );
				}
			} break;

			case AAMethod::SMAA2X: {
				{
					RenderTargetDesc rtDesc;
					rtDesc.format(Format::sRGBA8)
						  .additionalViewFormat(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);

					for (unsigned int i = 0; i < 2; i++) {
						rtDesc.name("Subsample separate " + std::to_string(i));
						renderGraph.createRenderTarget(renderer, static_cast<Rendertargets::Rendertargets>(Rendertargets::Subsample1 + i), rtDesc);
					}
				}

				{
					RenderPassDesc rpDesc;
					rpDesc.name("Separate")
						  .color(0, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead)
						  .color(1, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead);

					FramebufferDesc fbDesc;
					fbDesc.name("Separate")
						  .color(0, renderGraph.renderTargets[Rendertargets::Subsample1])
						  .color(1, renderGraph.renderTargets[Rendertargets::Subsample2]);

					renderGraph.renderPass(renderer, RenderPasses::Separate, rpDesc, Framebuffers::Separate, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSeparate(rp); } );
				}

				// TODO: clean up the renderpass mess

				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::Edges, rtDesc);
				}

				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::Subsample1, 0); } );
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::BlendWeights, rtDesc);

					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA weights");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA weights")
						  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAWeights, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 0); } );
				}

				// full effect
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
					      .name("SMAA2x blend 0");

					FramebufferDesc fbDesc;
					fbDesc.name("Temporal resolve 0")
						  .color(0, renderGraph.renderTargets[Rendertargets::TemporalCurrent]);
					renderGraph.renderPass(renderer, RenderPasses::SMAA2XBlend1, rpDesc, Framebuffers::TemporalCurrent, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::Subsample1, 0); } );
				}

				// edges pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges2, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::Subsample2, 1); } );
				}

				// blendweights pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA weights");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA weights")
						  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);

					renderGraph.renderPass(renderer, RenderPasses::SMAAWeights2, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 1); } );
				}

				// full effect
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::sRGBA8, PassBegin::Keep, Layout::ColorAttachment, Layout::ColorAttachment)
					      .name("SMAA2x blend 1");

					FramebufferDesc fbDesc;
					fbDesc.name("Temporal resolve 0")
						  .color(0, renderGraph.renderTargets[Rendertargets::TemporalCurrent]);

					renderGraph.renderPass(renderer, RenderPasses::SMAA2XBlend2, rpDesc, Framebuffers::TemporalCurrent, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::Subsample2, 1); } );
				}

				// FIXME: move to renderpass
				renderGraph.layoutTransition(Rendertargets::TemporalCurrent, Layout::ColorAttachment, Layout::ShaderRead);
				{
					RenderPassDesc rpDesc;
					// TODO: check this
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
					      .name("final");

					FramebufferDesc fbDesc;
					fbDesc.name("final")
						  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
					renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderTemporalAA(rp); } );
				}
			} break;
			}

		} else {
			// no temporal AA
			switch (aaMethod) {
			case AAMethod::MSAA: {
				renderGraph.resolveMSAA(Rendertargets::MainColor, Rendertargets::FinalRender);
			} break;

			case AAMethod::FXAA: {
				RenderPassDesc rpDesc;
				rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
				      .name("FXAA no temporal");

				FramebufferDesc fbDesc;
				fbDesc.name("final")
					  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
				renderGraph.renderPass(renderer, RenderPasses::FXAA, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderFXAA(rp); } );
			} break;

			case AAMethod::SMAA: {
				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::Edges, rtDesc);
				}

				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::MainColor, 0); } );
				}

				switch (debugMode) {
				case 0:
					// blendweights pass
					{
						RenderTargetDesc rtDesc;
						rtDesc.name("SMAA weights")
							  .format(Format::RGBA8)
							  .width(windowWidth)
							  .height(windowHeight);
						renderGraph.createRenderTarget(renderer, Rendertargets::BlendWeights, rtDesc);

						RenderPassDesc rpDesc;
						rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
						      .name("SMAA weights");

						FramebufferDesc fbDesc;
						fbDesc.name("SMAA weights")
							  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);
						renderGraph.renderPass(renderer, RenderPasses::SMAAWeights, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 0); } );
					}

					// full effect
					{
						RenderPassDesc rpDesc;
						// TODO: check this
						rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
						      .name("final");

						FramebufferDesc fbDesc;
						fbDesc.name("final")
							  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
						renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::MainColor, 0); } );
					}

					break;

				case 1:
					// visualize edges
					{
						RenderPassDesc rpDesc;
						// TODO: check this
						rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
						      .name("final");

						FramebufferDesc fbDesc;
						fbDesc.name("final")
							  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
						renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAADebug(rp, Rendertargets::Edges); } );
					}

					break;

				case 2:
					// blendweights pass
					{
						RenderTargetDesc rtDesc;
						rtDesc.name("SMAA weights")
							  .format(Format::RGBA8)
							  .width(windowWidth)
							  .height(windowHeight);
						renderGraph.createRenderTarget(renderer, Rendertargets::BlendWeights, rtDesc);

						RenderPassDesc rpDesc;
						rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
						      .name("SMAA weights");

						FramebufferDesc fbDesc;
						fbDesc.name("SMAA weights")
							  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);
						renderGraph.renderPass(renderer, RenderPasses::SMAAWeights, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 0); } );
					}

					// visualize blend weights
					{
						RenderPassDesc rpDesc;
						// TODO: check this
						rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
						      .name("final");

						FramebufferDesc fbDesc;
						fbDesc.name("final")
							  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
						renderGraph.renderPass(renderer, RenderPasses::Final, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAADebug(rp, Rendertargets::BlendWeights); } );
					}

					break;
				}
			} break;

			case AAMethod::SMAA2X: {
				{
					RenderTargetDesc rtDesc;
					rtDesc.format(Format::sRGBA8)
						  .additionalViewFormat(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);

					for (unsigned int i = 0; i < 2; i++) {
						rtDesc.name("Subsample separate " + std::to_string(i));
						renderGraph.createRenderTarget(renderer, static_cast<Rendertargets::Rendertargets>(Rendertargets::Subsample1 + i), rtDesc);
					}

					RenderPassDesc rpDesc;
					rpDesc.name("Separate")
						  .color(0, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead)
						  .color(1, Format::sRGBA8, PassBegin::DontCare, Layout::Undefined, Layout::ShaderRead);

					FramebufferDesc fbDesc;
					fbDesc.name("Separate")
						  .color(0, renderGraph.renderTargets[Rendertargets::Subsample1])
						  .color(1, renderGraph.renderTargets[Rendertargets::Subsample2]);
					renderGraph.renderPass(renderer, RenderPasses::Separate, rpDesc, Framebuffers::Separate, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSeparate(rp); } );
				}

				// TODO: clean up the renderpass mess
				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::Edges, rtDesc);

					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::Subsample1, 0); } );
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
						  .format(Format::RGBA8)
						  .width(windowWidth)
						  .height(windowHeight);
					renderGraph.createRenderTarget(renderer, Rendertargets::BlendWeights, rtDesc);

					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA weights");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA weights")
						  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAWeights, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 0); } );
				}

				// final blend pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::sRGBA8, PassBegin::Clear, Layout::Undefined, Layout::ColorAttachment)
					       .name("SMAA2x blend 0");

					FramebufferDesc fbDesc;
					fbDesc.name("final")
						  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
					renderGraph.renderPass(renderer, RenderPasses::SMAA2XBlend1, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::Subsample1, 0); } );
				}

				// second pass
				// edges pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA edges");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA edges")
						  .color(0, renderGraph.renderTargets[Rendertargets::Edges]);
					renderGraph.renderPass(renderer, RenderPasses::SMAAEdges2, rpDesc, Framebuffers::SMAAEdges, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAEdges(rp, Rendertargets::Subsample2, 1); } );
				}

				// blendweights pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::RGBA8, PassBegin::Clear, Layout::Undefined, Layout::ShaderRead)
					      .name("SMAA weights");

					FramebufferDesc fbDesc;
					fbDesc.name("SMAA weights")
						  .color(0, renderGraph.renderTargets[Rendertargets::BlendWeights]);

					renderGraph.renderPass(renderer, RenderPasses::SMAAWeights2, rpDesc, Framebuffers::SMAAWeights, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAAWeights(rp, 1); } );
				}

				// final blend pass
				{
					RenderPassDesc rpDesc;
					rpDesc.color(0, Format::sRGBA8, PassBegin::Keep, Layout::ColorAttachment, Layout::ColorAttachment)
					      .name("SMAA2x blend 1");

					FramebufferDesc fbDesc;
					fbDesc.name("final")
						  .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);

					renderGraph.renderPass(renderer, RenderPasses::SMAA2XBlend2, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderSMAABlend(rp, Rendertargets::Subsample2, 1); } );
				}

			} break;
			}
		}
	} else {
		renderGraph.blit(Rendertargets::MainColor, Rendertargets::FinalRender);
	}

	{
		RenderPassDesc rpDesc;
		rpDesc.color(0, Format::sRGBA8, PassBegin::Keep, Layout::ColorAttachment, Layout::TransferSrc);
		rpDesc.name("GUI only");

		FramebufferDesc fbDesc;
		fbDesc.name("final")
		      .color(0, renderGraph.renderTargets[Rendertargets::FinalRender]);
		renderGraph.renderPass(renderer, RenderPasses::GUI, rpDesc, Framebuffers::Final, fbDesc, [this] (RenderPasses::RenderPasses rp) { this->renderGUI(rp); } );
	}

	renderGraph.presentRenderTarget(Rendertargets::FinalRender);

	renderGraph.build(renderer);

	cubePipeline           = PipelineHandle();
	imagePipeline          = PipelineHandle();
	blitPipeline           = PipelineHandle();
	guiPipeline            = PipelineHandle();
	separatePipeline       = PipelineHandle();
	temporalAAPipelines[0] = PipelineHandle();
	temporalAAPipelines[1] = PipelineHandle();
	fxaaPipeline           = PipelineHandle();

	smaaPipelines.edgePipeline         = PipelineHandle();
	smaaPipelines.blendWeightPipeline  = PipelineHandle();
	smaaPipelines.neighborPipelines[0] = PipelineHandle();
	smaaPipelines.neighborPipelines[1] = PipelineHandle();

	for (unsigned int i = 0; i < 2; i++) {
		temporalAAPipelines[i] = PipelineHandle();
	}

	separatePipeline = PipelineHandle();

	rebuildRG = false;
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


void SMAADemo::processInput() {
	ImGuiIO& io = ImGui::GetIO();

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

			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_LSHIFT:
				leftShift = true;
				break;

			case SDL_SCANCODE_RSHIFT:
				rightShift = true;
				break;

			case SDL_SCANCODE_LALT:
				leftAlt = true;
				break;

			case SDL_SCANCODE_RALT:
				rightAlt = true;
				break;

			case SDL_SCANCODE_LCTRL:
				leftCtrl = true;
				break;

			case SDL_SCANCODE_RCTRL:
				rightCtrl = true;
				break;

			default:
				break;
			}

			if (textInputActive) {
				break;
			}

			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				keepGoing = false;
				break;

			case SDL_SCANCODE_SPACE:
				rotateCubes = !rotateCubes;
				break;

			case SDL_SCANCODE_A:
				antialiasing = !antialiasing;
				if (aaMethod == AAMethod::MSAA || aaMethod == AAMethod::SMAA2X) {
					recreateFramebuffers = true;
				}
				if (temporalAA && aaMethod != AAMethod::MSAA) {
					// TODO: implement MSAA temporal AA
					temporalAAFirstFrame = true;
				}
				rebuildRG = true;
				break;

			case SDL_SCANCODE_C:
				if (rightShift || leftShift) {
					colorMode = (colorMode + 1) % 2;
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
					rebuildRG = true;
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
					rebuildRG = true;
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
					rebuildRG            = true;
					break;

				case AAMethod::FXAA:
					if (leftShift || rightShift) {
						fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
					} else {
						fxaaQuality = fxaaQuality + 1;
					}
					fxaaQuality = fxaaQuality % maxFXAAQuality;
					fxaaPipeline = PipelineHandle();
					break;

				case AAMethod::SMAA:
				case AAMethod::SMAA2X:
					if (leftShift || rightShift) {
						smaaQuality = smaaQuality + maxSMAAQuality - 1;
					} else {
						smaaQuality = smaaQuality + 1;
					}
					smaaQuality = smaaQuality % maxSMAAQuality;
					smaaParameters  = defaultSMAAParameters[smaaQuality];

					smaaPipelines.edgePipeline         = PipelineHandle();
					smaaPipelines.blendWeightPipeline  = PipelineHandle();
					smaaPipelines.neighborPipelines[0] = PipelineHandle();
					smaaPipelines.neighborPipelines[1] = PipelineHandle();

					break;

				}
				break;

			case SDL_SCANCODE_T:
				// TODO: implement MSAA temporal AA
				if (aaMethod != AAMethod::MSAA) {
					temporalAA = !temporalAA;
					if (temporalAA) {
						recreateFramebuffers = true;
						temporalAAFirstFrame = true;
						rebuildRG = true;
					}
				}
				break;

			case SDL_SCANCODE_V:
				switch (rendererDesc.swapchain.vsync) {
				case VSync::On:
					rendererDesc.swapchain.vsync = VSync::LateSwapTear;
					break;

				case VSync::LateSwapTear:
					rendererDesc.swapchain.vsync = VSync::Off;
					break;

				case VSync::Off:
					rendererDesc.swapchain.vsync = VSync::On;
					break;
				}
				recreateSwapchain = true;
				break;

			case SDL_SCANCODE_F:
				rendererDesc.swapchain.fullscreen = !rendererDesc.swapchain.fullscreen;
				recreateSwapchain = true;
				break;

			case SDL_SCANCODE_LEFT:
				sceneIncrement = -1;
				// fallthrough
			case SDL_SCANCODE_RIGHT:
				{
					// if old or new scene is cubes we must rebuild RG
					if (activeScene == 0) {
						rebuildRG = true;
					}

					// all images + cubes scene
					unsigned int numScenes = static_cast<unsigned int>(images.size()) + 1;
					activeScene = (activeScene + sceneIncrement + numScenes) % numScenes;

					if (activeScene == 0) {
						rebuildRG = true;
					}
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

			case SDL_SCANCODE_LALT:
				leftAlt = false;
				break;

			case SDL_SCANCODE_RALT:
				rightAlt = false;
				break;

			case SDL_SCANCODE_LCTRL:
				leftCtrl = false;
				break;

			case SDL_SCANCODE_RCTRL:
				rightCtrl = false;
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
				rendererDesc.swapchain.width  = event.window.data1;
				rendererDesc.swapchain.height = event.window.data2;
				recreateSwapchain = true;
				LOG("window resize to %ux%u\n", rendererDesc.swapchain.width, rendererDesc.swapchain.height);
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

	io.KeyShift = leftShift || rightShift;
	io.KeyAlt   = leftAlt   || rightAlt;
	io.KeyCtrl  = leftCtrl  || rightCtrl;
}


void SMAADemo::mainLoopIteration() {
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

	processInput();

	updateGUI(elapsed);

	if (activeScene == 0 && rotateCubes) {
		rotationTime += elapsed;

		// TODO: increasing rotation period can make cubes spin backwards
		const uint64_t rotationPeriod = rotationPeriodSeconds * 1000000000ULL;
		rotationTime   = rotationTime % rotationPeriod;
		cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
	}

	if (temporalAA && aaMethod != AAMethod::MSAA) {
		// TODO: implement MSAA temporal AA
		temporalFrame = (temporalFrame + 1) % 2;

		switch (aaMethod) {
		case AAMethod::MSAA:
		case AAMethod::FXAA:
			// not used
			subsampleIndices[0] = glm::vec4(0.0f);
			subsampleIndices[1] = glm::vec4(0.0f);
			break;

		case AAMethod::SMAA: {
			float v       = float(temporalFrame + 1);
			subsampleIndices[0] = glm::vec4(v, v, v, 0.0f);
			subsampleIndices[1] = glm::vec4(0.0f);
		} break;

		case AAMethod::SMAA2X:
			if (temporalFrame == 0) {
				subsampleIndices[0] = glm::vec4(5.0f, 3.0f, 1.0f, 3.0f);
				subsampleIndices[1] = glm::vec4(4.0f, 6.0f, 2.0f, 3.0f);
			} else {
				assert(temporalFrame == 1);
				subsampleIndices[0] = glm::vec4(3.0f, 5.0f, 1.0f, 4.0f);
				subsampleIndices[1] = glm::vec4(6.0f, 4.0f, 2.0f, 4.0f);
			}
			break;

		}
	} else {
		if (aaMethod == AAMethod::SMAA2X) {
			subsampleIndices[0] = glm::vec4(1.0, 1.0, 1.0, 0.0f);
		} else {
			subsampleIndices[0] = glm::vec4(0.0f);
		}
		subsampleIndices[1] = glm::vec4(2.0f, 2.0f, 2.0f, 0.0f);
	}

	render();
}


void SMAADemo::render() {
	if (recreateSwapchain) {
		renderer.setSwapchainDesc(rendererDesc.swapchain);
	}

	if (recreateSwapchain || recreateFramebuffers) {
		// TODO: shouldn't need this
		rebuildRG = true;
	}

	if (rebuildRG) {
		rebuildRenderGraph();
		assert(!rebuildRG);
	}

	// TODO: this should be in RenderGraph
	renderer.beginFrame();
	if (recreateSwapchain || recreateFramebuffers) {
		recreateSwapchain = false;
		recreateFramebuffers = false;

		glm::uvec2 size = renderer.getDrawableSize();
		LOG("drawable size: %ux%u\n", size.x, size.y);
		logFlush();
		rendererDesc.swapchain.width  = size.x;
		rendererDesc.swapchain.height = size.y;

		// TODO: shouldn't need this
		rebuildRG = true;
		rebuildRenderGraph();
	}

	if (temporalAA && aaMethod != AAMethod::MSAA) {
		// TODO: implement MSAA temporal AA
		renderGraph.bindExternalRT(Rendertargets::TemporalPrevious, temporalRTs[1 - temporalFrame]);
		renderGraph.bindExternalRT(Rendertargets::TemporalCurrent,  temporalRTs[    temporalFrame]);
	}

	renderGraph.render(renderer);
}


void RenderGraph::reset(Renderer &renderer) {
	assert(state == State::Invalid || state == State::Ready);
	state = State::Building;

	usedRenderPasses.clear();

	for (auto &p : pipelines) {
		renderer.deletePipeline(p.handle);
		p.handle = PipelineHandle();
	}
	pipelines.clear();

	for (unsigned int i = 0; i < Framebuffers::Count; i++) {
		if (framebuffers[i]) {
			renderer.deleteFramebuffer(framebuffers[i]);
			framebuffers[i] = FramebufferHandle();
		}
	}

	for (unsigned int i = 0; i < Rendertargets::Count; i++) {
		if (renderTargets[i]) {
			renderer.deleteRenderTarget(renderTargets[i]);
			renderTargets[i] = RenderTargetHandle();
		}
	}

	for (unsigned int i = 0; i < RenderPasses::Count; i++) {
		if (renderPasses[i]) {
			renderer.deleteRenderPass(renderPasses[i]);
			renderPasses[i] = RenderPassHandle();
		}
	}

	functions.clear();
}


PipelineHandle RenderGraph::createPipeline(Renderer &renderer, RenderPasses::RenderPasses rp, PipelineDesc &desc) {
	assert(state == State::Ready || state == State::Rendering);

	assert(renderPasses[rp]);
	desc.renderPass(renderPasses[rp]);

	// TODO: use hash map
	for (const auto &pipeline : pipelines) {
		if (pipeline.desc == desc) {
			return pipeline.handle;
		}
	}

	auto handle = renderer.createPipeline(desc);

	Pipeline pipeline;
	pipeline.desc   = desc;
	pipeline.handle = handle;
	pipelines.emplace_back(std::move(pipeline));

	return handle;
}


void RenderGraph::createRenderTarget(Renderer &renderer, Rendertargets::Rendertargets rt, const RenderTargetDesc &desc) {
	assert(state == State::Building);
	assert(!renderTargets[rt]);

	renderTargets[rt] = renderer.createRenderTarget(desc);
}


void RenderGraph::externalRenderTarget(Rendertargets::Rendertargets rt UNUSED, Format /* format */) {
	assert(!renderTargets[rt]);
	// TODO
}


void RenderGraph::bindExternalRT(Rendertargets::Rendertargets /* rt */, RenderTargetHandle /* handle */) {
	// TODO
}


void RenderGraph::renderPass(Renderer &renderer, RenderPasses::RenderPasses rp, const RenderPassDesc &rpDesc, Framebuffers::Framebuffers fb, const FramebufferDesc &fbDesc_, RenderPassFunc f) {
	assert(state == State::Building);

	assert(!renderPasses[rp]);

	renderPasses[rp] = renderer.createRenderPass(rpDesc);

	assert(renderPasses[rp]);
	auto rpHandle = renderPasses[rp];

	FramebufferDesc fbDesc(fbDesc_);
	fbDesc.renderPass(renderPasses[rp]);

	if (!framebuffers[fb]) {
		framebuffers[fb] = renderer.createFramebuffer(fbDesc);
	}

	auto fbHandle = framebuffers[fb];
	assert(fbHandle);

	auto temp UNUSED = usedRenderPasses.insert(rp);
	assert(temp.second);

	functions.push_back([rpHandle, fbHandle, rp, f] (Renderer &r) {
		r.beginRenderPass(rpHandle, fbHandle);
		f(rp);
		r.endRenderPass();
	} );
}


void RenderGraph::resolveMSAA(Rendertargets::Rendertargets source, Rendertargets::Rendertargets target) {
	assert(state == State::Building);

	RenderTargetHandle sourceHandle = renderTargets[source];
	RenderTargetHandle targetHandle = renderTargets[target];
	functions.push_back([=] (Renderer &r) {
		r.layoutTransition(targetHandle, Layout::Undefined, Layout::TransferDst);
		r.resolveMSAA(sourceHandle, targetHandle);
		r.layoutTransition(targetHandle, Layout::TransferDst, Layout::ColorAttachment);
	} );

	// TODO
}


void RenderGraph::blit(Rendertargets::Rendertargets source, Rendertargets::Rendertargets target) {
	assert(state == State::Building);

	RenderTargetHandle sourceHandle = renderTargets[source];
	RenderTargetHandle targetHandle = renderTargets[target];
	functions.push_back([=] (Renderer &r) {
		r.layoutTransition(targetHandle, Layout::Undefined, Layout::TransferDst);
		r.blit(sourceHandle, targetHandle);
		r.layoutTransition(targetHandle, Layout::TransferDst, Layout::ColorAttachment);
	} );

	// TODO
}


void RenderGraph::layoutTransition(Rendertargets::Rendertargets image, Layout src, Layout dest) {
	assert(state == State::Building);

	RenderTargetHandle imageHandle = renderTargets[image];
	functions.push_back([=] (Renderer &r) {
		r.layoutTransition(imageHandle, src, dest);
	} );

	// TODO
}


void RenderGraph::presentRenderTarget(Rendertargets::Rendertargets rt) {
	assert(state == State::Building);

	finalTarget = rt;

	// TODO
}



void RenderGraph::build(Renderer & /* renderer */) {
	assert(state == State::Building);
	state = State::Ready;

	// TODO
}


void RenderGraph::render(Renderer &renderer) {
	assert(state == State::Ready);
	state = State::Rendering;

	for (const auto &f : functions) {
		f(renderer);
	}

	renderer.presentFrame(renderTargets[finalTarget]);

	assert(state == State::Rendering);
	state = State::Ready;
}


void SMAADemo::renderCubeScene(RenderPasses::RenderPasses rp) {
	if (!cubePipeline) {
		std::string name = "cubes";
		if (numSamples > 1) {
			name += " MSAA x" + std::to_string(numSamples);
		}

		PipelineDesc plDesc;
		plDesc.name(name)
		      .vertexShader("cube")
		      .fragmentShader("cube")
		      .numSamples(numSamples)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<CubeSceneDS>(1)
		      .vertexAttrib(ATTR_POS, 0, 3, VtxFormat::Float, 0)
		      .vertexBufferStride(ATTR_POS, sizeof(Vertex))
		      .depthWrite(true)
		      .depthTest(true)
		      .cullFaces(true);

		cubePipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}
    assert(cubePipeline);

	renderer.bindPipeline(cubePipeline);

	const unsigned int windowWidth  = rendererDesc.swapchain.width;
	const unsigned int windowHeight = rendererDesc.swapchain.height;

	ShaderDefines::Globals globals;
	globals.screenSize            = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
	globals.guiOrtho              = glm::ortho(0.0f, float(windowWidth), float(windowHeight), 0.0f);

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
	if (antialiasing && temporalAA && aaMethod != AAMethod::MSAA) {
		// TODO: implement MSAA temporal AA
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
		glm::mat4 jitterMatrix = glm::translate(glm::identity<glm::mat4>(), glm::vec3(jitter, 0.0f));
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
	// FIXME: remove unused UBO hack
	uint32_t temp    = 0;
	cubeDS.unused    = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	cubeDS.instances = renderer.createEphemeralBuffer(BufferType::Storage, static_cast<uint32_t>(sizeof(ShaderDefines::Cube) * cubes.size()), &cubes[0]);
	renderer.bindDescriptorSet(1, cubeDS);

	unsigned int numCubes = static_cast<unsigned int>(cubes.size());
	if (visualizeCubeOrder) {
		cubeOrderNum = cubeOrderNum % numCubes;
		cubeOrderNum++;
		numCubes     = cubeOrderNum;
	}

	renderer.drawIndexedInstanced(3 * 2 * 6, numCubes);
}


void SMAADemo::renderImageScene(RenderPasses::RenderPasses rp) {
	if (!imagePipeline) {
		PipelineDesc plDesc;
		plDesc.numSamples(numSamples)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader("image")
		      .fragmentShader("image")
		      .name("image");

		imagePipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	renderer.bindPipeline(imagePipeline);

	assert(activeScene > 0);
	const auto &image = images.at(activeScene - 1);

	const unsigned int windowWidth  = rendererDesc.swapchain.width;
	const unsigned int windowHeight = rendererDesc.swapchain.height;

	renderer.setViewport(0, 0, windowWidth, windowHeight);

	ShaderDefines::Globals globals;
	globals.screenSize            = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
	globals.guiOrtho              = glm::ortho(0.0f, float(windowWidth), float(windowHeight), 0.0f);

	GlobalDS globalDS;
	globalDS.globalUniforms  = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::Globals), &globals);
	globalDS.linearSampler   = linearSampler;
	globalDS.nearestSampler  = nearestSampler;
	renderer.bindDescriptorSet(0, globalDS);

	assert(activeScene - 1 < images.size());
	ColorTexDS colorDS;
	// FIXME: remove unused UBO hack
	uint32_t temp  = 0;
	colorDS.unused = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	colorDS.color = image.tex;
	renderer.bindDescriptorSet(1, colorDS);
	renderer.draw(0, 3);
}


void SMAADemo::renderFXAA(RenderPasses::RenderPasses rp) {
	if (!fxaaPipeline) {
		std::string qualityString(fxaaQualityLevels[fxaaQuality]);

		ShaderMacros macros;
		macros.emplace("FXAA_QUALITY_PRESET", qualityString);

		PipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorCombinedDS>(1)
		      .shaderMacros(macros)
		      .vertexShader("fxaa")
		      .fragmentShader("fxaa")
		      .name(std::string("FXAA ") + qualityString);

		fxaaPipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}
	assert(fxaaPipeline);

	renderer.bindPipeline(fxaaPipeline);
	ColorCombinedDS colorDS;
	// FIXME: remove unused UBO hack
	uint32_t temp         = 0;
	colorDS.unused        = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	colorDS.color.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::MainColor]);
	colorDS.color.sampler = linearSampler;
	renderer.bindDescriptorSet(1, colorDS);
	renderer.draw(0, 3);
}


void SMAADemo::renderSeparate(RenderPasses::RenderPasses rp) {
	if (!separatePipeline) {
		PipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
			  .descriptorSetLayout<ColorCombinedDS>(1)  // TODO: does this need its own DS?
			  .vertexShader("temporal")
			  .fragmentShader("separate")
			  .name("subsample separate");

		separatePipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	renderer.bindPipeline(separatePipeline);
	ColorCombinedDS separateDS;
	// FIXME: remove unused UBO hack
	uint32_t temp            = 0;
	separateDS.unused        = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	separateDS.color.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::MainColor]);
	separateDS.color.sampler = nearestSampler;
	renderer.bindDescriptorSet(1, separateDS);
	renderer.draw(0, 3);
}


void SMAADemo::renderSMAAEdges(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets input, int pass) {
	if (!smaaPipelines.edgePipeline) {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.emplace(qualityString, "1");

		if (smaaEdgeMethod != SMAAEdgeMethod::Color) {
			macros.emplace("EDGEMETHOD", std::to_string(static_cast<uint8_t>(smaaEdgeMethod)));
		}

		if (smaaPredication && smaaEdgeMethod != SMAAEdgeMethod::Depth) {
			macros.emplace("SMAA_PREDICATION", "1");
		}

		PipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .shaderMacros(macros)
		      .descriptorSetLayout<EdgeDetectionDS>(1)
		      .vertexShader("smaaEdge")
		      .fragmentShader("smaaEdge")
		      .name(std::string("SMAA edges ") + std::to_string(smaaQuality));
		smaaPipelines.edgePipeline      = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	renderer.bindPipeline(smaaPipelines.edgePipeline);

	// TODO: this is redundant, clean it up
	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[pass];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	EdgeDetectionDS edgeDS;
	edgeDS.smaaUBO = smaaUBOBuf;
	if (smaaEdgeMethod == SMAAEdgeMethod::Depth) {
		edgeDS.color.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::MainDepth]);
	} else {
		edgeDS.color.tex     = renderer.getRenderTargetView(renderGraph.renderTargets[input], Format::RGBA8);
	}
	edgeDS.color.sampler = nearestSampler;
	edgeDS.predicationTex.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::MainDepth]);
	edgeDS.predicationTex.sampler = nearestSampler;
	renderer.bindDescriptorSet(1, edgeDS);
	renderer.draw(0, 3);
}


void SMAADemo::renderSMAAWeights(RenderPasses::RenderPasses rp, int pass) {
	if (!smaaPipelines.blendWeightPipeline) {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.emplace(qualityString, "1");

		PipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<BlendWeightDS>(1)
		      .shaderMacros(macros)
		      .vertexShader("smaaBlendWeight")
		      .fragmentShader("smaaBlendWeight")
		      .name(std::string("SMAA weights ") + std::to_string(smaaQuality));
		smaaPipelines.blendWeightPipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	// TODO: this is redundant, clean it up
	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[pass];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	renderer.bindPipeline(smaaPipelines.blendWeightPipeline);
	BlendWeightDS blendWeightDS;
	blendWeightDS.smaaUBO           = smaaUBOBuf;
	blendWeightDS.edgesTex.tex      = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::Edges]);
	blendWeightDS.edgesTex.sampler  = linearSampler;
	blendWeightDS.areaTex.tex       = areaTex;
	blendWeightDS.areaTex.sampler   = linearSampler;
	blendWeightDS.searchTex.tex     = searchTex;
	blendWeightDS.searchTex.sampler = linearSampler;
	renderer.bindDescriptorSet(1, blendWeightDS);

	renderer.draw(0, 3);
}


void SMAADemo::renderSMAABlend(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets input, int pass) {
	if (!smaaPipelines.neighborPipelines[pass]) {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.emplace(qualityString, "1");

		PipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<NeighborBlendDS>(1)
		      .shaderMacros(macros)
		      .vertexShader("smaaNeighbor")
		      .fragmentShader("smaaNeighbor");

		if (pass == 0) {
			plDesc.name(std::string("SMAA blend ") + std::to_string(smaaQuality));
		} else {
			assert(pass == 1);
			plDesc.blending(true)
			      .sourceBlend(BlendFunc::Constant)
			      .destinationBlend(BlendFunc::Constant)
			      .name(std::string("SMAA blend (S2X) ") + std::to_string(smaaQuality));
		}

		smaaPipelines.neighborPipelines[pass] = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	// TODO: this is redundant, clean it up
	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[pass];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	// full effect
	renderer.bindPipeline(smaaPipelines.neighborPipelines[pass]);

	NeighborBlendDS neighborBlendDS;
	neighborBlendDS.smaaUBO              = smaaUBOBuf;
	neighborBlendDS.color.tex            = renderer.getRenderTargetTexture(renderGraph.renderTargets[input]);
	neighborBlendDS.color.sampler        = linearSampler;
	neighborBlendDS.blendweights.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::BlendWeights]);
	neighborBlendDS.blendweights.sampler = linearSampler;
	renderer.bindDescriptorSet(1, neighborBlendDS);

	renderer.draw(0, 3);
}


void SMAADemo::renderSMAADebug(RenderPasses::RenderPasses rp, Rendertargets::Rendertargets rt) {
	if (!blitPipeline) {
		PipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader("blit")
		      .fragmentShader("blit")
		      .name("blit");

		blitPipeline = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	ColorTexDS blitDS;
	renderer.bindPipeline(blitPipeline);
	// FIXME: remove unused UBO hack
	uint32_t temp  = 0;
	blitDS.unused  = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	blitDS.color   = renderer.getRenderTargetTexture(renderGraph.renderTargets[rt] );
	renderer.bindDescriptorSet(1, blitDS);

	renderer.draw(0, 3);
}


void SMAADemo::renderTemporalAA(RenderPasses::RenderPasses rp) {
	if (!temporalAAPipelines[temporalReproject]) {
		ShaderMacros macros;
		macros.emplace("SMAA_REPROJECTION", std::to_string(temporalReproject));

		PipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
			  .descriptorSetLayout<TemporalAADS>(1)
			  .vertexShader("temporal")
			  .fragmentShader("temporal")
			  .shaderMacros(macros)
			  .name("temporal AA");

		temporalAAPipelines[temporalReproject] = renderGraph.createPipeline(renderer, rp, plDesc);
	}

	renderer.bindPipeline(temporalAAPipelines[temporalReproject]);

	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[0];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	TemporalAADS temporalDS;
	temporalDS.smaaUBO             = smaaUBOBuf;
	temporalDS.currentTex.tex      = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::TemporalCurrent]);
	temporalDS.currentTex.sampler  = nearestSampler;
	if (temporalAAFirstFrame) {
		// to prevent flicker on first frame after enabling
		// TODO: should just do blit
		temporalDS.previousTex.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::TemporalCurrent]);
		temporalDS.previousTex.sampler = nearestSampler;
		temporalAAFirstFrame = false;
	} else {
		temporalDS.previousTex.tex     = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::TemporalPrevious]);
		temporalDS.previousTex.sampler = nearestSampler;
	}
	temporalDS.velocityTex.tex         = renderer.getRenderTargetTexture(renderGraph.renderTargets[Rendertargets::Velocity]);
	temporalDS.velocityTex.sampler     = nearestSampler;

	renderer.bindDescriptorSet(1, temporalDS);
	renderer.draw(0, 3);
}


void SMAADemo::updateGUI(uint64_t elapsed) {
	const unsigned int windowWidth  = rendererDesc.swapchain.width;
	const unsigned int windowHeight = rendererDesc.swapchain.height;

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
			if (aaChanged && temporalAA && aaMethod != AAMethod::MSAA) {
				// TODO: implement MSAA temporal AA
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

				bool tempTAA = temporalAA;
				if (aaMethod == AAMethod::MSAA) {
					// TODO: implement MSAA temporal AA
					tempTAA = false;
				}
				if (ImGui::Checkbox("Temporal AA", &tempTAA)) {
					recreateFramebuffers = true;
					rebuildRG            = true;
				}
				if (aaMethod != AAMethod::MSAA) {
					temporalAA = tempTAA;
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
				rebuildRG = true;
			}

			if (msaaChanged && aaMethod == AAMethod::MSAA) {
				assert(msaaq >= 0);
				msaaQuality = static_cast<unsigned int>(msaaq);
				recreateFramebuffers = true;
				rebuildRG            = true;
			}

			ImGui::Separator();
			int sq = smaaQuality;
			ImGui::Combo("SMAA quality", &sq, smaaQualityLevels, maxSMAAQuality);
			assert(sq >= 0);
			assert(sq < int(maxSMAAQuality));
			if (int(smaaQuality) != sq) {
				smaaQuality = sq;
				if (sq != 0) {
					smaaParameters  = defaultSMAAParameters[sq];
				}
				smaaPipelines.edgePipeline         = PipelineHandle();
				smaaPipelines.blendWeightPipeline  = PipelineHandle();
				smaaPipelines.neighborPipelines[0] = PipelineHandle();
				smaaPipelines.neighborPipelines[1] = PipelineHandle();
			}

			if (ImGui::CollapsingHeader("SMAA custom properties")) {
				// parameters can only be changed in custom mode
				// https://github.com/ocornut/imgui/issues/211
				if (smaaQuality != 0) {
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

				if (smaaQuality != 0) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}

			ImGui::Checkbox("Predicated thresholding", &smaaPredication);

			if (!smaaPredication) {
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

			if (!smaaPredication) {
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}

			int em = static_cast<int>(smaaEdgeMethod);
			ImGui::Text("SMAA edge detection");
			ImGui::RadioButton("Color", &em, static_cast<int>(SMAAEdgeMethod::Color));
			ImGui::RadioButton("Luma",  &em, static_cast<int>(SMAAEdgeMethod::Luma));
			ImGui::RadioButton("Depth", &em, static_cast<int>(SMAAEdgeMethod::Depth));
			smaaEdgeMethod = static_cast<SMAAEdgeMethod>(em);

			int d = debugMode;
			ImGui::Combo("SMAA debug", &d, smaaDebugModes, 3);
			assert(d >= 0);
			assert(d < 3);
			if (int(debugMode) != d) {
				debugMode = d;
				rebuildRG = true;
			}

			int fq = fxaaQuality;
			ImGui::Separator();
			ImGui::Combo("FXAA quality", &fq, fxaaQualityLevels, maxFXAAQuality);
			assert(fq >= 0);
			assert(fq < int(maxFXAAQuality));
			if (fq != int(fxaaQuality)) {
				fxaaPipeline = PipelineHandle();
				fxaaQuality = fq;
			}
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
			if (s != int(activeScene)) {
				// if old or new scene is cubes we must rebuild RG
				if (activeScene == 0 || s == 0) {
					rebuildRG = true;
				}
				activeScene = s;
			}

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
			bool temp = ImGui::Checkbox("Fullscreen", &rendererDesc.swapchain.fullscreen);
            // don't nuke recreateSwapchain in case it was already true
			if (temp) {
				recreateSwapchain = true;
			}

			int vsyncTemp = static_cast<int>(rendererDesc.swapchain.vsync);
			ImGui::Text("V-Sync");
			ImGui::RadioButton("Off",            &vsyncTemp, 0);
			ImGui::RadioButton("On",             &vsyncTemp, 1);
			ImGui::RadioButton("Late swap tear", &vsyncTemp, 2);

			if (vsyncTemp != static_cast<int>(rendererDesc.swapchain.vsync)) {
				recreateSwapchain = true;
				rendererDesc.swapchain.vsync = static_cast<VSync>(vsyncTemp);
			}

			int n = rendererDesc.swapchain.numFrames;
			// TODO: ask Renderer for the limits
			if (ImGui::SliderInt("frames ahead", &n, 1, 16)) {
				rendererDesc.swapchain.numFrames = n;
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
}


void SMAADemo::renderGUI(RenderPasses::RenderPasses rp) {
	auto drawData = ImGui::GetDrawData();
	assert(drawData->Valid);

	if (drawData->CmdListsCount > 0) {
		assert(drawData->CmdLists      != nullptr);
		assert(drawData->TotalVtxCount >  0);
		assert(drawData->TotalIdxCount >  0);

        if (!guiPipeline) {
			PipelineDesc plDesc;
			plDesc.descriptorSetLayout<GlobalDS>(0)
				  .descriptorSetLayout<ColorTexDS>(1)
				  .vertexShader("gui")
				  .fragmentShader("gui")
				  .blending(true)
				  .sourceBlend(BlendFunc::SrcAlpha)
				  .destinationBlend(BlendFunc::OneMinusSrcAlpha)
				  .scissorTest(true)
				  .vertexAttrib(ATTR_POS,   0, 2, VtxFormat::Float,  offsetof(ImDrawVert, pos))
				  .vertexAttrib(ATTR_UV,    0, 2, VtxFormat::Float,  offsetof(ImDrawVert, uv))
				  .vertexAttrib(ATTR_COLOR, 0, 4, VtxFormat::UNorm8, offsetof(ImDrawVert, col))
				  .vertexBufferStride(ATTR_POS, sizeof(ImDrawVert))
				  .name("gui");

			guiPipeline = renderGraph.createPipeline(renderer, rp, plDesc);
		}

		renderer.bindPipeline(guiPipeline);
		ColorTexDS colorDS;
		// FIXME: remove unused UBO hack
		uint32_t temp = 0;
		colorDS.unused = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
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
					renderer.drawIndexedOffset(pcmd->ElemCount, idx_buffer_offset, 0, cmd_list->VtxBuffer.Size);
				}
				idx_buffer_offset += pcmd->ElemCount;
			}
		}
#if 0
		LOG("CmdListsCount: %d\n", drawData->CmdListsCount);
		LOG("TotalVtxCount: %d\n", drawData->TotalVtxCount);
		LOG("TotalIdxCount: %d\n", drawData->TotalIdxCount);
#endif // 0
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
				logFlush();
				break;
			} catch (...) {
				LOG("caught unknown exception\n");
				logFlush();
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
