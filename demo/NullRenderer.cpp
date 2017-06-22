#ifdef RENDERER_NULL

#include "RendererInternal.h"
#include "Utils.h"



RendererImpl::RendererImpl(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, savePreprocessedShaders(false)
, frameNum(0)
, ringBufSize(0)
, ringBufPtr(0)
, numBuffers(0)
, numPipelines(0)
, numSamplers(0)
, numTextures(0)
, inFrame(false)
, inRenderPass(false)
, validPipeline(false)
, pipelineDrawn(false)
{

	SDL_Init(SDL_INIT_EVENTS);
}


RendererImpl::~RendererImpl() {
	SDL_Quit();
}


BufferHandle RendererImpl::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: check desc

	numBuffers++;
	return numBuffers;
}


BufferHandle RendererImpl::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	return 0;
}


RenderPassHandle RendererImpl::createRenderPass(const RenderPassDesc & /* desc */) {
	return RenderPassHandle(0);
}


PipelineHandle RendererImpl::createPipeline(const PipelineDesc & /* desc */) {
	numPipelines++;
	return numPipelines;
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	return 0;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc & /* desc */) {
	// TODO: check desc

	numSamplers++;
	return numSamplers;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	return VertexShaderHandle (0);
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	return FragmentShaderHandle (0);
}


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	// TODO: check data

	numTextures++;
	return numTextures;
}


DescriptorSetLayoutHandle RendererImpl::createDescriptorSetLayout(const DescriptorLayout * /* layout */) {
	return 0;
}


void RendererImpl::deleteBuffer(BufferHandle /* handle */) {
}


void RendererImpl::deleteRenderPass(RenderPassHandle /* fbo */) {
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &) {
}


void RendererImpl::deleteSampler(SamplerHandle /* handle */) {
}


void RendererImpl::deleteTexture(TextureHandle /* handle */) {
}


void RendererImpl::recreateSwapchain(const SwapchainDesc & /* desc */) {
}


void RendererImpl::beginFrame() {
	assert(!inFrame);
	inFrame       = true;
	inRenderPass  = false;
	validPipeline = false;
	pipelineDrawn = true;
}


void RendererImpl::presentFrame(RenderTargetHandle /* rt */) {
	assert(inFrame);
	inFrame = false;
}


void RendererImpl::beginRenderPass(RenderPassHandle /* pass */) {
	assert(inFrame);
	assert(!inRenderPass);
	inRenderPass  = true;
	validPipeline = false;
}


void RendererImpl::endRenderPass() {
	assert(inFrame);
	assert(inRenderPass);
	inRenderPass = false;
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(inFrame);
	assert(pipeline != 0);
	assert(inRenderPass);
	assert(pipelineDrawn);
	pipelineDrawn = false;
	validPipeline = true;
}


void RendererImpl::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
	assert(inFrame);
	assert(validPipeline);
}


void RendererImpl::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */) {
	assert(inFrame);
	assert(validPipeline);
}


void RendererImpl::bindDescriptorSet(unsigned int /* index */, DescriptorSetLayoutHandle /* layout */, const void * /* data_ */) {
	assert(validPipeline);
}


void RendererImpl::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(inFrame);
}


void RendererImpl::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(validPipeline);
}


void RendererImpl::draw(unsigned int /* firstVertex */, unsigned int /* vertexCount */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;
}


void RendererImpl::drawIndexedInstanced(unsigned int /* vertexCount */, unsigned int /* instanceCount */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;
}


void RendererImpl::drawIndexedOffset(unsigned int /* vertexCount */, unsigned int /* firstIndex */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;
}


#endif //  RENDERER_NULL
