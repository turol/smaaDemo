#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H


#include "Renderer.h"
#include "Utils.h"

#include <shaderc/shaderc.hpp>


#ifdef RENDERER_OPENGL

#include <GL/glew.h>


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */);


struct VertexShader {
	GLuint shader;


	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	VertexShader();

	~VertexShader();
};


struct FragmentShader {
	GLuint shader;


	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&) = delete;
	FragmentShader &operator=(FragmentShader &&) = delete;


	FragmentShader();

	~FragmentShader();
};


struct Shader {
	GLuint program;

	Shader() = delete;
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&) = delete;
	Shader &operator=(Shader &&) = delete;

	explicit Shader(GLuint program_);

	~Shader();
};


struct RenderTarget {
	GLuint tex;
	GLuint readFBO;
	unsigned int width, height;


	RenderTarget()
	: tex(0)
	, readFBO(0)
	, width(0)
	, height(0)
	{
	}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other)
	: tex(other.tex)
	, readFBO(other.readFBO)
	, width(other.width)
	, height(other.height)
	{
		other.tex    = 0;
		other.readFBO = 0;
		other.width  = 0;
		other.height = 0;
	}

	RenderTarget &operator=(RenderTarget &&other) {
		if (this == &other) {
			return *this;
		}

		std::swap(tex,    other.tex);
		std::swap(readFBO, other.readFBO);
		std::swap(width,  other.width);
		std::swap(height, other.height);

		return *this;
	};

	~RenderTarget();
};


struct Framebuffer {
	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;


	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other)
	: fbo(other.fbo)
	, colorTex(other.colorTex)
	, depthTex(other.depthTex)
	, width(other.width)
	, height(other.height)
	{
		other.fbo      = 0;
		other.colorTex = 0;
		other.depthTex = 0;
		other.width    = 0;
		other.height   = 0;
	}

	Framebuffer &operator=(Framebuffer &&other) {
		if (this == &other) {
			return *this;
		}

		std::swap(fbo,      other.fbo);
		std::swap(colorTex, other.colorTex);
		std::swap(depthTex, other.depthTex);
		std::swap(width,    other.width);
		std::swap(height,   other.height);

		return *this;
	}


	Framebuffer()
	: fbo(0)
	, colorTex(0)
	, depthTex(0)
	, width(0)
	, height(0)
	{
	}


	~Framebuffer();
};


#elif defined(RENDERER_VULKAN)

// TODO: _WIN32
#define VK_USE_PLATFORM_XCB_KHR 1

#include <vulkan/vulkan.hpp>


struct VertexShader {
	vk::ShaderModule shaderModule;


	VertexShader() {}

	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&)                 = default;
	VertexShader &operator=(VertexShader &&)      = default;

	~VertexShader() {}
};


struct FragmentShader {
	vk::ShaderModule shaderModule;


	FragmentShader() {}

	FragmentShader(const FragmentShader &)            = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&)                 = default;
	FragmentShader &operator=(FragmentShader &&)      = default;

	~FragmentShader() {}
};


struct RenderPass {
	vk::RenderPass renderPass;

	RenderPass() {}

	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&)                 = default;
	RenderPass &operator=(RenderPass &&)      = default;

	~RenderPass() {}
};


struct RenderTarget{
	vk::Image     image;
	// TODO: use one large allocation
	vk::DeviceMemory  mem;
	vk::ImageView imageView;

	RenderTarget() {}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&)                 = default;
	RenderTarget &operator=(RenderTarget &&)      = default;

	~RenderTarget() {}
};


#elif defined(RENDERER_NULL)

#else

#error "No renderer specified"


#endif  // RENDERER


template <class T>
class ResourceContainer {
	std::unordered_map<unsigned int, T> resources;


public:
	ResourceContainer() {}

	ResourceContainer(const ResourceContainer<T> &)            = delete;
	ResourceContainer &operator=(const ResourceContainer<T> &) = delete;

	ResourceContainer(ResourceContainer<T> &&)                 = delete;
	ResourceContainer &operator=(ResourceContainer<T> &&)      = delete;

	~ResourceContainer() {}

	std::pair<T &, unsigned int> add() {
		// TODO: something better, this breaks with remove()
		unsigned int handle = resources.size() + 1;
		auto result = resources.emplace(handle, T());
		assert(result.second);
		return std::make_pair(std::ref(result.first->second), handle);
	}

