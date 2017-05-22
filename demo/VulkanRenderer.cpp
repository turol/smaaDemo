#ifdef RENDERER_VULKAN


#define STUBBED(str) \
	{ \
		static bool seen = false; \
		if (!seen) { \
			printf("STUBBED: %s in %s at %s:%d\n", str, __PRETTY_FUNCTION__, __FILE__,  __LINE__); \
			seen = true; \
		} \
	}



#include "Renderer.h"
#include "Utils.h"


// TODO: remove these when officially in SDL
#include <xcb/xcb.h>
#define SDL_WINDOW_VULKAN 0x10000000


static SDL_bool SDL_Vulkan_GetInstanceExtensions_Helper(unsigned *userCount, const char **userNames, unsigned nameCount, const char *const *names) {
    if(userNames)
    {
        if(*userCount != nameCount)
        {
            SDL_SetError(
                "count doesn't match count from previous call of SDL_Vulkan_GetInstanceExtensions");
            return SDL_FALSE;
        }
        for(unsigned i = 0; i < nameCount; i++)
        {
            userNames[i] = names[i];
        }
    }
    else
    {
        *userCount = nameCount;
    }
    return SDL_TRUE;
}

static SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window * /*window */, unsigned *count, const char **names) {
	static const char *const extensionsForXCB[] = {
		VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	};
	return SDL_Vulkan_GetInstanceExtensions_Helper(
		count, names, SDL_arraysize(extensionsForXCB), extensionsForXCB);
}


Renderer *Renderer::createRenderer(const RendererDesc &desc) {
	return new Renderer(desc);
}


Renderer::Renderer(const RendererDesc &desc)
: inRenderPass(false)
{
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	printf("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		printf("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			printf("Display mode %i : width %i, height %i, BPP %i, refresh %u Hz\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
		}
	}

	int flags = SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_VULKAN;
	// TODO: fullscreen, resizable

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	// TODO: log stuff about window size, screen modes etc

	unsigned int numExtensions = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, NULL)) {
		printf("SDL_Vulkan_GetInstanceExtensions failed\n");
		exit(1);
	}

	printf("Vulkan extension count: %u\n", numExtensions);

	std::vector<const char *> extensions(numExtensions, nullptr);

	if(!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, &extensions[0])) {
		printf("SDL_Vulkan_GetInstanceExtensions failed\n");
		exit(1);
	}

	for (const auto &e : extensions) {
		printf("%s\n", e);
	}

	STUBBED("");
}


Renderer::~Renderer() {
	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	STUBBED("");

	return 0;
}


BufferHandle Renderer::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	STUBBED("");

	return 0;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc & /* desc */) {
	STUBBED("");

	return 0;
}


PipelineHandle Renderer::createPipeline(const PipelineDesc & /* desc */) {
	STUBBED("");

	return 0;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	STUBBED("");

	return 0;
}


SamplerHandle Renderer::createSampler(const SamplerDesc & /* desc */) {
	STUBBED("");

	return 0;
}


ShaderHandle Renderer::createShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	STUBBED("");

	return ShaderHandle (0);
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	STUBBED("");

	return 0;
}


void Renderer::deleteBuffer(BufferHandle /* handle */) {
	STUBBED("");
}


void Renderer::deleteFramebuffer(FramebufferHandle /* fbo */) {
	STUBBED("");
}


void Renderer::deleteRenderTarget(RenderTargetHandle &) {
	STUBBED("");
}


void Renderer::deleteSampler(SamplerHandle /* handle */) {
	STUBBED("");
}


void Renderer::deleteTexture(TextureHandle /* handle */) {
	STUBBED("");
}


void Renderer::recreateSwapchain(const SwapchainDesc & /* desc */) {
	STUBBED("");
}


void Renderer::blitFBO(FramebufferHandle /* src */, FramebufferHandle /* dest */) {
	STUBBED("");
}


void Renderer::beginFrame() {
	STUBBED("");
}


void Renderer::presentFrame(FramebufferHandle /* fbo */) {
	STUBBED("");
}


void Renderer::beginRenderPass(FramebufferHandle /* fbo */) {
	assert(!inRenderPass);
	inRenderPass = true;

	STUBBED("");
}


void Renderer::endRenderPass() {
	assert(inRenderPass);
	inRenderPass = false;

	STUBBED("");
}


void Renderer::bindFramebuffer(FramebufferHandle fbo) {
	assert(fbo.handle);

	STUBBED("");
}


void Renderer::bindShader(ShaderHandle shader) {
	assert(shader.handle != 0);

	STUBBED("");
}


void Renderer::bindPipeline(PipelineHandle /* pipeline */) {
	// assert(pipeline != 0);

	STUBBED("");
}


void Renderer::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
	STUBBED("");
}


void Renderer::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */, unsigned int /* stride */) {
	STUBBED("");
}


void Renderer::bindTexture(unsigned int /* unit */, TextureHandle /* tex */, SamplerHandle /* sampler */) {
	STUBBED("");
}


void Renderer::bindUniformBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
	STUBBED("");
}


void Renderer::bindStorageBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
	STUBBED("");
}


void Renderer::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	STUBBED("");
}


void Renderer::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	STUBBED("");
}


void Renderer::draw(unsigned int /* firstVertex */, unsigned int /* vertexCount */) {
	assert(inRenderPass);

	STUBBED("");
}


void Renderer::drawIndexedInstanced(unsigned int /* vertexCount */, unsigned int /* instanceCount */) {
	assert(inRenderPass);

	STUBBED("");
}


#endif  // RENDERER_VULKAN
