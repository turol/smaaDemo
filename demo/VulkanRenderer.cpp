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


Renderer *Renderer::createRenderer(const RendererDesc &desc) {
	return new Renderer(desc);
}


Renderer::Renderer(const RendererDesc &desc)
: inRenderPass(false)
{
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

	int flags = SDL_WINDOW_RESIZABLE;

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

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
