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


namespace renderer {


BETTER_ENUM(ShaderStage, uint8_t
	, Vertex
	, Fragment
)


template <class T, typename HandleBaseType, bool owned>
class ResourceContainer {
    using HandleType = Handle<T, HandleBaseType>;

	HashMap<unsigned int, T>            resources;
	unsigned int                        next;


public:
	ResourceContainer()
	: next(1)
	{
	}

	ResourceContainer(const ResourceContainer<T> &)            = delete;
	ResourceContainer &operator=(const ResourceContainer<T> &) = delete;

	ResourceContainer(ResourceContainer<T> &&) noexcept            = delete;
	ResourceContainer &operator=(ResourceContainer<T> &&) noexcept = delete;

	~ResourceContainer() {}

	std::pair<T &, HandleType > add() {
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


struct CacheData {
	unsigned int              version;
	uint64_t                  hash;
	std::vector<std::string>  dependencies;


	CacheData()
	: version(0)
	, hash(0)
	{
	}

	CacheData(const CacheData &)                = default;
	CacheData &operator=(const CacheData &)     = default;

	CacheData(CacheData &&) noexcept            = default;
	CacheData &operator=(CacheData &&) noexcept = default;

	~CacheData() {}

	static CacheData parse(const std::vector<char> &cacheStr_);

	std::string serialize() const;
};


struct FrameBase {
	uint32_t                  lastFrameNum;

	FrameBase()
	: lastFrameNum(0)
	{
	}

	FrameBase(const FrameBase &)                = delete;
	FrameBase &operator=(const FrameBase &)     = delete;

	FrameBase(FrameBase &&) noexcept            = default;
	FrameBase &operator=(FrameBase &&) noexcept = default;

	~FrameBase() {}
};


struct RendererBase {
	class Includer final : public glslang::TShader::Includer {
		HashMap<std::string, std::vector<char> > &cache;

	public:

		IncludeResult *includeSystem(const char *headerName, const char *includerName, size_t inclusionDepth) override {
			return includeLocal(headerName, includerName, inclusionDepth);
		}

		IncludeResult *includeLocal(const char *headerName, const char * /* includerName */, size_t /* inclusionDepth */) override {
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

	HashMap<std::string, std::vector<char> >             shaderSources;
	HashMap<std::string, std::vector<char> >             includeCache;

	Includer                                             includer;


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

	bool loadCachedSPV(const std::string &name, const std::string &shaderName, ShaderStage stage, std::vector<uint32_t> &spirv);

	void addCachedSPV(const std::string &shaderName, ShaderStage stage, const std::vector<uint32_t> &spirv);

	std::vector<uint32_t> compileSpirv(const std::string &name, const ShaderMacros &macros, ShaderStage stage);

	explicit RendererBase(const RendererDesc &desc);


	RendererBase(const RendererBase &)            = delete;
	RendererBase(RendererBase &&) noexcept            = delete;

	RendererBase &operator=(const RendererBase &) = delete;
	RendererBase &operator=(RendererBase &&) noexcept = delete;

	~RendererBase();
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
