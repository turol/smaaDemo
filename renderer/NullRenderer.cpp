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
	// TODO: use valgrind to make sure we only write to intended parts of ring buffer
}


RendererImpl::~RendererImpl() {
	while (!waitForDeviceIdle()) {
		// run event loop to avoid hangs
		SDL_PumpEvents();
		// TODO: wait?
	}

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		assert(!f.outstanding);
		deleteFrameInternal(f);
	}
	frames.clear();

	SDL_Quit();
}


bool Renderer::isRenderTargetFormatSupported(Format /* format */) const {
	// TODO: actually check it...
	return true;
}


BufferHandle Renderer::createBuffer(BufferType /* type */, uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	auto result            = impl->buffers.add();
	Buffer &buffer         = result.first;
	buffer.ringBufferAlloc = false;
	buffer.beginOffs       = 0;
	buffer.size            = size;

	// TODO: store contents into buffer

	return result.second;
}


BufferHandle Renderer::createEphemeralBuffer(BufferType /* type */, uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	unsigned int beginPtr = impl->ringBufferAllocate(size, 256);

	// TODO: use valgrind to enforce we only write to intended parts of ring buffer
	memcpy(&impl->ringBuffer[beginPtr], contents, size);

	auto result    = impl->buffers.add();
	Buffer &buffer = result.first;
	buffer.ringBufferAlloc = true;
	buffer.beginOffs       = beginPtr;
	buffer.size            = size;

	impl->frames.at(impl->currentFrameIdx).ephemeralBuffers.push_back(result.second);

	return result.second;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	auto result = impl->framebuffers.add();
	auto &fb = result.first;
	fb.renderPass = desc.renderPass_;

	return result.second;
}


RenderPassHandle RendererImpl::createRenderPass(const RenderPassDesc &desc) {
	auto result = renderpasses.add();
	auto &rp    = result.first;
	rp.desc     = desc;

	return result.second;
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	auto result = impl->pipelines.add();
	auto &pipeline = result.first;
	pipeline.desc = desc;
	return result.second;
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != +Format::Invalid);

	auto result = rendertargets.add();
	auto &rendertarget = result.first;
	rendertarget.desc = desc;
	return result.second;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc &desc) {
	auto result = samplers.add();
	Sampler &sampler = result.first;
	// TODO: check desc
	sampler.desc = desc;

	return result.second;
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


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	// TODO: check data

	auto result = textures.add();
	Texture &texture = result.first;
	// TODO: check desc
	texture.desc = desc;

	return result.second;
}


DSLayoutHandle RendererImpl::createDescriptorSetLayout(const DescriptorLayout *layout) {
	auto result = dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;

	while (layout->type != +DescriptorType::End) {
		dsLayout.layout.push_back(*layout);
		layout++;
	}
	assert(layout->offset == 0);

	return result.second;
}


TextureHandle RendererImpl::getRenderTargetView(RenderTargetHandle /* handle */, Format /* f */) {
	TextureHandle handle;

	return handle;
}


void RendererImpl::deleteBuffer(BufferHandle /* handle */) {
}


void RendererImpl::deleteFramebuffer(FramebufferHandle /*  */) {
}


void RendererImpl::deletePipeline(PipelineHandle /* handle */) {
}


void RendererImpl::deleteRenderPass(RenderPassHandle /* fbo */) {
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &) {
}


void RendererImpl::deleteSampler(SamplerHandle /* handle */) {
}


void RendererImpl::deleteTexture(TextureHandle /* handle */) {
}


void RendererImpl::setSwapchainDesc(const SwapchainDesc &desc) {
	swapchainDesc  = desc;
}


glm::uvec2 Renderer::getDrawableSize() const {
	return glm::uvec2(impl->swapchainDesc.width, impl->swapchainDesc.height);
}


MemoryStats RendererImpl::getMemStats() const {
	MemoryStats stats;
	return stats;
}


