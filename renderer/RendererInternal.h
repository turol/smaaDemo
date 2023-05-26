/*
Copyright (c) 2015-2022 Alternative Games Ltd / Turo Lamminen

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


#include "Renderer.h"
#include "utils/Utils.h"

#include <glslang/Public/ShaderLang.h>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>


namespace renderer {


enum class ShaderStage : uint8_t {
	  Vertex
	, Fragment
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
	int64_t            timestamp;
	std::vector<char>  contents;
};


struct ShaderCacheKey {
	std::string   name;
	ShaderStage   stage;
	ShaderMacros  macros;

	bool operator==(const ShaderCacheKey &other) const {
		if (stage != other.stage) {
			return false;
		}

		if (name != other.name) {
			return false;
		}

		if (macros != other.macros) {
			return false;
		}

		return true;
	}
};


struct ShaderCacheData {
	uint64_t              spirvHash;
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


	template <> struct hash<renderer::ShaderMacro> {
		size_t operator()(const renderer::ShaderMacro &m) const {
			size_t h = 0;
			h = combineHashes(h, std::hash<std::string>()(m.key));
			h = combineHashes(h, std::hash<std::string>()(m.value));
			return h;
		}
	};


	template <> struct hash<renderer::ShaderMacros> {
		size_t operator()(const renderer::ShaderMacros &macros) const;
	};


	template <> struct hash<renderer::ShaderCacheKey> {
		size_t operator()(const renderer::ShaderCacheKey &key) const {
			size_t h = 0;
			h = combineHashes(h, hash<std::string>()(key.name));
			h = combineHashes(h, hash<int>()(magic_enum::enum_integer(key.stage)));
			h = combineHashes(h, hash<renderer::ShaderMacros>()(key.macros));
			return h;
		}
	};


} // namespace std


namespace renderer {


struct RendererBase {
	struct Includer final : public glslang::TShader::Includer {
		// not owned
		RendererBase         *renderer;
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
	bool                                                 swapchainDirty;
	Format                                               swapchainFormat;

	uint64_t                                             frameTimeoutNanos;
	uint32_t                                             currentFrameIdx;
	uint32_t                                             lastSyncedFrame;

	unsigned int                                         currentRefreshRate;
	unsigned int                                         maxRefreshRate;
	RendererFeatures                                     features;
	bool                                                 synchronizationDebugMode;

	bool                                                 skipShaderCache;
	bool                                                 optimizeShaders;
	bool                                                 validateShaders;
	unsigned int                                         frameNum;

	unsigned int                                         uboAlign;
	unsigned int                                         ssboAlign;

	RenderPassHandle                                     currentRenderPass;
	FramebufferHandle                                    currentFramebuffer;
	bool                                                 renderingToSwapchain;

	unsigned int                                         ringBufSize;
	unsigned int                                         ringBufPtr;
	// we have synced with the GPU up to this ringbuffer index
	unsigned int                                         lastSyncedRingBufPtr;

	HashMap<std::string, ShaderSourceData>               shaderSources;
	HashMap<ShaderCacheKey, ShaderCacheData>             shaderCache;
	bool                                                 cacheModified;


#ifndef NDEBUG
	// debugging
	bool                                                 inFrame;
	bool                                                 inRenderPass;
	bool                                                 validPipeline;
	bool                                                 pipelineDrawn;
	bool                                                 scissorSet;
#endif //  NDEBUG

	std::string                                          spirvCacheDir;


	std::vector<char> loadSource(const std::string &name);

	std::string makeSPVCacheName(uint64_t hash, ShaderStage stage);

	void loadShaderCache();

	void saveShaderCache();

	std::vector<uint32_t> compileSpirv(const std::string &name, const ShaderMacros &macros, ShaderStage stage);

	explicit RendererBase(const RendererDesc &desc);

	glslang::TShader::Includer::IncludeResult *handleInclude(Includer &includer, const char *headerName, const char *includerName, size_t inclusionDepth);


	RendererBase(const RendererBase &)            = delete;
	RendererBase(RendererBase &&) noexcept            = delete;

	RendererBase &operator=(const RendererBase &) = delete;
	RendererBase &operator=(RendererBase &&) noexcept = delete;

	~RendererBase();

	static size_t hashMacros(const ShaderMacros &macros) {
		return hashRange(macros.impl.begin(), macros.impl.end());
	}

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
	uint8_t set;
	uint8_t binding;


	bool operator==(const DSIndex &other) const {
		return (set == other.set) && (binding == other.binding);
	}

	bool operator!=(const DSIndex &other) const {
		return (set != other.set) || (binding != other.binding);
	}
};


}		// namespace renderer


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
