/*
Copyright (c) 2015-2021 Alternative Games Ltd / Turo Lamminen

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


#ifdef RENDERER_NULL

#include "RendererInternal.h"
#include "utils/Utils.h"


namespace renderer {


RendererImpl::RendererImpl(const RendererDesc &desc)
: RendererBase(desc)
{
	SDL_Init(SDL_INIT_EVENTS);

	swapchainFormat = Format::sRGBA8;

	currentRefreshRate = 60;
	maxRefreshRate     = 60;

	recreateRingBuffer(desc.ephemeralRingBufSize);

	frames.resize(desc.swapchain.numFrames);
}


void RendererImpl::recreateRingBuffer(unsigned int newSize) {
	assert(newSize > 0);

	ringBufPtr  = 0;
	ringBufSize = newSize;
	ringBuffer.resize(newSize, 0);
	LOG_TODO("use valgrind to make sure we only write to intended parts of ring buffer");
}


RendererImpl::~RendererImpl() {
	waitForDeviceIdle();

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		assert(!f.outstanding);
		deleteFrameInternal(f);
	}
	frames.clear();

	SDL_Quit();
}


bool Renderer::isRenderTargetFormatSupported(Format /* format */) const {
	LOG_TODO("actually check it...");
	return true;
}


BufferHandle Renderer::createBuffer(BufferType /* type */, uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	Buffer buffer;
	buffer.ringBufferAlloc = false;
	buffer.beginOffs       = 0;
	buffer.size            = size;

	LOG_TODO("store contents into buffer");

	return impl->buffers.add(std::move(buffer));
}


BufferHandle Renderer::createEphemeralBuffer(BufferType /* type */, uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	unsigned int beginPtr = impl->ringBufferAllocate(size, 256);

	LOG_TODO("use valgrind to enforce we only write to intended parts of ring buffer");
	memcpy(&impl->ringBuffer[beginPtr], contents, size);

	Buffer buffer;
	buffer.ringBufferAlloc = true;
	buffer.beginOffs       = beginPtr;
	buffer.size            = size;

	auto handle = impl->buffers.add(std::move(buffer));

	// move the the owning handle and return a non-owning copy
	BufferHandle result = handle;
	impl->frames.at(impl->currentFrameIdx).ephemeralBuffers.push_back(std::move(handle));

	return result;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	Framebuffer fb;
	fb.renderPass = desc.renderPass_;

	return impl->framebuffers.add(std::move(fb));
}


RenderPassHandle Renderer::createRenderPass(const RenderPassDesc &desc) {
	RenderPass rp;
	rp.desc     = desc;

	return impl->renderpasses.add(std::move(rp));
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	Pipeline pipeline;
	pipeline.desc = desc;
	return impl->pipelines.add(std::move(pipeline));
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != +Format::Invalid);

	RenderTarget rendertarget;
	rendertarget.desc = desc;
	return impl->rendertargets.add(std::move(rendertarget));
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	Sampler sampler;
	LOG_TODO("check desc");
	sampler.desc = desc;

	return impl->samplers.add(std::move(sampler));
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros & /* macros */) {
	std::string vertexShaderName   = name + ".vert";

	auto result_ = vertexShaders.add();
	auto &v = result_.first;
	v.name      = vertexShaderName;

	return result_.second;
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros & /* macros */) {
	std::string fragmentShaderName = name + ".frag";

	auto result_ = fragmentShaders.add();
	auto &f = result_.first;
	f.name      = fragmentShaderName;

	return result_.second;
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	LOG_TODO("check data");

	Texture texture;
	LOG_TODO("check desc");
	texture.desc = desc;

	return impl->textures.add(std::move(texture));
}


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	DescriptorSetLayout dsLayout;

	while (layout->type != +DescriptorType::End) {
		dsLayout.layout.push_back(*layout);
		layout++;
	}
	assert(layout->offset == 0);

	return impl->dsLayouts.add(std::move(dsLayout));
}


TextureHandle Renderer::getRenderTargetView(RenderTargetHandle /* handle */, Format /* f */) {
	TextureHandle handle;

	return handle;
}


void Renderer::deleteBuffer(BufferHandle &&handle) {
	impl->buffers.remove(std::move(handle));
}


void Renderer::deleteFramebuffer(FramebufferHandle &&handle) {
	impl->framebuffers.remove(std::move(handle));
}


void Renderer::deletePipeline(PipelineHandle &&handle) {
	impl->pipelines.remove(std::move(handle));
}


void Renderer::deleteRenderPass(RenderPassHandle &&handle) {
	impl->renderpasses.remove(std::move(handle));
}


void Renderer::deleteRenderTarget(RenderTargetHandle &&handle) {
	impl->rendertargets.remove(std::move(handle));
}


void Renderer::deleteSampler(SamplerHandle &&handle) {
	impl->samplers.remove(std::move(handle));
}


void Renderer::deleteTexture(TextureHandle &&handle) {
	impl->textures.remove(std::move(handle));
}


void Renderer::setSwapchainDesc(const SwapchainDesc &desc) {
	impl->swapchainDesc  = desc;
}


glm::uvec2 Renderer::getDrawableSize() const {
	return glm::uvec2(impl->swapchainDesc.width, impl->swapchainDesc.height);
}


