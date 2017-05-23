#ifdef RENDERER_NULL

#include "Renderer.h"
#include "Utils.h"


Renderer *Renderer::createRenderer(const RendererDesc &desc) {
	return new Renderer(desc);
}


Renderer::Renderer(const RendererDesc & /* desc */)
: numBuffers(0)
, numPipelines(0)
, numSamplers(0)
, numTextures(0)
, inRenderPass(false)
{

	SDL_Init(SDL_INIT_EVENTS);
}


Renderer::~Renderer() {
	SDL_Quit();
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: check desc

	numBuffers++;
	return numBuffers;
}


BufferHandle Renderer::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	return 0;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc & /* desc */) {
	return 0;
}


PipelineHandle Renderer::createPipeline(const PipelineDesc & /* desc */) {
	numPipelines++;
	return numPipelines;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	return 0;
}


SamplerHandle Renderer::createSampler(const SamplerDesc & /* desc */) {
	// TODO: check desc

	numSamplers++;
	return numSamplers;
}


VertexShaderHandle Renderer::createVertexShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	return VertexShaderHandle (0);
}


FragmentShaderHandle Renderer::createFragmentShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	return FragmentShaderHandle (0);
}


ShaderHandle Renderer::createShader(VertexShaderHandle /* vertexShader */, FragmentShaderHandle /* fragmentShader */) {
	return ShaderHandle (0);
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	// TODO: check data

	numTextures++;
	return numTextures;
}


void Renderer::deleteBuffer(BufferHandle /* handle */) {
}


void Renderer::deleteFramebuffer(FramebufferHandle /* fbo */) {
}


void Renderer::deleteRenderTarget(RenderTargetHandle &) {
}


void Renderer::deleteSampler(SamplerHandle /* handle */) {
}


void Renderer::deleteTexture(TextureHandle /* handle */) {
}


void Renderer::recreateSwapchain(const SwapchainDesc & /* desc */) {
}


void Renderer::blitFBO(FramebufferHandle /* src */, FramebufferHandle /* dest */) {
}


void Renderer::beginFrame() {
}


void Renderer::presentFrame(FramebufferHandle /* fbo */) {
}


void Renderer::beginRenderPass(FramebufferHandle /* fbo */) {
	assert(!inRenderPass);
	inRenderPass = true;
}


void Renderer::endRenderPass() {
	assert(inRenderPass);
	inRenderPass = false;
}


void Renderer::bindFramebuffer(FramebufferHandle fbo) {
	assert(fbo.handle);
}


void Renderer::bindShader(ShaderHandle shader) {
	assert(shader.handle != 0);
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
	assert(pipeline != 0);
}


void Renderer::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
}


void Renderer::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */, unsigned int /* stride */) {
}


void Renderer::bindTexture(unsigned int /* unit */, TextureHandle /* tex */, SamplerHandle /* sampler */) {
}


void Renderer::bindUniformBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
}


void Renderer::bindStorageBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
}


void Renderer::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
}


void Renderer::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
}


void Renderer::draw(unsigned int /* firstVertex */, unsigned int /* vertexCount */) {
	assert(inRenderPass);
}


void Renderer::drawIndexedInstanced(unsigned int /* vertexCount */, unsigned int /* instanceCount */) {
	assert(inRenderPass);
}


#endif //  RENDERER_NULL
