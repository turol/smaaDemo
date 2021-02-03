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


#include "RendererInternal.h"
#include "utils/Utils.h"

#include <algorithm>
#include <sstream>
#include <boost/algorithm/string/split.hpp>

#include <spirv-tools/optimizer.hpp>
#include <SPIRV/SPVRemapper.h>
#include <spirv_cross.hpp>

#ifndef nssv_CONFIG_SELECT_STRING_VIEW
#define nssv_CONFIG_SELECT_STRING_VIEW nssv_STRING_VIEW_NONSTD
#endif  // nssv_CONFIG_SELECT_STRING_VIEW
#include <nonstd/string_view.hpp>

#include <xxhash.h>

#include <glslang/Public/ShaderLang.h>
#include <glslang/StandAlone/ResourceLimits.h>
#include <glslang/SPIRV/SpvTools.h>
#include <SPIRV/GlslangToSpv.h>


using namespace glslang;


namespace renderer {


bool isDepthFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
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

	// TODO: only check enabled attributes
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


class Includer final : public TShader::Includer {
	HashMap<std::string, std::vector<char> > &cache;

public:

	IncludeResult* includeSystem(const char *headerName, const char *includerName, size_t inclusionDepth) override {
		return includeLocal(headerName, includerName, inclusionDepth);
	}

	IncludeResult* includeLocal(const char *headerName, const char * /* includerName */, size_t /* inclusionDepth */) override {
		std::string filename(headerName);

		// HashMap<std::string, std::vector<char> >::iterator it = cache.find(filename);
		auto it = cache.find(filename);
		if (it == cache.end()) {
			auto contents = readFile(headerName);
			bool inserted = false;
			std::tie(it, inserted) = cache.emplace(std::move(filename), std::move(contents));
			// since we just checked it's not there this must succeed
			assert(inserted);
		}

		return new IncludeResult(filename, it->second.data(), it->second.size(), nullptr);
	}

	void releaseInclude(IncludeResult *data) override {
		assert(data);
		// no need to delete any of data's contents, they're owned by someone else

		delete data;
	}

	explicit Includer(HashMap<std::string, std::vector<char> > &cache_)
	: cache(cache_)
	{
	}

	Includer(const Includer &)                = delete;
	Includer &operator=(const Includer &)     = delete;

	Includer(Includer &&) noexcept            = delete;
	Includer &operator=(Includer &&) noexcept = delete;

