#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H


#include <vulkan/vulkan.hpp>

#include <limits.h>  // required but not included by vk_mem_alloc.h

#define VMA_STATS_STRING_ENABLED 0
#include "vk_mem_alloc.h"


struct Buffer {
	vk::Buffer buffer;
	bool                 ringBufferAlloc;
	VmaAllocation        memory;
	uint32_t             size;
	uint32_t             offset;
	// TODO: access type bits (for debugging)


	Buffer()
	: ringBufferAlloc(false)
	, memory(nullptr)
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&)                 = default;
	Buffer &operator=(Buffer &&)      = default;

	~Buffer() {}
};


struct DescriptorSetLayout {
	vk::DescriptorSetLayout layout;
	std::vector<DescriptorLayout>  descriptors;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&)                 = default;
	DescriptorSetLayout &operator=(DescriptorSetLayout &&)      = default;

	~DescriptorSetLayout() {}
};


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


struct Framebuffer {
	vk::Framebuffer  framebuffer;
	FramebufferDesc   desc;
	unsigned int     width, height;
	// TODO: store info about attachments to allow tracking layout


	Framebuffer()
	: width(0)
	, height(0)
	{}

	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&)                 = default;
	Framebuffer &operator=(Framebuffer &&)      = default;

	~Framebuffer() {}
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
	unsigned int  width, height;
	vk::Image     image;
	vk::Format    format;
	vk::ImageView imageView;
	Layout               currentLayout;
	TextureHandle        texture;


	RenderTarget()
	: width(0)
	, height(0)
	, currentLayout(InvalidLayout)
	{}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&)                 = default;
	RenderTarget &operator=(RenderTarget &&)      = default;

	~RenderTarget() {}
};


struct Pipeline {
	vk::Pipeline       pipeline;
	vk::PipelineLayout layout;
	bool               scissor;


	Pipeline()
	: scissor(false)
	{}

	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&)                 = default;
	Pipeline &operator=(Pipeline &&)      = default;

	~Pipeline() {}
};


struct Sampler {
	vk::Sampler sampler;


	Sampler() {}

	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&)                 = default;
	Sampler &operator=(Sampler &&)      = default;

	~Sampler() {}
};


struct Texture {
	unsigned int         width, height;
	vk::Image            image;
	vk::ImageView        imageView;
	VmaAllocation        memory;
	bool                 renderTarget;


	Texture()
	: width(0)
	, height(0)
	, memory(nullptr)
	, renderTarget(false)
	{}


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&)                 = default;
	Texture &operator=(Texture &&)      = default;

	~Texture() {}
};


struct Frame {
	vk::Image          image;
	vk::Fence          fence;
	vk::DescriptorPool dsPool;
	vk::CommandPool    commandPool;
	vk::CommandBuffer  commandBuffer;
	std::vector<BufferHandle> ephemeralBuffers;
	bool                      outstanding;


	Frame() {}

	~Frame() {
		assert(!image);
		assert(!fence);
		assert(!dsPool);
		assert(!commandPool);
		assert(!commandBuffer);
		assert(ephemeralBuffers.empty());
		assert(!outstanding);
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other)
	: image(other.image)
	, fence(other.fence)
	, dsPool(other.dsPool)
	, commandPool(other.commandPool)
	, commandBuffer(other.commandBuffer)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	, outstanding(other.outstanding)
	{
		other.image = vk::Image();
		other.fence = vk::Fence();
		other.dsPool = vk::DescriptorPool();
		other.commandPool = vk::CommandPool();
		other.commandBuffer = vk::CommandBuffer();
		other.outstanding      = false;
	}

	Frame &operator=(Frame &&other) {
		assert(!image);
		image = other.image;
		other.image = vk::Image();

		assert(!fence);
		fence = other.fence;
		other.fence = vk::Fence();

		assert(!dsPool);
		dsPool = other.dsPool;
		other.dsPool = vk::DescriptorPool();

		assert(!commandPool);
		commandPool = other.commandPool;
		other.commandPool = vk::CommandPool();

		assert(!commandBuffer);
		commandBuffer = other.commandBuffer;
		other.commandBuffer = vk::CommandBuffer();

		assert(ephemeralBuffers.empty());
		ephemeralBuffers = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());
	}
};


struct RendererBase {
	SDL_Window *window;
	vk::Instance instance;
	vk::DebugReportCallbackEXT         debugCallback;
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
	vk::Queue                          queue;

	vk::Semaphore                      acquireSem;
	vk::Semaphore                      renderDoneSem;

	vk::CommandBuffer                  currentCommandBuffer;
	vk::PipelineLayout                 currentPipelineLayout;
	vk::Viewport                       currentViewport;

	VmaAllocator                       allocator;

	ResourceContainer<Buffer>              buffers;
	ResourceContainer<DescriptorSetLayout> dsLayouts;
	ResourceContainer<FragmentShader>      fragmentShaders;
	ResourceContainer<Framebuffer>         framebuffers;
	ResourceContainer<Pipeline>            pipelines;
	ResourceContainer<RenderPass>          renderPasses;
	ResourceContainer<Sampler>             samplers;
	ResourceContainer<RenderTarget>  renderTargets;
	ResourceContainer<Texture>             textures;
	ResourceContainer<VertexShader>        vertexShaders;

	vk::Buffer           ringBuffer;
	VmaAllocation        ringBufferMem;
	char                *persistentMapping;

	std::vector<Frame>        frames;
	uint32_t                  currentFrameIdx;


	unsigned int ringBufferAlloc(unsigned int size);

	RendererBase();

	~RendererBase();
};


#endif  // VULKANRENDERER_H