MemoryStats Renderer::getMemStats() const {
	MemoryStats stats;
	return stats;
}


void RendererImpl::waitForDeviceIdle() {
	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		if (f.outstanding) {
			// try to wait
			waitForFrame(i);
			assert(!f.outstanding);
		}
	}
}


void Renderer::beginFrame() {
	assert(!impl->inFrame);
	impl->inFrame       = true;
	impl->inRenderPass  = false;
	impl->validPipeline = false;
	impl->pipelineDrawn = true;

	impl->currentFrameIdx        = impl->frameNum % impl->frames.size();
	assert(impl->currentFrameIdx < impl->frames.size());
	auto &frame                  = impl->frames.at(impl->currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	if (frame.outstanding) {
		impl->waitForFrame(impl->currentFrameIdx);
	}
	assert(!frame.outstanding);
}


void Renderer::beginRenderPassSwapchain(RenderPassHandle rpHandle) {
	assert(rpHandle);
	assert(impl->inFrame);
	assert(!impl->inRenderPass);
	impl->inRenderPass  = true;
	impl->validPipeline = false;
	impl->pipelineDrawn = true;

	assert(!impl->renderingToSwapchain);
	impl->renderingToSwapchain = true;

	impl->currentFrameIdx        = impl->frameNum % impl->frames.size();
	assert(impl->currentFrameIdx < impl->frames.size());
	auto &frame                  = impl->frames.at(impl->currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	if (frame.outstanding) {
		impl->waitForFrame(impl->currentFrameIdx);
	}
	assert(!frame.outstanding);
}


void Renderer::presentFrame() {
	assert(impl->inFrame);
	impl->inFrame = false;

	auto &frame = impl->frames.at(impl->currentFrameIdx);

	frame.usedRingBufPtr = impl->ringBufPtr;
	frame.outstanding    = true;
	frame.lastFrameNum   = impl->frameNum;

	impl->frameNum++;
}


void RendererImpl::waitForFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.outstanding);

	for (auto &handle : frame.ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		if (buffer.ringBufferAlloc) {
			buffer.ringBufferAlloc = false;
		}

		assert(buffer.size   >  0);
		buffer.size = 0;
		buffer.beginOffs = 0;

		buffers.remove(std::move(handle));
	}
	frame.ephemeralBuffers.clear();
	frame.outstanding    = false;
	lastSyncedFrame      = std::max(lastSyncedFrame, frame.lastFrameNum);
	lastSyncedRingBufPtr = std::max(lastSyncedRingBufPtr, frame.usedRingBufPtr);
}


void RendererImpl::deleteFrameInternal(Frame &f) {
	assert(!f.outstanding);
}


void Renderer::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
	assert(impl->inFrame);
	assert(!impl->inRenderPass);
	assert(!impl->renderingToSwapchain);

	impl->inRenderPass  = true;
	impl->validPipeline = false;

	assert(fbHandle);
	const auto &fb = impl->framebuffers.get(fbHandle);

	// make sure renderpass and framebuffer match
	assert(fb.renderPass == rpHandle);
}


void Renderer::endRenderPass() {
	assert(impl->inFrame);
	assert(impl->inRenderPass);
	impl->inRenderPass = false;
	impl->renderingToSwapchain = false;
}


void Renderer::layoutTransition(RenderTargetHandle image, Layout src, Layout dest) {
	assert(image);
	assert(dest != +Layout::Undefined);
	assert(src != dest);
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
	assert(impl->inFrame);
	assert(pipeline);
	assert(impl->inRenderPass);
	assert(impl->pipelineDrawn);
	impl->pipelineDrawn = false;
	impl->validPipeline = true;
	impl->scissorSet    = false;

	impl->currentPipeline = impl->pipelines.get(pipeline).desc;
}


void Renderer::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
	assert(impl->inFrame);
	assert(impl->validPipeline);
}


void Renderer::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */) {
	assert(impl->inFrame);
	assert(impl->validPipeline);
}


void Renderer::bindDescriptorSet(unsigned int /* index */, DSLayoutHandle /* layout */, const void * /* data_ */) {
	assert(impl->validPipeline);
}


void Renderer::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(impl->inFrame);
}


void Renderer::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(impl->validPipeline);
	assert(impl->currentPipeline.scissorTest_);
	impl->scissorSet = true;
}


void Renderer::blit(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);
}


void Renderer::resolveMSAA(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);
}


void Renderer::resolveMSAAToSwapchain(RenderTargetHandle source, Layout finalLayout) {
	assert(source);
	assert(finalLayout != +Layout::Undefined);

	assert(impl->inFrame);
	assert(!impl->inRenderPass);
	assert(!impl->renderingToSwapchain);
}


void Renderer::draw(unsigned int /* firstVertex */, unsigned int vertexCount) {
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	assert(!impl->currentPipeline.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	assert(instanceCount > 0);
	assert(!impl->currentPipeline.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
}


void Renderer::drawIndexedOffset(unsigned int vertexCount, unsigned int /* firstIndex */, unsigned int /* minIndex */, unsigned int /* maxIndex */) {
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	assert(!impl->currentPipeline.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
}


} // namespace renderer


#endif //  RENDERER_NULL
