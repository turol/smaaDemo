#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H


#ifndef SDL_VIDEO_VULKAN_SURFACE

// TODO: _WIN32
#define VK_USE_PLATFORM_XCB_KHR 1

#endif  // SDL_VIDEO_VULKAN_SURFACE


#include <vulkan/vulkan.hpp>

#include <limits.h>  // required but not included by vk_mem_alloc.h
#include "vk_mem_alloc.h"


struct Buffer {
	vk::Buffer buffer;
	bool                 ringBufferAlloc;
	VkMappedMemoryRange  memory;
	// TODO: access type bits (for debugging)


	Buffer()
	: ringBufferAlloc(false)
	, memory(vk::MappedMemoryRange())
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&)                 = default;
	Buffer &operator=(Buffer &&)      = default;

	~Buffer() {}
};


struct DescriptorSetLayout {
	vk::DescriptorSetLayout layout;


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


struct RenderPass {
	vk::RenderPass renderPass;
	vk::Framebuffer  framebuffer;
	RenderPassDesc   desc;
	unsigned int     width, height;
	// TODO: store info about attachments to allow tracking layout


	RenderPass()
	: width(0)
	, height(0)
	{}

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
	VkMappedMemoryRange  memory;
	vk::ImageView imageView;
	Layout               currentLayout;


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


	Pipeline() {}

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


struct RendererBase {
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

	VmaAllocator                       allocator;

	std::unordered_map<unsigned int, VertexShader>  vertexShaders;
	std::unordered_map<unsigned int, FragmentShader>  fragmentShaders;

	ResourceContainer<Buffer>              buffers;
	ResourceContainer<DescriptorSetLayout> dsLayouts;
	ResourceContainer<Pipeline>            pipelines;
	ResourceContainer<RenderPass>          renderPasses;
	ResourceContainer<struct Sampler>      samplers;
	ResourceContainer<RenderTarget>  renderTargets;

	vk::Buffer           ringBuffer;
	VkMappedMemoryRange  ringBufferMem;
	char                *persistentMapping;

	std::vector<BufferHandle> ephemeralBuffers;


	unsigned int ringBufferAlloc(unsigned int size);

	RendererBase();

	~RendererBase();
};


#endif  // VULKANRENDERER_H