	T &add(unsigned int handle) {
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


struct RendererImpl {
	SwapchainDesc swapchainDesc;

	shaderc::Compiler compiler;

	bool savePreprocessedShaders;
	unsigned int frameNum;

#ifdef RENDERER_OPENGL

	PipelineDesc  currentPipeline;

	SDL_Window *window;
	SDL_GLContext context;

	GLuint vao;
	bool idxBuf16Bit;

	struct Pipeline : public PipelineDesc {
		GLuint shader;

		Pipeline(const PipelineDesc &desc, GLuint shader_)
		: PipelineDesc(desc)
		, shader(shader_)
		{
		}
	};

	ResourceContainer<Framebuffer> framebuffers;
	std::unordered_map<GLuint, std::unique_ptr<VertexShader> >    vertexShaders;
	std::unordered_map<GLuint, std::unique_ptr<FragmentShader> >  fragmentShaders;
	std::unordered_map<GLuint, std::unique_ptr<Shader> > shaders;
	std::unordered_map<uint32_t, Pipeline>                        pipelines;

	ResourceContainer<RenderTarget> renderTargets;

	std::vector<BufferHandle> ephemeralBuffers;

#endif  // RENDERER_OPENGL


#ifdef RENDERER_NULL

	PipelineDesc  currentPipeline;

	unsigned int numBuffers;
	unsigned int numPipelines;
	unsigned int numSamplers;
	unsigned int numTextures;

#endif   // RENDERER_NULL


#ifdef RENDERER_VULKAN

	SDL_Window *window;
	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::PhysicalDeviceProperties deviceProperties;
	vk::PhysicalDeviceFeatures   deviceFeatures;
	vk::Device                   device;
	vk::SurfaceKHR               surface;
	vk::PhysicalDeviceMemoryProperties memoryProperties;
	uint32_t                           graphicsQueueIndex;
	std::vector<vk::SurfaceFormatKHR>  surfaceFormats;
	vk::SurfaceCapabilitiesKHR         surfaceCapabilities;
	std::vector<vk::PresentModeKHR>    surfacePresentModes;
	vk::SwapchainKHR                   swapchain;
	std::vector<vk::Image>             swapchainImages;
	vk::Queue                          queue;

	vk::CommandPool                    commandPool;

	vk::CommandBuffer                  currentCommandBuffer;

	std::unordered_map<unsigned int, VertexShader>  vertexShaders;
	std::unordered_map<unsigned int, FragmentShader>  fragmentShaders;
	std::unordered_map<unsigned int, RenderPass>      renderPasses;

	ResourceContainer<RenderTarget>  renderTargets;


	vk::DeviceMemory allocateMemory(uint32_t size, uint32_t align, uint32_t typeBits);

#endif   // RENDERER_VULKAN


	std::unordered_map<std::string, std::vector<char> > shaderSources;

	bool inRenderPass;


	std::vector<char> loadSource(const std::string &name);


	RendererImpl(const RendererDesc &desc);

	RendererImpl(const RendererImpl &)            = default;
	RendererImpl(RendererImpl &&)                 = default;

	RendererImpl &operator=(const RendererImpl &) = default;
	RendererImpl &operator=(RendererImpl &&)      = default;

	~RendererImpl();


	RenderTargetHandle   createRenderTarget(const RenderTargetDesc &desc);
	FramebufferHandle    createFramebuffer(const FramebufferDesc &desc);
	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);
	RenderPassHandle     createRenderPass(FramebufferHandle fbo, const RenderPassDesc &desc);
	PipelineHandle       createPipeline(const PipelineDesc &desc);
	BufferHandle         createBuffer(uint32_t size, const void *contents);
	BufferHandle         createEphemeralBuffer(uint32_t size, const void *contents);
	SamplerHandle        createSampler(const SamplerDesc &desc);
	TextureHandle        createTexture(const TextureDesc &desc);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);
	void deleteRenderTarget(RenderTargetHandle &fbo);


	void recreateSwapchain(const SwapchainDesc &desc);

	void beginFrame();
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle pass, FramebufferHandle fbo);
	void endRenderPass();

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void blitFBO(FramebufferHandle src, FramebufferHandle dest);

	void bindFramebuffer(FramebufferHandle fbo);
	void bindPipeline(PipelineHandle pipeline);
	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	// TODO: replace with descriptor set stuff
	void bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler);
	void bindUniformBuffer(unsigned int index, BufferHandle buffer);
	void bindStorageBuffer(unsigned int index, BufferHandle buffer);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
};


#endif  // RENDERERINTERNAL_H
