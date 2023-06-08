/*
Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

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
#include <charconv>
#include <string_view>

#include <spirv-tools/optimizer.hpp>
#include <SPIRV/SPVRemapper.h>
#include <spirv_cross.hpp>

#include <magic_enum_containers.hpp>

#include <xxhash.h>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/SpvTools.h>
#include <SPIRV/GlslangToSpv.h>


namespace renderer {


auto format_as(ShaderLanguage language) {
	return magic_enum::enum_name(language);
}


auto format_as(ShaderStage stage) {
	return magic_enum::enum_name(stage);
}


auto format_as(ShaderCacheKey cacheKey) {
	if (!cacheKey.macros.empty()) {
		return fmt::format("{} {} {} {}", cacheKey.stage, cacheKey.language, cacheKey.filename, renderer::RendererBase::formatMacros(cacheKey.macros));
	} else {
		return fmt::format("{} {} {}", cacheKey.stage, cacheKey.language, cacheKey.filename);
	}
}


bool isColorFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
		return false;

	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
	case Format::sRGBA8:
	case Format::BGRA8:
	case Format::sBGRA8:
	case Format::RG16Float:
	case Format::RGBA16Float:
	case Format::RGBA32Float:
		return true;

	case Format::Depth16:
	case Format::Depth16S8:
	case Format::Depth24S8:
	case Format::Depth24X8:
	case Format::Depth32Float:
		return false;

	}

	HEDLEY_UNREACHABLE();
	return false;
}


bool isDepthFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
		return false;

	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
	case Format::sRGBA8:
	case Format::BGRA8:
	case Format::sBGRA8:
	case Format::RG16Float:
	case Format::RGBA16Float:
	case Format::RGBA32Float:
		return false;

	case Format::Depth16:
	case Format::Depth16S8:
	case Format::Depth24S8:
	case Format::Depth24X8:
	case Format::Depth32Float:
		return true;

	}

	HEDLEY_UNREACHABLE();
	return false;
}


bool issRGBFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
		return false;

	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
	case Format::BGRA8:
	case Format::RG16Float:
	case Format::RGBA16Float:
	case Format::RGBA32Float:
		return false;

	case Format::sBGRA8:
	case Format::sRGBA8:
		return true;

	case Format::Depth16:
	case Format::Depth16S8:
	case Format::Depth24S8:
	case Format::Depth24X8:
	case Format::Depth32Float:
		return false;

	}

	HEDLEY_UNREACHABLE();
	return false;
}


uint32_t formatSize(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
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

	case Format::BGRA8:
	case Format::sBGRA8:
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

	HEDLEY_UNREACHABLE();
	return 4;
}


bool PipelineDesc::VertexAttr::operator==(const VertexAttr &other) const {
	if (this->bufBinding != other.bufBinding) {
		return false;
	}

	if (this->count      != other.count) {
		return false;
	}

	if (this->format     != other.format) {
		return false;
	}

	if (this->offset     != other.offset) {
		return false;
	}

	return true;
}


bool PipelineDesc::VertexAttr::operator!=(const VertexAttr &other) const {
	return !(*this == other);
}


bool PipelineDesc::VertexBuf::operator==(const VertexBuf &other) const {
	if (this->stride != other.stride) {
		return false;
	}

	return true;
}


bool PipelineDesc::VertexBuf::operator!=(const VertexBuf &other) const {
	return !(*this == other);
}


bool PipelineDesc::operator==(const PipelineDesc &other) const {
	if (this->vertexShaderName   != other.vertexShaderName) {
		return false;
	}

	if (this->fragmentShaderName != other.fragmentShaderName) {
		return false;
	}

	if (this->renderPass_        != other.renderPass_) {
		return false;
	}

	if (this->shaderMacros_      != other.shaderMacros_) {
		return false;
	}

	if (this->vertexAttribMask   != other.vertexAttribMask) {
		return false;
	}

	if (this->numSamples_        != other.numSamples_) {
		return false;
	}

	if (this->depthWrite_        != other.depthWrite_) {
		return false;
	}

	if (this->depthTest_         != other.depthTest_) {
		return false;
	}

	if (this->cullFaces_         != other.cullFaces_) {
		return false;
	}

	if (this->scissorTest_       != other.scissorTest_) {
		return false;
	}

	if (this->blending_          != other.blending_) {
		return false;
	}

	if (this->blending_) {
		if (this->sourceBlend_      != other.sourceBlend_) {
			return false;
		}

		if (this->destinationBlend_ != other.destinationBlend_) {
			return false;
		}
	}

	LOG_TODO("only check enabled attributes");
	for (unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; i++) {
		if (this->vertexAttribs[i] != other.vertexAttribs[i]) {
			return false;
		}
	}

	for (unsigned int i = 0; i < MAX_VERTEX_BUFFERS; i++) {
		if (this->vertexBuffers[i] != other.vertexBuffers[i]) {
			return false;
		}
	}

	for (unsigned int i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
		if (this->descriptorSetLayouts[i] != other.descriptorSetLayouts[i]) {
			return false;
		}
	}

	if (this->name_ != other.name_) {
		return false;
	}

	return true;
}


RendererBase::RendererBase(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, wantedSwapchain(desc.swapchain)
, skipShaderCache(desc.skipShaderCache || !desc.optimizeShaders)
, optimizeShaders(desc.optimizeShaders)
, validateShaders(desc.validateShaders)
, renderingToSwapchain(false)
{
	char *prefPath = SDL_GetPrefPath("", "SMAADemo");
	spirvCacheDir = prefPath;
	SDL_free(prefPath);

	bool success = glslang::InitializeProcess();
	if (!success) {
		THROW_ERROR("glslang initialization failed");
	}

	if (skipShaderCache) {
		LOG("Shader cache loading skipped");
	} else {
		loadShaderCache();
	}
}


RendererBase::~RendererBase() {
	glslang::FinalizeProcess();

	if (!skipShaderCache) {
		saveShaderCache();
	}
}


std::vector<char> RendererBase::loadSource(const std::string &name) {
	auto it = shaderSources.find(name);
	if (it != shaderSources.end()) {
		return it->second.contents;
	} else {
		ShaderSourceData d;
		d.contents  = readFile(name);
		d.timestamp = getFileTimestamp(name);

		bool inserted = false;
		std::tie(it, inserted) = shaderSources.emplace(name, std::move(d));
		assert(inserted);

		return it->second.contents;
	}
}


// increase this when the shader compiler options change
// so that the same source generates a different SPV
// or the cache json format changes
const unsigned int shaderVersion = 109;


// helper for storing in cache .json
// like ShaderSourceData but with file size instead of contents
struct ShaderSourceCacheData {
	int64_t  timestamp;
	uint32_t fileSize;
};


void to_json(nlohmann::json &j, const ShaderStage &stage) {
	j = magic_enum::enum_name(stage);
}


void from_json(const nlohmann::json &j, ShaderStage &stage) {
	stage = *magic_enum::enum_cast<ShaderStage>(j.get<std::string>());
}


void to_json(nlohmann::json &j, const ShaderLanguage &language) {
	j = magic_enum::enum_name(language);
}


void from_json(const nlohmann::json &j, ShaderLanguage &language) {
	language = *magic_enum::enum_cast<ShaderLanguage>(j.get<std::string>());
}


void to_json(nlohmann::json &j, const ShaderSourceCacheData &data) {
	j = nlohmann::json {
		  { "timestamp", data.timestamp }
		, { "fileSize",  data.fileSize  }
	};
}


void from_json(const nlohmann::json &j, ShaderSourceCacheData &data) {
	j.at("timestamp").get_to(data.timestamp);
	j.at("fileSize").get_to(data.fileSize);
}


static const magic_enum::containers::array<ShaderStage, const char *> shaderStages = { "vert", "frag" };


std::string RendererBase::makeSPVCacheName(uint64_t hash, ShaderStage stage) {
	return fmt::format("{}{:08x}.{}.spv", spirvCacheDir, hash, shaderStages[stage]);
}


void to_json(nlohmann::json &j, const ShaderMacros &macros) {
	// hax because impl is private
	RendererBase::to_json(j, macros);
}


void RendererBase::to_json(nlohmann::json &j, const ShaderMacros &macros) {
	j = nlohmann::json();

	for (const ShaderMacro &macro : macros.impl) {
		j[macro.key] = macro.value;
	}
}


void from_json(const nlohmann::json &j, ShaderMacros &macros) {
	// hax because impl is private
	RendererBase::from_json(j, macros);
}


void RendererBase::from_json(const nlohmann::json &j, ShaderMacros &macros) {
	macros.impl.reserve(j.size());
	for (const auto &jsonMacro : j.items()) {
		ShaderMacro macro;
		macro.key   = jsonMacro.key();
		macro.value = jsonMacro.value().get<std::string>();
		macros.impl.emplace_back(std::move(macro));
	}

	std::sort(macros.impl.begin(), macros.impl.end());
}


void to_json(nlohmann::json &j, const ShaderCacheKey &key) {
	j = nlohmann::json {
		  { "name",   key.filename }
		, { "stage",  key.stage  }
		, { "language",  key.language  }
		, { "macros", key.macros }
	};
}


void from_json(const nlohmann::json &j, ShaderCacheKey &key) {
	j.at("name").get_to(key.filename);
	j.at("stage").get_to(key.stage);
	j.at("language").get_to(key.language);
	j.at("macros").get_to(key.macros);
}


void to_json(nlohmann::json &j, const ShaderCacheData &data) {
	j = nlohmann::json {
		  { "hash",     fmt::format(FMT_STRING("{:08x}"), data.spirvHash) }
		, { "includes", data.includes  }
	};
}


void from_json(const nlohmann::json &j, ShaderCacheData &data) {
	const std::string_view str = j.at("hash").get<std::string_view>();
	const char *b = str.data();
	const char *e = str.data() + str.size();
	auto result = std::from_chars(b, e, data.spirvHash, 16);
	if (result.ptr != e) {
		throw std::runtime_error(fmt::format(FMT_STRING("Failed to parse cache key \"{}\""), str));
	}
	j.at("includes").get_to(data.includes);
}


static void logSpvMessage(spv_message_level_t level_, const char *source, const spv_position_t &position, const char *message) {
	const char *level;
	switch (level_) {
	case SPV_MSG_FATAL:
		level = "FATAL";
		break;

	case SPV_MSG_INTERNAL_ERROR:
		level = "INTERNAL ERROR";
		break;

	case SPV_MSG_ERROR:
		level = "ERROR";
		break;

	case SPV_MSG_WARNING:
		level = "WARNING";
		break;

	case SPV_MSG_INFO:
		level = "INFO";
		break;

	case SPV_MSG_DEBUG:
		level = "DEBUG";
		break;

	default:
		level = "UNKNOWN";
		break;
	}

	LOG("{}: {} from {} at {}:{}", level, message, source, position.line, position.column);
}


static void checkSPVBindings(const std::vector<uint32_t> &spirv) {
	spirv_cross::Compiler compiler(spirv);
	HashSet<DSIndex> bindings;
	HashMap<DSIndex, uint32_t> uboSizes;

	auto spvResources = compiler.get_shader_resources();

	for (const auto &ubo : spvResources.uniform_buffers) {
		DSIndex idx;
		idx.set     = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
		idx.binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			THROW_ERROR("Duplicate UBO binding ({}, {})", idx.set, idx.binding);
		}

		uint32_t maxOffset = 0;
		LOG("UBO {} ({}, {}) ranges:", static_cast<uint32_t>(ubo.id), idx.set, idx.binding);
		for (const auto &r : compiler.get_active_buffer_ranges(ubo.id)) {
			LOG("  {}:  {}  {}", r.index, r.offset, r.range);
			maxOffset = std::max(maxOffset, static_cast<uint32_t>(r.offset + r.range));
		}
		LOG(" max offset: {}", maxOffset);
		uboSizes.emplace(idx, maxOffset);
	}

	for (const auto &ssbo : spvResources.storage_buffers) {
		DSIndex idx;
		idx.set     = compiler.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
		idx.binding = compiler.get_decoration(ssbo.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			THROW_ERROR("Duplicate SSBO binding ({}, {})", idx.set, idx.binding);
		}
	}

	for (const auto &s : spvResources.sampled_images) {
		DSIndex idx;
		idx.set     = compiler.get_decoration(s.id, spv::DecorationDescriptorSet);
		idx.binding = compiler.get_decoration(s.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			THROW_ERROR("Duplicate combined image/sampler binding ({}, {})", idx.set, idx.binding);
		}
	}

	for (const auto &s : spvResources.separate_images) {
		DSIndex idx;
		idx.set     = compiler.get_decoration(s.id, spv::DecorationDescriptorSet);
		idx.binding = compiler.get_decoration(s.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			THROW_ERROR("Duplicate image binding ({}, {})", idx.set, idx.binding);
		}
	}

	for (const auto &s : spvResources.separate_samplers) {
		DSIndex idx;
		idx.set     = compiler.get_decoration(s.id, spv::DecorationDescriptorSet);
		idx.binding = compiler.get_decoration(s.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			THROW_ERROR("Duplicate image sampler binding ({}, {}) \"{}\"", idx.set, idx.binding, s.name);
		}
	}
}


static const std::array<const char *, 6> spirvOptLogLevels = {
	  "fatal error"
	, "internal error"
	, "error"
	, "warning"
	, "info"
	, "debug"
};



static constexpr magic_enum::containers::array<ShaderStage, const char *>  shaderFilenameExtensions =
{
	  ".vert"
	, ".frag"
};


std::vector<uint32_t> RendererBase::compileSpirv(const std::string &name, ShaderLanguage shaderLanguage, const ShaderMacros &macros, ShaderStage stage) {
	// check spir-v cache first
	ShaderCacheKey cacheKey;
	cacheKey.filename = name + shaderFilenameExtensions[stage];
	cacheKey.stage  = stage;
	cacheKey.language = shaderLanguage;
	cacheKey.macros = macros;

	if (!skipShaderCache) {
		LOG("Looking for shader \"{}\" in cache...", cacheKey);
		try {
			auto it = shaderCache.find(cacheKey);
			if (it != shaderCache.end()) {
				std::string spvName = makeSPVCacheName(it->second.spirvHash, stage);
				LOG("\"{}\" found in memory cache as {}", cacheKey, spvName);
				LOG_TODO("avoid unnecessary copy of SPIR-V data");
				auto temp = readFile(spvName);
				if (temp.size() % 4 != 0) {
					LOG_ERROR("Shader \"{}\" has incorrect size {}", spvName, temp.size());
					goto compilationNeeded;
				}

				std::vector<uint32_t> spirv(temp.size() / 4);
				memcpy(&spirv[0], &temp[0], temp.size());

				LOG_TODO("only in debug");
				// need to move debug flag to base class
				if (true) {
					checkSPVBindings(spirv);
				}

				LOG("Loaded shader \"{}\" from cache", cacheKey);

				return spirv;
			}
		} catch (std::exception &e) {
			LOG_ERROR("Exception while loading shader cache data: \"{}\"", e.what());
		} catch (...) {
			LOG_ERROR("Unknown exception while loading shader cache data");
		}
	}

compilationNeeded:
	LOG("Shader \"{}\" not found in cache", cacheKey);

	std::function<bool(const std::vector<uint32_t> &)> validate;
	if (validateShaders) {
		validate =
			[this] (const std::vector<uint32_t> &s) {
				spvtools::SpirvTools tools(this->spirvEnvironment);
				tools.SetMessageConsumer(logSpvMessage);
				return tools.Validate(s);
			}
		;
	} else {
		validate = [] (const std::vector<uint32_t> &) { return true; };
	}

	Includer includer(this);

	std::vector<uint32_t> spirv;
	{
		auto src = loadSource(cacheKey.filename);

		// we need GOOGLE_include_directive to use #include
		// glslang has no interface for enabling extensions or predefining macros
		// use preamble
		std::vector<char> preamble;

		std::string_view ext = "#extension GL_GOOGLE_include_directive : enable\n";
		preamble.insert(preamble.end(), ext.begin(), ext.end());

		if (!macros.impl.empty()) {
			assert(std::is_sorted(macros.impl.begin(), macros.impl.end()));

			std::string_view define = "#define ";
			for (const auto &macro : macros.impl) {
				preamble.insert(preamble.end(), define.begin(), define.end());
				preamble.insert(preamble.end(), macro.key.begin(), macro.key.end());
				if (!macro.value.empty()) {
					preamble.push_back(' ');
					preamble.insert(preamble.end(), macro.value.begin(), macro.value.end());
				}
				preamble.push_back('\n');
			}
		}

		preamble.push_back('\0');
		LOG("shader preamble: \"\n{}\"\n", preamble.data());

		EShLanguage language;
		switch (stage) {
		case ShaderStage::Vertex:
			language = EShLangVertex;
			break;

		case ShaderStage::Fragment:
			language = EShLangFragment;
			break;

		default:
			HEDLEY_UNREACHABLE();  // shouldn't happen
			break;

		}

		glslang::TShader shader(language);

		char *sourceString   = src.data();
		int sourceLen        = int(src.size());
		const char *filename = cacheKey.filename.c_str();

		shader.setPreamble(preamble.data());
		shader.setStringsWithLengthsAndNames(&sourceString, &sourceLen, &filename, 1);
		shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 450);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

		// compile
		bool success = shader.parse(GetDefaultResources(), 450, ECoreProfile, false, false, EShMsgDefault, includer);

		const char *infoLog = shader.getInfoLog();
		if (infoLog != nullptr && infoLog[0] != '\0') {
			LOG("Shader info log:\n\"{}\"", infoLog);
		}

		if (!success) {
			THROW_ERROR("Failed to compile shader");
		}

		// link
		glslang::TProgram program;
		program.addShader(&shader);
		success = program.link(EShMsgDefault);

		infoLog = program.getInfoLog();
		if (infoLog != nullptr && infoLog[0] != '\0') {
			LOG("Program info log:\n\"{}\"", infoLog);
		}

		if (!success) {
			THROW_ERROR("Failed to link shader");
		}

		// convert to SPIR-V
		spv::SpvBuildLogger logger;
		glslang::SpvOptions spvOptions;
		LOG_TODO("only when tracing");
		if (true) {
			spvOptions.generateDebugInfo = true;
		} else {
			spvOptions.stripDebugInfo = true;
		}
		spvOptions.disableOptimizer = true;
		spvOptions.optimizeSize     = false;
		spvOptions.validate         = validateShaders;
		glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &logger, &spvOptions);

		if (!validate(spirv)) {
			THROW_ERROR("SPIR-V for shader \"{}\" is not valid after compilation", cacheKey);
		}
	}

	LOG_TODO("only in debug");
	// need to move debug flag to base class
	if (true) {
		checkSPVBindings(spirv);
	}

	// SPIR-V optimization
	if (optimizeShaders) {
		LOG_TODO("better target environment selection?");
		spvtools::Optimizer opt(spirvEnvironment);

		opt.SetMessageConsumer([] (spv_message_level_t level, const char *source, const spv_position_t &position, const char *message) {
			LOG("SPIR-V opt {}: {} {}:{}:{} {}", spirvOptLogLevels[level], source, position.line, position.column, position.index, message);
		});

		// SPIRV-Tools optimizer
		opt.RegisterPerformancePasses();

		std::vector<uint32_t> optimized;
		optimized.reserve(spirv.size());
		bool success = opt.Run(&spirv[0], spirv.size(), &optimized);
		if (!success) {
			THROW_ERROR("Shader optimization failed");
		}

		if (!validate(optimized)) {
			THROW_ERROR("SPIR-V for shader \"{}\" is not valid after optimization", cacheKey);
		}

		// glslang SPV remapper
		{
			spv::spirvbin_t remapper;
			remapper.remap(optimized);

			if (!validate(optimized)) {
				THROW_ERROR("SPIR-V for shader \"{}\" is not valid after remapping", cacheKey);
			}
		}

		std::swap(spirv, optimized);
	}

	if (!skipShaderCache) {
		uint64_t hash = XXH64(spirv.data(), spirv.size() * 4, 0);

		std::string spvName  = makeSPVCacheName(hash, stage);
		LOG("Writing shader \"{}\" to \"{}\"", cacheKey, spvName);
		writeFile(spvName, &spirv[0], spirv.size() * 4);

		ShaderCacheData cacheData;
		cacheData.spirvHash = hash;
		cacheData.includes  = includer.included;

		auto DEBUG_ASSERTED inserted = shaderCache.emplace(std::move(cacheKey), std::move(cacheData));
		assert(inserted.second);
		cacheModified = true;
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


unsigned int Renderer::getCurrentRefreshRate() const {
	return impl->currentRefreshRate;
}


unsigned int Renderer::getMaxRefreshRate() const {
	return impl->maxRefreshRate;
}


bool Renderer::getSynchronizationDebugMode() const {
	return impl->synchronizationDebugMode;
}


void Renderer::setSynchronizationDebugMode(bool mode) {
	impl->synchronizationDebugMode = mode;
}


const RendererFeatures &Renderer::getFeatures() const {
	return impl->features;
}


Format Renderer::getSwapchainFormat() const {
	return impl->swapchainFormat;
}


void Renderer::waitForDeviceIdle() {
	return impl->waitForDeviceIdle();
}


unsigned int RendererImpl::ringBufferAllocate(unsigned int size, unsigned int alignment) {
	assert(alignment != 0);
	assert(isPow2(alignment));

	if (size > ringBufSize) {
		unsigned int newSize = nextPow2(size);
		LOG("WARNING: out of ringbuffer space, reallocating to {} bytes", newSize);
		recreateRingBuffer(newSize);

		assert(ringBufPtr == 0);
	}

	// sub-allocate from persistent coherent buffer
	// round current pointer up to necessary alignment
	const unsigned int add   = alignment - 1;
	const unsigned int mask  = ~add;
	unsigned int alignedPtr  = (ringBufPtr + add) & mask;
	assert(ringBufPtr <= alignedPtr);
	LOG_TODO("ring buffer size should be pow2, se should use add & mask here too");
	unsigned int beginPtr    =  alignedPtr % ringBufSize;

	if (beginPtr + size >= ringBufSize) {
		// we went past the end and have to go back to beginning
		LOG_TODO("add and mask here too");
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

		LOG("WARNING: out of ringbuffer space, reallocating to {} bytes", newSize);
		recreateRingBuffer(newSize);

		assert(ringBufPtr == 0);
		beginPtr   = 0;
		ringBufPtr = size;
	}

	return beginPtr;
}



bool Renderer::isSwapchainDirty() const {
	return impl->swapchainDirty;
}


glslang::TShader::Includer::IncludeResult *RendererBase::handleInclude(Includer &includer, const char *headerName, const char *includerName, size_t inclusionDepth) {
	assert(headerName   != nullptr);
	assert(includerName != nullptr);

	LOG("\"{}\" includes \"{}\" at depth {}", includerName, headerName, inclusionDepth);

	std::string filename(headerName);
	includer.included.insert(filename);

	auto it = shaderSources.find(filename);
	if (it == shaderSources.end()) {
		ShaderSourceData d;
		d.contents  = readFile(filename);
		d.timestamp = getFileTimestamp(filename);

		bool inserted = false;
		std::tie(it, inserted) = shaderSources.emplace(filename, std::move(d));
		// since we just checked it's not there this must succeed
		assert(inserted);
	}

	return new glslang::TShader::Includer::IncludeResult(filename, it->second.contents.data(), it->second.contents.size(), nullptr);
}


void RendererBase::loadShaderCache() {
	std::string cacheFile = spirvCacheDir + "shaderCache.json";
	if (!fileExists(cacheFile)) {
		LOG("Shader cache file {} not found", cacheFile);
		return;
	}
	LOG("Load cache from {}", cacheFile);

	try {
		auto cacheData    = readFile(cacheFile);
		nlohmann::json j  = nlohmann::json::parse(cacheData.begin(), cacheData.end());
		auto version      = j.at("version").get<unsigned int>();
		if (version != shaderVersion) {
			// version mismatch, don't try to continue parsing
			LOG("Cache version mismatch, is {}, expected {}", version, shaderVersion);
			return;
		}

		HashMap<std::string, ShaderSourceCacheData> newShaderSourceData;
		j.at("filedata").get_to(newShaderSourceData);

		HashSet<std::string> changedFiles;
		HashMap<std::string, ShaderSourceData> newShaderSources;
		newShaderSources.reserve(newShaderSourceData.size());

		for (const auto &kvp : newShaderSourceData) {
			const std::string &filename              = kvp.first;
			const ShaderSourceCacheData &fileCacheData = kvp.second;

			if (!fileExists(filename)) {
				LOG("cache mentions file \"{}\" which no longer exists", filename);
				changedFiles.insert(filename);
				continue;
			}

			int64_t timestamp = getFileTimestamp(filename);
			auto contents = readFile(filename);
			if (timestamp > fileCacheData.timestamp || contents.size() != fileCacheData.fileSize) {
				LOG("cache mentions file \"{}\" which has changed", filename);
				changedFiles.insert(filename);
				continue;
			}

			ShaderSourceData fileData;
			fileData.contents  = std::move(contents);
			fileData.timestamp = timestamp;

			newShaderSources.emplace(filename, std::move(fileData));
		}

		HashMap<ShaderCacheKey, ShaderCacheData> newShaderCache;
		j.at("shaderCache").get_to(newShaderCache);

		unsigned int numInvalidated = 0;
		for (auto it = newShaderCache.begin(); it != newShaderCache.end(); ) {
			const ShaderCacheKey  &key  = it->first;
			const ShaderCacheData &data = it->second;
			bool invalidate = false;

			if (changedFiles.find(key.filename) != changedFiles.end()) {
				LOG("Invalidate shader \"{}\" because source file has changed", key);
				invalidate = true;
			}

			for (const std::string &include : data.includes) {
				if (changedFiles.find(include) != changedFiles.end()) {
					LOG("Invalidate shader \"{}\" because it includes file {} which has changed", key, include);
					invalidate = true;
					break;
				}
			}

			std::string spvName = makeSPVCacheName(data.spirvHash, key.stage);
			if (!fileExists(spvName)) {
				LOG("Invalidate shader \"{}\" because it points to SPIR-V file {} which no longer exists", key, spvName);
				invalidate = true;
				break;
			}

			if (invalidate) {
				it = newShaderCache.erase(it);
				cacheModified = true;
			} else {
				it++;
			}
		}

		LOG("Loaded cache containing {} shader variants and {} files", newShaderCache.size(), newShaderSources.size());
		LOG("Invalidated {} cache entries", numInvalidated);

		shaderSources = std::move(newShaderSources);
		shaderCache   = std::move(newShaderCache);
	} catch (std::exception &e) {
		LOG("Error while loading shader cache: \"{}\", cache discarded", e.what());
	} catch (...) {
		LOG("Unknown error while loading shader cache, cache discarded");
	}
}


void RendererBase::saveShaderCache() {
	LOG("Cache contains {} shader variants and {} source files", shaderCache.size(), shaderSources.size());

	if (!cacheModified) {
		LOG("Cache not modified, skipping save");
		return;
	}

	HashMap<std::string, ShaderSourceCacheData> shaderFileData;
	shaderFileData.reserve(shaderSources.size());
	for (const auto &d : shaderSources) {
		ShaderSourceCacheData fileData;
		fileData.timestamp = d.second.timestamp;
		fileData.fileSize  = d.second.contents.size();
		shaderFileData.emplace(d.first, std::move(fileData));
	}

	nlohmann::json j = {
		  { "version" ,    shaderVersion  }
		, { "filedata",    shaderFileData }
		, { "shaderCache", shaderCache    }
	};

	std::string cacheData = j.dump(2);

	std::string cacheFile = spirvCacheDir + "shaderCache.json";
	LOG("Save cache to {}", cacheFile);
	writeFile(cacheFile, cacheData.c_str(), cacheData.size());
}


}	// namespace renderer


namespace std {


size_t hash<renderer::ShaderMacros>::operator()(const renderer::ShaderMacros &macros) const {
	return renderer::RendererBase::hashMacros(macros);
}


}  // namespace std
