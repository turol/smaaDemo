/*
Copyright (c) 2015-2018 Alternative Games Ltd / Turo Lamminen

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
#include "utils/Utils.h"

#include <algorithm>

#include <shaderc/shaderc.hpp>
#include <spirv-tools/optimizer.hpp>
#include <SPIRV/SPVRemapper.h>


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


bool isDepthFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		UNREACHABLE();
		return false;

	case Format::R8:
		return false;

	case Format::RG8:
		return false;

	case Format::RGB8:
		return false;

	case Format::RGBA8:
		return false;

	case Format::sRGBA8:
		return false;

	case Format::RG16Float:
	case Format::RGBA16Float:
		return false;

	case Format::RGBA32Float:
		return false;

	case Format::Depth16:
		return true;

	case Format::Depth16S8:
		return true;

	case Format::Depth24S8:
		return true;

	case Format::Depth24X8:
		return true;

	case Format::Depth32Float:
		return true;

	}

	UNREACHABLE();
	return false;
}


bool issRGBFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		UNREACHABLE();
		return false;

	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
	case Format::RG16Float:
	case Format::RGBA16Float:
	case Format::RGBA32Float:
		return false;

	case Format::sRGBA8:
		return true;

	case Format::Depth16:
	case Format::Depth16S8:
	case Format::Depth24S8:
	case Format::Depth24X8:
	case Format::Depth32Float:
		return false;

	}

	UNREACHABLE();
	return false;
}


const char *layoutName(Layout layout) {
	switch (layout) {
	case Layout::Undefined:
		return "Undefined";

	case Layout::ShaderRead:
		return "ShaderRead";

	case Layout::TransferSrc:
		return "TransferSrc";

	case Layout::TransferDst:
		return "TransferDst";

	case Layout::ColorAttachment:
		return "ColorAttachment";

	}

	UNREACHABLE();
	return "";
}


const char *formatName(Format format) {
   switch (format) {
	case Format::Invalid:
		return "Invalid";

	case Format::R8:
		return "R8";

	case Format::RG8:
		return "RG8";

	case Format::RGB8:
		return "RGB8";

	case Format::RGBA8:
		return "RGBA8";

	case Format::sRGBA8:
		return "sRGBA8";

	case Format::RG16Float:
		return "RG16Float";

	case Format::RGBA16Float:
		return "RGBA16Float";

	case Format::RGBA32Float:
		return "RGBA32Float";

	case Format::Depth16:
		return "Depth16";

	case Format::Depth16S8:
		return "Depth16S8";

	case Format::Depth24S8:
		return "Depth24S8";

	case Format::Depth24X8:
		return "Depth24X8";

	case Format::Depth32Float:
		return "Depth32Float";

	}

	UNREACHABLE();
	return "";
}


uint32_t formatSize(Format format) {
	switch (format) {
	case Format::Invalid:
		UNREACHABLE();
		return 4;

	case Format::R8:
		return 1;

	case Format::RG8:
		return 2;

	case Format::RGB8:
		return 3;

	case Format::RGBA8:
		return 4;

	case Format::sRGBA8:
		return 4;

	case Format::RG16Float:
		return 2 * 2;

	case Format::RGBA16Float:
		return 4 * 2;

	case Format::RGBA32Float:
		return 4 * 4;

	case Format::Depth16:
		return 2;

	case Format::Depth16S8:
		return 4;  // ?

	case Format::Depth24S8:
		return 4;

	case Format::Depth24X8:
		return 4;

	case Format::Depth32Float:
		return 4;

	}

	UNREACHABLE();
	return 4;
}


class Includer final : public shaderc::CompileOptions::IncluderInterface {
	std::unordered_map<std::string, std::vector<char> > &cache;


public:

	explicit Includer(std::unordered_map<std::string, std::vector<char> > &cache_)
	: cache(cache_)
	{
	}

	Includer(const Includer &)            = delete;
	Includer(Includer &&)                 = default;

	Includer &operator=(const Includer &) = delete;
	Includer &operator=(Includer &&)      = default;

	~Includer() {}


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
		// no need to delete any of data's contents, they're owned by someone else
		delete data;
	}
};


std::vector<char> RendererBase::loadSource(const std::string &name) {
	auto it = shaderSources.find(name);
	if (it != shaderSources.end()) {
		return it->second;
	} else {
		auto source = readFile(name);
		shaderSources.emplace(name, source);
		return source;
	}
}


// increase this when the shader compiler options change
// so that the same source generates a different SPV
const unsigned int shaderVersion = 13;


std::vector<uint32_t> RendererBase::compileSpirv(const std::string &name, const ShaderMacros &macros, ShaderKind kind_) {
	// check spir-v cache first

	shaderc_shader_kind kind;
	switch (kind_) {
	case ShaderKind::Vertex:
		kind = shaderc_glsl_vertex_shader;
		break;

	case ShaderKind::Fragment:
		kind = shaderc_glsl_fragment_shader;
		break;
	}

	std::string spvName = spirvCacheDir + name;
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
	LOG("Looking for \"%s\" in cache...\n", spvName.c_str());
	std::string cacheName = spvName + ".cache";
	spvName = spvName + ".spv";

	if (!skipShaderCache && fileExists(cacheName) && fileExists(spvName)) {
		auto cacheStr_ = readFile(cacheName);
		std::string cacheStr(cacheStr_.begin(), cacheStr_.end());
		int version = atoi(cacheStr.c_str());
		if (version != int(shaderVersion)) {
			LOG("version mismatch, found %d when expected %u\n", version, shaderVersion);
		} else {
		// check timestamp against source and header files
		int64_t sourceTime = getFileTimestamp(name);
		int64_t cacheTime = getFileTimestamp(spvName);

			auto comma = cacheStr.find(',');
			if (comma != std::string::npos) {
				std::string str = cacheStr.substr(comma + 1);
				while (!str.empty()) {
					comma = str.find(',');
					std::string filename;
					if (comma == std::string::npos) {
						filename = str;
						str.clear();
					} else {
						filename = str.substr(0, comma);
					}
					int64_t includeTime = getFileTimestamp(filename);
					sourceTime = std::max(sourceTime, includeTime);

					str = str.substr(comma + 1);
				}
			}

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
	}

	auto src = loadSource(name);

	// TODO: cache includes globally
	std::unordered_map<std::string, std::vector<char> > cache;
	shaderc::CompileOptions options;
	// TODO: optimization level?
	options.SetIncluder(std::make_unique<Includer>(cache));

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

	// SPIR-V optimization
	if (optimizeShaders) {
		// TODO: better target environment selection?
		spvtools::Optimizer opt(SPV_ENV_UNIVERSAL_1_2);

		opt.SetMessageConsumer([] (spv_message_level_t level, const char *source, const spv_position_t &position, const char *message) {
			logWrite("%u: %s %u:%u:%u %s\n", level, source, uint32_t(position.line), uint32_t(position.column), uint32_t(position.index), message);
		});

		// SPIRV-Tools optimizer
		opt.RegisterPerformancePasses();

		std::vector<uint32_t> optimized;
		optimized.reserve(spirv.size());
		bool success = opt.Run(&spirv[0], spirv.size(), &optimized);
		if (!success) {
			throw std::runtime_error("Shader optimization failed");
		}

		// glslang SPV remapper
		{
			spv::spirvbin_t remapper;
			remapper.remap(optimized);
		}

		std::swap(spirv, optimized);
	}

	if (!skipShaderCache) {
		std::string cacheStr = std::to_string(shaderVersion);

		for (const auto &p : cache) {
			cacheStr += ",";
			cacheStr += p.first;
		}

		writeFile(cacheName, cacheStr.c_str(), cacheStr.size());
		writeFile(spvName, &spirv[0], spirv.size() * 4);
	}

	return spirv;
}


Renderer Renderer::createRenderer(const RendererDesc &desc) {
	return Renderer(new RendererImpl(desc));
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


unsigned int Renderer::getCurrentRefreshRate() const {
	return impl->currentRefreshRate;
}


unsigned int Renderer::getMaxRefreshRate() const {
	return impl->maxRefreshRate;
}


const RendererFeatures &Renderer::getFeatures() const {
	return impl->features;
}


BufferHandle Renderer::createBuffer(BufferType type, uint32_t size, const void *contents) {
	return impl->createBuffer(type, size, contents);
}


BufferHandle Renderer::createEphemeralBuffer(BufferType type, uint32_t size, const void *contents) {
	return impl->createEphemeralBuffer(type, size, contents);
}


FragmentShaderHandle Renderer::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	return impl->createFragmentShader(name, macros);
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	return impl->createFramebuffer(desc);
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	return impl->createPipeline(desc);
}


RenderPassHandle Renderer::createRenderPass(const RenderPassDesc &desc) {
	return impl->createRenderPass(desc);
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


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	return impl->createTexture(desc);
}


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	return impl->createDescriptorSetLayout(layout);
}


TextureHandle Renderer::getRenderTargetTexture(RenderTargetHandle handle) {
	return impl->getRenderTargetTexture(handle);
}


TextureHandle Renderer::getRenderTargetView(RenderTargetHandle handle, Format f) {
	return impl->getRenderTargetView(handle, f);
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->deleteBuffer(handle);
}


void Renderer::deleteFramebuffer(FramebufferHandle handle) {
	impl->deleteFramebuffer(handle);
}


void Renderer::deleteRenderPass(RenderPassHandle handle) {
	impl->deleteRenderPass(handle);
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


void Renderer::setSwapchainDesc(const SwapchainDesc &desc) {
	impl->setSwapchainDesc(desc);
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


void Renderer::layoutTransition(RenderTargetHandle image, Layout src, Layout dest) {
	impl->layoutTransition(image, src, dest);
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


void Renderer::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setScissorRect(x, y, width, height);
}


void Renderer::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setViewport(x, y, width, height);
}


void Renderer::blit(FramebufferHandle source, FramebufferHandle target, unsigned int n) {
	impl->blit(source, target, n);
}


void Renderer::resolveMSAA(FramebufferHandle source, FramebufferHandle target, unsigned int n) {
	impl->resolveMSAA(source, target, n);
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
	assert(alignment != 0);
	assert(isPow2(alignment));

	if (size > ringBufSize) {
		unsigned int newSize = nextPow2(size);
		LOG("WARNING: out of ringbuffer space, reallocating to %u bytes\n", newSize);
		recreateRingBuffer(newSize);

		assert(ringBufPtr == 0);
	}

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

	// ran out of buffer space?
	if (ringBufPtr >= lastSyncedRingBufPtr + ringBufSize) {
		unsigned int newSize = ringBufSize * 2;
		assert(size < newSize);

		LOG("WARNING: out of ringbuffer space, reallocating to %u bytes\n", newSize);
		recreateRingBuffer(newSize);

		assert(ringBufPtr == 0);
		beginPtr   = 0;
		ringBufPtr = size;
	}

	return beginPtr;
}


glm::uvec2 Renderer::getDrawableSize() const {
	return impl->drawableSize;
}


}	// namespace renderer
