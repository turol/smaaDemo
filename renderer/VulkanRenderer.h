/*
Copyright (c) 2015-2018 Alternative Games Ltd / Turo Lamminen

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

#if VK_HEADER_VERSION < 64
#error "Vulkan header too old"
#endif  // VK_HEADER_VERSION < 64

#include <limits.h>  // required but not included by vk_mem_alloc.h

#include "vk_mem_alloc.h"


#if defined _MSC_VER && !defined __x86_64
#define VK_HASH(x) std::hash<uint64_t>()(static_cast<uint64_t>(x))
#else
#define VK_HASH(x) std::hash<uint64_t>()(reinterpret_cast<uint64_t>(x))
#endif


namespace std {

	template <> struct hash<vk::Format> {
		size_t operator()(const vk::Format &f) const {
			return hash<uint32_t>()(static_cast<uint32_t>(VkFormat(f)));
		}
	};

	template <> struct hash<vk::PresentModeKHR> {
		size_t operator()(const vk::PresentModeKHR &m) const {
			return hash<uint32_t>()(static_cast<uint32_t>(VkPresentModeKHR(m)));
		}
	};

}  // namespace std


namespace renderer {


struct Buffer {
	bool           ringBufferAlloc;
	uint32_t       size;
	uint32_t       offset;
	vk::Buffer     buffer;
	VmaAllocation  memory;
	uint32_t       lastUsedFrame;
	BufferType     type;


	Buffer()
	: ringBufferAlloc(false)
	, size(0)
	, offset(0)
	, memory(nullptr)
	, lastUsedFrame(0)
	, type(BufferType::Invalid)
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other)
	: ringBufferAlloc(other.ringBufferAlloc)
	, size(other.size)
	, offset(other.offset)
	, buffer(other.buffer)
	, memory(other.memory)
	, lastUsedFrame(other.lastUsedFrame)
	, type(other.type)
	{

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = vk::Buffer();
		other.memory          = 0;
		other.lastUsedFrame   = 0;
		other.type            = BufferType::Invalid;
	}

	Buffer &operator=(Buffer &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!buffer);
		assert(!memory);

		ringBufferAlloc       = other.ringBufferAlloc;
		size                  = other.size;
		offset                = other.offset;
		buffer                = other.buffer;
		memory                = other.memory;
		lastUsedFrame         = other.lastUsedFrame;
		type                  = other.type;

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = vk::Buffer();
		other.memory          = 0;
		other.lastUsedFrame   = 0;
		other.type            = BufferType::Invalid;
		assert(type == BufferType::Invalid);

		return *this;
	}

	~Buffer() {
		assert(!ringBufferAlloc);
		assert(size   == 0);
		assert(offset == 0);
		assert(!buffer);
		assert(!memory);
	}

	bool operator==(const Buffer &other) const {
		return this->buffer == other.buffer;
	}

	size_t getHash() const {
		return VK_HASH(VkBuffer(buffer));
	}
};


struct DescriptorSetLayout {
	std::vector<DescriptorLayout>  descriptors;
	vk::DescriptorSetLayout        layout;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other)
	: descriptors(std::move(other.descriptors))
	, layout(other.layout)
	{
		other.layout = vk::DescriptorSetLayout();
		assert(descriptors.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!layout);

		descriptors  = std::move(other.descriptors);
		assert(descriptors.empty());

		layout       = other.layout;
		other.layout = vk::DescriptorSetLayout();

		return *this;
	}

	~DescriptorSetLayout() {
		assert(!layout);
	}
};


struct FragmentShader {
	vk::ShaderModule shaderModule;


	FragmentShader() {}

	FragmentShader(const FragmentShader &)            = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&other)
	: shaderModule(other.shaderModule)
	{
		other.shaderModule = vk::ShaderModule();
	}

	FragmentShader &operator=(FragmentShader &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!shaderModule);
		shaderModule       = other.shaderModule;
		other.shaderModule = vk::ShaderModule();

		return *this;
	}

	~FragmentShader() {
		assert(!shaderModule);
	}
};


struct Framebuffer {
	unsigned int     width, height;
	vk::Framebuffer  framebuffer;
	FramebufferDesc  desc;
	// TODO: store info about attachments to allow tracking layout


	Framebuffer()
	: width(0)
	, height(0)
	{}

	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other)
	: width(other.width)
	, height(other.height)
	, framebuffer(other.framebuffer)
	, desc(other.desc)
	{
		other.width       = 0;
		other.height      = 0;
		other.framebuffer = vk::Framebuffer();
	}

	Framebuffer &operator=(Framebuffer &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!framebuffer);

		width             = other.width;
		height            = other.height;
		framebuffer       = other.framebuffer;
		desc              = other.desc;

		other.width       = 0;
		other.height      = 0;
		other.framebuffer = vk::Framebuffer();

		return *this;
	}

	~Framebuffer() {
		assert(!framebuffer);
	}


	bool operator==(const Framebuffer &other) const {
		return this->framebuffer == other.framebuffer;
	}

	size_t getHash() const {
		return VK_HASH(VkFramebuffer(framebuffer));
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

	Pipeline(Pipeline &&other)
	: pipeline(other.pipeline)
	, layout(other.layout)
	, scissor(other.scissor)
	{
		other.pipeline = vk::Pipeline();
		other.layout   = vk::PipelineLayout();
		other.scissor  = false;
	}

	Pipeline &operator=(Pipeline &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!pipeline);
		assert(!layout);

		pipeline       = other.pipeline;
		layout         = other.layout;
		scissor        = other.scissor;

		other.pipeline = vk::Pipeline();
		other.layout   = vk::PipelineLayout();
		other.scissor  = false;

		return *this;
	}

	~Pipeline() {
		assert(!pipeline);
		assert(!layout);
	}
};


struct RenderPass {
	vk::RenderPass renderPass;
	unsigned int                   clearValueCount;
	std::array<vk::ClearValue, MAX_COLOR_RENDERTARGETS + 1>  clearValues;
	unsigned int                   numSamples;
	unsigned int                   numColorAttachments;
	RenderPassDesc                 desc;


	RenderPass()
	: clearValueCount(0)
	, numSamples(0)
	, numColorAttachments(0)
	{
	}

	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other)
	: renderPass(other.renderPass)
	, clearValueCount(other.clearValueCount)
	, numSamples(other.numSamples)
	, numColorAttachments(other.numColorAttachments)
	, desc(other.desc)
	{
		for (unsigned int i = 0; i < other.clearValueCount; i++) {
			clearValues[i] = other.clearValues[i];
		}

		other.renderPass = vk::RenderPass();
		other.clearValueCount = 0;
		other.numSamples      = 0;
		other.numColorAttachments = 0;
	}

	RenderPass &operator=(RenderPass &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!renderPass);

		renderPass       = other.renderPass;
		clearValueCount  = other.clearValueCount;
		numSamples       = other.numSamples;
		desc             = other.desc;
		numColorAttachments = other.numColorAttachments;

		for (unsigned int i = 0; i < other.clearValueCount; i++) {
			clearValues[i] = other.clearValues[i];
		}

		other.renderPass = vk::RenderPass();
		other.clearValueCount = 0;
		other.numSamples      = 0;
		other.numColorAttachments = 0;

		return *this;
	}

	~RenderPass() {
		assert(!renderPass);
		assert(clearValueCount == 0);
		assert(numSamples == 0);
		assert(numColorAttachments == 0);
	}

	bool operator==(const RenderPass &other) const {
		return this->renderPass == other.renderPass;
	}

	size_t getHash() const {
		return VK_HASH(VkRenderPass(renderPass));
	}
};


struct RenderTarget{
	unsigned int  width, height;
	Layout               currentLayout;
	TextureHandle        texture;
	TextureHandle        additionalView;
	vk::Image     image;
	vk::Format    format;
	vk::ImageView imageView;


	RenderTarget()
	: width(0)
	, height(0)
	, currentLayout(Layout::Undefined)
	, format(vk::Format::eUndefined)
	{}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other)
	: width(other.width)
	, height(other.height)
	, currentLayout(other.currentLayout)
	, texture(other.texture)
	, additionalView(other.additionalView)
	, image(other.image)
	, format(other.format)
	, imageView(other.imageView)
	{
		other.width         = 0;
		other.height        = 0;
		other.currentLayout = Layout::Undefined;
		other.texture       = TextureHandle();
		other.additionalView = TextureHandle();
		other.image         = vk::Image();
		other.format        = vk::Format::eUndefined;
		other.imageView     = vk::ImageView();
	}

	RenderTarget &operator=(RenderTarget &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!image);
		assert(!imageView);

		width               = other.width;
		height              = other.height;
		currentLayout       = other.currentLayout;
		texture             = other.texture;
		additionalView      = other.additionalView;
		image               = other.image;
		format              = other.format;
		imageView           = other.imageView;

		other.width         = 0;
		other.height        = 0;
		other.currentLayout = Layout::Undefined;
		other.texture       = TextureHandle();
		other.additionalView = TextureHandle();
		other.image         = vk::Image();
		other.format        = vk::Format::eUndefined;
		other.imageView     = vk::ImageView();

		return *this;
	}

	~RenderTarget() {
		assert(!image);
		assert(!imageView);
	}


	bool operator==(const RenderTarget &other) const {
		return this->image == other.image;
	}

	size_t getHash() const {
		return VK_HASH(VkImage(image));
	}
};


struct Sampler {
	vk::Sampler sampler;


	Sampler()
	{
	}

	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other)
	: sampler(other.sampler)
	{
		other.sampler = vk::Sampler();
	}

	Sampler &operator=(Sampler &&other) {
		if (this == &other) {
			return *this;
		}

		sampler       = other.sampler;

		other.sampler = vk::Sampler();

		return *this;
	}

	~Sampler() {
		assert(!sampler);
	}

	bool operator==(const Sampler &other) const {
		return this->sampler == other.sampler;
	}

	size_t getHash() const {
		return VK_HASH(VkSampler(sampler));
	}
};


struct Texture {
	unsigned int         width, height;
	bool                 renderTarget;
	vk::Image            image;
	vk::ImageView        imageView;
	VmaAllocation        memory;


	Texture()
	: width(0)
	, height(0)
	, renderTarget(false)
	, memory(nullptr)
	{
	}


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other)
	: width(other.width)
	, height(other.height)
	, renderTarget(other.renderTarget)
	, image(other.image)
	, imageView(other.imageView)
	, memory(other.memory)
	{
		other.width        = 0;
		other.height       = 0;
		other.image        = vk::Image();
		other.imageView    = vk::ImageView();
		other.memory       = 0;
		other.renderTarget = false;
	}

	Texture &operator=(Texture &&other) {
		if (this == &other) {
			return *this;
		}

		width              = other.width;
		height             = other.height;
		renderTarget       = other.renderTarget;
		image              = other.image;
		imageView          = other.imageView;
		memory             = other.memory;

		other.width        = 0;
		other.height       = 0;
		other.renderTarget = false;
		other.image        = vk::Image();
		other.imageView    = vk::ImageView();
		other.memory       = 0;

		return *this;
	}

	~Texture() {
		assert(!image);
		assert(!imageView);
	}

	bool operator==(const Texture &other) const {
		return this->image == other.image;
	}

	size_t getHash() const {
		return VK_HASH(VkImage(image));
	}
};


struct VertexShader {
	vk::ShaderModule shaderModule;


	VertexShader()
	{
	}

	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&other)
	: shaderModule(other.shaderModule)
	{
		other.shaderModule = vk::ShaderModule();
	}

	VertexShader &operator=(VertexShader &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!shaderModule);

		shaderModule       = other.shaderModule;
		other.shaderModule = vk::ShaderModule();

		return *this;
	}

	~VertexShader() {
		assert(!shaderModule);
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


}	// namespace renderer


namespace std {

	template <> struct hash<renderer::Resource> {
		size_t operator()(const renderer::Resource &r) const {
			return boost::apply_visitor(renderer::ResourceHasher(), r);
		}
	};

}  // namespace std


namespace renderer {


struct UploadOp {
	vk::CommandBuffer       cmdBuf;
	vk::Semaphore           semaphore;
	vk::PipelineStageFlags  semWaitMask;
	vk::Buffer              stagingBuffer;
	VmaAllocation           memory;
	VmaAllocationInfo       allocationInfo;


	UploadOp() noexcept
	: memory(VK_NULL_HANDLE)
	{
		allocationInfo = {};
	}

	~UploadOp() noexcept {
		assert(!cmdBuf);
		assert(!semaphore);
		assert(!semWaitMask);
		assert(!stagingBuffer);
		assert(!memory);
	}


	UploadOp(const UploadOp &)            = delete;
	UploadOp &operator=(const UploadOp &) = delete;


	UploadOp(UploadOp &&other) noexcept
	: cmdBuf(other.cmdBuf)
	, semaphore(other.semaphore)
	, semWaitMask(other.semWaitMask)
	, stagingBuffer(other.stagingBuffer)
	, memory(other.memory)
	, allocationInfo(other.allocationInfo)
	{
		other.cmdBuf        = vk::CommandBuffer();
		other.semaphore     = vk::Semaphore();
		other.semWaitMask   = vk::PipelineStageFlags();
		other.stagingBuffer = vk::Buffer();
		other.memory        = VK_NULL_HANDLE;
	}


	UploadOp &operator=(UploadOp &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!cmdBuf);
		assert(!semaphore);
		assert(!semWaitMask);
		assert(!stagingBuffer);
		assert(!memory);

		cmdBuf              = other.cmdBuf;
		other.cmdBuf        = vk::CommandBuffer();

		semaphore           = other.semaphore;
		other.semaphore     = vk::Semaphore();

		semWaitMask         = other.semWaitMask;
		other.semWaitMask   = vk::PipelineStageFlags();

		stagingBuffer       = other.stagingBuffer;
		other.stagingBuffer = vk::Buffer();

		memory              = other.memory;
		other.memory        = VK_NULL_HANDLE;

		allocationInfo      = other.allocationInfo;

		return *this;
	}
};


struct Frame {
	bool                          outstanding;
	uint32_t                      lastFrameNum;
	unsigned int                  usedRingBufPtr;
	std::vector<BufferHandle>     ephemeralBuffers;
	vk::Fence                     fence;
	vk::Image                     image;
	vk::DescriptorPool            dsPool;
	vk::CommandPool               commandPool;
	vk::CommandBuffer             commandBuffer;
	vk::CommandBuffer             presentCmdBuf;

	// std::vector has some kind of issue with variant with non-copyable types, so use unordered_set
	std::unordered_set<Resource>  deleteResources;
	std::vector<UploadOp>         uploads;


	Frame()
	: outstanding(false)
	, lastFrameNum(0)
	, usedRingBufPtr(0)
	{}

	~Frame() {
		assert(ephemeralBuffers.empty());
		assert(!fence);
		assert(!image);
		assert(!dsPool);
		assert(!commandPool);
		assert(!commandBuffer);
		assert(!presentCmdBuf);
		assert(!outstanding);
		assert(deleteResources.empty());
		assert(uploads.empty());
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other)
	: outstanding(other.outstanding)
	, lastFrameNum(other.lastFrameNum)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	, fence(other.fence)
	, image(other.image)
	, dsPool(other.dsPool)
	, commandPool(other.commandPool)
	, commandBuffer(other.commandBuffer)
	, presentCmdBuf(other.presentCmdBuf)
	, deleteResources(std::move(other.deleteResources))
	, uploads(std::move(other.uploads))
	{
		other.image = vk::Image();
		other.fence = vk::Fence();
		other.dsPool = vk::DescriptorPool();
		other.commandPool = vk::CommandPool();
		other.commandBuffer = vk::CommandBuffer();
		other.presentCmdBuf = vk::CommandBuffer();
		other.outstanding      = false;
		other.lastFrameNum     = 0;
		other.usedRingBufPtr   = 0;
		assert(other.deleteResources.empty());
		assert(other.uploads.empty());
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

		assert(!presentCmdBuf);
		presentCmdBuf       = other.presentCmdBuf;
		other.presentCmdBuf = vk::CommandBuffer();

		assert(ephemeralBuffers.empty());
		ephemeralBuffers = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());

		outstanding = other.outstanding;
		other.outstanding = false;

		lastFrameNum = other.lastFrameNum;
		other.lastFrameNum = 0;

		usedRingBufPtr       = other.usedRingBufPtr;
		other.usedRingBufPtr = 0;

		deleteResources = std::move(other.deleteResources);
		assert(other.deleteResources.empty());

		assert(uploads.empty());
		uploads = std::move(other.uploads);
		assert(other.uploads.empty());

		return *this;
	}
};


struct RendererImpl : public RendererBase {
	SDL_Window                              *window;

	std::vector<Frame>                      frames;

	ResourceContainer<Buffer>               buffers;
	ResourceContainer<DescriptorSetLayout>  dsLayouts;
	ResourceContainer<FragmentShader>       fragmentShaders;
	ResourceContainer<Framebuffer>          framebuffers;
	ResourceContainer<Pipeline>             pipelines;
	ResourceContainer<RenderPass>           renderPasses;
	ResourceContainer<RenderTarget>         renderTargets;
	ResourceContainer<Sampler>              samplers;
	ResourceContainer<Texture>              textures;
	ResourceContainer<VertexShader>         vertexShaders;

	vk::Instance                            instance;
	vk::DebugReportCallbackEXT              debugCallback;
	vk::PhysicalDevice                      physicalDevice;
	vk::PhysicalDeviceProperties            deviceProperties;
	vk::PhysicalDeviceFeatures              deviceFeatures;
	vk::Device                              device;
	vk::SurfaceKHR                          surface;
	vk::PhysicalDeviceMemoryProperties      memoryProperties;
	uint32_t                                graphicsQueueIndex;
	uint32_t                                transferQueueIndex;
	std::unordered_set<vk::Format>          surfaceFormats;
	vk::SurfaceCapabilitiesKHR              surfaceCapabilities;
	std::unordered_set<vk::PresentModeKHR>  surfacePresentModes;
	vk::SwapchainKHR                        swapchain;
	vk::PipelineCache                       pipelineCache;
	vk::Queue                               queue;
	vk::Queue                               transferQueue;

	vk::Semaphore                           acquireSem;
	vk::Semaphore                           renderDoneSem;

	vk::CommandBuffer                       currentCommandBuffer;
	vk::PipelineLayout                      currentPipelineLayout;
	vk::Viewport                            currentViewport;
	RenderPassHandle                        currentRenderPass;
	FramebufferHandle                       currentFramebuffer;

	VmaAllocator                            allocator;

	vk::CommandPool                         transferCmdPool;
	std::vector<UploadOp>                   uploads;
	unsigned int                            numUploads;

	bool                                    amdShaderInfo;
	bool                                    debugMarkers;

	vk::Buffer                              ringBuffer;
	VmaAllocation                           ringBufferMem;
	char                                    *persistentMapping;

	// std::vector has some kind of issue with variant with non-copyable types, so use unordered_set
	std::unordered_set<Resource>            deleteResources;


	unsigned int bufferAlignment(BufferType type);

	void recreateSwapchain();
	void recreateRingBuffer(unsigned int newSize);
	unsigned int ringBufferAllocate(unsigned int size, unsigned int alignPower);

	void waitForFrame(unsigned int frameIdx);

	UploadOp allocateUploadOp(uint32_t size);
	void submitUploadOp(UploadOp &&op);

	void deleteBufferInternal(Buffer &b);
	void deleteFramebufferInternal(Framebuffer &fb);
	void deleteRenderPassInternal(RenderPass &rp);
	void deleteRenderTargetInternal(RenderTarget &rt);
	void deleteResourceInternal(Resource &r);
	void deleteSamplerInternal(Sampler &s);
	void deleteTextureInternal(Texture &tex);
	void deleteFrameInternal(Frame &f);

	explicit RendererImpl(const RendererDesc &desc);

	~RendererImpl();


	struct ResourceDeleter final : public boost::static_visitor<> {
		RendererImpl *r;


		ResourceDeleter(RendererImpl *r_)
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


	bool isRenderTargetFormatSupported(Format format) const;

	RenderTargetHandle   createRenderTarget(const RenderTargetDesc &desc);
	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);
	FramebufferHandle    createFramebuffer(const FramebufferDesc &desc);
	RenderPassHandle     createRenderPass(const RenderPassDesc &desc);
	PipelineHandle       createPipeline(const PipelineDesc &desc);
	BufferHandle         createBuffer(BufferType type, uint32_t size, const void *contents);
	BufferHandle         createEphemeralBuffer(BufferType type, uint32_t size, const void *contents);
	SamplerHandle        createSampler(const SamplerDesc &desc);
	TextureHandle        createTexture(const TextureDesc &desc);

	DSLayoutHandle       createDescriptorSetLayout(const DescriptorLayout *layout);

	TextureHandle        getRenderTargetTexture(RenderTargetHandle handle);
	TextureHandle        getRenderTargetView(RenderTargetHandle handle, Format f);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deleteRenderPass(RenderPassHandle fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);
	void deleteRenderTarget(RenderTargetHandle &fbo);


	void setSwapchainDesc(const SwapchainDesc &desc);
	MemoryStats getMemStats() const;

	void beginFrame();
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle);
	void endRenderPass();

	void layoutTransition(RenderTargetHandle image, Layout src, Layout dest);

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void bindPipeline(PipelineHandle pipeline);
	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	void bindDescriptorSet(unsigned int index, DSLayoutHandle layout, const void *data);

	void resolveMSAA(FramebufferHandle source, FramebufferHandle target, unsigned int n);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex);
};


}	// namespace renderer


#endif  // VULKANRENDERER_H