bool RendererImpl::waitForDeviceIdle() {
	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		if (f.outstanding) {
			// try to wait
			if (!waitForFrame(i)) {
				assert(f.outstanding);
				return false;
			}
			assert(!f.outstanding);
		}
	}

	return true;
}


bool RendererImpl::beginFrame() {
	assert(!inFrame);
	inFrame       = true;
	inRenderPass  = false;
	validPipeline = false;
	pipelineDrawn = true;

	currentFrameIdx        = frameNum % frames.size();
	assert(currentFrameIdx < frames.size());
	auto &frame            = frames.at(currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	if (frame.outstanding) {
		while (! waitForFrame(currentFrameIdx)) {}
	}
	assert(!frame.outstanding);

	return true;
}


void RendererImpl::presentFrame(RenderTargetHandle /* rt */) {
	assert(inFrame);
	inFrame = false;

	auto &frame = frames.at(currentFrameIdx);

	frame.usedRingBufPtr = ringBufPtr;
	frame.outstanding    = true;
	frame.lastFrameNum   = frameNum;

	frameNum++;
}


bool RendererImpl::waitForFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.outstanding);

	for (auto handle : frame.ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		if (buffer.ringBufferAlloc) {
			buffer.ringBufferAlloc = false;
		}

		assert(buffer.size   >  0);
		buffer.size = 0;
		buffer.beginOffs = 0;

		buffers.remove(handle);
	}
	frame.ephemeralBuffers.clear();
	frame.outstanding    = false;
	lastSyncedFrame      = std::max(lastSyncedFrame, frame.lastFrameNum);
	lastSyncedRingBufPtr = std::max(lastSyncedRingBufPtr, frame.usedRingBufPtr);

	return true;
}


void RendererImpl::deleteFrameInternal(Frame &f) {
	assert(!f.outstanding);
}


void RendererImpl::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
	assert(inFrame);
	assert(!inRenderPass);
	inRenderPass  = true;
	validPipeline = false;

	assert(fbHandle);
	const auto &fb = framebuffers.get(fbHandle);

	// make sure renderpass and framebuffer match
	assert(fb.renderPass == rpHandle);
}


void RendererImpl::endRenderPass() {
	assert(inFrame);
	assert(inRenderPass);
	inRenderPass = false;
}


void RendererImpl::layoutTransition(RenderTargetHandle image, Layout src, Layout dest) {
	assert(image);
	assert(dest != +Layout::Undefined);
	assert(src != dest);
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(inFrame);
	assert(pipeline);
	assert(inRenderPass);
	assert(pipelineDrawn);
	pipelineDrawn = false;
	validPipeline = true;
	scissorSet = false;

	currentPipeline = pipelines.get(pipeline).desc;
}


void RendererImpl::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
	assert(inFrame);
	assert(validPipeline);
}


void RendererImpl::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */) {
	assert(inFrame);
	assert(validPipeline);
}


void RendererImpl::bindDescriptorSet(unsigned int /* index */, DSLayoutHandle /* layout */, const void * /* data_ */) {
	assert(validPipeline);
}


void RendererImpl::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(inFrame);
}


void RendererImpl::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	assert(validPipeline);
	assert(currentPipeline.scissorTest_);
	scissorSet = true;
}


void RendererImpl::blit(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!inRenderPass);
}


void RendererImpl::resolveMSAA(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!inRenderPass);
}


void RendererImpl::draw(unsigned int /* firstVertex */, unsigned int vertexCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	pipelineDrawn = true;
}


void RendererImpl::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(instanceCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	pipelineDrawn = true;
}


void RendererImpl::drawIndexedOffset(unsigned int vertexCount, unsigned int /* firstIndex */, unsigned int /* minIndex */, unsigned int /* maxIndex */) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	pipelineDrawn = true;
}


} // namespace renderer


#endif //  RENDERER_NULL
