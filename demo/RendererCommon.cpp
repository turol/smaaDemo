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


void Renderer::beginRenderPass(RenderPassHandle pass) {
	impl->beginRenderPass(pass);
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


void Renderer::bindDescriptorSet(unsigned int index, const DescriptorLayout *layout, const void *data) {
	impl->bindDescriptorSet(index, layout, data);
}


void Renderer::bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler) {
	impl->bindTexture(unit, tex, sampler);
}


void Renderer::bindUniformBuffer(unsigned int index, BufferHandle buffer) {
	impl->bindUniformBuffer(index, buffer);
}


void Renderer::bindStorageBuffer(unsigned int index, BufferHandle buffer) {
	impl->bindStorageBuffer(index, buffer);
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
