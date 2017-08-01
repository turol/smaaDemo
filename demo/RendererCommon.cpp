#include "RendererInternal.h"
#include "Utils.h"


std::vector<char> RendererImpl::loadSource(const std::string &name) {
	auto it = shaderSources.find(name);
	if (it != shaderSources.end()) {
		return it->second;
	} else {
		auto source = readFile(name);
		shaderSources.emplace(name, source);
		return source;
	}
}


Renderer Renderer::createRenderer(const RendererDesc &desc) {
	return Renderer(new RendererImpl(desc));
}


Renderer &Renderer::operator=(Renderer &&other) {
	assert(impl == nullptr);
	impl = other.impl;
	other.impl = nullptr;

	return *this;
}


Renderer::Renderer(Renderer &&other)
: impl(other.impl)
{
	other.impl = nullptr;
}


Renderer::Renderer()
: impl(nullptr)
{
}


Renderer::Renderer(RendererImpl *impl_)
: impl(impl_)
{
	assert(impl != nullptr);
}


Renderer::~Renderer() {
	if (impl) {
		delete impl;
		impl = nullptr;
	}
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	return impl->createBuffer(size, contents);
}


BufferHandle Renderer::createEphemeralBuffer(uint32_t size, const void *contents) {
	return impl->createEphemeralBuffer(size, contents);
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	return impl->createFramebuffer(desc);
}


RenderPassHandle Renderer::createRenderPass(const RenderPassDesc &desc) {
	return impl->createRenderPass(desc);
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	return impl->createPipeline(desc);
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	return impl->createRenderTarget(desc);
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	return impl->createSampler(desc);
}


VertexShaderHandle Renderer::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	return impl->createVertexShader(name, macros);
}


FragmentShaderHandle Renderer::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	return impl->createFragmentShader(name, macros);
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	return impl->createTexture(desc);
}


DescriptorSetLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	return impl->createDescriptorSetLayout(layout);
}


TextureHandle Renderer::getRenderTargetTexture(RenderTargetHandle handle) {
	return impl->getRenderTargetTexture(handle);
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->deleteBuffer(handle);
}


void Renderer::deleteRenderTarget(RenderTargetHandle &rt) {
	impl->deleteRenderTarget(rt);
}


void Renderer::deleteSampler(SamplerHandle handle) {
	impl->deleteSampler(handle);
}


void Renderer::deleteTexture(TextureHandle handle) {
	impl->deleteTexture(handle);
}


void Renderer::recreateSwapchain(const SwapchainDesc &desc) {
	impl->recreateSwapchain(desc);
}


void Renderer::beginFrame() {
	impl->beginFrame();
}


void Renderer::presentFrame(RenderTargetHandle image) {
	impl->presentFrame(image);
}


void Renderer::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
	impl->beginRenderPass(rpHandle, fbHandle);
}


void Renderer::endRenderPass() {
	impl->endRenderPass();
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
	impl->bindPipeline(pipeline);
}


void Renderer::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	impl->bindIndexBuffer(buffer, bit16);
}


void Renderer::bindVertexBuffer(unsigned int binding, BufferHandle buffer) {
	impl->bindVertexBuffer(binding, buffer);
}


void Renderer::bindDescriptorSet(unsigned int index, DescriptorSetLayoutHandle layout, const void *data) {
	impl->bindDescriptorSet(index, layout, data);
}


void Renderer::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setViewport(x, y, width, height);
}


void Renderer::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setScissorRect(x, y, width, height);
}


void Renderer::draw(unsigned int firstVertex, unsigned int vertexCount) {
	impl->draw(firstVertex, vertexCount);
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	impl->drawIndexedInstanced(vertexCount, instanceCount);
}


void Renderer::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex) {
	impl->drawIndexedOffset(vertexCount, firstIndex);
}


unsigned int RendererImpl::ringBufferAllocate(unsigned int size, unsigned int alignment) {
	// sub-allocate from persistent coherent buffer
	// round current pointer up to necessary alignment
	const unsigned int add   = alignment - 1;
	const unsigned int mask  = ~add;
	unsigned int alignedPtr  = (ringBufPtr + add) & mask;
	assert(ringBufPtr <= alignedPtr);
	// TODO: ring buffer size should be pow2, se should use add & mask here too
	unsigned int beginPtr    =  alignedPtr % ringBufSize;

	if (beginPtr + size >= ringBufSize) {
		// we went past the end and have to go back to beginning
		// TODO: add and mask here too
		ringBufPtr = (ringBufPtr / ringBufSize + 1) * ringBufSize;
		assert((ringBufPtr & ~mask) == 0);
		alignedPtr  = (ringBufPtr + add) & mask;
		beginPtr    =  alignedPtr % ringBufSize;
		assert(beginPtr + size < ringBufSize);
		assert(beginPtr == 0);
	}
	ringBufPtr = alignedPtr + size;

	return beginPtr;
}

