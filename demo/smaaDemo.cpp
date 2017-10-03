/*
Copyright (c) 2015-2017 Alternative Games Ltd / Turo Lamminen

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
#include <vector>

#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tclap/CmdLine.h>

#include "Renderer.h"
#include "Utils.h"

#include "AreaTex.h"
#include "SearchTex.h"

// AFTER Renderer.h because it sets GLM_FORCE_* macros which affect these
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


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


const char *smaaDebugModes[3] = { "None", "Edges", "Weights" };


static const char *smaaDebugModeStr(unsigned int mode) {
	return smaaDebugModes[mode];
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


namespace RenderTargets {

	enum RenderTargets {
		  MainColor
		, MainDepth
		, Edges
		, BlendWeights
		, FinalRender
		, Count
	};

}  // namespace RenderTargets


class SMAADemo {
	unsigned int windowWidth, windowHeight;
	unsigned int numFrames;
	bool vsync;
	bool fullscreen;
	bool recreateSwapchain;

	Renderer renderer;
	bool glDebug;

	PipelineHandle cubePipeline;
	PipelineHandle imagePipeline;
	PipelineHandle blitPipeline;
	PipelineHandle guiPipeline;

	RenderPassHandle sceneRenderPass;
	FramebufferHandle sceneFramebuffer;
	RenderPassHandle finalRenderPass;
	FramebufferHandle finalFramebuffer;

	BufferHandle cubeVBO;
	BufferHandle cubeIBO;

	SamplerHandle linearSampler;
	SamplerHandle nearestSampler;

	unsigned int cubePower;

	std::array<RenderTargetHandle, RenderTargets::Count> rendertargets;

	bool antialiasing;
	AAMethod::AAMethod aaMethod;

	std::array<PipelineHandle, maxFXAAQuality> fxaaPipelines;

	std::array<PipelineHandle, maxSMAAQuality> smaaEdgePipelines;
	std::array<PipelineHandle, maxSMAAQuality> smaaBlendWeightPipelines;
	std::array<PipelineHandle, maxSMAAQuality> smaaNeighborPipelines;
	FramebufferHandle smaaEdgesFramebuffer;
	FramebufferHandle smaaWeightsFramebuffer;
	RenderPassHandle smaaEdgesRenderPass;
	RenderPassHandle smaaWeightsRenderPass;
	TextureHandle areaTex;
	TextureHandle searchTex;

	TextureHandle imguiFontsTex;

	bool rotateCubes;
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
	bool textInputActive;


	struct Image {
		std::string filename;
		std::string   shortName;
		TextureHandle tex;
		unsigned int  width, height;


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

	std::vector<Image> images;

	std::vector<ShaderDefines::Cube> cubes;

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

	void createCubes();

	void colorCubes();

	void mainLoopIteration();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();

	void drawGUI(uint64_t elapsed);
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, numFrames(3)
, vsync(true)
, fullscreen(false)
, recreateSwapchain(false)
, glDebug(false)
, cubePower(3)
, antialiasing(true)
, aaMethod(AAMethod::SMAA)
, rotateCubes(false)
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
, textInputActive(false)
{
	freq = SDL_GetPerformanceFrequency();
	lastTime = SDL_GetPerformanceCounter();

	// TODO: detect screens, log interesting display parameters etc
	// TODO: initialize random using external source
}


SMAADemo::~SMAADemo() {
	ImGui::Shutdown();

	if (sceneFramebuffer) {
		renderer.deleteFramebuffer(sceneFramebuffer);

		assert(finalFramebuffer);
		renderer.deleteFramebuffer(finalFramebuffer);

		assert(smaaEdgesFramebuffer);
		renderer.deleteFramebuffer(smaaEdgesFramebuffer);

		assert(smaaWeightsFramebuffer);
		renderer.deleteFramebuffer(smaaWeightsFramebuffer);

		for (unsigned int i = 0; i < RenderTargets::Count; i++) {
			assert(rendertargets[i]);
			renderer.deleteRenderTarget(rendertargets[i]);
		}
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

		TCLAP::SwitchArg glDebugSwitch("", "gldebug", "Enable OpenGL debugging", cmd, false);
		TCLAP::SwitchArg fullscreenSwitch("f", "fullscreen", "Start in full screen mode", cmd, false);
		TCLAP::ValueArg<unsigned int> windowWidthSwitch("", "width", "Window width", false, windowWidth, "width", cmd);
		TCLAP::ValueArg<unsigned int> windowHeightSwitch("", "height", "Window height", false, windowHeight, "height", cmd);
		TCLAP::UnlabeledMultiArg<std::string> imagesArg("images", "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		glDebug = glDebugSwitch.getValue();
		fullscreen   = fullscreenSwitch.getValue();
		windowWidth = windowWidthSwitch.getValue();
		windowHeight = windowHeightSwitch.getValue();

		const auto &imageFiles = imagesArg.getValue();
		images.reserve(imageFiles.size());
		for (const auto &filename : imageFiles) {
			images.push_back(Image());
			auto &img = images.back();
			img.filename = filename;
			auto lastSlash = filename.rfind('/');
			if (lastSlash != std::string::npos) {
                img.shortName = filename.substr(lastSlash + 1);
			} else {
				img.shortName = filename;
			}
		}

	} catch (TCLAP::ArgException &e) {
		printf("parseCommandLine exception: %s for arg %s\n", e.error().c_str(), e.argId().c_str());
	} catch (...) {
		printf("parseCommandLine: unknown exception\n");
	}
}


struct GlobalDS {
	BufferHandle   globalUniforms;


	static const DescriptorLayout layout[];
	static DescriptorSetLayoutHandle layoutHandle;
};


const DescriptorLayout GlobalDS::layout[] = {
	  { DescriptorType::UniformBuffer,  offsetof(GlobalDS, globalUniforms) }
	, { DescriptorType::End,            0                                  }
};

DescriptorSetLayoutHandle GlobalDS::layoutHandle;


struct CubeSceneDS {
    BufferHandle instances;

	static const DescriptorLayout layout[];
	static DescriptorSetLayoutHandle layoutHandle;
};


const DescriptorLayout CubeSceneDS::layout[] = {
	  { DescriptorType::StorageBuffer,  offsetof(CubeSceneDS, instances) }
	, { DescriptorType::End,            0                                }
};

DescriptorSetLayoutHandle CubeSceneDS::layoutHandle;


struct ColorTexDS {
	CSampler color;

	static const DescriptorLayout layout[];
	static DescriptorSetLayoutHandle layoutHandle;
};


const DescriptorLayout ColorTexDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(ColorTexDS, color) }
	, { DescriptorType::End,              0,                          }
};

DescriptorSetLayoutHandle ColorTexDS::layoutHandle;


struct BlendWeightDS {
	CSampler edgesTex;
	CSampler areaTex;
	CSampler searchTex;

	static const DescriptorLayout layout[];
	static DescriptorSetLayoutHandle layoutHandle;
};


const DescriptorLayout BlendWeightDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, edgesTex)       }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, areaTex)        }
	, { DescriptorType::CombinedSampler,  offsetof(BlendWeightDS, searchTex)      }
	, { DescriptorType::End,              0,                                      }
};

DescriptorSetLayoutHandle BlendWeightDS::layoutHandle;


struct NeighborBlendDS {
	CSampler color;
	CSampler blendweights;

	static const DescriptorLayout layout[];
	static DescriptorSetLayoutHandle layoutHandle;
};


const DescriptorLayout NeighborBlendDS::layout[] = {
	  { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, color)              }
	, { DescriptorType::CombinedSampler,  offsetof(NeighborBlendDS, blendweights)       }
	, { DescriptorType::End        ,      0                                             }
};

DescriptorSetLayoutHandle NeighborBlendDS::layoutHandle;


void SMAADemo::initRender() {
	RendererDesc desc;
	desc.debug                = glDebug;
	desc.swapchain.fullscreen = fullscreen;
	desc.swapchain.width      = windowWidth;
	desc.swapchain.height     = windowHeight;
	desc.swapchain.vsync      = vsync;

	renderer = Renderer::createRenderer(desc);

	renderer.registerDescriptorSetLayout<GlobalDS>();
	renderer.registerDescriptorSetLayout<CubeSceneDS>();
	renderer.registerDescriptorSetLayout<ColorTexDS>();
	renderer.registerDescriptorSetLayout<BlendWeightDS>();
	renderer.registerDescriptorSetLayout<NeighborBlendDS>();

	RenderPassDesc rpDesc;
	rpDesc.color(0, RGBA8);
	rpDesc.colorFinalLayout(Layout::TransferSrc);
	finalRenderPass       = renderer.createRenderPass(rpDesc.name("final"));

	rpDesc.colorFinalLayout(Layout::ShaderRead);
	smaaEdgesRenderPass   = renderer.createRenderPass(rpDesc.name("SMAA edges"));
	smaaWeightsRenderPass = renderer.createRenderPass(rpDesc.name("SMAA weights"));

	rpDesc.depthStencil(Depth16);
	sceneRenderPass       = renderer.createRenderPass(rpDesc.name("scene"));

	createFramebuffers();

	PipelineDesc plDesc;
	plDesc.depthWrite(false)
	      .depthTest(false)
	      .cullFaces(true);
	plDesc.descriptorSetLayout<GlobalDS>(0);

	// all shader stages are affected by quality (SMAA_MAX_SEARCH_STEPS)
	// TODO: fix that
	// final blend is not affected by quality, TODO: check
	// if so, share
	for (unsigned int i = 0; i < maxSMAAQuality; i++) {
		ShaderMacros macros;
		std::string qualityString(std::string("SMAA_PRESET_") + smaaQualityLevels[i]);
		macros.emplace(qualityString, "1");

		auto vertexShader   = renderer.createVertexShader("smaaEdge", macros);
		auto fragmentShader = renderer.createFragmentShader("smaaEdge", macros);

		plDesc.renderPass(smaaEdgesRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<ColorTexDS>(1);
		std::string passName = std::string("SMAA edges ") + std::to_string(i);
		plDesc.name(passName.c_str());
		smaaEdgePipelines[i]       = renderer.createPipeline(plDesc);

		vertexShader                = renderer.createVertexShader("smaaBlendWeight", macros);
		fragmentShader              = renderer.createFragmentShader("smaaBlendWeight", macros);
		plDesc.renderPass(smaaWeightsRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<BlendWeightDS>(1);
		passName = std::string("SMAA weights ") + std::to_string(i);
		plDesc.name(passName.c_str());
		smaaBlendWeightPipelines[i] = renderer.createPipeline(plDesc);

		vertexShader                = renderer.createVertexShader("smaaNeighbor", macros);
		fragmentShader              = renderer.createFragmentShader("smaaNeighbor", macros);
		plDesc.renderPass(finalRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<NeighborBlendDS>(1);
		passName = std::string("SMAA blend ") + std::to_string(i);
		plDesc.name(passName.c_str());
		smaaNeighborPipelines[i]   = renderer.createPipeline(plDesc);
	}

	ShaderMacros macros;

	// TODO: vertex shader not affected by quality, share it
	for (unsigned int i = 0; i < maxFXAAQuality; i++) {
		std::string qualityString(fxaaQualityLevels[i]);

		macros.emplace("FXAA_QUALITY_PRESET", qualityString);
		auto vertexShader   = renderer.createVertexShader("fxaa", macros);
		auto fragmentShader = renderer.createFragmentShader("fxaa", macros);
		plDesc.renderPass(finalRenderPass);
		plDesc.vertexShader(vertexShader)
		      .fragmentShader(fragmentShader);
		plDesc.descriptorSetLayout<ColorTexDS>(1);
		std::string passName = std::string("FXAA ") + std::to_string(i);
		plDesc.name(passName.c_str());
		fxaaPipelines[i] = renderer.createPipeline(plDesc);
	}

	macros.clear();

	auto vertexShader   = renderer.createVertexShader("cube", macros);
	auto fragmentShader = renderer.createFragmentShader("cube", macros);

	cubePipeline = renderer.createPipeline(PipelineDesc()
	                                        .vertexShader(vertexShader)
	                                        .fragmentShader(fragmentShader)
	                                        .renderPass(sceneRenderPass)
	                                        .descriptorSetLayout<GlobalDS>(0)
	                                        .descriptorSetLayout<CubeSceneDS>(1)
	                                        .vertexAttrib(ATTR_POS, 0, 3, VtxFormat::Float, 0)
	                                        .vertexBufferStride(ATTR_POS, sizeof(Vertex))
	                                        .depthWrite(true)
	                                        .depthTest(true)
	                                        .cullFaces(true)
	                                        .name("cubes")
	                                       );

	vertexShader   = renderer.createVertexShader("image", macros);
	fragmentShader = renderer.createFragmentShader("image", macros);

	plDesc.renderPass(sceneRenderPass);
	plDesc.vertexShader(vertexShader)
	      .fragmentShader(fragmentShader);
	plDesc.depthWrite(false)
	      .depthTest(false)
	      .cullFaces(true);
	plDesc.name("image");
	plDesc.descriptorSetLayout<ColorTexDS>(1);

	imagePipeline = renderer.createPipeline(plDesc);

	vertexShader   = renderer.createVertexShader("blit", macros);
	fragmentShader = renderer.createFragmentShader("blit", macros);

	plDesc.renderPass(finalRenderPass);
	plDesc.vertexShader(vertexShader)
	      .fragmentShader(fragmentShader);
	plDesc.name("blit");
	blitPipeline = renderer.createPipeline(plDesc);

	macros.clear();

	vertexShader   = renderer.createVertexShader("gui", macros);
	fragmentShader = renderer.createFragmentShader("gui", macros);
	plDesc.renderPass(finalRenderPass);
	plDesc.vertexShader(vertexShader)
	      .fragmentShader(fragmentShader)
	      .cullFaces(false)
	      .blending(true)
	      .scissorTest(true);
	plDesc.vertexAttrib(ATTR_POS,   0, 2, VtxFormat::Float,  offsetof(ImDrawVert, pos))
	      .vertexAttrib(ATTR_UV,    0, 2, VtxFormat::Float,  offsetof(ImDrawVert, uv))
	      .vertexAttrib(ATTR_COLOR, 0, 4, VtxFormat::UNorm8, offsetof(ImDrawVert, col))
	      .vertexBufferStride(ATTR_POS, sizeof(ImDrawVert));
	plDesc.name("gui");
	guiPipeline = renderer.createPipeline(plDesc);

	linearSampler  = renderer.createSampler(SamplerDesc().minFilter(Linear). magFilter(Linear));
	nearestSampler = renderer.createSampler(SamplerDesc().minFilter(Nearest).magFilter(Nearest));

	cubeVBO = renderer.createBuffer(sizeof(vertices), &vertices[0]);
	cubeIBO = renderer.createBuffer(sizeof(indices), &indices[0]);

#ifdef RENDERER_OPENGL

	const bool flipSMAATextures = true;

#else  // RENDERER_OPENGL

	const bool flipSMAATextures = false;

#endif  // RENDERER_OPENGL

	TextureDesc texDesc;
	texDesc.width(AREATEX_WIDTH)
	       .height(AREATEX_HEIGHT)
	       .format(RG8);

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
	       .format(R8);
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

	for (auto &img : images) {
		const auto filename = img.filename.c_str();
		int width = 0, height = 0;
		unsigned char *imageData = stbi_load(filename, &width, &height, NULL, 4);
		printf(" %p  %dx%d\n", imageData, width, height);

		texDesc.width(width)
		       .height(height)
		       .format(RGBA8);

		texDesc.mipLevelData(0, imageData, width * height * 4);
		img.width  = width;
		img.height = height;
		img.tex = renderer.createTexture(texDesc);

		stbi_image_free(imageData);
	}

	// default scene to last image or cubes if none
	activeScene = static_cast<unsigned int>(images.size());

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
		io.SetClipboardTextFn = nullptr;
		io.GetClipboardTextFn = nullptr;
		io.ClipboardUserData  = nullptr;

		// Build texture atlas
		unsigned char *pixels = nullptr;
		int width = 0, height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		texDesc.width(width)
		       .height(height)
		       .format(RGBA8)
		       .mipLevelData(0, pixels, width * height * 4);
		imguiFontsTex = renderer.createTexture(texDesc);
		io.Fonts->TexID = nullptr;
		ImGui::SetNextWindowPosCenter();
	}
}


void SMAADemo::createFramebuffers()	{
	if (rendertargets[0]) {
		assert(sceneFramebuffer);
		renderer.deleteFramebuffer(sceneFramebuffer);

		assert(finalFramebuffer);
		renderer.deleteFramebuffer(finalFramebuffer);

		assert(smaaEdgesFramebuffer);
		renderer.deleteFramebuffer(smaaEdgesFramebuffer);

		assert(smaaWeightsFramebuffer);
		renderer.deleteFramebuffer(smaaWeightsFramebuffer);

		for (unsigned int i = 0; i < RenderTargets::Count; i++) {
			assert(rendertargets[i]);
			renderer.deleteRenderTarget(rendertargets[i]);
		}
	}

	RenderTargetDesc rtDesc;
	rtDesc.width(windowWidth).height(windowHeight).format(RGBA8).name("main color");
	rendertargets[RenderTargets::MainColor] = renderer.createRenderTarget(rtDesc);

	rtDesc.width(windowWidth).height(windowHeight).format(RGBA8).name("final");
	rendertargets[RenderTargets::FinalRender] = renderer.createRenderTarget(rtDesc);

	rtDesc.format(Depth16).name("main depth");
	rendertargets[RenderTargets::MainDepth] = renderer.createRenderTarget(rtDesc);

	FramebufferDesc fbDesc;
	fbDesc.depthStencil(rendertargets[RenderTargets::MainDepth]).color(0, rendertargets[RenderTargets::MainColor]);
	fbDesc.name("scene");
	fbDesc.renderPass(sceneRenderPass);
	sceneFramebuffer = renderer.createFramebuffer(fbDesc);

	fbDesc.depthStencil(RenderTargetHandle()).color(0, rendertargets[RenderTargets::FinalRender]);
	fbDesc.name("final");
	fbDesc.renderPass(finalRenderPass);
	finalFramebuffer = renderer.createFramebuffer(fbDesc);

	// SMAA edges texture and FBO
	rtDesc.width(windowWidth).height(windowHeight).format(RGBA8).name("SMAA edges");
	rendertargets[RenderTargets::Edges] = renderer.createRenderTarget(rtDesc);
	fbDesc.depthStencil(RenderTargetHandle()).color(0, rendertargets[RenderTargets::Edges]);
	fbDesc.name("SMAA edges");
	fbDesc.renderPass(smaaEdgesRenderPass);
	smaaEdgesFramebuffer = renderer.createFramebuffer(fbDesc);

	// SMAA blending weights texture and FBO
	fbDesc.name("SMAA weights");
	rendertargets[RenderTargets::BlendWeights] = renderer.createRenderTarget(rtDesc);
	fbDesc.depthStencil(RenderTargetHandle()).color(0, rendertargets[RenderTargets::BlendWeights]);
	fbDesc.name("SMAA weights");
	fbDesc.renderPass(smaaWeightsRenderPass);
	smaaWeightsFramebuffer = renderer.createFramebuffer(fbDesc);
}


void SMAADemo::createCubes() {
	// cubes on a side is some power of 2
	const unsigned int cubesSide = static_cast<unsigned int>(pow(2, cubePower));

	// cube of cubes, n^3 cubes total
	const unsigned int numCubes = static_cast<unsigned int>(pow(cubesSide, 3));

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

				ShaderDefines::Cube cube;
				cube.position = glm::vec3((x * cubeDistance) - (bigCubeSide / 2.0f)
				                        , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                        , (z * cubeDistance) - (bigCubeSide / 2.0f));

				cube.rotation = glm::vec4(qx, qy, qz, qw);
				cube.color = white.val;
				cubes.emplace_back(cube);
			}
		}
	}

	colorCubes();
}


void SMAADemo::colorCubes() {
	if (colorMode == 0) {
		for (auto &cube : cubes) {
			Color col;
			// random RGB, alpha = 1.0
			// FIXME: we're abusing little-endianness, make it portable
			col.val = random.randU32() | 0xFF000000;
			cube.color = col.val;
		}
	} else {
		for (auto &cube : cubes) {
			Color col;
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			// TODO: use the same luma as shader

			float y = 0.5f;
			const float c_red = 0.299f
				, c_green = 0.587f
			, c_blue = 0.114f;
			float cb = random.randFloat();
			float cr = random.randFloat();

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			col.r = static_cast<uint8_t>(255.0f * r);
			col.g = static_cast<uint8_t>(255.0f * g);
			col.b = static_cast<uint8_t>(255.0f * b);
			col.a = 0xFF;
			cube.color = col.val;
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
	printf(" SPACE - toggle cube rotation\n");
	printf(" ESC   - quit\n");
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
				printf("cube rotation is %s\n", rotateCubes ? "on" : "off");
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
					printf("FXAA quality set to %s (%u)\n", fxaaQualityLevels[fxaaQuality], fxaaQuality);
					break;

				case AAMethod::SMAA:
					if (leftShift || rightShift) {
						smaaQuality = smaaQuality + maxSMAAQuality - 1;
					} else {
						smaaQuality = smaaQuality + 1;
					}
					smaaQuality = smaaQuality % maxSMAAQuality;
					printf("SMAA quality set to %s (%u)\n", smaaQualityLevels[smaaQuality], smaaQuality);
					break;

				}
				break;

			case SDL_SCANCODE_V:
				vsync = !vsync;
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

		renderer.recreateSwapchain(desc);
		recreateSwapchain = false;

		glm::uvec2 size = renderer.getDrawableSize();
		printf("drawable size: %ux%u\n", size.x, size.y);
		windowWidth  = size.x;
		windowHeight = size.y;

		createFramebuffers();
	}

	uint64_t ticks = SDL_GetPerformanceCounter();
	uint64_t elapsed = ticks - lastTime;

	lastTime = ticks;

	ShaderDefines::Globals globals;
	globals.screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

#ifdef RENDERER_VULKAN
	globals.guiOrtho   = glm::ortho(0.0f, float(windowWidth), 0.0f, float(windowHeight));
#else
	globals.guiOrtho   = glm::ortho(0.0f, float(windowWidth), float(windowHeight), 0.0f);
#endif

	renderer.beginFrame();
	renderer.beginRenderPass(sceneRenderPass, sceneFramebuffer);

	if (activeScene == 0) {
		renderer.bindPipeline(cubePipeline);

		if (rotateCubes) {
			rotationTime += elapsed;

			const uint64_t rotationPeriod = 30 * freq;
			rotationTime = rotationTime % rotationPeriod;
			cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
		}
		glm::mat4 model = glm::rotate(glm::mat4(1.0f), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
		// TODO: make camera distance (25.0f) modifiable from ui
		glm::mat4 view = glm::lookAt(glm::vec3(25.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, 0.1f, 100.0f);
		globals.viewProj = proj * view * model;

		renderer.setViewport(0, 0, windowWidth, windowHeight);

		GlobalDS globalDS;
		globalDS.globalUniforms = renderer.createEphemeralBuffer(sizeof(ShaderDefines::Globals), &globals);
		renderer.bindDescriptorSet(0, globalDS);

		renderer.bindVertexBuffer(0, cubeVBO);
		renderer.bindIndexBuffer(cubeIBO, false);

		CubeSceneDS cubeDS;
		cubeDS.instances = renderer.createEphemeralBuffer(static_cast<uint32_t>(sizeof(ShaderDefines::Cube) * cubes.size()), &cubes[0]);
		renderer.bindDescriptorSet(1, cubeDS);

		renderer.drawIndexedInstanced(3 * 2 * 6, static_cast<unsigned int>(cubes.size()));
	} else {
		renderer.bindPipeline(imagePipeline);

		const auto &image = images[activeScene - 1];

		renderer.setViewport(0, 0, windowWidth, windowHeight);

		GlobalDS globalDS;
		globalDS.globalUniforms = renderer.createEphemeralBuffer(sizeof(ShaderDefines::Globals), &globals);
		renderer.bindDescriptorSet(0, globalDS);

		assert(activeScene - 1 < images.size());
		ColorTexDS colorDS;
		colorDS.color.tex     = image.tex;
		colorDS.color.sampler = nearestSampler;
		renderer.bindDescriptorSet(1, colorDS);
		renderer.draw(0, 3);
	}
	renderer.endRenderPass();

	if (antialiasing) {
		switch (aaMethod) {
		case AAMethod::FXAA: {
			renderer.beginRenderPass(finalRenderPass, finalFramebuffer);
			renderer.bindPipeline(fxaaPipelines[fxaaQuality]);
			ColorTexDS colorDS;
			colorDS.color.tex     = renderer.getRenderTargetTexture(rendertargets[RenderTargets::MainColor]);
			colorDS.color.sampler = linearSampler;
			renderer.bindDescriptorSet(1, colorDS);
			renderer.draw(0, 3);
			drawGUI(elapsed);
			renderer.endRenderPass();
		} break;

		case AAMethod::SMAA: {
			// edges pass
			renderer.beginRenderPass(smaaEdgesRenderPass, smaaEdgesFramebuffer);
			renderer.bindPipeline(smaaEdgePipelines[smaaQuality]);

			ColorTexDS colorDS;
			colorDS.color.tex     = renderer.getRenderTargetTexture(rendertargets[RenderTargets::MainColor]);
			colorDS.color.sampler = nearestSampler;
			renderer.bindDescriptorSet(1, colorDS);
			renderer.draw(0, 3);
			renderer.endRenderPass();

			// blendweights pass
			renderer.beginRenderPass(smaaWeightsRenderPass, smaaWeightsFramebuffer);
			renderer.bindPipeline(smaaBlendWeightPipelines[smaaQuality]);
			BlendWeightDS blendWeightDS;
			blendWeightDS.edgesTex.tex      = renderer.getRenderTargetTexture(rendertargets[RenderTargets::Edges]);
			blendWeightDS.edgesTex.sampler  = linearSampler;
			blendWeightDS.areaTex.tex       = areaTex;
			blendWeightDS.areaTex.sampler   = linearSampler;
			blendWeightDS.searchTex.tex     = searchTex;
			blendWeightDS.searchTex.sampler = linearSampler;
			renderer.bindDescriptorSet(1, blendWeightDS);

			renderer.draw(0, 3);
			renderer.endRenderPass();

			// final blending pass/debug pass
			renderer.beginRenderPass(finalRenderPass, finalFramebuffer);

			switch (debugMode) {
			case 0: {
				// full effect
				renderer.bindPipeline(smaaNeighborPipelines[smaaQuality]);

				NeighborBlendDS neighborBlendDS;
				neighborBlendDS.color.tex            = renderer.getRenderTargetTexture(rendertargets[RenderTargets::MainColor]);
				neighborBlendDS.color.sampler        = linearSampler;
				neighborBlendDS.blendweights.tex     = renderer.getRenderTargetTexture(rendertargets[RenderTargets::BlendWeights]);
				neighborBlendDS.blendweights.sampler = linearSampler;
				renderer.bindDescriptorSet(1, neighborBlendDS);
			} break;

			case 1: {
				// visualize edges
				renderer.bindPipeline(blitPipeline);
				colorDS.color.tex   = renderer.getRenderTargetTexture(rendertargets[RenderTargets::Edges]);
				renderer.bindDescriptorSet(1, colorDS);
			} break;

			case 2: {
				// visualize blend weights
				renderer.bindPipeline(blitPipeline);
				colorDS.color.tex   = renderer.getRenderTargetTexture(rendertargets[RenderTargets::BlendWeights]);
				renderer.bindDescriptorSet(1, colorDS);
			} break;

			}
			renderer.draw(0, 3);
			drawGUI(elapsed);

			renderer.endRenderPass();
		} break;
		}

	} else {
		renderer.beginRenderPass(finalRenderPass, finalFramebuffer);
		renderer.bindPipeline(blitPipeline);
		ColorTexDS colorDS;
		colorDS.color.tex     = renderer.getRenderTargetTexture(rendertargets[RenderTargets::MainColor]);
		colorDS.color.sampler = linearSampler;
		renderer.bindDescriptorSet(1, colorDS);
		renderer.draw(0, 3);
		drawGUI(elapsed);
		renderer.endRenderPass();
	}

	renderer.presentFrame(rendertargets[RenderTargets::FinalRender]);

}


void SMAADemo::drawGUI(uint64_t elapsed) {
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = float(double(elapsed) / double(1000000000ULL));

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
		ImGui::Checkbox("Antialiasing", &antialiasing);
		int aa = static_cast<int>(aaMethod);
		ImGui::RadioButton("FXAA", &aa, static_cast<int>(AAMethod::FXAA)); ImGui::SameLine();
		ImGui::RadioButton("SMAA", &aa, static_cast<int>(AAMethod::SMAA));
		aaMethod = static_cast<AAMethod::AAMethod>(aa);

		int sq = smaaQuality;
		ImGui::Separator();
		ImGui::Combo("SMAA quality", &sq, smaaQualityLevels, maxSMAAQuality);
		assert(sq >= 0);
		assert(sq < int(maxSMAAQuality));
		smaaQuality = sq;

		int fq = fxaaQuality;
		ImGui::Separator();
		ImGui::Combo("FXAA quality", &fq, fxaaQualityLevels, maxFXAAQuality);
		assert(fq >= 0);
		assert(fq < int(maxFXAAQuality));
		fxaaQuality = fq;

		int d = debugMode;
		ImGui::Separator();
		ImGui::Combo("SMAA debug", &d, smaaDebugModes, 3);
		assert(d >= 0);
		assert(d < 3);
		debugMode = d;

		ImGui::Separator();
		{
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
		}

		ImGui::Checkbox("Rotate cubes", &rotateCubes);

		ImGui::Separator();
		ImGui::Text("Cube coloring mode");
		int newColorMode = colorMode;
		ImGui::RadioButton("RGB",   &newColorMode, 0);
		ImGui::RadioButton("YCbCr", &newColorMode, 1);
		colorMode = newColorMode;

		if (ImGui::Button("Re-color cubes")) {
			colorCubes();
		}

		ImGui::Separator();
		recreateSwapchain = ImGui::Checkbox("Fullscreen", &fullscreen);
		recreateSwapchain = ImGui::Checkbox("V-Sync", &vsync)          || recreateSwapchain;

		int n = numFrames;
		// TODO: ask Renderer for the limits
		if (ImGui::SliderInt("frames ahead", &n, 1, 16)) {
			numFrames = n;
			recreateSwapchain = true;
		}

		ImGui::Separator();
		if (ImGui::Button("Quit")) {
			keepGoing = false;
		}

		ImGui::Separator();
		// TODO: measure actual GPU time
		ImGui::LabelText("FPS", "%.1f", io.Framerate);
		ImGui::LabelText("Frame time ms", "%.1f", 1000.0f / io.Framerate);
	}

	// move the window to right edge of screen
	ImVec2 pos;
	pos.x =  windowWidth  - ImGui::GetWindowWidth();
	pos.y = (windowHeight - ImGui::GetWindowHeight()) / 2.0f;
	ImGui::SetWindowPos(pos);

	ImGui::End();

#if 0
	bool demoVisible = true;
	ImGui::ShowTestWindow(&demoVisible);
#endif  // 0

	ImGui::Render();

	auto drawData = ImGui::GetDrawData();
	assert(drawData->Valid);
	if (drawData->CmdListsCount > 0) {
		assert(drawData->CmdLists      != nullptr);
		assert(drawData->TotalVtxCount >  0);
		assert(drawData->TotalIdxCount >  0);

		renderer.bindPipeline(guiPipeline);
		ColorTexDS colorDS;
		colorDS.color.tex     = imguiFontsTex;
		colorDS.color.sampler = linearSampler;
		renderer.bindDescriptorSet(1, colorDS);
		// TODO: upload all buffers first, render after
		// and one buffer each vertex/index

		for (int n = 0; n < drawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = drawData->CmdLists[n];

			BufferHandle vtxBuf = renderer.createEphemeralBuffer(cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data);
			BufferHandle idxBuf = renderer.createEphemeralBuffer(cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data);
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
					renderer.setScissorRect(pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z - pcmd->ClipRect.x, pcmd->ClipRect.w - pcmd->ClipRect.y);
					renderer.drawIndexedOffset(pcmd->ElemCount, idx_buffer_offset);
				}
				idx_buffer_offset += pcmd->ElemCount;
			}
		}
#if 0
		printf("CmdListsCount: %d\n", drawData->CmdListsCount);
		printf("TotalVtxCount: %d\n", drawData->TotalVtxCount);
		printf("TotalIdxCount: %d\n", drawData->TotalIdxCount);
#endif // 0
	} else {
		assert(drawData->CmdLists      == nullptr);
		assert(drawData->TotalVtxCount == 0);
		assert(drawData->TotalIdxCount == 0);
	}
}


int main(int argc, char *argv[]) {
	try {
		auto demo = std::make_unique<SMAADemo>();

		demo->parseCommandLine(argc, argv);

		demo->initRender();
		demo->createCubes();
		printHelp();

		while (demo->shouldKeepGoing()) {
			try {
				demo->mainLoopIteration();
			} catch (std::exception &e) {
				printf("caught std::exception: \"%s\"\n", e.what());
				break;
			} catch (...) {
				printf("caught unknown exception\n");
				break;
			}
		}
	} catch (std::exception &e) {
		printf("caught std::exception \"%s\"\n", e.what());
#ifndef _MSC_VER
		// so native dumps core
		throw;
#endif
	} catch (...) {
		printf("unknown exception\n");
#ifndef _MSC_VER
		// so native dumps core
		throw;
#endif
	}
	return 0;
}