	~Includer() {}
};


RendererBase::RendererBase(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, wantedSwapchain(desc.swapchain)
, swapchainDirty(true)
, currentFrameIdx(0)
, lastSyncedFrame(0)
, currentRefreshRate(0)
, maxRefreshRate(0)
, skipShaderCache(desc.skipShaderCache || !desc.optimizeShaders)
, optimizeShaders(desc.optimizeShaders)
, validateShaders(desc.validateShaders)
, frameNum(0)
, uboAlign(0)
, ssboAlign(0)
, ringBufSize(0)
, ringBufPtr(0)
, lastSyncedRingBufPtr(0)
#ifndef NDEBUG
, inFrame(false)
, inRenderPass(false)
, validPipeline(false)
, pipelineDrawn(false)
, scissorSet(false)
#endif //  NDEBUG
{
	char *prefPath = SDL_GetPrefPath("", "SMAADemo");
	spirvCacheDir = prefPath;
	SDL_free(prefPath);

	bool success = InitializeProcess();
	if (!success) {
		throw std::runtime_error("glslang initialization failed");
	}
}


RendererBase::~RendererBase() {
	FinalizeProcess();
}


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
const unsigned int shaderVersion = 79;


struct CacheData {
	unsigned int              version;
	uint64_t                  hash;
	std::vector<std::string>  dependencies;


	CacheData()
	: version(0)
	, hash(0)
	{
	}

	CacheData(const CacheData &)            = default;
	CacheData &operator=(const CacheData &) = default;

	CacheData(CacheData &&)                 = default;
	CacheData &operator=(CacheData &&)      = default;

	~CacheData() {}


	static CacheData parse(const std::vector<char> &cacheStr_) {
		std::vector<std::string> split;
		split.reserve(3);
		{
		std::string cacheStr(cacheStr_.begin(), cacheStr_.end());
			boost::algorithm::split(split, cacheStr, [] (char c) -> bool { return c == ','; });
		}

		CacheData cacheData;
		if (split.size() < 2) {
			// not enough components, parse fails
            return cacheData;
		}

		cacheData.version = atoi(split[0].c_str());
		if (cacheData.version != shaderVersion) {
			// version mismatch, don't try to continue parsing
			return cacheData;
		}

		try {
			cacheData.hash = std::stoull(split[1].c_str(), nullptr, 16);
		} catch (...) {
			// parsing fails
			cacheData.version = 0;
			return cacheData;
		}

		if (split.size() >= 3) {
			cacheData.dependencies.insert(cacheData.dependencies.end(), split.begin() + 2, split.end());
		}

		return cacheData;
	}


	std::string serialize() const {
		std::stringstream cacheStr;
		cacheStr << version;

		cacheStr << "," << std::hex << hash;

		for (const auto &f : dependencies) {
			cacheStr << "," << f;
		}

		return cacheStr.str();
	}
};


std::string RendererBase::makeSPVCacheName(uint64_t hash) {
	return fmt::format("{}{:08x}.spv", spirvCacheDir, hash);
}


bool RendererBase::loadCachedSPV(const std::string &name, const std::string &shaderName, std::vector<uint32_t> &spirv) {
	std::string cacheName = spirvCacheDir + shaderName + ".cache";
	if (!fileExists(cacheName)) {
		return false;
	}

	CacheData cacheData = CacheData::parse(readFile(cacheName));
	if (cacheData.version != int(shaderVersion)) {
		LOG("version mismatch, found {} when expected {}", cacheData.version, shaderVersion);
		return false;
	}

	std::string spvName   = makeSPVCacheName(cacheData.hash);
	if (!fileExists(spvName)) {
		return false;
	}

	// check timestamp against source and header files
	int64_t sourceTime = getFileTimestamp(name);
	int64_t cacheTime  = getFileTimestamp(cacheName);

	if (sourceTime > cacheTime) {
		LOG("Shader \"{}\" source is newer than cache, recompiling", spvName);
		return false;
	}

	for (const auto &filename : cacheData.dependencies) {
		int64_t includeTime = getFileTimestamp(filename);
		if (includeTime > cacheTime) {
			LOG("Include \"{}\" is newer than cache, recompiling", filename);
			return false;
		}
	}

	auto temp = readFile(spvName);
	if (temp.size() % 4 != 0) {
		LOG("Shader \"{}\" has incorrect size", spvName);
		return false;
	}

	spirv.resize(temp.size() / 4);
	memcpy(&spirv[0], &temp[0], temp.size());
	LOG("Loaded shader \"{}\" from cache", spvName);

	return true;
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
			LOG("Duplicate UBO binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate UBO binding");
		}

		uint32_t maxOffset = 0;
		LOG("UBO {} ({}, {}) ranges:", ubo.id, idx.set, idx.binding);
		for (auto r : compiler.get_active_buffer_ranges(ubo.id)) {
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
			LOG("Duplicate SSBO binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate SSBO binding");
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
			LOG("Duplicate combined image/sampler binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate combined image/sampler binding");
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
			LOG("Duplicate image binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate image binding");
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
			LOG("Duplicate image sampler binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate sampler binding");
		}
	}
}


