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


#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H

#define VULKAN_HPP_TYPESAFE_CONVERSION 1


#include <unordered_set>

// TODO: use std::variant if the compiler has C++17
#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>


#include <vulkan/vulkan.hpp>

#include <limits.h>  // required but not included by vk_mem_alloc.h

#include "vk_mem_alloc.h"


struct Buffer {
	vk::Buffer     buffer;
	bool           ringBufferAlloc;
	VmaAllocation  memory;
	uint32_t       size;
	uint32_t       offset;
	uint32_t       lastUsedFrame;
	// TODO: access type bits (for debugging)


	Buffer()
	: ringBufferAlloc(false)
	, memory(nullptr)
	, size(0)
	, offset(0)
	, lastUsedFrame(0)
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other)
	: buffer(other.buffer)
	, ringBufferAlloc(other.ringBufferAlloc)
	, memory(other.memory)
	, size(other.size)
	, offset(other.offset)
	, lastUsedFrame(other.lastUsedFrame)
	{

		other.buffer          = vk::Buffer();
		other.ringBufferAlloc = false;
		other.memory          = 0;
		other.size            = 0;
		other.offset          = 0;
		other.lastUsedFrame   = 0;
	}

	Buffer &operator=(Buffer &&other) {
		buffer                = other.buffer;
		ringBufferAlloc       = other.ringBufferAlloc;
		memory                = other.memory;
		size                  = other.size;
		offset                = other.offset;
		lastUsedFrame         = other.lastUsedFrame;

		other.buffer          = vk::Buffer();
		other.ringBufferAlloc = false;
		other.memory          = 0;
		other.size            = 0;
		other.offset          = 0;
		other.lastUsedFrame   = 0;

		return *this;
	}

	~Buffer() {
		assert(!buffer);
		assert(!ringBufferAlloc);
		assert(!memory);
		assert(size   == 0);
		assert(offset == 0);
	}

	bool operator==(const Buffer &other) const {
		return this->buffer == other.buffer;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkBuffer(buffer)));
	}
};


struct DescriptorSetLayout {
	vk::DescriptorSetLayout        layout;
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


	bool operator==(const Framebuffer &other) const {
		return this->framebuffer == other.framebuffer;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkFramebuffer(framebuffer)));
	}
};


struct RenderPass {
	vk::RenderPass renderPass;


	RenderPass() {}

	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&)                 = default;
	RenderPass &operator=(RenderPass &&)      = default;

	~RenderPass() {}

	bool operator==(const RenderPass &other) const {
		return this->renderPass == other.renderPass;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkRenderPass(renderPass)));
	}
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
	, currentLayout(Layout::Invalid)
	{}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&)                 = default;
	RenderTarget &operator=(RenderTarget &&)      = default;

	~RenderTarget() {}


	bool operator==(const RenderTarget &other) const {
		return this->image == other.image;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkImage(image)));
	}
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

	bool operator==(const Sampler &other) const {
		return this->sampler == other.sampler;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkSampler(sampler)));
	}
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

	bool operator==(const Texture &other) const {
		return this->image == other.image;
	}

	size_t getHash() const {
		return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(VkImage(image)));
	}
};


typedef boost::variant<Buffer, Framebuffer, RenderPass, RenderTarget, Sampler, Texture> Resource;


struct ResourceHasher final : public boost::static_visitor<size_t> {
	size_t operator()(const Buffer &b) const {
		return b.getHash();
	}

	size_t operator()(const Framebuffer &fb) const {
		return fb.getHash();
	}

	size_t operator()(const RenderPass &rp) const {
		return rp.getHash();
	}

	size_t operator()(const RenderTarget &rt) const {
		return rt.getHash();
	}

	size_t operator()(const Sampler &s) const {
		return s.getHash();
	}

	size_t operator()(const Texture &t) const {
		return t.getHash();
	}
};


namespace std {

	template <> struct hash<Resource> {
		size_t operator()(const Resource &r) const {
			return boost::apply_visitor(ResourceHasher(), r);
		}
	};

}  // namespace std


struct Frame {
	vk::Image          image;
	vk::Fence          fence;
	vk::DescriptorPool dsPool;
	vk::CommandPool    commandPool;
	vk::CommandBuffer  commandBuffer;
	std::vector<BufferHandle> ephemeralBuffers;
	bool                      outstanding;
	uint32_t                  lastFrameNum;

	// std::vector has some kind of issue with variant with non-copyable types, so use unordered_set
	std::unordered_set<Resource>     deleteResources;


	Frame()
	: outstanding(false)
	, lastFrameNum(0)
	{}

	~Frame() {
		assert(!image);
		assert(!fence);
		assert(!dsPool);
		assert(!commandPool);
		assert(!commandBuffer);
		assert(ephemeralBuffers.empty());
		assert(!outstanding);
		assert(deleteResources.empty());
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
	, lastFrameNum(other.lastFrameNum)
	, deleteResources(std::move(other.deleteResources))
	{
		other.image = vk::Image();
		other.fence = vk::Fence();
		other.dsPool = vk::DescriptorPool();
		other.commandPool = vk::CommandPool();
		other.commandBuffer = vk::CommandBuffer();
		other.outstanding      = false;
		other.lastFrameNum     = 0;
		assert(other.deleteResources.empty());
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

		outstanding = other.outstanding;
		other.outstanding = false;

		lastFrameNum = other.lastFrameNum;
		other.lastFrameNum = 0;

		deleteResources = std::move(other.deleteResources);
		assert(other.deleteResources.empty());

		return *this;
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
	uint32_t                  lastSyncedFrame;

	// std::vector has some kind of issue with variant with non-copyable types, so use unordered_set
	std::unordered_set<Resource>     deleteResources;


	unsigned int ringBufferAlloc(unsigned int size);

	void waitForFrame(unsigned int frameIdx);

	void deleteBufferInternal(Buffer &b);
	void deleteFramebufferInternal(Framebuffer &fb);
	void deleteRenderPassInternal(RenderPass &rp);
	void deleteRenderTargetInternal(RenderTarget &rt);
	void deleteResourceInternal(Resource &r);
	void deleteSamplerInternal(Sampler &s);
	void deleteTextureInternal(Texture &tex);
	void deleteFrameInternal(Frame &f);

	RendererBase();

	~RendererBase();


	struct ResourceDeleter final : public boost::static_visitor<> {
		RendererBase *r;


		ResourceDeleter(RendererBase *r_)
		: r(r_)
		{
		}

		ResourceDeleter(const ResourceDeleter &)            = default;
		ResourceDeleter(ResourceDeleter &&)                 = default;

		ResourceDeleter &operator=(const ResourceDeleter &) = default;
		ResourceDeleter &operator=(ResourceDeleter &&)      = default;

		~ResourceDeleter() {}

		void operator()(Buffer &b) const {
			r->deleteBufferInternal(b);
		}

		void operator()(Framebuffer &fb) const {
			r->deleteFramebufferInternal(fb);
		}

		void operator()(RenderPass &rp) const {
			r->deleteRenderPassInternal(rp);
		}

		void operator()(RenderTarget &rt) const {
			r->deleteRenderTargetInternal(rt);
		}

		void operator()(Sampler &s) const {
			r->deleteSamplerInternal(s);
		}

		void operator()(Texture &t) const {
			r->deleteTextureInternal(t);
		}

	};

};


#endif  // VULKANRENDERER_H
