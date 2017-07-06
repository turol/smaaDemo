#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H


#include "Renderer.h"
#include "Utils.h"

#include <shaderc/shaderc.hpp>

#include <spirv_cross.hpp>


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

	std::pair<T &, unsigned int> add() {
		// TODO: something better, this breaks with remove()
		unsigned int handle = next;
		next++;
		auto result = resources.emplace(handle, T());
		assert(result.second);
		return std::make_pair(std::ref(result.first->second), handle);
	}

	T &add(unsigned int handle) {
		if (handle >= next) {
			next = handle;
		}
		auto result = resources.emplace(handle, T());
		assert(result.second);
		return result.first->second;
	}

	const T &get(unsigned int handle) const {
		auto it = resources.find(handle);
		assert(it != resources.end());

		return it->second;
	}

	T &get(unsigned int handle) {
		auto it = resources.find(handle);
		assert(it != resources.end());

		return it->second;
	}

	void remove(unsigned int handle) {
		auto it = resources.find(handle);
		assert(it != resources.end());
		resources.erase(it);
	}


	void remove(Handle<T> handle) {
		auto it = resources.find(handle.handle);
		assert(it != resources.end());
		resources.erase(it);
	}


	template <typename F> void removeWith(unsigned int handle, F &&f) {
		auto it = resources.find(handle);
		assert(it != resources.end());
		f(it->second);
		resources.erase(it);
	}

	void clear() {
		resources.clear();
	}

	template <typename F> void clearWith(F &&f) {
		auto it = resources.begin();
		while (it != resources.end()) {
			f(it->second);
			it = resources.erase(it);
		}
	}
};


#ifdef RENDERER_OPENGL

#include "OpenGLRenderer.h"

#elif defined(RENDERER_VULKAN)

#include "VulkanRenderer.h"

#elif defined(RENDERER_NULL)

#include "NullRenderer.h"

#else

#error "No renderer specified"

#endif  // RENDERER


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


struct RendererImpl : public RendererBase {
	SwapchainDesc swapchainDesc;

	shaderc::Compiler compiler;

	bool savePreprocessedShaders;
	unsigned int frameNum;

	unsigned int  ringBufSize;
	unsigned int  ringBufPtr;

	std::unordered_map<std::string, std::vector<char> > shaderSources;

	// debugging
	// TODO: remove when NDEBUG?
	bool inFrame;
	bool inRenderPass;
	bool validPipeline;
	bool pipelineDrawn;
	bool scissorSet;


	std::vector<char> loadSource(const std::string &name);

	unsigned int ringBufferAllocate(unsigned int size);

	RendererImpl(const RendererDesc &desc);

	RendererImpl(const RendererImpl &)            = default;
	RendererImpl(RendererImpl &&)                 = default;

	RendererImpl &operator=(const RendererImpl &) = default;
	RendererImpl &operator=(RendererImpl &&)      = default;

	~RendererImpl();


	RenderTargetHandle   createRenderTarget(const RenderTargetDesc &desc);
	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);
	RenderPassHandle     createRenderPass(const RenderPassDesc &desc);
	PipelineHandle       createPipeline(const PipelineDesc &desc);
	BufferHandle         createBuffer(uint32_t size, const void *contents);
	BufferHandle         createEphemeralBuffer(uint32_t size, const void *contents);
	SamplerHandle        createSampler(const SamplerDesc &desc);
	TextureHandle        createTexture(const TextureDesc &desc);

	DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorLayout *layout);

	TextureHandle        getRenderTargetTexture(RenderTargetHandle handle);

	void deleteBuffer(BufferHandle handle);
	void deleteRenderPass(RenderPassHandle fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);
	void deleteRenderTarget(RenderTargetHandle &fbo);


	void recreateSwapchain(const SwapchainDesc &desc);

	void beginFrame();
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle pass);
	void endRenderPass();

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void bindPipeline(PipelineHandle pipeline);
	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	void bindDescriptorSet(unsigned int index, DescriptorSetLayoutHandle layout, const void *data);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex);
};


#endif  // RENDERERINTERNAL_H
