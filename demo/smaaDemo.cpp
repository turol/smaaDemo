/*
Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

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

#include <charconv>
#include <chrono>
#include <thread>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <imgui.h>

#include <magic_enum/magic_enum_all.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tclap/CmdLine.h>

#define PAR_SHAPES_T uint32_t
#include <par_shapes.h>

#include <pcg_random.hpp>

#include "renderer/Renderer.h"
#include "renderer/RenderGraph.h"
#include "utils/Hash.h"
#include "utils/Utils.h"

#include "AreaTex.h"
#include "SearchTex.h"

// AFTER Renderer.h because it sets GLM_FORCE_* macros which affect these
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


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
};


enum class SMAADebugMode : uint8_t {
	  None
	, Edges
	, Weights
};


enum class Shape : uint8_t {
	  Cube
	, Tetrahedron
	, Octahedron
	, Dodecahedron
	, Icosahedron
	, Sphere
	, Torus
	, KleinBottle
	, TrefoilKnot
};


enum class ColorMode : uint8_t {
	  RGB
	, YCbCr
};


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
	RandomGen(RandomGen &&) noexcept            = delete;
	RandomGen &operator=(RandomGen &&) noexcept = delete;

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
	assert(q > 0);
	assert(isPow2(q));

	if (q == 1) {
		return 0;
	}

	LOG_TODO("have a more specific function for this")
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


static const float defaultCameraDistance = 25.0f;


enum class SMAAEdgeMethod : uint8_t {
	  Color
	, Luma
	, Depth
};


#ifndef IMGUI_DISABLE


template <typename Enum> Enum enumRadioButton(Enum current) {
	int i = magic_enum::enum_integer(current);

	for (Enum e : magic_enum::enum_values<Enum>()) {
		ImGui::RadioButton(magic_enum::enum_name(e).data(), &i, magic_enum::enum_integer(e));
	}

	return magic_enum::enum_value<Enum>(i);
}


#endif  // IMGUI_DISABLE


struct Image {
	std::string    filename;
	std::string    shortName;
	TextureHandle  tex;
	unsigned int   width = 0;
	unsigned int   height = 0;


	Image()
	{
	}


	Image(const Image &)             = default;
	Image &operator=(const Image &)  = default;

	Image(Image &&) noexcept            = default;
	Image &operator=(Image &&) noexcept = default;

	~Image() {}
};


enum class Rendertargets : uint32_t {
	  Invalid
	, MainColor
	, MainDepth
	, Velocity
	, VelocityMS
	, SMAAEdges
	, SMAABlendWeights
	, TemporalPrevious
	, TemporalCurrent
	, Subsample1
	, Subsample2
	, FinalRender
};


static const char *to_string(Rendertargets r) {
	return magic_enum::enum_name(r).data();
}


enum class RenderPasses : uint32_t {
	  Invalid
	, Scene
	, Final
	, GUI
	, MSAAResolve
	, FXAA
	, Separate
	, SMAAEdges
	, SMAAWeights
	, SMAABlend
	, SMAAEdges2
	, SMAAWeights2
	, SMAA2XBlend1
	, SMAA2XBlend2
};


static const char *to_string(RenderPasses r) {
	return magic_enum::enum_name(r).data();
}


namespace std {


template <> struct hash<RenderPasses> {
	size_t operator()(const RenderPasses &k) const {
		return hash<size_t>()(static_cast<size_t>(k));
	}

};


template <> struct hash<Rendertargets> {
	size_t operator()(const Rendertargets &k) const {
		return hash<size_t>()(static_cast<size_t>(k));
	}

};


template <> struct hash<Format> {
	size_t operator()(const Format &k) const {
		return hash<size_t>()(static_cast<size_t>(k));
	}
};


template <> struct hash<std::pair<Rendertargets, Format> > {
	size_t operator()(const std::pair<Rendertargets, Format> &k) const {
		size_t a = hash<Rendertargets>()(k.first);
		size_t b = hash<Format>()(k.second);

		LOG_TODO("put this in a helper function")
		a ^= b + 0x9e3779b9 + (a << 6) + (a >> 2);

		return a;
	}
};


} // namespace std


struct SMAAPipelines {
	GraphicsPipelineHandle                 edgePipeline;
	GraphicsPipelineHandle                 blendWeightPipeline;
	std::array<GraphicsPipelineHandle, 2>  neighborPipelines;
};


namespace renderer {


template<>
struct Default<Rendertargets> {
	static const Rendertargets  value;
};

const Rendertargets  Default<Rendertargets>::value = Rendertargets::Invalid;


template<>
struct Default<RenderPasses> {
	static const RenderPasses  value;
};

const RenderPasses  Default<RenderPasses>::value = RenderPasses::Invalid;


} // namespace renderer


struct ShapeRenderBuffers {
	unsigned int  numVertices = 0;
	BufferHandle  vertices;
	BufferHandle  indices;


	ShapeRenderBuffers()
	{}

	// not copyable
	ShapeRenderBuffers(const ShapeRenderBuffers &other)            = delete;
	ShapeRenderBuffers &operator=(const ShapeRenderBuffers &other) = delete;

	// movable
	ShapeRenderBuffers(ShapeRenderBuffers &&other) noexcept {
		numVertices = other.numVertices;
		other.numVertices = 0;

		vertices = std::move(other.vertices);
		other.vertices.reset();

		indices  = std::move(other.indices);
		other.indices.reset();
	}

	ShapeRenderBuffers &operator=(ShapeRenderBuffers &&other) {
		assert(this != &other);

		assert(!numVertices);
		assert(!vertices);
		assert(!indices);

		numVertices = other.numVertices;
		other.numVertices = 0;

		vertices = std::move(other.vertices);
		other.vertices.reset();

		indices  = std::move(other.indices);
		other.indices.reset();

		return *this;
	}

	~ShapeRenderBuffers() {
		assert(!numVertices);
		assert(!vertices);
		assert(!indices);
	}
};


struct GameControllerDeleter {
	void operator()(SDL_GameController *gc) { SDL_GameControllerClose(gc); }
};


using GameController = std::unique_ptr<SDL_GameController, GameControllerDeleter>;


class SMAADemo {
	using DemoRenderGraph = RenderGraph<Rendertargets, RenderPasses>;

	RendererDesc                                      rendererDesc;
	glm::uvec2                                        renderSize;
	DemoRenderGraph                                   renderGraph;

	// command line things
	std::vector<std::string>                          imageFiles;

	bool                                              recreateSwapchain = false;
	bool                                              rebuildRG         = true;
	bool                                              keepGoing         = true;
	bool                                              autoMode          = false;
	LayoutUsage                                       layoutUsage       = LayoutUsage::Specific;

	// aa things
	bool                                              antialiasing            = true;
	AAMethod                                          aaMethod                = AAMethod::SMAA;
	bool                                              aaMethodSetExplicitly   = false;
	bool                                              temporalAA              = false;
	bool                                              temporalAAFirstFrame    = false;
	unsigned int                                      temporalFrame           = 0;
	bool                                              temporalReproject       = true;
	float                                             reprojectionWeightScale = 30.0f;
	// number of samples in current scene fb
	// 1 or 2 if SMAA
	// 2.. if MSAA
	unsigned int                                      numSamples     = 1;
	SMAADebugMode                                     debugMode      = SMAADebugMode::None;
	unsigned int                                      fxaaQuality    = maxFXAAQuality - 1;
	unsigned int                                      msaaQuality    = 0;
	unsigned int                                      maxMSAAQuality = 1;
	ShaderLanguage                                    preferredShaderLanguage = ShaderLanguage::GLSL;

	unsigned int                                      smaaQuality;
	SMAAEdgeMethod                                    smaaEdgeMethod;
	bool                                              smaaPredication;
	ShaderDefines::SMAAParameters                     smaaParameters;

	float                                             predicationThreshold = 0.01f;
	float                                             predicationScale     = 2.0f;
	float                                             predicationStrength  = 0.4f;

	// timing things
	bool                                              fpsLimitActive = true;
	uint32_t                                          fpsLimit       = 0;
	uint64_t                                          sleepFudge     = 0;
	uint64_t                                          tickBase       = 0;
	uint64_t                                          lastTime       = 0;
	uint64_t                                          freqMult       = 0;
	uint64_t                                          freqDiv        = 0;

	// scene things
	// 0 for shapes
	// 1.. for images
	unsigned int                                      activeScene            = 0;
	unsigned int                                      shapesPerSide          = 8;
	ColorMode                                         colorMode              = ColorMode::RGB;
	bool                                              rotateShapes           = false;
	Shape                                             activeShape            = Shape::Cube;
	bool                                              visualizeShapeOrder    = false;
	unsigned int                                      shapeOrderNum          = 1;
	float                                             cameraRotation         = 0.0f;
	float                                             cameraDistance         = defaultCameraDistance;
	float                                             fovDegrees             = 65.0f;
	uint64_t                                          rotationTime           = 0;
	unsigned int                                      rotationPeriodSeconds  = 30;
	unsigned int                                      numRenderedFrames      = 0;
	unsigned int                                      maxRenderedFrames      = 0;
	RandomGen                                         random                 { 1 };
	std::vector<Image>                                images;
	std::vector<ShaderDefines::Shape>                 shapes;
	BufferHandle                                      shapesBuffer;

	glm::mat4                                         currViewProj;
	glm::mat4                                         prevViewProj;
	std::array<glm::vec4, 2>                          subsampleIndices;

	Renderer                                          renderer;
	Format                                            depthFormat = Format::Invalid;

	std::array<RenderTargetHandle, 2>                 temporalRTs;

	GraphicsPipelineHandle                                    shapePipeline;
	GraphicsPipelineHandle                                    imagePipeline;
	GraphicsPipelineHandle                                    blitPipeline;
	GraphicsPipelineHandle                                    guiPipeline;
	GraphicsPipelineHandle                                    separatePipeline;
	std::array<GraphicsPipelineHandle, 2>                     temporalAAPipelines;
	GraphicsPipelineHandle                                    fxaaPipeline;

	magic_enum::containers::array<Shape, ShapeRenderBuffers>  shapeBuffers;

	SamplerHandle                                     linearSampler;
	SamplerHandle                                     nearestSampler;

	SMAAPipelines                                     smaaPipelines;
	TextureHandle                                     areaTex;
	TextureHandle                                     searchTex;

	// input
	bool                                              rightShift      = false;
	bool                                              leftShift       = false;
	bool                                              rightAlt        = false;
	bool                                              leftAlt         = false;
	bool                                              rightCtrl       = false;
	bool                                              leftCtrl        = false;

#ifndef IMGUI_DISABLE

	std::vector<GameController>                       gameControllers;

	// gui things
	TextureHandle                                     imguiFontsTex;
	ImGuiContext                                      *imGuiContext   = nullptr;
	bool                                              textInputActive = false;
	char                                              imageFileName[inputTextBufferSize];
	char                                              clipboardText[inputTextBufferSize];

	std::vector<ImDrawVert>                           guiVertices;
	std::vector<ImDrawIdx>                            guiIndices;

#endif  // IMGUI_DISABLE


	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) noexcept            = delete;
	SMAADemo &operator=(SMAADemo &&) noexcept = delete;

	void recreateSMAATextures();

	void rebuildRenderGraph();

	template <typename F, typename PipelineHandle>
	PipelineHandle getCachedPipeline(PipelineHandle &cachedPipeline, F &&createFunc) {
		if (cachedPipeline) {
			return cachedPipeline;
		}

		cachedPipeline = createFunc();
		assert(cachedPipeline);
		return cachedPipeline;
	}

	void renderFXAA(RenderPasses rp, DemoRenderGraph::PassResources &r);

	void renderSeparate(RenderPasses rp, DemoRenderGraph::PassResources &r);

	void renderSMAAEdges(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets input, int pass);

	void renderSMAAWeights(RenderPasses rp, DemoRenderGraph::PassResources &r, int pass);

	void renderSMAABlend(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets input, int pass);

	void renderSMAADebug(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets rt);

	void renderTemporalAA(RenderPasses rp, DemoRenderGraph::PassResources &r);

#ifndef IMGUI_DISABLE

	void recreateImguiTexture();

	void updateGUI(uint64_t elapsed);

	void renderGUI(RenderPasses rp, DemoRenderGraph::PassResources &r);

#endif  // IMGUI_DISABLE

	void renderShapeScene(RenderPasses rp, DemoRenderGraph::PassResources &r);

	void renderImageScene(RenderPasses rp, DemoRenderGraph::PassResources &r);

	void loadImage(const char *filename);

	uint64_t getNanoseconds() {
		return (SDL_GetPerformanceCounter() - tickBase) * freqMult / freqDiv;
	}

	void shuffleShapeRendering();

	void reorderShapeRendering();

	void colorShapes();

	void setAntialiasing(bool enabled);

	void setTemporalAA(bool enabled);

	bool isAAMethodSupported(AAMethod method) const;

	void setPrevAAMethod();

	void setNextAAMethod();

	bool isImageScene() const {
		return activeScene != 0;
	}


public:

	SMAADemo();

	~SMAADemo();

	void parseCommandLine(int argc, char *argv[]);

	void initRender();

	void createShapes();

	void runAuto();

	void mainLoopIteration();

	void processInput();

	bool isAutoMode() const {
		return autoMode;
	}

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();
};


SMAADemo::SMAADemo()
{
	rendererDesc.swapchain.width  = 1280;
	rendererDesc.swapchain.height = 720;

	rendererDesc.applicationName           = "SMAA demo";
	rendererDesc.applicationVersion.major  = 1;
	rendererDesc.applicationVersion.minor  = 0;
	rendererDesc.applicationVersion.patch  = 0;
	rendererDesc.engineName                = "SMAA demo";
	rendererDesc.engineVersion.major       = 1;
	rendererDesc.engineVersion.minor       = 0;
	rendererDesc.engineVersion.patch       = 0;

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
	LOG("freqMult: {}", freqMult);
	LOG("freqDiv: {}", freqDiv);

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

	LOG("sleep fudge (nanoseconds): {}", sleepFudge);

#ifndef IMGUI_DISABLE
	memset(imageFileName, 0, inputTextBufferSize);
	memset(clipboardText, 0, inputTextBufferSize);
#endif  // IMGUI_DISABLE
}


SMAADemo::~SMAADemo() {
	LOG("{} frames rendered", numRenderedFrames);

#ifndef IMGUI_DISABLE
	if (imGuiContext) {
		assert(imguiFontsTex);
		renderer.deleteTexture(std::move(imguiFontsTex));

		ImGui::DestroyContext(imGuiContext);
		imGuiContext = nullptr;
	}

	gameControllers.clear();

#endif  // IMGUI_DISABLE

	if (temporalRTs[0]) {
		assert(temporalRTs[1]);
		renderer.deleteRenderTarget(std::move(temporalRTs[0]));
		renderer.deleteRenderTarget(std::move(temporalRTs[1]));
	}

	if (renderer) {
		if (shapesBuffer) {
			renderer.deleteBuffer(std::move(shapesBuffer));
		}

		renderGraph.reset(renderer);
		renderGraph.clearCaches(renderer);

		for (unsigned int i = 0; i < magic_enum::enum_count<Shape>(); i++) {
			Shape s = magic_enum::enum_value<Shape>(i);
			if (shapeBuffers[s].vertices) {
				assert(shapeBuffers[s].indices);
				assert(shapeBuffers[s].numVertices);

				shapeBuffers[s].numVertices = 0;
				renderer.deleteBuffer(std::move(shapeBuffers[s].vertices));
				renderer.deleteBuffer(std::move(shapeBuffers[s].indices));
			}
		}

		if (linearSampler) {
			assert(nearestSampler);
			renderer.deleteSampler(std::move(linearSampler));
			renderer.deleteSampler(std::move(nearestSampler));
		}

		if (areaTex) {
			assert(searchTex);
			renderer.deleteTexture(std::move(areaTex));
			renderer.deleteTexture(std::move(searchTex));
		}
	}
}


void SMAADemo::parseCommandLine(int argc, char *argv[]) {
	try {
		TCLAP::CmdLine cmd("SMAA demo", ' ', "1.0");

		TCLAP::SwitchArg                       autoSwitch("",            "auto",              "Run through modes and exit",       cmd, false);
		TCLAP::SwitchArg                       debugSwitch("",           "debug",             "Enable renderer debugging",     cmd, false);
		TCLAP::SwitchArg                       syncDebugSwitch("",       "syncdebug",         "Enable synchronization debugging", cmd, false);
		TCLAP::SwitchArg                       robustSwitch("",          "robust",            "Enable renderer robustness",    cmd, false);
		TCLAP::SwitchArg                       tracingSwitch("",         "trace",             "Enable renderer tracing",       cmd, false);
		TCLAP::SwitchArg                       noCacheSwitch("",         "nocache",           "Don't load shaders from cache", cmd, false);
		TCLAP::SwitchArg                       noOptSwitch("",           "noopt",             "Don't optimize shaders",        cmd, false);
		TCLAP::SwitchArg                       validateSwitch("",        "validate",          "Validate shader SPIR-V",        cmd, false);
		TCLAP::SwitchArg                       fullscreenSwitch("f",     "fullscreen",        "Start in fullscreen mode",      cmd, false);
		TCLAP::SwitchArg                       noVsyncSwitch("",         "novsync",           "Disable vsync",                 cmd, false);
		TCLAP::SwitchArg                       noTransferQSwitch("",     "no-transfer-queue", "Disable transfer queue",        cmd, false);

		TCLAP::ValueArg<unsigned int>          windowWidthSwitch("",     "width",             "Window width",             false, rendererDesc.swapchain.width,  "width",                 cmd);
		TCLAP::ValueArg<unsigned int>          windowHeightSwitch("",    "height",            "Window height",            false, rendererDesc.swapchain.height, "height",                cmd);

		TCLAP::ValueArg<int>                   fpsSwitch("",             "fps",               "FPS limit",                false, -1,                            "FPS",                   cmd);
		TCLAP::ValueArg<unsigned int>          framesSwitch("",          "frames",            "Frames to render",         false, 0,                             "Frames",                cmd);

		TCLAP::ValueArg<unsigned int>          rotateSwitch("",          "rotate",            "Rotation period",          false, 0,                             "seconds",               cmd);

		TCLAP::ValueArg<std::string>           aaMethodSwitch("m",       "method",            "AA Method",                false, "SMAA",                        "SMAA/SMAA2X/FXAA/MSAA", cmd);
		TCLAP::ValueArg<std::string>           aaQualitySwitch("q",      "quality",           "AA Quality",               false, "",                            "",                      cmd);
		TCLAP::ValueArg<std::string>           debugModeSwitch("d",      "debugmode",         "SMAA debug mode",          false, "None",                        "None/Edges/Weights",    cmd);
		TCLAP::ValueArg<std::string>           shaderLanguageSwitch("l", "shader-language",   "shader language",          false, "GLSL",                        "GLSL/HLSL",             cmd);
		TCLAP::ValueArg<std::string>           deviceSwitch("",          "device",            "Set Vulkan device filter", false, "",                            "device name",           cmd);

		TCLAP::SwitchArg                       temporalAASwitch("t",     "temporal",          "Temporal AA", cmd, false);

		TCLAP::UnlabeledMultiArg<std::string>  imagesArg("images",       "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		autoMode                           = autoSwitch.getValue();

		rendererDesc.debug                 = debugSwitch.getValue();
		rendererDesc.synchronizationDebug  = syncDebugSwitch.getValue();
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
		rendererDesc.vulkanDeviceFilter    = deviceSwitch.getValue();

		{
			int temp = fpsSwitch.getValue();
			if (temp < 0) {
				// automatic
				fpsLimitActive = true;
				fpsLimit       = 0;
			} else if (temp == 0) {
				// disabled
				fpsLimitActive = false;
				fpsLimit       = 0;
			} else {
				// limited to specific
				fpsLimitActive = true;
				fpsLimit       = static_cast<unsigned int>(temp);
			}
		}
		maxRenderedFrames = framesSwitch.getValue();

		unsigned int r = rotateSwitch.getValue();
		if (r != 0) {
			rotateShapes           = true;
			rotationPeriodSeconds = std::max(1U, std::min(r, 60U));
		}

		{
			std::string aaMethodStr = aaMethodSwitch.getValue();
			if (!aaMethodStr.empty()) {
				auto parsed = magic_enum::enum_cast<AAMethod>(aaMethodStr, magic_enum::case_insensitive);
				if (parsed) {
					aaMethod = *parsed;
					// we don't know supported methods yet, delay the check until we do
					aaMethodSetExplicitly = true;
				} else {
					if ((aaMethodStr.size() == 4) && (strncasecmp(aaMethodStr.data(), "none", 4) == 0)) {
						antialiasing = false;
					} else {
						THROW_ERROR("Bad AA method \"{}\"", aaMethodStr)
					}
				}
			}
		}

		{
			std::string aaQualityStr = aaQualitySwitch.getValue();
			switch (aaMethod) {
			case AAMethod::SMAA:
				if (!aaQualityStr.empty()) {
					std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
					for (unsigned int i = 0; i < maxSMAAQuality; i++) {
						if (aaQualityStr == smaaQualityLevels[i]) {
							smaaQuality = i;
							break;
						}
					}
				}
				break;

			case AAMethod::SMAA2X:
				if (!aaQualityStr.empty()) {
					std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
					for (unsigned int i = 0; i < maxSMAAQuality; i++) {
						if (aaQualityStr == smaaQualityLevels[i]) {
							smaaQuality = i;
							break;
						}
					}
				}
				break;

			case AAMethod::FXAA:
				if (!aaQualityStr.empty()) {
					std::transform(aaQualityStr.begin(), aaQualityStr.end(), aaQualityStr.begin(), ::toupper);
					for (unsigned int i = 0; i < maxFXAAQuality; i++) {
						if (aaQualityStr == fxaaQualityLevels[i]) {
							fxaaQuality = i;
							break;
						}
					}
				}
				break;

			case AAMethod::MSAA:
				int n = 0;
				const char *b = aaQualityStr.data();
				const char *e = aaQualityStr.data() + aaQualityStr.size();
				auto result = std::from_chars(b, e, n);
				if (result.ptr != e) {
					LOG_ERROR("Bad quality value: \"{}\" is not a number", aaQualityStr);
					exit(1);
				}

				if (n > 0) {
					if (!isPow2(n)) {
						n = nextPow2(n);
					}

					msaaQuality = msaaSamplesToQuality(n);
				}
				break;

			}
		}

		{
			std::string debugModeStr = debugModeSwitch.getValue();
			if (!debugModeStr.empty()) {
				auto maybe = magic_enum::enum_cast<SMAADebugMode>(debugModeStr, magic_enum::case_insensitive);
				if (maybe) {
					debugMode = *maybe;
				} else {
					THROW_ERROR("Bad SMAA debug mode \"{}\"", debugModeStr)
				}
			}
		}

		{
			std::string languageStr = shaderLanguageSwitch.getValue();
			if (!languageStr.empty()) {
				auto maybe = magic_enum::enum_cast<ShaderLanguage>(languageStr, magic_enum::case_insensitive);
				if (maybe) {
					preferredShaderLanguage = *maybe;
				} else {
					THROW_ERROR("Bad shader language \"{}\"", languageStr)
				}
			}
		}

		temporalAA = temporalAASwitch.getValue();

		imageFiles    = imagesArg.getValue();

	} catch (TCLAP::ArgException &e) {
		THROW_ERROR("parseCommandLine exception: {} for arg {}", e.error(), e.argId())
	} catch (std::runtime_error &) {
		throw;
	} catch (...) {
		THROW_ERROR("parseCommandLine: unknown exception")
	}
}


// not used for BufferHandle because the type alone does not differentiate UBO and SSBO
template <typename T> struct DescriptorTyper;

template<>
struct DescriptorTyper<CSampler> {
	static constexpr DescriptorType value = DescriptorType::CombinedSampler;
};

template<>
struct DescriptorTyper<SamplerHandle> {
	static constexpr DescriptorType value = DescriptorType::Sampler;
};

template<>
struct DescriptorTyper<TextureHandle> {
	static constexpr DescriptorType value = DescriptorType::Texture;
};


#define DESCRIPTOR(ds, member) { DescriptorTyper<decltype(member)>::value, offsetof(ds, member) }

#define DS_LAYOUT_MEMBERS                    \
    static const DescriptorLayout layout[];  \
    static DSLayoutHandle layoutHandle


struct GlobalDS {
	BufferHandle   globalUniforms;
	SamplerHandle  linearSampler;
	SamplerHandle  nearestSampler;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout GlobalDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(GlobalDS, globalUniforms) }
	, DESCRIPTOR(GlobalDS, linearSampler )
	, DESCRIPTOR(GlobalDS, nearestSampler)
	, { DescriptorType::End,            0                                  }
};

DSLayoutHandle GlobalDS::layoutHandle;


struct ShapeSceneDS {
	BufferHandle  unused;
	BufferHandle  instances;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout ShapeSceneDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(ShapeSceneDS, unused)    }
	, { DescriptorType::StorageBuffer,  offsetof(ShapeSceneDS, instances) }
	, { DescriptorType::End,            0                                 }
};

DSLayoutHandle ShapeSceneDS::layoutHandle;


struct ColorCombinedDS {
	BufferHandle  unused;
	CSampler      color;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout ColorCombinedDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(ColorCombinedDS, unused) }
	, DESCRIPTOR(ColorCombinedDS, color)
	, { DescriptorType::End,              0,                                }
};

DSLayoutHandle ColorCombinedDS::layoutHandle;


struct ColorTexDS {
	BufferHandle   unused;
	TextureHandle  color;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout ColorTexDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(ColorTexDS, unused) }
	, DESCRIPTOR(ColorTexDS, color)
	, { DescriptorType::End,            0,                           }
};

DSLayoutHandle ColorTexDS::layoutHandle;


struct EdgeDetectionDS {
	BufferHandle   smaaUBO;

	TextureHandle  color;
	TextureHandle  predicationTex;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout EdgeDetectionDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(EdgeDetectionDS, smaaUBO) }
	, DESCRIPTOR(EdgeDetectionDS, color)
	, DESCRIPTOR(EdgeDetectionDS, predicationTex)
	, { DescriptorType::End,              0,                                 }
};

DSLayoutHandle EdgeDetectionDS::layoutHandle;


struct BlendWeightDS {
	BufferHandle   smaaUBO;

	TextureHandle  edgesTex;
	TextureHandle  areaTex;
	TextureHandle  searchTex;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout BlendWeightDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(BlendWeightDS, smaaUBO) }
	, DESCRIPTOR(BlendWeightDS, edgesTex)
	, DESCRIPTOR(BlendWeightDS, areaTex)
	, DESCRIPTOR(BlendWeightDS, searchTex)
	, { DescriptorType::End,              0,                               }
};

DSLayoutHandle BlendWeightDS::layoutHandle;


struct NeighborBlendDS {
	BufferHandle   smaaUBO;

	TextureHandle  color;
	TextureHandle  blendweights;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout NeighborBlendDS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(NeighborBlendDS, smaaUBO) }
	, DESCRIPTOR(NeighborBlendDS, color)
	, DESCRIPTOR(NeighborBlendDS, blendweights)
	, { DescriptorType::End,              0                                  }
};

DSLayoutHandle NeighborBlendDS::layoutHandle;


struct TemporalAADS {
	BufferHandle   smaaUBO;

	TextureHandle  currentTex;
	TextureHandle  previousTex;
	TextureHandle  velocityTex;

	DS_LAYOUT_MEMBERS;
};


const DescriptorLayout TemporalAADS::layout[] = {
	  { DescriptorType::UniformBuffer,    offsetof(TemporalAADS, smaaUBO) }
	, DESCRIPTOR(TemporalAADS, currentTex)
	, DESCRIPTOR(TemporalAADS, previousTex)
	, DESCRIPTOR(TemporalAADS, velocityTex)
	, { DescriptorType::End,              0                               }
};

DSLayoutHandle TemporalAADS::layoutHandle;



static const int numDepths = 5;
static const std::array<Format, numDepths> depths
  = { Format::Depth24X8, Format::Depth24S8, Format::Depth32Float, Format::Depth16, Format::Depth16S8 };


void SMAADemo::initRender() {
	renderer   = Renderer::createRenderer(rendererDesc);
	renderSize = renderer.getDrawableSize();
	const auto &features = renderer.getFeatures();
	LOG("Max MSAA samples: {}",  features.maxMSAASamples);
	LOG("sRGB frame buffer: {}", features.sRGBFramebuffer ? "yes" : "no");
	LOG("swapchain can be used as storage image: {}", features.swapchainStorage ? "yes" : "no");
	LOG("SSBO support: {}",      features.SSBOSupported   ? "yes" : "no");
	maxMSAAQuality = msaaSamplesToQuality(features.maxMSAASamples) + 1;
	if (msaaQuality >= maxMSAAQuality) {
		msaaQuality = maxMSAAQuality - 1;
	}

	unsigned int supportedCount = 0;
	for (AAMethod a : magic_enum::enum_values<AAMethod>()) {
		bool supported = isAAMethodSupported(a);
		if (supported) {
			supportedCount++;
		}
		LOG("AA method {}: {}", magic_enum::enum_name(a), supported ? "supported" : "not supported");
	}

	if (supportedCount == 0) {
		THROW_ERROR("No supported AA methods")
	}

	if (!isAAMethodSupported(aaMethod)) {
		if (aaMethodSetExplicitly) {
			THROW_ERROR("Requested AA method {} is not supported", magic_enum::enum_name(aaMethod))
		}
		setNextAAMethod();
	}

	unsigned int refreshRate = renderer.getCurrentRefreshRate();

	if (refreshRate == 0) {
		LOG("Failed to get current refresh rate, using max");
		refreshRate = renderer.getMaxRefreshRate();
	}

	if (fpsLimitActive && fpsLimit == 0) {
		if (refreshRate == 0) {
			LOG("Failed to get refresh rate, defaulting to 60");
			fpsLimit = 2 * 60;
		} else {
			fpsLimit = 2 * refreshRate;
		}
	}

	for (auto depth : depths) {
		if (renderer.isRenderTargetFormatSupported(depth)) {
			depthFormat = depth;
			break;
		}
	}
	if (depthFormat == Format::Invalid) {
		THROW_ERROR("no supported depth formats")
	}
	LOG("Using depth format {}", magic_enum::enum_name(depthFormat));

	renderer.registerDescriptorSetLayout<GlobalDS>();
	renderer.registerDescriptorSetLayout<ShapeSceneDS>();
	renderer.registerDescriptorSetLayout<ColorCombinedDS>();
	renderer.registerDescriptorSetLayout<ColorTexDS>();
	renderer.registerDescriptorSetLayout<EdgeDetectionDS>();
	renderer.registerDescriptorSetLayout<BlendWeightDS>();
	renderer.registerDescriptorSetLayout<NeighborBlendDS>();
	renderer.registerDescriptorSetLayout<TemporalAADS>();

	linearSampler  = renderer.createSampler(SamplerDesc().minFilter(FilterMode::Linear). magFilter(FilterMode::Linear) .name("linear"));
	nearestSampler = renderer.createSampler(SamplerDesc().minFilter(FilterMode::Nearest).magFilter(FilterMode::Nearest).name("nearest"));

	auto createShapeBuffers = [this] (Shape shape, float scale, par_shapes_mesh *mesh) -> ShapeRenderBuffers {
		assert(mesh);

		float aabb[6] = {};
		par_shapes_compute_aabb(mesh, aabb);
		aabb[0] = (aabb[0] + aabb[3]) * 0.5f;
		aabb[1] = (aabb[1] + aabb[4]) * 0.5f;
		aabb[2] = (aabb[2] + aabb[5]) * 0.5f;

		LOG("{} center: ({}, {}, {})", magic_enum::enum_name(shape), aabb[0],  aabb[1],  aabb[2]);
		par_shapes_translate(mesh, -aabb[0], -aabb[1], -aabb[2]);

		par_shapes_scale(mesh, scale, scale, scale);

		ShapeRenderBuffers srb;
		srb.numVertices = mesh->ntriangles * 3;
		srb.vertices    = renderer.createBuffer(BufferType::Vertex, mesh->npoints    * 3 * sizeof(float),        mesh->points);
		srb.indices     = renderer.createBuffer(BufferType::Index,  mesh->ntriangles * 3 * sizeof(PAR_SHAPES_T), mesh->triangles);

		par_shapes_free_mesh(mesh);

		return srb;
	};

	shapeBuffers[Shape::Cube]         = createShapeBuffers(Shape::Cube,         sqrtf(3.0f), par_shapes_create_cube());
	shapeBuffers[Shape::Tetrahedron]  = createShapeBuffers(Shape::Tetrahedron,  1.75f, par_shapes_create_tetrahedron());
	shapeBuffers[Shape::Octahedron]   = createShapeBuffers(Shape::Octahedron,   1.5f,  par_shapes_create_octahedron());
	shapeBuffers[Shape::Dodecahedron] = createShapeBuffers(Shape::Dodecahedron, 1.5f,  par_shapes_create_dodecahedron());
	shapeBuffers[Shape::Icosahedron]  = createShapeBuffers(Shape::Icosahedron,  1.5f,  par_shapes_create_icosahedron());
	shapeBuffers[Shape::Sphere]       = createShapeBuffers(Shape::Sphere,       1.25f, par_shapes_create_subdivided_sphere(3));
	shapeBuffers[Shape::Torus]        = createShapeBuffers(Shape::Torus,        1.0f,  par_shapes_create_torus(24, 32, 0.25f));
	shapeBuffers[Shape::KleinBottle]  = createShapeBuffers(Shape::KleinBottle,  0.15f, par_shapes_create_klein_bottle(16, 40));
	shapeBuffers[Shape::TrefoilKnot]  = createShapeBuffers(Shape::TrefoilKnot,  1.5f,  par_shapes_create_trefoil_knot(48, 72, 0.5f));

	recreateSMAATextures();

	images.reserve(imageFiles.size());
	for (const auto &filename : imageFiles) {
		loadImage(filename.c_str());
	}

#ifndef IMGUI_DISABLE

	int numJoysticks = SDL_NumJoysticks();
	if (numJoysticks >= 0) {
		LOG("{} joystick{}", numJoysticks, (numJoysticks == 1) ? "" : "s");
		for (int i = 0; i < numJoysticks; i++) {
			bool isGameController = SDL_IsGameController(i);
			LOG("Joystick {} is {}a gamecontroller", i, (isGameController ? "" : "not "));
			if (!isGameController) {
				continue;
			}

			GameController gc(SDL_GameControllerOpen(i));
			if (!gc) {
				LOG("Failed to open gamecontroller {}: {}", i, SDL_GetError());
				continue;
			}

			const char *name = SDL_GameControllerName(gc.get());
			LOG(" name: \"{}\"", (name != nullptr) ? name : "<no name>");

			char *mapping = SDL_GameControllerMapping(gc.get());
			if (mapping) {
				LOG(" mapping string: \"{}\"", mapping);
				SDL_free(mapping);
			} else {
				LOG(" no mapping");
			}

			gameControllers.emplace_back(std::move(gc));
		}

		if (!gameControllers.empty()) {
			int oldState = SDL_GameControllerEventState(SDL_QUERY);
			LOG("SDL gamecontroller event state is {}", oldState);
			if (oldState != SDL_ENABLE) {
				LOG("Enabling SDL game controller events");
				int newState = SDL_GameControllerEventState(SDL_ENABLE);
				if (newState == SDL_ENABLE) {
					LOG(" succeeded");
				} else {
					LOG(" failed");
				}
			}
		}
	} else {
		LOG("SDL_NumJoysticks() failed: {}", SDL_GetError());
	}

	// imgui setup
	{
		imGuiContext = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename                 = nullptr;

		if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0) {
			if (!gameControllers.empty()) {
				io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
				io.ConfigFlags  |= ImGuiConfigFlags_NavEnableGamepad;
			}
		}

		LOG_TODO("clipboard")
		io.SetClipboardTextFn = SetClipboardText;
		io.GetClipboardTextFn = GetClipboardText;
		io.ClipboardUserData  = clipboardText;

		recreateImguiTexture();
	}
#endif  // IMGUI_DISABLE
}


void SMAADemo::recreateSMAATextures() {
	if (areaTex) {
		assert(searchTex);
		renderer.deleteTexture(std::move(areaTex));
		renderer.deleteTexture(std::move(searchTex));
	} else {
		assert(!searchTex);
	}

#ifdef RENDERER_OPENGL

	const bool flipSMAATextures = true;

#else  // RENDERER_OPENGL

	const bool flipSMAATextures = false;

#endif  // RENDERER_OPENGL

	TextureDesc texDesc;
	texDesc.width(AREATEX_WIDTH)
	       .height(AREATEX_HEIGHT)
	       .format(Format::RG8)
	       .usage({ TextureUsage::Sampling })
	       .name("SMAA area texture");

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
	       .format(Format::R8)
	       .name("SMAA search texture");
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
}


void SMAADemo::rebuildRenderGraph() {
	assert(rebuildRG);

	using namespace std::placeholders;

	if (temporalRTs[0]) {
		assert(temporalRTs[1]);
		renderer.deleteRenderTarget(std::move(temporalRTs[0]));
		renderer.deleteRenderTarget(std::move(temporalRTs[1]));
	}

	renderGraph.reset(renderer);
	renderGraph.layoutUsage(layoutUsage);

	if (antialiasing && aaMethod == AAMethod::MSAA) {
		numSamples = msaaQualityToSamples(msaaQuality);
		assert(numSamples > 1);
	} else if (antialiasing && aaMethod == AAMethod::SMAA2X) {
		numSamples = 2;
	} else {
		numSamples = 1;
	}

	renderSize = renderer.getDrawableSize();
	LOG("drawable size: {}x{}", renderSize.x, renderSize.y);

	const unsigned int windowWidth  = renderSize.x;
	const unsigned int windowHeight = renderSize.y;

	LOG("create framebuffers at size {}x{}", windowWidth, windowHeight);
	logFlush();

	{
		RenderTargetDesc rtDesc;
		rtDesc.name("final")
		      .format(Format::sRGBA8)
		      .width(windowWidth)
		      .height(windowHeight);

		if (antialiasing) {
			switch (aaMethod) {
			case AAMethod::MSAA:
				rtDesc.usage({ TextureUsage::BlitSource, TextureUsage::ResolveDestination });
				break;

			case AAMethod::FXAA:
			case AAMethod::SMAA:
			case AAMethod::SMAA2X:
				rtDesc.usage({ TextureUsage::BlitSource });
				break;
			}
		} else {
			rtDesc.usage({ TextureUsage::BlitSource });
		}
		renderGraph.renderTarget(Rendertargets::FinalRender, rtDesc);
	}

	if (!isImageScene()) {
		// shape scene

		// when any AA is enabled render to temporary rendertarget "MainColor"
		// when AA is disabled render directly to "FinalRender"
		auto renderRT = Rendertargets::MainColor;
		if (antialiasing) {
			RenderTargetDesc rtDesc;
			rtDesc.name("main color")
			      .numSamples(numSamples)
			      .format(Format::sRGBA8)
			      .additionalViewFormat(Format::RGBA8)
			      .width(windowWidth)
			      .height(windowHeight);
			switch (aaMethod) {
			case AAMethod::MSAA:
				rtDesc.usage({ TextureUsage::ResolveSource });
				break;

			case AAMethod::FXAA:
			case AAMethod::SMAA:
			case AAMethod::SMAA2X:
				rtDesc.usage({ TextureUsage::Sampling });
				break;
			}
			renderGraph.renderTarget(Rendertargets::MainColor, rtDesc);
		} else {
			renderRT = Rendertargets::FinalRender;
		}

		LOG_TODO("only create velocity buffer when doing temporal AA")
		{
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity")
			      .numSamples(1)
			      .format(Format::RG16Float)
			      .width(windowWidth)
			      .height(windowHeight);
			switch (aaMethod) {
			case AAMethod::MSAA:
			case AAMethod::SMAA2X:
				rtDesc.usage({ TextureUsage::Sampling, TextureUsage::ResolveDestination });
				break;

			case AAMethod::FXAA:
			case AAMethod::SMAA:
				rtDesc.usage({ TextureUsage::Sampling });
				break;
			}
			renderGraph.renderTarget(Rendertargets::Velocity, rtDesc);
		}

		auto velocityRT = Rendertargets::Velocity;
		if (numSamples > 1) {
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity multisample")
			      .numSamples(numSamples)
			      .format(Format::RG16Float)
			      .usage({ TextureUsage::ResolveSource })
			      .width(windowWidth)
			      .height(windowHeight);
			renderGraph.renderTarget(Rendertargets::VelocityMS, rtDesc);

			velocityRT = Rendertargets::VelocityMS;
		}

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main depth")
			      .numSamples(numSamples)
			      .format(depthFormat)
			      .width(windowWidth)
			      .height(windowHeight);

			if (antialiasing) {
				switch (aaMethod) {
				case AAMethod::MSAA:
					rtDesc.usage({ TextureUsage::ResolveSource });
					break;

				case AAMethod::FXAA:
				case AAMethod::SMAA:
				case AAMethod::SMAA2X:
					rtDesc.usage({ TextureUsage::Sampling });
					break;
				}
			} else {
				rtDesc.usage({ TextureUsage::Sampling });
			}
			renderGraph.renderTarget(Rendertargets::MainDepth, rtDesc);
		}

		DemoRenderGraph::PassDesc desc;
		desc.color(0, renderRT,                 PassBegin::Clear)
		    .color(1, velocityRT,               PassBegin::Clear)
		    .depthStencil(Rendertargets::MainDepth,  PassBegin::Clear)
		    .clearDepth(1.0f)
		    .name("Scene")
		    .numSamples(numSamples);

		renderGraph.renderPass(RenderPasses::Scene, desc, std::bind(&SMAADemo::renderShapeScene, this, _1, _2));
	} else {
		// image scene

		// when any AA is enabled render to temporary rendertarget "MainColor"
		// when AA is disabled render directly to "FinalRender"
		auto renderRT = Rendertargets::MainColor;
		if (antialiasing) {
			RenderTargetDesc rtDesc;
			rtDesc.name("main color")
			      .numSamples(numSamples)
			      .format(Format::sRGBA8)
			      .additionalViewFormat(Format::RGBA8)
			      .width(windowWidth)
			      .height(windowHeight);
			switch (aaMethod) {
			case AAMethod::MSAA:
				rtDesc.usage({ TextureUsage::ResolveSource });
				break;

			case AAMethod::FXAA:
			case AAMethod::SMAA:
			case AAMethod::SMAA2X:
				rtDesc.usage({ TextureUsage::Sampling });
				break;
			}
			renderGraph.renderTarget(Rendertargets::MainColor, rtDesc);
		} else {
			renderRT = Rendertargets::FinalRender;
		}

		LOG_TODO("don't use velocity buffer")
		{
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity")
			      .numSamples(1)
			      .format(Format::RG16Float)
			      .width(windowWidth)
			      .height(windowHeight);
			switch (aaMethod) {
			case AAMethod::MSAA:
			case AAMethod::SMAA2X:
				rtDesc.usage({ TextureUsage::Sampling, TextureUsage::ResolveDestination });
				break;

			case AAMethod::FXAA:
			case AAMethod::SMAA:
				rtDesc.usage({ TextureUsage::Sampling });
				break;
			}
			renderGraph.renderTarget(Rendertargets::Velocity, rtDesc);
		}

		auto velocityRT = Rendertargets::Velocity;
		if (numSamples > 1) {
			RenderTargetDesc rtDesc;
			rtDesc.name("velocity multisample")
			      .numSamples(numSamples)
			      .format(Format::RG16Float)
			      .usage({ TextureUsage::ResolveSource })
			      .width(windowWidth)
			      .height(windowHeight);
			renderGraph.renderTarget(Rendertargets::VelocityMS, rtDesc);
			velocityRT = Rendertargets::VelocityMS;
		}

		{
			RenderTargetDesc rtDesc;
			rtDesc.name("main depth")
			      .numSamples(numSamples)
			      .format(depthFormat)
			      .usage({ TextureUsage::Sampling })
			      .width(windowWidth)
			      .height(windowHeight);
			renderGraph.renderTarget(Rendertargets::MainDepth, rtDesc);
		}

		DemoRenderGraph::PassDesc desc;
		desc.color(0, renderRT,                 PassBegin::Clear)
		    .color(1, velocityRT,               PassBegin::Clear)
		    .depthStencil(Rendertargets::MainDepth,  PassBegin::Clear)
		    .clearDepth(1.0f)
		    .name("Scene")
		    .numSamples(numSamples);

		renderGraph.renderPass(RenderPasses::Scene, desc, std::bind(&SMAADemo::renderImageScene, this, _1, _2));
	}

	if (antialiasing) {
		if (temporalAA && !isImageScene()) {
			{
				RenderTargetDesc rtDesc;
				rtDesc.name("Temporal resolve 1")
				      .format(Format::sRGBA8)
				      .width(windowWidth)
				      .height(windowHeight);
				switch (aaMethod) {
				case AAMethod::MSAA:
					rtDesc.usage({ TextureUsage::Sampling, TextureUsage::ResolveDestination });
					break;

				case AAMethod::FXAA:
				case AAMethod::SMAA:
				case AAMethod::SMAA2X:
					rtDesc.usage({ TextureUsage::Sampling });
					break;
				}
				temporalRTs[0] = renderer.createRenderTarget(rtDesc);

				rtDesc.name("Temporal resolve 2");
				temporalRTs[1] = renderer.createRenderTarget(rtDesc);

				renderGraph.externalRenderTarget(Rendertargets::TemporalPrevious, Format::sRGBA8, Layout::ShaderRead, Layout::ShaderRead, { TextureUsage::Sampling });
				renderGraph.externalRenderTarget(Rendertargets::TemporalCurrent,  Format::sRGBA8, Layout::Undefined,  Layout::ShaderRead, { TextureUsage::Sampling });
			}

			if (numSamples > 1) {
				renderGraph.resolveMSAA(RenderPasses::MSAAResolve, Rendertargets::VelocityMS, Rendertargets::Velocity);
			}

			switch (aaMethod) {
			case AAMethod::MSAA: {
				renderGraph.resolveMSAA(RenderPasses::MSAAResolve, Rendertargets::MainColor, Rendertargets::TemporalCurrent);
			} break;

			case AAMethod::FXAA: {
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::TemporalCurrent, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::MainColor)
					    .name("FXAA temporal");

					renderGraph.renderPass(RenderPasses::FXAA, desc, std::bind(&SMAADemo::renderFXAA, this, _1, _2));
				}
			} break;

			case AAMethod::SMAA: {
				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAAEdges, rtDesc);
				}

				{
					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::MainColor)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::MainColor, 0));
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAABlendWeights, rtDesc);

					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::SMAAEdges)
					    .name("SMAA weights");

					renderGraph.renderPass(RenderPasses::SMAAWeights, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 0));
				}

				// full effect
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::TemporalCurrent, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::MainColor)
					    .inputRendertarget(Rendertargets::SMAABlendWeights)
					    .name("SMAA blend");

					renderGraph.renderPass(RenderPasses::SMAABlend, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::MainColor, 0));
				}
			} break;

			case AAMethod::SMAA2X: {
				{
					RenderTargetDesc rtDesc;
					rtDesc.format(Format::sRGBA8)
					      .additionalViewFormat(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);

					rtDesc.name("Subsample separate 1");
					renderGraph.renderTarget(Rendertargets::Subsample1, rtDesc);

					rtDesc.name("Subsample separate 2");
					renderGraph.renderTarget(Rendertargets::Subsample2, rtDesc);
				}

				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::Subsample1, PassBegin::DontCare)
					    .color(1, Rendertargets::Subsample2, PassBegin::DontCare)
					    .inputRendertarget(Rendertargets::MainColor)
					    .name("Subsample separate");

					renderGraph.renderPass(RenderPasses::Separate, desc, std::bind(&SMAADemo::renderSeparate, this, _1, _2));
				}

				LOG_TODO("clean up the renderpass mess")

				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAAEdges, rtDesc);
				}

				{
					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample1)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::Subsample1, 0));
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAABlendWeights, rtDesc);

					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::SMAAEdges)
					    .name("SMAA weights");

					renderGraph.renderPass(RenderPasses::SMAAWeights, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 0));
				}

				// full effect
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::TemporalCurrent, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample1)
					    .inputRendertarget(Rendertargets::SMAABlendWeights)
					    .name("SMAA2x blend 1");

					renderGraph.renderPass(RenderPasses::SMAA2XBlend1, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::Subsample1, 0));
				}

				// edges pass
				{
					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample2)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges2, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::Subsample2, 1));
				}

				// blendweights pass
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::SMAAEdges)
					    .name("SMAA weights");

					renderGraph.renderPass(RenderPasses::SMAAWeights2, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 1));
				}

				// full effect
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::TemporalCurrent, PassBegin::Keep)
					    .inputRendertarget(Rendertargets::Subsample2)
					    .inputRendertarget(Rendertargets::SMAABlendWeights)
					    .name("SMAA2x blend 2");

					renderGraph.renderPass(RenderPasses::SMAA2XBlend2, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::Subsample2, 1));
				}
			} break;
			}

			{
				DemoRenderGraph::PassDesc desc;
				desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
				    .inputRendertarget(Rendertargets::TemporalPrevious)
				    .inputRendertarget(Rendertargets::TemporalCurrent)
				    .inputRendertarget(Rendertargets::Velocity)
				    .name("Temporal AA");

				renderGraph.renderPass(RenderPasses::Final, desc, std::bind(&SMAADemo::renderTemporalAA, this, _1, _2));
			}
		} else {
			// no temporal AA
			switch (aaMethod) {
			case AAMethod::MSAA: {
				renderGraph.resolveMSAA(RenderPasses::MSAAResolve, Rendertargets::MainColor, Rendertargets::FinalRender);
			} break;

			case AAMethod::FXAA: {
				DemoRenderGraph::PassDesc desc;
				desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
				    .inputRendertarget(Rendertargets::MainColor)
				    .name("FXAA");

				renderGraph.renderPass(RenderPasses::FXAA, desc, std::bind(&SMAADemo::renderFXAA, this, _1, _2));
			} break;

			case AAMethod::SMAA: {
				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAAEdges, rtDesc);
				}

				{
					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::MainColor)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::MainColor, 0));
				}

				switch (debugMode) {
				case SMAADebugMode::None:
					// blendweights pass
					{
						RenderTargetDesc rtDesc;
						rtDesc.name("SMAA weights")
						      .format(Format::RGBA8)
						      .usage({ TextureUsage::Sampling })
						      .width(windowWidth)
						      .height(windowHeight);
						renderGraph.renderTarget(Rendertargets::SMAABlendWeights, rtDesc);

						DemoRenderGraph::PassDesc desc;
						desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
						    .inputRendertarget(Rendertargets::SMAAEdges)
						    .name("SMAA weights");

						renderGraph.renderPass(RenderPasses::SMAAWeights, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 0));
					}

					// full effect
					{
						DemoRenderGraph::PassDesc desc;
						desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
						    .inputRendertarget(Rendertargets::MainColor)
						    .inputRendertarget(Rendertargets::SMAABlendWeights)
						    .name("SMAA blend");

						renderGraph.renderPass(RenderPasses::SMAABlend, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::MainColor, 0));
					}

					break;

				case SMAADebugMode::Edges:
					// visualize edges
					{
						DemoRenderGraph::PassDesc desc;
						desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
						    .inputRendertarget(Rendertargets::SMAAEdges)
						    .name("Visualize edges");

						renderGraph.renderPass(RenderPasses::Final, desc, std::bind(&SMAADemo::renderSMAADebug, this, _1, _2, Rendertargets::SMAAEdges));
					}

					break;

				case SMAADebugMode::Weights:
					// blendweights pass
					{
						RenderTargetDesc rtDesc;
						rtDesc.name("SMAA weights")
						      .format(Format::RGBA8)
						      .usage({ TextureUsage::Sampling })
						      .width(windowWidth)
						      .height(windowHeight);
						renderGraph.renderTarget(Rendertargets::SMAABlendWeights, rtDesc);

						DemoRenderGraph::PassDesc desc;
						desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
						    .inputRendertarget(Rendertargets::SMAAEdges)
						    .name("SMAA weights");

						renderGraph.renderPass(RenderPasses::SMAAWeights, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 0));
					}

					// visualize blend weights
					{
						DemoRenderGraph::PassDesc desc;
						desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
						    .inputRendertarget(Rendertargets::SMAABlendWeights)
						    .name("Visualize blend weights");

						renderGraph.renderPass(RenderPasses::Final, desc, std::bind(&SMAADemo::renderSMAADebug, this, _1, _2, Rendertargets::SMAABlendWeights));
					}

					break;
				}
			} break;

			case AAMethod::SMAA2X: {
				{
					RenderTargetDesc rtDesc;
					rtDesc.format(Format::sRGBA8)
					      .additionalViewFormat(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);

					rtDesc.name("Subsample separate 1");
					renderGraph.renderTarget(Rendertargets::Subsample1, rtDesc);

					rtDesc.name("Subsample separate 2");
					renderGraph.renderTarget(Rendertargets::Subsample2, rtDesc);

					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::Subsample1, PassBegin::DontCare)
					    .color(1, Rendertargets::Subsample2, PassBegin::DontCare)
					    .inputRendertarget(Rendertargets::MainColor)
					    .name("Subsample separate");

					renderGraph.renderPass(RenderPasses::Separate, desc, std::bind(&SMAADemo::renderSeparate, this, _1, _2));
				}

				// edges pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA edges")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAAEdges, rtDesc);

					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample1)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::Subsample1, 0));
				}

				// blendweights pass
				{
					RenderTargetDesc rtDesc;
					rtDesc.name("SMAA weights")
					      .format(Format::RGBA8)
					      .usage({ TextureUsage::Sampling })
					      .width(windowWidth)
					      .height(windowHeight);
					renderGraph.renderTarget(Rendertargets::SMAABlendWeights, rtDesc);

					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::SMAAEdges)
					    .name("SMAA weights");

					renderGraph.renderPass(RenderPasses::SMAAWeights, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 0));
				}

				// final blend pass
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::FinalRender, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample1)
					    .inputRendertarget(Rendertargets::SMAABlendWeights)
					    .name("SMAA2x blend 1");

					renderGraph.renderPass(RenderPasses::SMAA2XBlend1, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::Subsample1, 0));
				}

				// second pass
				// edges pass
				{
					LOG_TODO("only add MainDepth when using predication")
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAAEdges, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::Subsample2)
					    .inputRendertarget(Rendertargets::MainDepth)
					    .name("SMAA edges");

					renderGraph.renderPass(RenderPasses::SMAAEdges2, desc, std::bind(&SMAADemo::renderSMAAEdges, this, _1, _2, Rendertargets::Subsample2, 1));
				}

				// blendweights pass
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::SMAABlendWeights, PassBegin::Clear)
					    .inputRendertarget(Rendertargets::SMAAEdges)
					    .name("SMAA weights");

					renderGraph.renderPass(RenderPasses::SMAAWeights2, desc, std::bind(&SMAADemo::renderSMAAWeights, this, _1, _2, 1));
				}

				// final blend pass
				{
					DemoRenderGraph::PassDesc desc;
					desc.color(0, Rendertargets::FinalRender, PassBegin::Keep)
					    .inputRendertarget(Rendertargets::Subsample2)
					    .inputRendertarget(Rendertargets::SMAABlendWeights)
					    .name("SMAA2x blend 2");

					renderGraph.renderPass(RenderPasses::SMAA2XBlend2, desc, std::bind(&SMAADemo::renderSMAABlend, this, _1, _2, Rendertargets::Subsample2, 1));
				}

			} break;
			}
		}
	}

#ifndef IMGUI_DISABLE

	{
		DemoRenderGraph::PassDesc desc;
		desc.color(0, Rendertargets::FinalRender, PassBegin::Keep)
		    .name("GUI");

		renderGraph.renderPass(RenderPasses::GUI, desc, std::bind(&SMAADemo::renderGUI, this, _1, _2));
	}

#endif  // IMGUI_DISABLE

	renderGraph.presentRenderTarget(Rendertargets::FinalRender);

	renderGraph.build(renderer);

	shapePipeline.reset();
	imagePipeline.reset();
	blitPipeline.reset();
	guiPipeline.reset();
	separatePipeline.reset();
	temporalAAPipelines[0].reset();
	temporalAAPipelines[1].reset();
	fxaaPipeline.reset();

	smaaPipelines.edgePipeline.reset();
	smaaPipelines.blendWeightPipeline.reset();
	smaaPipelines.neighborPipelines[0].reset();
	smaaPipelines.neighborPipelines[1].reset();

	for (unsigned int i = 0; i < 2; i++) {
		temporalAAPipelines[i].reset();
	}

	separatePipeline.reset();

	rebuildRG = false;
}


void SMAADemo::loadImage(const char *filename) {
	int width = 0, height = 0;
	unsigned char *imageData = stbi_load(filename, &width, &height, nullptr, 4);
	LOG(" {} : {}x{}", filename, width, height);
	if (!imageData) {
		LOG("Bad image: {}", stbi_failure_reason());
		return;
	}

	images.push_back(Image());
	auto &img      = images.back();
	img.filename   = filename;
	auto lastSlash = img.filename.rfind('/');
	if (lastSlash != std::string::npos) {
		img.shortName = img.filename.substr(lastSlash + 1);
	}
	else {
		img.shortName = img.filename;
	}

	TextureDesc texDesc;
	texDesc.width(width)
	       .height(height)
	       .name(img.shortName)
	       .usage({ TextureUsage::Sampling })
	       .format(Format::sRGBA8);

	texDesc.mipLevelData(0, imageData, width * height * 4);
	img.width  = width;
	img.height = height;
	img.tex    = renderer.createTexture(texDesc);

	stbi_image_free(imageData);

	if (!isImageScene()) {
		rebuildRG = true;
	}
	activeScene = static_cast<unsigned int>(images.size());
}


void SMAADemo::createShapes() {
	// shape of shapes, n^3 shapes total
	const unsigned int numShapes = static_cast<unsigned int>(pow(shapesPerSide, 3));

	const float shapeDiameter = sqrtf(3.0f);
	const float shapeDistance = shapeDiameter + 1.0f;

	const float bigShapeSide = shapeDistance * shapesPerSide;

	if (shapesBuffer) {
		renderer.deleteBuffer(std::move(shapesBuffer));
	}
	shapes.clear();
	shapes.reserve(numShapes);

	unsigned int order = 0;
	for (unsigned int x = 0; x < shapesPerSide; x++) {
		for (unsigned int y = 0; y < shapesPerSide; y++) {
			for (unsigned int z = 0; z < shapesPerSide; z++) {
				float qx = random.randFloat();
				float qy = random.randFloat();
				float qz = random.randFloat();
				float qw = random.randFloat();
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				ShaderDefines::Shape shape;
				shape.position = glm::vec3((x * shapeDistance) - (bigShapeSide / 2.0f)
				                        , (y * shapeDistance) - (bigShapeSide / 2.0f)
				                        , (z * shapeDistance) - (bigShapeSide / 2.0f));

				shape.order    = order;
				order++;

				shape.rotation = glm::vec4(qx, qy, qz, qw);
				shape.color    = glm::vec3(1.0f, 1.0f, 1.0f);
				shapes.emplace_back(shape);
			}
		}
	}

	colorShapes();
}


void SMAADemo::shuffleShapeRendering() {
	if (shapesBuffer) {
		renderer.deleteBuffer(std::move(shapesBuffer));
	}

	const unsigned int numShapes = static_cast<unsigned int>(shapes.size());
	for (unsigned int i = 0; i < numShapes - 1; i++) {
		unsigned int victim = random.range(i, numShapes);
		std::swap(shapes[i], shapes[victim]);
	}
}


void SMAADemo::reorderShapeRendering() {
	if (shapesBuffer) {
		renderer.deleteBuffer(std::move(shapesBuffer));
	}

	auto shapeCompare = [] (const ShaderDefines::Shape &a, const ShaderDefines::Shape &b) {
		return a.order < b.order;
	};
	std::sort(shapes.begin(), shapes.end(), shapeCompare);
}


static float sRGB2linear(float v) {
    if (v <= 0.04045f) {
        return v / 12.92f;
    } else {
        return powf((v + 0.055f) / 1.055f, 2.4f);
    }
}


void SMAADemo::colorShapes() {
	if (shapesBuffer) {
		renderer.deleteBuffer(std::move(shapesBuffer));
	}

	if (colorMode == ColorMode::RGB) {
		for (auto &shape : shapes) {
			// random RGB
			shape.color.x = sRGB2linear(random.randFloat());
			shape.color.y = sRGB2linear(random.randFloat());
			shape.color.z = sRGB2linear(random.randFloat());
		}
	} else {
		for (auto &shape : shapes) {
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			LOG_TODO("use the same luma as shader")

			float y = 0.3f;
			const float c_red   = 0.299f
			          , c_green = 0.587f
			          , c_blue  = 0.114f;
			float cb = random.randFloat() * 2.0f - 1.0f;
			float cr = random.randFloat() * 2.0f - 1.0f;

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			shape.color.x = sRGB2linear(r);
			shape.color.y = sRGB2linear(g);
			shape.color.z = sRGB2linear(b);
		}
	}
}


void SMAADemo::setAntialiasing(bool enabled) {
	antialiasing = enabled;
	rebuildRG    = true;
}


void SMAADemo::setTemporalAA(bool enabled) {
	temporalAA = enabled;
	rebuildRG = true;
}


bool SMAADemo::isAAMethodSupported(AAMethod method) const {
	switch (method) {
	case AAMethod::MSAA:
		return (maxMSAAQuality > 1);

	case AAMethod::FXAA:
	case AAMethod::SMAA:
		return true;

	case AAMethod::SMAA2X:
		return (maxMSAAQuality > 1);
	}

	HEDLEY_UNREACHABLE();
}


void SMAADemo::setPrevAAMethod() {
	do {
		aaMethod = magic_enum::enum_prev_value_circular(aaMethod);
	} while (!isAAMethodSupported(aaMethod));
	rebuildRG = true;
}


void SMAADemo::setNextAAMethod() {

	do {
		aaMethod = magic_enum::enum_next_value_circular(aaMethod);
	} while (!isAAMethodSupported(aaMethod));
	rebuildRG = true;
}


static void printHelp() {
	printf(" a                - toggle antialiasing on/off\n");
	printf(" c                - re-color shapes\n");
	printf(" d                - cycle through debug visualizations\n");
	printf(" f                - toggle fullscreen\n");
	printf(" h                - print help\n");
	printf(" m                - change antialiasing method\n");
	printf(" q                - cycle through AA quality levels\n");
	printf(" t                - toggle temporal antialiasing on/off\n");
	printf(" v                - toggle vsync\n");
	printf(" LEFT/RIGHT ARROW - cycle through scenes\n");
	printf(" SPACE            - toggle shape rotation\n");
	printf(" ESC              - quit\n");
}


#ifndef IMGUI_DISABLE


// copied from ImGUI SDL2 backend
static ImGuiKey ImGui_ImplSDL2_KeycodeToImGuiKey(int keycode)
{
	switch (keycode)
	{
		case SDLK_TAB: return ImGuiKey_Tab;
		case SDLK_LEFT: return ImGuiKey_LeftArrow;
		case SDLK_RIGHT: return ImGuiKey_RightArrow;
		case SDLK_UP: return ImGuiKey_UpArrow;
		case SDLK_DOWN: return ImGuiKey_DownArrow;
		case SDLK_PAGEUP: return ImGuiKey_PageUp;
		case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
		case SDLK_HOME: return ImGuiKey_Home;
		case SDLK_END: return ImGuiKey_End;
		case SDLK_INSERT: return ImGuiKey_Insert;
		case SDLK_DELETE: return ImGuiKey_Delete;
		case SDLK_BACKSPACE: return ImGuiKey_Backspace;
		case SDLK_SPACE: return ImGuiKey_Space;
		case SDLK_RETURN: return ImGuiKey_Enter;
		case SDLK_ESCAPE: return ImGuiKey_Escape;
		case SDLK_QUOTE: return ImGuiKey_Apostrophe;
		case SDLK_COMMA: return ImGuiKey_Comma;
		case SDLK_MINUS: return ImGuiKey_Minus;
		case SDLK_PERIOD: return ImGuiKey_Period;
		case SDLK_SLASH: return ImGuiKey_Slash;
		case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
		case SDLK_EQUALS: return ImGuiKey_Equal;
		case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
		case SDLK_BACKSLASH: return ImGuiKey_Backslash;
		case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
		case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
		case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
		case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
		case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
		case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
		case SDLK_PAUSE: return ImGuiKey_Pause;
		case SDLK_KP_0: return ImGuiKey_Keypad0;
		case SDLK_KP_1: return ImGuiKey_Keypad1;
		case SDLK_KP_2: return ImGuiKey_Keypad2;
		case SDLK_KP_3: return ImGuiKey_Keypad3;
		case SDLK_KP_4: return ImGuiKey_Keypad4;
		case SDLK_KP_5: return ImGuiKey_Keypad5;
		case SDLK_KP_6: return ImGuiKey_Keypad6;
		case SDLK_KP_7: return ImGuiKey_Keypad7;
		case SDLK_KP_8: return ImGuiKey_Keypad8;
		case SDLK_KP_9: return ImGuiKey_Keypad9;
		case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
		case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
		case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
		case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
		case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
		case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
		case SDLK_LSHIFT: return ImGuiKey_LeftShift;
		case SDLK_LALT: return ImGuiKey_LeftAlt;
		case SDLK_LGUI: return ImGuiKey_LeftSuper;
		case SDLK_RCTRL: return ImGuiKey_RightCtrl;
		case SDLK_RSHIFT: return ImGuiKey_RightShift;
		case SDLK_RALT: return ImGuiKey_RightAlt;
		case SDLK_RGUI: return ImGuiKey_RightSuper;
		case SDLK_APPLICATION: return ImGuiKey_Menu;
		case SDLK_0: return ImGuiKey_0;
		case SDLK_1: return ImGuiKey_1;
		case SDLK_2: return ImGuiKey_2;
		case SDLK_3: return ImGuiKey_3;
		case SDLK_4: return ImGuiKey_4;
		case SDLK_5: return ImGuiKey_5;
		case SDLK_6: return ImGuiKey_6;
		case SDLK_7: return ImGuiKey_7;
		case SDLK_8: return ImGuiKey_8;
		case SDLK_9: return ImGuiKey_9;
		case SDLK_a: return ImGuiKey_A;
		case SDLK_b: return ImGuiKey_B;
		case SDLK_c: return ImGuiKey_C;
		case SDLK_d: return ImGuiKey_D;
		case SDLK_e: return ImGuiKey_E;
		case SDLK_f: return ImGuiKey_F;
		case SDLK_g: return ImGuiKey_G;
		case SDLK_h: return ImGuiKey_H;
		case SDLK_i: return ImGuiKey_I;
		case SDLK_j: return ImGuiKey_J;
		case SDLK_k: return ImGuiKey_K;
		case SDLK_l: return ImGuiKey_L;
		case SDLK_m: return ImGuiKey_M;
		case SDLK_n: return ImGuiKey_N;
		case SDLK_o: return ImGuiKey_O;
		case SDLK_p: return ImGuiKey_P;
		case SDLK_q: return ImGuiKey_Q;
		case SDLK_r: return ImGuiKey_R;
		case SDLK_s: return ImGuiKey_S;
		case SDLK_t: return ImGuiKey_T;
		case SDLK_u: return ImGuiKey_U;
		case SDLK_v: return ImGuiKey_V;
		case SDLK_w: return ImGuiKey_W;
		case SDLK_x: return ImGuiKey_X;
		case SDLK_y: return ImGuiKey_Y;
		case SDLK_z: return ImGuiKey_Z;
		case SDLK_F1: return ImGuiKey_F1;
		case SDLK_F2: return ImGuiKey_F2;
		case SDLK_F3: return ImGuiKey_F3;
		case SDLK_F4: return ImGuiKey_F4;
		case SDLK_F5: return ImGuiKey_F5;
		case SDLK_F6: return ImGuiKey_F6;
		case SDLK_F7: return ImGuiKey_F7;
		case SDLK_F8: return ImGuiKey_F8;
		case SDLK_F9: return ImGuiKey_F9;
		case SDLK_F10: return ImGuiKey_F10;
		case SDLK_F11: return ImGuiKey_F11;
		case SDLK_F12: return ImGuiKey_F12;
	}
	return ImGuiKey_None;
}


static const ImGuiKey ImGuiGamepadButtons[] =
{
	  ImGuiKey_GamepadFaceDown   // SDL_CONTROLLER_BUTTON_A
	, ImGuiKey_GamepadFaceRight  // SDL_CONTROLLER_BUTTON_B
	, ImGuiKey_GamepadFaceLeft   // SDL_CONTROLLER_BUTTON_X
	, ImGuiKey_GamepadFaceUp     // SDL_CONTROLLER_BUTTON_Y
	, ImGuiKey_GamepadBack       // SDL_CONTROLLER_BUTTON_BACK
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_GUIDE
	, ImGuiKey_GamepadStart      // SDL_CONTROLLER_BUTTON_START
	, ImGuiKey_GamepadL3         // SDL_CONTROLLER_BUTTON_LEFTSTICK
	, ImGuiKey_GamepadR3         // SDL_CONTROLLER_BUTTON_RIGHTSTICK
	, ImGuiKey_GamepadL1         // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	, ImGuiKey_GamepadR1         // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	, ImGuiKey_GamepadDpadUp     // SDL_CONTROLLER_BUTTON_DPAD_UP
	, ImGuiKey_GamepadDpadDown   // SDL_CONTROLLER_BUTTON_DPAD_DOWN
	, ImGuiKey_GamepadDpadLeft   // SDL_CONTROLLER_BUTTON_DPAD_LEFT
	, ImGuiKey_GamepadDpadRight  // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_MISC1
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_PADDLE1
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_PADDLE2
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_PADDLE3
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_PADDLE4
	, ImGuiKey_None              // SDL_CONTROLLER_BUTTON_TOUCHPAD
};


#endif  // IMGUI_DISABLE


void SMAADemo::processInput() {
#ifndef IMGUI_DISABLE
	ImGuiIO& io = ImGui::GetIO();
#endif  // IMGUI_DISABLE

	SDL_Event event;
	memset(&event, 0, sizeof(SDL_Event));
	while (SDL_PollEvent(&event)) {
		int sceneIncrement = 1;
		switch (event.type) {
		case SDL_QUIT:
			keepGoing = false;
			break;

		case SDL_KEYDOWN:
#ifndef IMGUI_DISABLE
			io.AddKeyEvent(ImGui_ImplSDL2_KeycodeToImGuiKey(event.key.keysym.sym), true);
#endif  // IMGUI_DISABLE

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

#ifndef IMGUI_DISABLE

			if (textInputActive) {
				break;
			}

#endif  // IMGUI_DISABLE

			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				keepGoing = false;
				break;

			case SDL_SCANCODE_SPACE:
				rotateShapes = !rotateShapes;
				break;

			case SDL_SCANCODE_A:
				setAntialiasing(!antialiasing);
				break;

			case SDL_SCANCODE_C:
				// pressing 'c' re-colors the shapes
				if (rightShift || leftShift) {
					// holding shift also changes mode
					colorMode = magic_enum::enum_next_value_circular(colorMode);
				}
				colorShapes();
				break;

			case SDL_SCANCODE_D:
				if (antialiasing && aaMethod == AAMethod::SMAA) {
					if (leftShift || rightShift) {
						debugMode = magic_enum::enum_prev_value_circular(debugMode);
					} else {
						debugMode = magic_enum::enum_next_value_circular(debugMode);
					}
					rebuildRG = true;
				}
				break;

			case SDL_SCANCODE_H:
				printHelp();
				break;

			case SDL_SCANCODE_L: {
				preferredShaderLanguage = magic_enum::enum_next_value_circular(preferredShaderLanguage);
			} break;

			case SDL_SCANCODE_M: {
				if (leftShift || rightShift) {
					setPrevAAMethod();
				} else {
					setNextAAMethod();
				}
				} break;

			case SDL_SCANCODE_Q:
				switch (aaMethod) {
				case AAMethod::MSAA:
					if (leftShift || rightShift) {
						msaaQuality = msaaQuality + maxSMAAQuality - 1;
					} else {
						msaaQuality = msaaQuality + 1;
					}
					msaaQuality = msaaQuality % maxMSAAQuality;
					rebuildRG            = true;
					break;

				case AAMethod::FXAA:
					if (leftShift || rightShift) {
						fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
					} else {
						fxaaQuality = fxaaQuality + 1;
					}
					fxaaQuality  = fxaaQuality % maxFXAAQuality;
					fxaaPipeline.reset();
					break;

				case AAMethod::SMAA:
				case AAMethod::SMAA2X:
					if (leftShift || rightShift) {
						smaaQuality = smaaQuality + maxSMAAQuality - 1;
					} else {
						smaaQuality = smaaQuality + 1;
					}
					smaaQuality     = smaaQuality % maxSMAAQuality;
					smaaParameters  = defaultSMAAParameters[smaaQuality];

					smaaPipelines.edgePipeline.reset();
					smaaPipelines.blendWeightPipeline.reset();
					smaaPipelines.neighborPipelines[0].reset();
					smaaPipelines.neighborPipelines[1].reset();

					break;

				}
				break;

			case SDL_SCANCODE_S: {
				if (leftShift || rightShift) {
					activeShape = magic_enum::enum_prev_value_circular(activeShape);
				} else {
					activeShape = magic_enum::enum_next_value_circular(activeShape);
				}
			} break;

			case SDL_SCANCODE_T:
				setTemporalAA(!temporalAA);
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
				HEDLEY_FALL_THROUGH;
			case SDL_SCANCODE_RIGHT:
				{
					// if old or new scene is shapes we must rebuild RG
					if (!isImageScene()) {
						rebuildRG = true;
					}

					// all images + shapes scene
					unsigned int numScenes = static_cast<unsigned int>(images.size()) + 1;
					activeScene = (activeScene + sceneIncrement + numScenes) % numScenes;

					if (!isImageScene()) {
						rebuildRG = true;
					}
				}
				break;

			default:
				break;
			}

			break;

		case SDL_KEYUP:
#ifndef IMGUI_DISABLE
			io.AddKeyEvent(ImGui_ImplSDL2_KeycodeToImGuiKey(event.key.keysym.sym), false);
#endif  // IMGUI_DISABLE

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

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			case SDL_WINDOWEVENT_RESIZED:
				rendererDesc.swapchain.width  = event.window.data1;
				rendererDesc.swapchain.height = event.window.data2;
				recreateSwapchain = true;
				LOG("window resize to {}x{}", rendererDesc.swapchain.width, rendererDesc.swapchain.height);
				logFlush();
				break;
			default:
				break;
			}
			break;

		case SDL_DROPFILE: {
				char *droppedFile = event.drop.file;
				loadImage(droppedFile);
				SDL_free(droppedFile);
			} break;

#ifndef IMGUI_DISABLE

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			assert(event.cbutton.button < sizeof(ImGuiGamepadButtons));
			ImGuiKey key = ImGuiGamepadButtons[event.cbutton.button];
			if (key != ImGuiKey_None) {
				io.AddKeyEvent(key, event.cbutton.state == SDL_PRESSED);
			}

		} break;

		case SDL_CONTROLLERAXISMOTION: {
			LOG_TODO("configurable dead zones")
			const int stickDeadZone = 8192;
			const int triggerDeadZone = 1024;

#define NORMALIZED_AXIS(min, max)  ((static_cast<float>(event.caxis.value) - min) / (max - min))

			switch (event.caxis.axis) {
			case SDL_CONTROLLER_AXIS_LEFTX:
				if (event.caxis.value > stickDeadZone) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, true , NORMALIZED_AXIS(stickDeadZone, 32767));
				} else if ((event.caxis.value < stickDeadZone)) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  true , NORMALIZED_AXIS(-stickDeadZone, -32768));
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, false, 0.0f);
				} else {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, false, 0.0f);
				}
				break;

			case SDL_CONTROLLER_AXIS_LEFTY:
				if (event.caxis.value > stickDeadZone) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,   false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, true , NORMALIZED_AXIS(stickDeadZone, 32767));
				} else if ((event.caxis.value < stickDeadZone)) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,   true , NORMALIZED_AXIS(-stickDeadZone, -32768));
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, false, 0.0f);
				} else {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,   false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, false, 0.0f);
				}
				break;

			case SDL_CONTROLLER_AXIS_RIGHTX:
				if (event.caxis.value > stickDeadZone) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, true , NORMALIZED_AXIS(stickDeadZone, 32767));
				} else if ((event.caxis.value < stickDeadZone)) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  true , NORMALIZED_AXIS(-stickDeadZone, -32768));
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, false, 0.0f);
				} else {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, false, 0.0f);
				}
				break;

			case SDL_CONTROLLER_AXIS_RIGHTY:
				if (event.caxis.value > stickDeadZone) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,   false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, true , NORMALIZED_AXIS(stickDeadZone, 32767));
				} else if ((event.caxis.value < stickDeadZone)) {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,   true , NORMALIZED_AXIS(-stickDeadZone, -32768));
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, false, 0.0f);
				} else {
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,   false, 0.0f);
					io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, false, 0.0f);
				}
				break;

			case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
				io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, event.caxis.value > triggerDeadZone, NORMALIZED_AXIS(triggerDeadZone, 32767));
				break;

			case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
				io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, event.caxis.value > triggerDeadZone, NORMALIZED_AXIS(triggerDeadZone, 32767));
				break;
			}

#undef NORMALIZED_AXIS
		} break;

		case SDL_TEXTINPUT:
			io.AddInputCharactersUTF8(event.text.text);
			break;

		case SDL_MOUSEMOTION:
			io.AddMousePosEvent(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y));
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (!io.WantCaptureMouse) {
				if (event.button.button == SDL_BUTTON_MIDDLE) {
					cameraDistance = defaultCameraDistance;
				}
			}

			HEDLEY_FALL_THROUGH;
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 6) {
				// SDL and imgui have left and middle in different order
				static const uint8_t SDLMouseLookup[5] = { 0, 2, 1, 3, 4 };
				io.AddMouseButtonEvent(SDLMouseLookup[event.button.button - 1], event.button.state == SDL_PRESSED);
			}
			break;

		case SDL_MOUSEWHEEL:
			io.AddMouseWheelEvent(static_cast<float>(event.wheel.x), static_cast<float>(event.wheel.y));
			if (!io.WantCaptureMouse) {
				cameraDistance -= event.wheel.y;
			}
			break;

#else   // IMGUI_DISABLE

		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_MIDDLE) {
				cameraDistance = defaultCameraDistance;
			}
			break;

		case SDL_MOUSEWHEEL:
			cameraDistance -= event.wheel.y;
			break;


#endif  // IMGUI_DISABLE

		}
	}
}


void SMAADemo::runAuto() {
	auto innermostLoop = [this] () {
		for (bool onOff : { true, false }) {
			antialiasing = onOff;
			rebuildRG    = true;
			for (Shape s : magic_enum::enum_values<Shape>()) {
				activeShape = s;
				if (temporalAA) {
					for (unsigned int i = 0; i < 3; i++) {
						mainLoopIteration();
					}
				} else {
					mainLoopIteration();
				}
			}
		}
	};

	for (auto method : magic_enum::enum_values<AAMethod>()) {
		if (!isAAMethodSupported(method)) {
			continue;
		}

#ifdef RENDERER_VULKAN

		for (auto l : magic_enum::enum_values<LayoutUsage>()) {
			layoutUsage = l;

#else // #RENDERER_VULKAN

		{

#endif // #RENDERER_VULKAN

			aaMethod  = method;
			for (bool taa : { false, true }) {
				temporalAA = taa;
				for (auto shaderLang : magic_enum::enum_values<ShaderLanguage>()) {
					preferredShaderLanguage = shaderLang;
					switch (method) {
					case AAMethod::MSAA:
						for (unsigned int q = 0; q < maxMSAAQuality; q++) {
							msaaQuality = q;
							innermostLoop();
						}
						break;

					case AAMethod::FXAA:
						for (unsigned int q = 0; q < maxFXAAQuality; q++) {
							fxaaQuality = q;

							innermostLoop();
						}
						break;

					case AAMethod::SMAA:
						for (unsigned int q = 0; q < maxSMAAQuality; q++) {
							smaaQuality = q;

							for (auto e : magic_enum::enum_values<SMAAEdgeMethod>()) {
								smaaEdgeMethod = e;
								for (auto d : magic_enum::enum_values<SMAADebugMode>()) {
									debugMode = d;
									innermostLoop();
								}
							}
						}
						break;

					case AAMethod::SMAA2X:
						for (unsigned int q = 0; q < maxSMAAQuality; q++) {
							smaaQuality = q;

							// depth causes problems because it's multisampled
							for (auto e : { SMAAEdgeMethod::Color, SMAAEdgeMethod::Luma }) {
								smaaEdgeMethod = e;
								innermostLoop();
							}
						}
					}

					if (!keepGoing) {
						return;
					}
				}
			}
		}
	}
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

#ifndef IMGUI_DISABLE

	updateGUI(elapsed);

#endif  // IMGUI_DISABLE

	if (!isImageScene() && rotateShapes) {
		rotationTime += elapsed;

		LOG_TODO("increasing rotation period can make shapes spin backwards")
		const uint64_t rotationPeriod = rotationPeriodSeconds * 1000000000ULL;
		rotationTime   = rotationTime % rotationPeriod;
		cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
	}

	if (antialiasing && temporalAA && !isImageScene()) {
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

		renderSize = renderer.getDrawableSize();
		LOG("drawable size: {}x{}", renderSize.x, renderSize.y);
		logFlush();

		// pump events
		processInput();

		rebuildRG = true;
		recreateSwapchain = false;
	}

	if (rebuildRG) {
		rebuildRenderGraph();
		assert(!rebuildRG);

		if (antialiasing && temporalAA) {
			temporalAAFirstFrame = true;
		}
	}

	if (antialiasing && temporalAA && !isImageScene()) {
		assert(temporalRTs[0]);
		assert(temporalRTs[1]);
		renderGraph.bindExternalRT(Rendertargets::TemporalPrevious, temporalRTs[1 - temporalFrame]);
		renderGraph.bindExternalRT(Rendertargets::TemporalCurrent,  temporalRTs[    temporalFrame]);
	}

	renderGraph.render(renderer);

	numRenderedFrames++;
	if (maxRenderedFrames > 0 && numRenderedFrames >= maxRenderedFrames) {
		keepGoing = false;
	}
}


void SMAADemo::renderShapeScene(RenderPasses rp, DemoRenderGraph::PassResources & /* r */) {
	renderer.bindGraphicsPipeline(getCachedPipeline(shapePipeline, [&] () {
		std::string name = "shapes";
		if (numSamples > 1) {
			name += " MSAA x" + std::to_string(numSamples);
		}

		GraphicsPipelineDesc plDesc;
		plDesc.name(name)
		      .vertexShader("shape")
		      .fragmentShader("shape")
		      .numSamples(numSamples)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ShapeSceneDS>(1)
		      .vertexAttrib(ATTR_POS, 0, 3, VtxFormat::Float, 0)
		      .vertexBufferStride(0, 3 * sizeof(float))
		      .depthWrite(true)
		      .depthTest(true)
		      .cullFaces(true);

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	} ));

	const unsigned int windowWidth  = rendererDesc.swapchain.width;
	const unsigned int windowHeight = rendererDesc.swapchain.height;

	ShaderDefines::Globals globals;
	globals.screenSize            = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
	globals.guiOrtho              = glm::ortho(0.0f, float(windowWidth), float(windowHeight), 0.0f);

	LOG_TODO("better calculation, and check shape size (side is sqrt(3) currently)")
	const float shapeDiameter = sqrtf(3.0f);
	const float shapeDistance = shapeDiameter + 1.0f;

	float nearPlane = std::max(0.1f, cameraDistance - shapeDistance * float(shapesPerSide + 1));

	glm::mat4 model  = glm::rotate(glm::mat4(1.0f), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 view   = glm::lookAt(glm::vec3(cameraDistance, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj   = glm::infinitePerspective(float(fovDegrees * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, nearPlane);
	glm::mat4 viewProj = proj * view * model;

	// temporal jitter
	if (antialiasing && temporalAA && !isImageScene()) {
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
	renderer.bindDescriptorSet(PipelineType::Graphics, 0, globalDS, layoutUsage);

	renderer.bindVertexBuffer(0, shapeBuffers[activeShape].vertices);
	renderer.bindIndexBuffer(shapeBuffers[activeShape].indices, IndexFormat::b32);

	if (!shapesBuffer) {
		shapesBuffer = renderer.createBuffer(BufferType::Storage, static_cast<uint32_t>(sizeof(ShaderDefines::Shape) * shapes.size()), &shapes[0]);
	}

	ShapeSceneDS shapeDS;
	// FIXME: remove unused UBO hack
	uint32_t temp    = 0;
	shapeDS.unused    = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	shapeDS.instances = shapesBuffer;
	renderer.bindDescriptorSet(PipelineType::Graphics, 1, shapeDS, layoutUsage);

	unsigned int numShapes = static_cast<unsigned int>(shapes.size());
	if (visualizeShapeOrder) {
		shapeOrderNum = shapeOrderNum % numShapes;
		shapeOrderNum++;
		numShapes     = shapeOrderNum;
	}

	renderer.drawIndexedInstanced(shapeBuffers[activeShape].numVertices, numShapes);
}


void SMAADemo::renderImageScene(RenderPasses rp, DemoRenderGraph::PassResources & /* r */) {
	renderer.bindGraphicsPipeline(getCachedPipeline(imagePipeline, [&] () {
		GraphicsPipelineDesc plDesc;
		plDesc.numSamples(numSamples)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader("image")
		      .fragmentShader("image")
		      .name("image");

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

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
	renderer.bindDescriptorSet(PipelineType::Graphics, 0, globalDS, layoutUsage);

	assert(activeScene - 1 < images.size());
	ColorTexDS colorDS;
	// FIXME: remove unused UBO hack
	uint32_t temp  = 0;
	colorDS.unused = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	colorDS.color = image.tex;
	renderer.bindDescriptorSet(PipelineType::Graphics, 1, colorDS, layoutUsage);
	renderer.draw(0, 3);
}


void SMAADemo::renderFXAA(RenderPasses rp, DemoRenderGraph::PassResources &r) {
	renderer.bindGraphicsPipeline(getCachedPipeline(fxaaPipeline, [&] () {
		std::string qualityString(fxaaQualityLevels[fxaaQuality]);

		ShaderMacros macros;
		macros.set("FXAA_QUALITY_PRESET", qualityString);

		GraphicsPipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorCombinedDS>(1)
		      .shaderMacros(macros)
		      .vertexShader("fxaa")
		      .fragmentShader("fxaa")
		      .shaderLanguage(preferredShaderLanguage)
		      .name(std::string("FXAA ") + qualityString);

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	ColorCombinedDS colorDS;
	// FIXME: remove unused UBO hack
	uint32_t temp         = 0;
	colorDS.unused        = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	colorDS.color.tex     = r.get(Rendertargets::MainColor);
	colorDS.color.sampler = linearSampler;
	renderer.bindDescriptorSet(PipelineType::Graphics, 1, colorDS, layoutUsage);
	renderer.draw(0, 3);
}


void SMAADemo::renderSeparate(RenderPasses rp, DemoRenderGraph::PassResources &r) {
	renderer.bindGraphicsPipeline(getCachedPipeline(separatePipeline, [&] () {
		LOG_TODO("does this need its own DS?")
		GraphicsPipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
			  .descriptorSetLayout<ColorCombinedDS>(1)
			  .vertexShader("temporal")
			  .fragmentShader("separate")
			  .name("subsample separate");

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	ColorCombinedDS separateDS;
	// FIXME: remove unused UBO hack
	uint32_t temp            = 0;
	separateDS.unused        = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	separateDS.color.tex     = r.get(Rendertargets::MainColor);
	separateDS.color.sampler = nearestSampler;
	renderer.bindDescriptorSet(PipelineType::Graphics, 1, separateDS, layoutUsage);
	renderer.draw(0, 3);
}


void SMAADemo::renderSMAAEdges(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets input, int pass) {
	renderer.bindGraphicsPipeline(getCachedPipeline(smaaPipelines.edgePipeline, [&] () {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.set(qualityString, "1");
		macros.set("SMAA_GLSL_SEPARATE_SAMPLER", "1");

		if (smaaEdgeMethod != SMAAEdgeMethod::Color) {
			macros.set("EDGEMETHOD", std::to_string(static_cast<uint8_t>(smaaEdgeMethod)));
		}

		if (smaaPredication && smaaEdgeMethod != SMAAEdgeMethod::Depth) {
			macros.set("SMAA_PREDICATION", "1");
		}

		GraphicsPipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<EdgeDetectionDS>(1)
		      .shaderMacros(macros)
		      .shaderLanguage(preferredShaderLanguage)
		      .vertexShader("smaaEdge")
		      .fragmentShader("smaaEdge")
		      .name(std::string("SMAA edges ") + std::to_string(smaaQuality));
		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	LOG_TODO("this is redundant, clean it up")
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
		edgeDS.color         = r.get(Rendertargets::MainDepth);
	} else {
		edgeDS.color         = r.get(input, Format::RGBA8);
	}

	LOG_TODO("only set when using predication")
	edgeDS.predicationTex    = r.get(Rendertargets::MainDepth);

	renderer.bindDescriptorSet(PipelineType::Graphics, 1, edgeDS, layoutUsage);
	renderer.draw(0, 3);
}


void SMAADemo::renderSMAAWeights(RenderPasses rp, DemoRenderGraph::PassResources &r, int pass) {
	renderer.bindGraphicsPipeline(getCachedPipeline(smaaPipelines.blendWeightPipeline, [&] () {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.set(qualityString, "1");
		macros.set("SMAA_GLSL_SEPARATE_SAMPLER", "1");

		GraphicsPipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<BlendWeightDS>(1)
		      .shaderMacros(macros)
		      .shaderLanguage(preferredShaderLanguage)
		      .vertexShader("smaaBlendWeight")
		      .fragmentShader("smaaBlendWeight")
		      .name(std::string("SMAA weights ") + std::to_string(smaaQuality));
		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	LOG_TODO("this is redundant, clean it up")
	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[pass];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	BlendWeightDS blendWeightDS;
	blendWeightDS.smaaUBO           = smaaUBOBuf;

	blendWeightDS.edgesTex          = r.get(Rendertargets::SMAAEdges);
	blendWeightDS.areaTex           = areaTex;
	blendWeightDS.searchTex         = searchTex;

	renderer.bindDescriptorSet(PipelineType::Graphics, 1, blendWeightDS, layoutUsage);

	renderer.draw(0, 3);
}


void SMAADemo::renderSMAABlend(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets input, int pass) {
	// full effect
	renderer.bindGraphicsPipeline(getCachedPipeline(smaaPipelines.neighborPipelines[pass], [&] () {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[smaaQuality]);
		macros.set(qualityString, "1");
		macros.set("SMAA_GLSL_SEPARATE_SAMPLER", "1");

		GraphicsPipelineDesc plDesc;
		plDesc.depthWrite(false)
		      .depthTest(false)
		      .cullFaces(true)
		      .descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<NeighborBlendDS>(1)
		      .shaderMacros(macros)
		      .shaderLanguage(preferredShaderLanguage)
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

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	LOG_TODO("this is redundant, clean it up")
	ShaderDefines::SMAAUBO smaaUBO;
	smaaUBO.smaaParameters        = smaaParameters;
	smaaUBO.predicationThreshold  = predicationThreshold;
	smaaUBO.predicationScale      = predicationScale;
	smaaUBO.predicationStrength   = predicationStrength;
	smaaUBO.reprojWeigthScale     = reprojectionWeightScale;
	smaaUBO.subsampleIndices      = subsampleIndices[pass];

	auto smaaUBOBuf = renderer.createEphemeralBuffer(BufferType::Uniform, sizeof(ShaderDefines::SMAAUBO), &smaaUBO);

	NeighborBlendDS neighborBlendDS;
	neighborBlendDS.smaaUBO              = smaaUBOBuf;

	neighborBlendDS.color                = r.get(input);
	neighborBlendDS.blendweights         = r.get(Rendertargets::SMAABlendWeights);

	renderer.bindDescriptorSet(PipelineType::Graphics, 1, neighborBlendDS, layoutUsage);

	renderer.draw(0, 3);
}


void SMAADemo::renderSMAADebug(RenderPasses rp, DemoRenderGraph::PassResources &r, Rendertargets rt) {
	renderer.bindGraphicsPipeline(getCachedPipeline(blitPipeline, [&] () {
		GraphicsPipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
		      .descriptorSetLayout<ColorTexDS>(1)
		      .vertexShader("blit")
		      .fragmentShader("blit")
		      .name("blit");

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

	ColorTexDS blitDS;
	// FIXME: remove unused UBO hack
	uint32_t temp  = 0;
	blitDS.unused  = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
	blitDS.color   = r.get(rt);
	renderer.bindDescriptorSet(PipelineType::Graphics, 1, blitDS, layoutUsage);

	renderer.draw(0, 3);
}


void SMAADemo::renderTemporalAA(RenderPasses rp, DemoRenderGraph::PassResources &r) {
	renderer.bindGraphicsPipeline(getCachedPipeline(temporalAAPipelines[temporalReproject], [&] () {
		ShaderMacros macros;
		macros.set("SMAA_REPROJECTION", std::to_string(temporalReproject));
		macros.set("SMAA_GLSL_SEPARATE_SAMPLER", "1");

		GraphicsPipelineDesc plDesc;
		plDesc.descriptorSetLayout<GlobalDS>(0)
			  .descriptorSetLayout<TemporalAADS>(1)
			  .vertexShader("temporal")
			  .fragmentShader("temporal")
			  .shaderMacros(macros)
			  .shaderLanguage(preferredShaderLanguage)
			  .name("temporal AA");

		return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
	}));

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

	temporalDS.currentTex      = r.get(Rendertargets::TemporalCurrent);
	if (temporalAAFirstFrame) {
		// to prevent flicker on first frame after enabling
		LOG_TODO("should just do blit")
		temporalDS.previousTex = r.get(Rendertargets::TemporalCurrent);
		temporalAAFirstFrame   = false;
	} else {
		temporalDS.previousTex = r.get(Rendertargets::TemporalPrevious);
	}
	temporalDS.velocityTex     = r.get(Rendertargets::Velocity);

	renderer.bindDescriptorSet(PipelineType::Graphics, 1, temporalDS, layoutUsage);
	renderer.draw(0, 3);
}


#ifndef IMGUI_DISABLE


void SMAADemo::recreateImguiTexture() {
	if (imguiFontsTex) {
		renderer.deleteTexture(std::move(imguiFontsTex));
	}

	ImGuiIO &io = ImGui::GetIO();

	// Build texture atlas
	unsigned char *pixels = nullptr;
	int width = 0, height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	TextureDesc texDesc;
	texDesc.width(width)
	       .height(height)
	       .format(Format::sRGBA8)
	       .usage({ TextureUsage::Sampling })
	       .name("GUI")
	       .mipLevelData(0, pixels, width * height * 4);
	imguiFontsTex = renderer.createTexture(texDesc);
	io.Fonts->TexID = nullptr;
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
			bool temp = antialiasing;
			bool aaChanged = ImGui::Checkbox("Antialiasing", &temp);
			if (aaChanged) {
				setAntialiasing(temp);
				assert(temp == antialiasing);
			}

			int aa = magic_enum::enum_integer(aaMethod);
			bool first = true;
			for (AAMethod a : magic_enum::enum_values<AAMethod>()) {
				if (!first) {
					ImGui::SameLine();
				}
				first = false;

				bool supported = isAAMethodSupported(a);
				if (!supported) {
					ImGui::BeginDisabled();
				}

				ImGui::RadioButton(magic_enum::enum_name(a).data(), &aa, magic_enum::enum_integer(a));

				if (!supported) {
					ImGui::EndDisabled();
				}
			}

			{
				bool tempTAA = temporalAA;
				if (ImGui::Checkbox("Temporal AA", &tempTAA)) {
					rebuildRG            = true;
				}
				if (temporalAA != tempTAA) {
					setTemporalAA(tempTAA);
					assert(temporalAA == tempTAA);
				}

				// temporal reprojection only enabled when temporal AA is
				if (!temporalAA) {
					ImGui::BeginDisabled();
				}
				ImGui::Checkbox("Temporal reprojection", &temporalReproject);
				if (!temporalAA) {
					ImGui::EndDisabled();
				}
			}

			float w = reprojectionWeightScale;
			ImGui::SliderFloat("Reprojection weight scale", &w, 0.0f, 80.0f);
			reprojectionWeightScale = w;

			ImGui::Separator();
			int msaaq = msaaQuality;
			bool msaaChanged = ImGui::Combo("MSAA quality", &msaaq, msaaQualityLevels, maxMSAAQuality);
			if (aaChanged || aa != magic_enum::enum_integer(aaMethod)) {
				aaMethod = magic_enum::enum_value<AAMethod>(aa);
				assert(isAAMethodSupported(aaMethod));
				rebuildRG = true;
			}

			if (msaaChanged && aaMethod == AAMethod::MSAA) {
				assert(msaaq >= 0);
				msaaQuality = static_cast<unsigned int>(msaaq);
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
				smaaPipelines.edgePipeline.reset();
				smaaPipelines.blendWeightPipeline.reset();
				smaaPipelines.neighborPipelines[0].reset();
				smaaPipelines.neighborPipelines[1].reset();
			}

			{
				if (smaaQuality != 0) {
					ImGui::BeginDisabled();
				}

				if (ImGui::CollapsingHeader("SMAA custom properties")) {

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
				}

				if (smaaQuality != 0) {
					ImGui::EndDisabled();
				}
			}

			ImGui::Checkbox("Predicated thresholding", &smaaPredication);

			if (!smaaPredication) {
				ImGui::BeginDisabled();
			}

			ImGui::SliderFloat("Predication threshold", &predicationThreshold, 0.0f, 1.0f, "%.4f");
			ImGui::SliderFloat("Predication scale",     &predicationScale,     1.0f, 5.0f);
			ImGui::SliderFloat("Predication strength",  &predicationStrength,  0.0f, 1.0f);
			if (ImGui::Button("Reset predication values")) {
				predicationThreshold = 0.01f;
				predicationScale     = 2.0f;
				predicationStrength  = 0.4f;
			}

			if (!smaaPredication) {
				ImGui::EndDisabled();
			}

			ImGui::Text("SMAA edge detection");
			smaaEdgeMethod = enumRadioButton(smaaEdgeMethod);

			{
				ImGui::Text("Shader language");
				auto newShaderLanguage = enumRadioButton(preferredShaderLanguage);
				if (preferredShaderLanguage != newShaderLanguage) {
					preferredShaderLanguage = newShaderLanguage;
					rebuildRG = true;
				}
			}

#ifdef RENDERER_VULKAN
			{
				ImGui::Text("Image layout usage");
				auto newLayoutUsage = enumRadioButton(layoutUsage);
				if (layoutUsage != newLayoutUsage) {
					layoutUsage = newLayoutUsage;
					rebuildRG = true;
				}
			}
#endif  // RENDERER_VULKAN

			{
				int d = magic_enum::enum_integer(debugMode);
				constexpr auto smaaDebugModes = magic_enum::enum_names<SMAADebugMode>();

				if (ImGui::BeginCombo("SMAA debug", smaaDebugModes[d].data())) {
					for (int i = 0; i < int(magic_enum::enum_count<SMAADebugMode>()); i++) {
						const bool is_selected = (i == d);
						if (ImGui::Selectable(smaaDebugModes[i].data(), is_selected)) {
							d = i;
						}

						if (is_selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();

					assert(d >= 0);
					assert(d < int(magic_enum::enum_count<SMAADebugMode>()));
					if (magic_enum::enum_integer(debugMode) != d) {
						debugMode = magic_enum::enum_value<SMAADebugMode>(d);
						rebuildRG = true;
					}
				}
			}

			{
				int fq = fxaaQuality;
				ImGui::Separator();
				ImGui::Combo("FXAA quality", &fq, fxaaQualityLevels, maxFXAAQuality);
				assert(fq >= 0);
				assert(fq < int(maxFXAAQuality));
				if (fq != int(fxaaQuality)) {
					fxaaPipeline.reset();
					fxaaQuality = fq;
				}
			}
		}

		if (ImGui::CollapsingHeader("Scene properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			{
				unsigned int s = activeScene;

				if (ImGui::BeginCombo("Scene", (activeScene == 0) ? "Shapes" : images[activeScene - 1].shortName.c_str())) {
					{
						bool selected = (activeScene == 0);
						if (ImGui::Selectable("Shapes", selected)) {
							s = 0;
						}

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
					}

					for (unsigned int i = 0; i < images.size(); i++) {
						bool selected = (activeScene == i + 1);
						if (ImGui::Selectable(images[i].shortName.c_str(), selected)) {
							s = i + 1;
						}

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}

				assert(s < images.size() + 1U);
				if (s != activeScene) {
					// if old or new scene is shapes we must rebuild RG
					if (activeScene == 0 || s == 0) {
						rebuildRG = true;
					}
					activeScene = s;
				}

				ImGui::InputText("Load image", imageFileName, inputTextBufferSize);
			}

			{
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
					loadImage(imageFileName);
				}

				ImGui::Columns(1);
			}

			// shape selection
			{
				if (ImGui::BeginCombo("Shape", magic_enum::enum_name(activeShape).data())) {
					for (Shape s : magic_enum::enum_values<Shape>()) {
						bool selected = (s == activeShape);
						if (ImGui::Selectable(magic_enum::enum_name(s).data(), selected)) {
							activeShape = s;
						}

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}
			}

			{
				int m = shapesPerSide;
				bool changed = ImGui::InputInt("Shapes per side", &m);
				if (changed && m > 0 && m < 55) {
					shapesPerSide = m;
					createShapes();
				}
			}

			float l = cameraDistance;
			if (ImGui::SliderFloat("Camera distance", &l, 1.0f, 256.0f, "%.1f")) {
				cameraDistance = l;
			}

			l = fovDegrees;
			if (ImGui::SliderFloat("FOV degrees", &l, 1.0, 179.0f, "%.1f")) {
				fovDegrees = l;
			}

			ImGui::Checkbox("Rotate shapes", &rotateShapes);
			int p = rotationPeriodSeconds;
			ImGui::SliderInt("Rotation period (sec)", &p, 1, 60);
			assert(p >= 1);
			assert(p <= 60);
			rotationPeriodSeconds = p;

			ImGui::Separator();
			ImGui::Text("Shape coloring mode");

			auto newColorMode = enumRadioButton(colorMode);

			if (colorMode != newColorMode) {
				colorMode = newColorMode;
				colorShapes();
			}

			if (ImGui::Button("Re-color shapes")) {
				colorShapes();
			}

			if (ImGui::Button("Shuffle shape rendering order")) {
				shuffleShapeRendering();
				shapeOrderNum = 1;
			}

			if (ImGui::Button("Reorder shape rendering order")) {
				reorderShapeRendering();
				shapeOrderNum = 1;
			}

			ImGui::Checkbox("Visualize shape order", &visualizeShapeOrder);
		}

		if (ImGui::CollapsingHeader("Swapchain properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool temp = ImGui::Checkbox("Fullscreen", &rendererDesc.swapchain.fullscreen);
            // don't nuke recreateSwapchain in case it was already true
			if (temp) {
				recreateSwapchain = true;
			}

			int vsyncTemp = magic_enum::enum_integer(rendererDesc.swapchain.vsync);
			ImGui::Text("V-Sync");
			ImGui::RadioButton("Off",            &vsyncTemp, 0);
			ImGui::RadioButton("On",             &vsyncTemp, 1);
			ImGui::RadioButton("Late swap tear", &vsyncTemp, 2);

			if (vsyncTemp != magic_enum::enum_integer(rendererDesc.swapchain.vsync)) {
				recreateSwapchain = true;
				rendererDesc.swapchain.vsync = magic_enum::enum_value<VSync>(vsyncTemp);
			}

			int n = rendererDesc.swapchain.numFrames;
			LOG_TODO("ask Renderer for the limits")
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

			{
				ImGui::Separator();
				bool syncDebug = renderer.getSynchronizationDebugMode();
				if (ImGui::Checkbox("Debug synchronization", &syncDebug)) {
					renderer.setSynchronizationDebugMode(syncDebug);
					// need to clear caches because synchronization debug is a renderer property
					// and not rendergraph
					renderGraph.clearCaches(renderer);
					rebuildRG = true;
				}
			}

			ImGui::Separator();
			LOG_TODO("measure actual GPU time")
			ImGui::LabelText("FPS", "%.1f", io.Framerate);
			ImGui::LabelText("Frame time ms", "%.1f", 1000.0f / io.Framerate);
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

	static bool demoVisible = true;
	ImGui::ShowDemoWindow(&demoVisible);

#endif  // 0

	ImGui::Render();
}


void SMAADemo::renderGUI(RenderPasses rp, DemoRenderGraph::PassResources & /* r */) {
	auto drawData = ImGui::GetDrawData();
	assert(drawData->Valid);

	if (drawData->CmdListsCount > 0) {
		assert(!drawData->CmdLists.empty());
		assert(drawData->TotalVtxCount >  0);
		assert(drawData->TotalIdxCount >  0);

		renderer.bindGraphicsPipeline(getCachedPipeline(guiPipeline, [&] () {
			GraphicsPipelineDesc plDesc;
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
				  .vertexBufferStride(0, sizeof(ImDrawVert))
				  .name("gui");

			return renderGraph.createGraphicsPipeline(renderer, rp, plDesc);
		}));

		ColorTexDS colorDS;
		// FIXME: remove unused UBO hack
		uint32_t temp = 0;
		colorDS.unused = renderer.createEphemeralBuffer(BufferType::Uniform, 4, &temp);
		colorDS.color = imguiFontsTex;
		renderer.bindDescriptorSet(PipelineType::Graphics, 1, colorDS, layoutUsage);

		assert(sizeof(ImDrawIdx) == sizeof(uint16_t) || sizeof(ImDrawIdx) == sizeof(uint32_t));

		int vertexCount = drawData->TotalVtxCount;
		int indexCount  = drawData->TotalIdxCount;

		if (guiVertices.size() < static_cast<size_t>(vertexCount)) {
			guiVertices.resize(vertexCount);
		}
		if (guiIndices.size() < static_cast<size_t>(indexCount)) {
			guiIndices.resize(indexCount);
		}

		int vertexOffset = 0;
		int indexOffset  = 0;
		for (int n = 0; n < drawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = drawData->CmdLists[n];
			memcpy(&guiVertices[vertexOffset], cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(&guiIndices[indexOffset],   cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vertexOffset += cmd_list->VtxBuffer.Size;
			indexOffset  += cmd_list->IdxBuffer.Size;
		}

		assert(vertexOffset == vertexCount);
		assert(indexOffset  == indexCount);

		BufferHandle vtxBuf = renderer.createEphemeralBuffer(BufferType::Vertex, vertexCount * sizeof(ImDrawVert), guiVertices.data());
		BufferHandle idxBuf = renderer.createEphemeralBuffer(BufferType::Index,  indexCount  * sizeof(ImDrawIdx),  guiIndices.data());

		indexOffset  = 0;
		vertexOffset = 0;
		for (int n = 0; n < drawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = drawData->CmdLists[n];

			renderer.bindIndexBuffer(idxBuf, (sizeof(ImDrawIdx) == sizeof(uint16_t)) ? IndexFormat::b16 : IndexFormat::b32);
			renderer.bindVertexBuffer(0, vtxBuf);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback) {
					LOG_TODO("this probably does nothing useful for us")
					assert(false);

					pcmd->UserCallback(cmd_list, pcmd);
				} else {
					assert(pcmd->TextureId == nullptr);
					renderer.setScissorRect(static_cast<unsigned int>(pcmd->ClipRect.x), static_cast<unsigned int>(pcmd->ClipRect.y),
						static_cast<unsigned int>(pcmd->ClipRect.z - pcmd->ClipRect.x), static_cast<unsigned int>(pcmd->ClipRect.w - pcmd->ClipRect.y));
					renderer.drawIndexedVertexOffset(pcmd->ElemCount, indexOffset, vertexOffset);
				}
				indexOffset  += pcmd->ElemCount;
			}

			vertexOffset += cmd_list->VtxBuffer.Size;
		}

#if 0

		LOG("CmdListsCount: {}", drawData->CmdListsCount);
		LOG("TotalVtxCount: {}", drawData->TotalVtxCount);
		LOG("TotalIdxCount: {}", drawData->TotalIdxCount);

#endif // 0

	} else {
		assert(drawData->CmdLists.empty());
		assert(drawData->TotalVtxCount == 0);
		assert(drawData->TotalIdxCount == 0);
	}
}


#endif  // IMGUI_DISABLE


int main(int argc, char *argv[]) {
	try {
		logInit();

		auto demo = std::make_unique<SMAADemo>();

		try {
			demo->parseCommandLine(argc, argv);
		} catch (std::exception &e) {
			LOG_ERROR("{}", e.what());
			exit(1);
		}

		demo->initRender();
		demo->createShapes();
		printHelp();

		if (demo->isAutoMode()) {
			demo->runAuto();
		} else {
			while (demo->shouldKeepGoing()) {
				try {
					demo->mainLoopIteration();
				} catch (std::exception &e) {
					LOG("caught std::exception: \"{}\"", e.what());
					logFlush();
					printf("caught std::exception: \"%s\"\n", e.what());
					break;
				} catch (...) {
					LOG("caught unknown exception");
					logFlush();
					break;
				}
			}
		}
	} catch (std::exception &e) {
		LOG("caught std::exception \"{}\"", e.what());
#ifndef _MSC_VER
		logShutdown();
		// so native dumps core
		throw;
#endif
	} catch (...) {
		LOG("unknown exception");
#ifndef _MSC_VER
		logShutdown();
		// so native dumps core
		throw;
#endif
	}
	logShutdown();
	return 0;
}
