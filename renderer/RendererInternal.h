/*
Copyright (c) 2015-2024 Alternative Games Ltd / Turo Lamminen

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


#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H


#include <variant>

#include "Renderer.h"

#include "utils/Hash.h"
#include "utils/Utils.h"

#include <spirv-tools/libspirv.h>

#include <glslang/Public/ShaderLang.h>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>


namespace renderer {


enum class ShaderStage : uint8_t {
	  Vertex
	, Fragment
	, Compute
};


template <class T, typename HandleBaseType, bool owned>
class ResourceContainer {
    using HandleType = Handle<T, HandleBaseType>;

	HashMap<unsigned int, T>            resources;
	unsigned int                        next       = 1;


public:
	ResourceContainer() {
	}

	ResourceContainer(const ResourceContainer<T> &)            = delete;
	ResourceContainer &operator=(const ResourceContainer<T> &) = delete;

	ResourceContainer(ResourceContainer<T> &&) noexcept            = delete;
	ResourceContainer &operator=(ResourceContainer<T> &&) noexcept = delete;

	~ResourceContainer() {}

	std::pair<T &, HandleType> add() {
		unsigned int handle = next;
		next++;
		auto result = resources.emplace(handle, T());
		assert(result.second);

#ifdef HANDLE_OWNERSHIP_DEBUG

		return std::make_pair(std::ref(result.first->second), HandleType(handle, !owned));

#else //  HANDLE_OWNERSHIP_DEBUG

		return std::make_pair(std::ref(result.first->second), HandleType(handle));

#endif  // HANDLE_OWNERSHIP_DEBUG
	}


	HandleType add(T &&resource) {
		unsigned int handle = next;
		next++;
		auto result DEBUG_ASSERTED = resources.emplace(handle, std::move(resource));
		assert(result.second);

#ifdef HANDLE_OWNERSHIP_DEBUG

		return HandleType(handle, !owned);

#else  // HANDLE_OWNERSHIP_DEBUG

		return HandleType(handle);

#endif  // HANDLE_OWNERSHIP_DEBUG
	}


	const T &get(HandleType handle) const {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());

		return it->second;
	}


	T &get(HandleType handle) {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());

		return it->second;
	}


	void remove(HandleType &&handle) {
		assert(handle.handle != 0);

#ifdef HANDLE_OWNERSHIP_DEBUG

		assert(handle.owned);
		handle.owned = false;

#endif // HANDLE_OWNERSHIP_DEBUG

		auto it = resources.find(handle.handle);
		assert(it != resources.end());
		resources.erase(it);

		handle.handle = 0;
	}


	template <typename F> void removeWith(HandleType &&handle, F &&f) {
		assert(handle.handle != 0);

#ifdef HANDLE_OWNERSHIP_DEBUG

		assert(handle.owned);
		handle.owned = false;

#endif // HANDLE_OWNERSHIP_DEBUG

		auto it = resources.find(handle.handle);
		assert(it != resources.end());
		f(it->second);
		resources.erase(it);

		handle.handle = 0;
	}


	template <typename F> void clearWith(F &&f) {
		auto it = resources.begin();
		while (it != resources.end()) {
			f(it->second);
			it = resources.erase(it);
		}
	}
};


struct FragmentShader;
struct VertexShader;


using FragmentShaderHandle = Handle<FragmentShader>;
using VertexShaderHandle   = Handle<VertexShader>;


struct ShaderSourceData {
	int64_t            timestamp  = 0;
	std::vector<char>  contents;
};


struct ShaderCacheKey {
	std::string     filename;
	std::string     entryPoint;
	ShaderStage     stage     = ShaderStage::Vertex;
	ShaderLanguage  language  = ShaderLanguage::GLSL;
	spv_target_env  spirvEnvironment  = SPV_ENV_UNIVERSAL_1_0;
	ShaderMacros    macros;

	bool operator==(const ShaderCacheKey &other) const {
		if (stage != other.stage) {
			return false;
		}

		if (language != other.language) {
			return false;
		}

		if (spirvEnvironment != other.spirvEnvironment) {
			return false;
		}

		if (filename != other.filename) {
			return false;
		}

		if (entryPoint != other.entryPoint) {
			return false;
		}

		if (macros != other.macros) {
			return false;
		}

		return true;
	}
};


struct ShaderCacheData {
	uint64_t              spirvHash  = 0;
	// can't store SPIR-V bytecode here because we don't want to
	// unconditionally load it on initial cache load
	HashSet<std::string>  includes;
};


struct FrameBase {
	uint32_t                  lastFrameNum = 0;

	FrameBase() {
	}

	FrameBase(const FrameBase &)                = delete;
	FrameBase &operator=(const FrameBase &)     = delete;

	FrameBase(FrameBase &&) noexcept            = default;
	FrameBase &operator=(FrameBase &&) noexcept = default;

	~FrameBase() {}
};


}  // namespace renderer


namespace std {


	template <> struct hash<renderer::ShaderCacheKey> {
		size_t operator()(const renderer::ShaderCacheKey &key) const {
			size_t h = 0;
			hashCombine(h, key.filename);
			hashCombine(h, key.entryPoint);
			hashCombine(h, magic_enum::enum_integer(key.stage));
			hashCombine(h, magic_enum::enum_integer(key.language));
			hashCombine(h, magic_enum::enum_integer(key.spirvEnvironment));
			hashCombine(h, key.macros);
			return h;
		}
	};


} // namespace std


namespace renderer {


using PipelineHandle = std::variant<std::nullopt_t, GraphicsPipelineHandle, ComputePipelineHandle>;


struct RendererBase {
	struct Includer final : public glslang::TShader::Includer {
		// not owned
		RendererBase         *renderer  = nullptr;
		HashSet<std::string> included;


		IncludeResult *includeSystem(const char *headerName, const char *includerName, size_t inclusionDepth) override {
			return renderer->handleInclude(*this, headerName, includerName, inclusionDepth);
		}

		IncludeResult *includeLocal(const char *headerName, const char *includerName, size_t inclusionDepth) override {
			return renderer->handleInclude(*this, headerName, includerName, inclusionDepth);
		}

		void releaseInclude(IncludeResult *data) override {
			assert(data);
			// no need to delete any of data's contents, they're owned by someone else

			delete data;
		}

		explicit Includer(RendererBase *renderer_)
		: renderer(renderer_)
		{
		}

		Includer(const Includer &)                = delete;
		Includer &operator=(const Includer &)     = delete;

		Includer(Includer &&) noexcept            = delete;
		Includer &operator=(Includer &&) noexcept = delete;

		~Includer() override {}
	};


	SwapchainDesc                                        swapchainDesc;
	SwapchainDesc                                        wantedSwapchain;
	bool                                                 swapchainDirty            = true;

	uint64_t                                             frameTimeoutNanos         = 1000000000ULL;
	uint32_t                                             currentFrameIdx           = 0;
	uint32_t                                             lastSyncedFrame           = 0;

	unsigned int                                         currentRefreshRate        = 0;
	unsigned int                                         maxRefreshRate            = 0;
	RendererFeatures                                     features;
	bool                                                 synchronizationDebugMode  = false;

	spv_target_env                                       spirvEnvironment          = SPV_ENV_UNIVERSAL_1_0;

	bool                                                 skipShaderCache           = false;
	bool                                                 optimizeShaders           = true;
	bool                                                 validateShaders           = false;
	unsigned int                                         frameNum                  = 0;

	unsigned int                                         uboAlign                  = 0;
	unsigned int                                         ssboAlign                 = 0;

	RenderPassHandle                                     currentRenderPass;
	FramebufferHandle                                    currentFramebuffer;
	PipelineHandle                                       currentPipeline           = std::nullopt;

	unsigned int                                         ringBufSize               = 0;
	unsigned int                                         ringBufPtr                = 0;
	// we have synced with the GPU up to this ringbuffer index
	unsigned int                                         lastSyncedRingBufPtr      = 0;

	HashMap<std::string, ShaderSourceData>               shaderSources;
	HashMap<ShaderCacheKey, ShaderCacheData>             shaderCache;
	bool                                                 cacheModified             = false;
	bool                                                 debug                     = false;
	bool                                                 tracing                   = false;

	unsigned int                                         activeDebugGroups         = 0;

#ifndef NDEBUG
	// debugging
	bool                                                 inFrame                   = false;
	bool                                                 inRenderPass              = false;
	bool                                                 pipelineUsed              = false;
	bool                                                 scissorSet                = false;
#endif //  NDEBUG

	std::string                                          spirvCacheDir;


	std::vector<char> loadSource(const std::string &name);

	std::string makeSPVCacheName(uint64_t hash, ShaderStage stage);

	void loadShaderCache();

	void saveShaderCache();

	std::vector<uint32_t> compileSpirv(const std::string &name, const std::string &entryPoint, ShaderLanguage /* shaderLanguage */, const ShaderMacros &macros, ShaderStage stage);

	explicit RendererBase(const RendererDesc &desc);

	glslang::TShader::Includer::IncludeResult *handleInclude(Includer &includer, const char *headerName, const char *includerName, size_t inclusionDepth);


	RendererBase(const RendererBase &)            = delete;
	RendererBase(RendererBase &&) noexcept            = delete;

	RendererBase &operator=(const RendererBase &) = delete;
	RendererBase &operator=(RendererBase &&) noexcept = delete;

	~RendererBase();

	static void to_json(nlohmann::json &j, const ShaderMacros &macros);

	static void from_json(const nlohmann::json &j, ShaderMacros &macros);

	static std::string formatMacros(const ShaderMacros &macros) {
		std::string result;
		bool first = true;

		for (const ShaderMacro &macro : macros.impl) {
			if (!first) {
				result += " ";
			}

			result += macro.key;
			if (!macro.value.empty()) {
				result += "=" + macro.value;
			}

			first = false;
		}

		return result;
	}
};


struct DSIndex {
	uint8_t set      = 0;
	uint8_t binding  = 0;


	bool operator==(const DSIndex &other) const {
		return (set == other.set) && (binding == other.binding);
	}

	bool operator!=(const DSIndex &other) const {
		return (set != other.set) || (binding != other.binding);
	}
};


}  // namespace renderer


namespace std {

	template <> struct hash<renderer::DSIndex> {
		size_t operator()(const renderer::DSIndex &ds) const {
			uint16_t val = (uint16_t(ds.set) << 8) | ds.binding;
			return hash<uint16_t>()(val);
		}
	};

}  // namespace std


#ifdef RENDERER_OPENGL

#include "OpenGLRenderer.h"

#elif defined(RENDERER_VULKAN)

#include "VulkanRenderer.h"

#elif defined(RENDERER_NULL)

#include "NullRenderer.h"

#else

#error "No renderer specified"

#endif  // RENDERER


#endif  // RENDERERINTERNAL_H
