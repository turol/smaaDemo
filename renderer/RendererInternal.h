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


#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H


#include "Renderer.h"
#include "utils/Utils.h"

#include <shaderc/shaderc.h>


namespace renderer {


template <class T>
class ResourceContainer {
	std::unordered_map<unsigned int, T> resources;
	unsigned int                        next;


public:
	ResourceContainer()
	: next(1)
	{
	}

	ResourceContainer(const ResourceContainer<T> &)            = delete;
	ResourceContainer &operator=(const ResourceContainer<T> &) = delete;

	ResourceContainer(ResourceContainer<T> &&)                 = delete;
	ResourceContainer &operator=(ResourceContainer<T> &&)      = delete;

	~ResourceContainer() {}

	std::pair<T &, Handle<T> > add() {
		unsigned int handle = next;
		next++;
		auto result = resources.emplace(handle, T());
		assert(result.second);
		return std::make_pair(std::ref(result.first->second), Handle<T>(handle));
	}


	const T &get(Handle<T> handle) const {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());

		return it->second;
	}


	T &get(Handle<T> handle) {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());

		return it->second;
	}


	void remove(Handle<T> handle) {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());
		resources.erase(it);
	}


	template <typename F> void removeWith(Handle<T> handle, F &&f) {
		assert(handle.handle != 0);

		auto it = resources.find(handle.handle);
		assert(it != resources.end());
		f(it->second);
		resources.erase(it);
	}


	template <typename F> void clearWith(F &&f) {
		auto it = resources.begin();
		while (it != resources.end()) {
			f(it->second);
			it = resources.erase(it);
		}
	}
};


const char *descriptorTypeName(DescriptorType t);


bool isDepthFormat(Format format);
bool issRGBFormat(Format format);


struct RendererBase {
	SwapchainDesc swapchainDesc;
	SwapchainDesc wantedSwapchain;
	bool          swapchainDirty;
	glm::uvec2    drawableSize;

	uint32_t                                 currentFrameIdx;
	uint32_t                                 lastSyncedFrame;

	unsigned int                             currentRefreshRate;
	unsigned int                             maxRefreshRate;
	RendererFeatures                         features;

	bool skipShaderCache;
	unsigned int frameNum;

	unsigned int   uboAlign;
	unsigned int   ssboAlign;

	unsigned int  ringBufSize;
	unsigned int  ringBufPtr;
	// we have synced with the GPU up to this ringbuffer index
	unsigned int              lastSyncedRingBufPtr;

	std::unordered_map<std::string, std::vector<char> > shaderSources;

	// debugging
	// TODO: remove when NDEBUG?
	bool inFrame;
	bool inRenderPass;
	bool validPipeline;
	bool pipelineDrawn;
	bool scissorSet;

	std::string spirvCacheDir;


	std::vector<char> loadSource(const std::string &name);

	std::vector<uint32_t> compileSpirv(const std::string &name, const ShaderMacros &macros, shaderc_shader_kind kind);

	explicit RendererBase(const RendererDesc &desc)
	: swapchainDesc(desc.swapchain)
	, wantedSwapchain(desc.swapchain)
	, swapchainDirty(true)
	, currentFrameIdx(0)
	, lastSyncedFrame(0)
	, currentRefreshRate(0)
	, maxRefreshRate(0)
	, skipShaderCache(desc.skipShaderCache)
	, frameNum(0)
	, uboAlign(0)
	, ssboAlign(0)
	, ringBufSize(0)
	, ringBufPtr(0)
	, lastSyncedRingBufPtr(0)
	, inFrame(false)
	, inRenderPass(false)
	, validPipeline(false)
	, pipelineDrawn(false)
	, scissorSet(false)
	{
		char *prefPath = SDL_GetPrefPath("", "SMAADemo");
		spirvCacheDir = prefPath;
		SDL_free(prefPath);
	}


	RendererBase(const RendererBase &)            = default;
	RendererBase(RendererBase &&)                 = default;

	RendererBase &operator=(const RendererBase &) = default;
	RendererBase &operator=(RendererBase &&)      = default;

	~RendererBase() {}
};


}		// namespace renderer


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