std::vector<uint32_t> RendererBase::compileSpirv(const std::string &name, const ShaderMacros &macros, ShaderKind kind_) {
	// check spir-v cache first
	std::string shaderName = name;
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
			shaderName += "_" + s;
		}
	}

	std::vector<uint32_t> spirv;
	if (!skipShaderCache) {
		LOG("Looking for \"{}\" in cache...", shaderName);
		bool found = loadCachedSPV(name, shaderName, spirv);
		if (found) {
			LOG("\"{}\" found in cache", shaderName);

			// TODO: only in debug
			// need to move debug flag to base class
			if (true) {
				checkSPVBindings(spirv);
			}

			return spirv;
		} else {
			LOG("\"{}\" not found in cache", shaderName);
		}
	}

	std::function<bool(const std::vector<uint32_t> &)> validate;
	if (validateShaders) {
		validate =
			[] (const std::vector<uint32_t> &s) {
				spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_2);
				tools.SetMessageConsumer(logSpvMessage);
				return tools.Validate(s);
			}
		;
	} else {
		validate = [] (const std::vector<uint32_t> &) { return true; };
	}

	// TODO: cache includes globally
	HashMap<std::string, std::vector<char> > includeCache;

	{
		auto src = loadSource(name);

		// we need GOOGLE_include_directive to use #include
		// glslang has no interface for enabling it
		{
			// break shader into lines
			std::vector<nonstd::string_view> lines;
			lines.reserve(128);  // picked a value larger than any current shader
			const char *it        = &src[0];
			const char *end       = &src[src.size() - 1] + 1;
			const char *lineStart = it;
			while (it < end) {
				if (*it == '\n') {
					const char *lineEnd = it;
					lines.emplace_back(lineStart, lineEnd - lineStart);
					lineStart = lineEnd + 1;
				}
				it++;
			}

			// find line with #version and add the appropriate #extension after it
			// in theory we should check for other #extension lines but we don't currently use any
			auto interestingLine = lines.begin();
			for ( ; interestingLine != lines.end(); interestingLine++) {
				if (interestingLine->starts_with("#version")) {
					interestingLine++;
					interestingLine = lines.emplace(interestingLine, "#extension GL_GOOGLE_include_directive : enable");
					interestingLine++;
					break;
				}
			}

			// glslang has no interface for predefining macros
			std::vector<std::string> sorted;
			if (!macros.empty()) {
				sorted.reserve(macros.size());
				for (const auto &macro : macros) {
					std::string s = "#define " + macro.first;
					if (!macro.second.empty()) {
						s += " ";
						s += macro.second;
					}
					sorted.emplace_back(std::move(s));
				}

				std::sort(sorted.begin(), sorted.end());
				for (const auto &s : sorted) {
					interestingLine = lines.emplace(interestingLine, s);
					interestingLine++;
				}
			}

			size_t len = lines.size();  // the newlines
			for (const auto &l : lines) {
				len += l.size();
			}

			std::vector<char> newSrc;
			newSrc.reserve(len);
			for (const auto &l : lines) {
				newSrc.insert(newSrc.end(), l.begin(), l.end());
				newSrc.emplace_back('\n');
			}

			std::swap(src, newSrc);
		}

		EShLanguage language;
		switch (kind_) {
		case ShaderKind::Vertex:
			language = EShLangVertex;
			break;

		case ShaderKind::Fragment:
			language = EShLangFragment;
			break;

		default:
			HEDLEY_UNREACHABLE();  // shouldn't happen
			break;

		}

		TShader shader(language);

		char *sourceString   = src.data();
		int sourceLen        = src.size();
		const char *filename = name.c_str();

		shader.setStringsWithLengthsAndNames(&sourceString, &sourceLen, &filename, 1);
		shader.setEnvInput(EShSourceGlsl, language, EShClientVulkan, 450);
		shader.setEnvClient(EShClientVulkan, EShTargetVulkan_1_0);
		shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_0);

		// TODO: move to RendererBase?
		TBuiltInResource resource(DefaultTBuiltInResource);
		Includer includer(includeCache);

		// compile
		bool success = shader.parse(&resource, 450, ECoreProfile, false, false, EShMsgDefault, includer);

		const char *infoLog = shader.getInfoLog();
		if (infoLog != nullptr && infoLog[0] != '\0') {
			LOG("Shader info log:\n\"{}\"", infoLog);
		}

		if (!success) {
			LOG("Failed to compile shader");
			throw std::runtime_error("Failed to compile shader");
		}

		// link
		TProgram program;
		program.addShader(&shader);
		success = program.link(EShMsgDefault);

		infoLog = program.getInfoLog();
		if (infoLog != nullptr && infoLog[0] != '\0') {
			LOG("Program info log:\n\"{}\"", infoLog);
		}

		if (!success) {
			LOG("Failed to link shader");
			throw std::runtime_error("Failed to link shader");
		}

		// convert to SPIR-V
		spv::SpvBuildLogger logger;
		glslang::SpvOptions spvOptions;
		// TODO: only when tracing
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
			LOG("SPIR-V for shader \"{}\" is not valid after compilation", shaderName);
			throw std::runtime_error("Shader validation failed");
		}
	}

	// TODO: only in debug
	// need to move debug flag to base class
	if (true) {
		checkSPVBindings(spirv);
	}

	// SPIR-V optimization
	if (optimizeShaders) {
		// TODO: better target environment selection?
		spvtools::Optimizer opt(SPV_ENV_UNIVERSAL_1_2);

		opt.SetMessageConsumer([] (spv_message_level_t level, const char *source, const spv_position_t &position, const char *message) {
			LOG("{}: {} {}:{}:{} {}", level, source, position.line, position.column, position.index, message);
		});

		// SPIRV-Tools optimizer
		opt.RegisterPerformancePasses();

		std::vector<uint32_t> optimized;
		optimized.reserve(spirv.size());
		bool success = opt.Run(&spirv[0], spirv.size(), &optimized);
		if (!success) {
			throw std::runtime_error("Shader optimization failed");
		}

		if (!validate(optimized)) {
			LOG("SPIR-V for shader \"{}\" is not valid after optimization", shaderName);
			throw std::runtime_error("Shader validation failed");
		}

		// glslang SPV remapper
		{
			spv::spirvbin_t remapper;
			remapper.remap(optimized);

			if (!validate(optimized)) {
				LOG("SPIR-V for shader \"{}\" is not valid after remapping", shaderName);
				throw std::runtime_error("Shader validation failed");
			}
		}

		std::swap(spirv, optimized);
	}

	if (!skipShaderCache) {
		CacheData cacheData;
		cacheData.version = shaderVersion;
		cacheData.hash    = XXH64(spirv.data(), spirv.size() * 4, 0);;
		std::string spvName   = makeSPVCacheName(cacheData.hash);
		LOG("Writing shader \"{}\" to \"{}\"", shaderName, spvName);
		cacheData.dependencies.reserve(includeCache.size());
		for (const auto &p : includeCache) {
			cacheData.dependencies.push_back(p.first);
		}
		std::string cacheStr = cacheData.serialize();

		std::string cacheName = spirvCacheDir + shaderName + ".cache";
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


unsigned int Renderer::getCurrentRefreshRate() const {
	return impl->currentRefreshRate;
}


unsigned int Renderer::getMaxRefreshRate() const {
	return impl->maxRefreshRate;
}


const RendererFeatures &Renderer::getFeatures() const {
	return impl->features;
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


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	return impl->createTexture(desc);
}


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	return impl->createDescriptorSetLayout(layout);
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


void Renderer::deletePipeline(PipelineHandle handle) {
	impl->deletePipeline(handle);
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


bool Renderer::waitForDeviceIdle() {
	return impl->waitForDeviceIdle();
}


bool Renderer::beginFrame() {
	return  impl->beginFrame();
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


void Renderer::blit(RenderTargetHandle source, RenderTargetHandle target) {
	impl->blit(source, target);
}


void Renderer::resolveMSAA(RenderTargetHandle source, RenderTargetHandle target) {
	impl->resolveMSAA(source, target);
}


void Renderer::draw(unsigned int firstVertex, unsigned int vertexCount) {
	impl->draw(firstVertex, vertexCount);
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	impl->drawIndexedInstanced(vertexCount, instanceCount);
}


void Renderer::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int minIndex, unsigned int maxIndex) {
	impl->drawIndexedOffset(vertexCount, firstIndex, minIndex, maxIndex);
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


}	// namespace renderer
