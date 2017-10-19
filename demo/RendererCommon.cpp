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


#include "RendererInternal.h"
#include "Utils.h"

#include <algorithm>

#include <shaderc/shaderc.hpp>


namespace renderer {


const char *descriptorTypeName(DescriptorType t) {
	switch (t) {
	case DescriptorType::End:
		return "End";

	case DescriptorType::UniformBuffer:
		return "UniformBuffer";

	case DescriptorType::StorageBuffer:
		return "StorageBuffer";

	case DescriptorType::Sampler:
		return "Sampler";

	case DescriptorType::Texture:
		return "Texture";

	case DescriptorType::CombinedSampler:
		return "CombinedSampler";

	case DescriptorType::Count:
		UNREACHABLE();  // shouldn't happen
		return "Count";

	}

	UNREACHABLE();
	return "ERROR!";
}


void RendererImpl::SDLPrefDirDel::operator()(char *ptr) const {
	SDL_free(ptr);
}


class Includer final : public shaderc::CompileOptions::IncluderInterface {
	std::unordered_map<std::string, std::vector<char> > cache;


	virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type /* type */, const char* /* requesting_source */, size_t /* include_depth */) {
		std::string filename(requested_source);

		// std::unordered_map<std::string, std::vector<char> >::iterator it = cache.find(filename);
		auto it = cache.find(filename);
		if (it == cache.end()) {
			auto contents = readFile(requested_source);
			bool inserted = false;
			std::tie(it, inserted) = cache.emplace(std::move(filename), std::move(contents));
			// since we just checked it's not there this must succeed
			assert(inserted);
		}

		auto data = new shaderc_include_result;
		data->source_name         = it->first.c_str();
		data->source_name_length  = it->first.size();
		data->content             = it->second.data();
		data->content_length      = it->second.size();
		data->user_data           = nullptr;

		return data;
	}

	virtual void ReleaseInclude(shaderc_include_result* data) {
		// no need to delete any of data's contents, they're owned by this class
		delete data;
	}
};


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


std::vector<uint32_t> RendererImpl::compileSpirv(const std::string &name, const ShaderMacros &macros, shaderc_shader_kind kind) {
	// check spir-v cache first

	std::string spvName = std::string(spirvCacheDir.get()) + name;
	{
		std::vector<std::string> sorted;
		sorted.reserve(macros.size());
		for (const auto &macro : macros) {
			std::string s = macro.first;
			if (!macro.second.empty()) {
				s += "=";
				s += macro.second;
			}
			sorted.emplace_back(std::move(s));
		}

		std::sort(sorted.begin(), sorted.end());
		for (const auto &s : sorted) {
			spvName += "_" + s;
		}
	}
	spvName = spvName + ".spv";

	if (!skipShaderCache && fileExists(spvName)) {
		// check timestamp against source file
		// TODO: should also check headers included during original compilation
		int64_t sourceTime = getFileTimestamp(name);
		int64_t cacheTime = getFileTimestamp(spvName);

		if (sourceTime <= cacheTime) {
			auto temp = readFile(spvName);
			if (temp.size() % 4 == 0) {
				std::vector<uint32_t> spirv;
				spirv.resize(temp.size() / 4);
				memcpy(&spirv[0], &temp[0], temp.size());
				LOG("Loaded shader \"%s\" from cache\n", spvName.c_str());

				return spirv;
			}
			LOG("Shader \"%s\" has incorrect size\n", spvName.c_str());
		} else {
			LOG("Shader \"%s\" in cache is older than source, recompiling\n", spvName.c_str());
		}
	}

	auto src = loadSource(name);

	shaderc::CompileOptions options;
	// TODO: optimization level?
	// TODO: cache includes globally
	options.SetIncluder(std::make_unique<Includer>());

	for (const auto &p : macros) {
		options.AddMacroDefinition(p.first, p.second);
	}

	shaderc::Compiler compiler;
	auto result = compiler.CompileGlslToSpv(&src[0], src.size(), kind, name.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		LOG("Shader %s compile failed: %s\n", name.c_str(), result.GetErrorMessage().c_str());
		throw std::runtime_error("Shader compile failed");
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());

	writeFile(spvName, &spirv[0], spirv.size() * 4);

	return spirv;
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


bool Renderer::isRenderTargetFormatSupported(Format format) const {
	return impl->isRenderTargetFormatSupported(format);
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


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	return impl->createDescriptorSetLayout(layout);
}


TextureHandle Renderer::getRenderTargetTexture(RenderTargetHandle handle) {
	return impl->getRenderTargetTexture(handle);
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->deleteBuffer(handle);
}


void Renderer::deleteFramebuffer(FramebufferHandle handle) {
	impl->deleteFramebuffer(handle);
}


void Renderer::deleteRenderTarget(RenderTargetHandle &rt) {
	impl->deleteRenderTarget(rt);
}


void Renderer::deleteRenderPass(RenderPassHandle handle) {
	impl->deleteRenderPass(handle);
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


MemoryStats Renderer::getMemStats() const {
	return impl->getMemStats();
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


void Renderer::bindDescriptorSet(unsigned int index, DSLayoutHandle layout, const void *data) {
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


glm::uvec2 Renderer::getDrawableSize() const {
	return glm::uvec2(impl->swapchainDesc.width, impl->swapchainDesc.height);
}


}	// namespace renderer
