/*
Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

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
#define VULKAN_HPP_NO_SMART_HANDLE     1


#include <variant>


#ifdef _WIN32

// some versions of vulkan.hpp need HMODULE but don't include windows.h

#ifndef NOMINMAX
#define NOMINMAX
#endif  // NOMINMAX

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // WIN32_LEAN_AND_MEAN

#include <windows.h>

#define VMA_USE_STL_SHARED_MUTEX 0

#endif  // _WIN32


// for portability_subset
#define VK_ENABLE_BETA_EXTENSIONS 1

#include <vulkan/vulkan.hpp>


#if VK_HEADER_VERSION < 98
#error "Vulkan header too old"
#endif  // VK_HEADER_VERSION < 98

#include <limits.h>  // required but not included by vk_mem_alloc.h

#include "utils/Hash.h"


// These are the new versions of these macros
// vk_mem_alloc.h requires them but they're not defined in old vulkan.h
#ifndef VK_API_VERSION_MAJOR
#define VK_API_VERSION_MAJOR(version) VK_VERSION_MAJOR(version)
#endif  // VK_API_VERSION_MAJOR

#ifndef VK_API_VERSION_MINOR
#define VK_API_VERSION_MINOR(version) VK_VERSION_MINOR(version)
#endif  // VK_API_VERSION_MINOR

#ifndef VK_API_VERSION_PATCH
#define VK_API_VERSION_PATCH(version) VK_VERSION_PATCH(version)
#endif  // VK_API_VERSION_PATCH


#define VMA_STATIC_VULKAN_FUNCTIONS 0


#include "vk_mem_alloc.h"


#if defined _MSC_VER && !defined _WIN64
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
	bool           ringBufferAlloc  = false;
	uint32_t       size             = 0;
	uint32_t       offset           = 0;
	vk::Buffer     buffer;
	VmaAllocation  memory           = nullptr;
	BufferType     type             = BufferType::Invalid;


	Buffer() noexcept {}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other) noexcept
	: ringBufferAlloc(other.ringBufferAlloc)
	, size(other.size)
	, offset(other.offset)
	, buffer(other.buffer)
	, memory(other.memory)
	, type(other.type)
	{

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = vk::Buffer();
		other.memory          = nullptr;
		other.type            = BufferType::Invalid;
	}

	Buffer &operator=(Buffer &&other) noexcept {
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
		type                  = other.type;

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = vk::Buffer();
		other.memory          = nullptr;
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


	DescriptorSetLayout() noexcept {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other) noexcept
	: descriptors(std::move(other.descriptors))
	, layout(other.layout)
	{
		other.layout = vk::DescriptorSetLayout();
		assert(other.descriptors.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!layout);

		descriptors  = std::move(other.descriptors);
		assert(other.descriptors.empty());

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


	FragmentShader() noexcept {}

	FragmentShader(const FragmentShader &)            = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&other) noexcept
	: shaderModule(other.shaderModule)
	{
		other.shaderModule = vk::ShaderModule();
	}

	FragmentShader &operator=(FragmentShader &&other) noexcept {
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
	struct RTInfo
	{
		Format  format         = Format::Invalid;
		Layout  initialLayout  = Layout::Undefined;
		Layout  finalLayout    = Layout::Undefined;
	};

	vk::Framebuffer                              framebuffer;
	unsigned int                                 width         = 0;
	unsigned int                                 height        = 0;
	unsigned int                                 numSamples    = 0;
	FramebufferDesc                              desc;
	std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorTargets;
	// depthStencilTargets layouts are not used, always Undefined
	// TODO: fix that (needs more things in Layout)
	RTInfo                                       depthStencilTarget;


	Framebuffer() noexcept
	{
	}

	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other) noexcept
	: framebuffer(other.framebuffer)
	, width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, desc(other.desc)
	{
		other.framebuffer = vk::Framebuffer();
		other.width       = 0;
		other.height      = 0;
		other.numSamples  = 0;
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorTargets[i].format              = other.colorTargets[i].format;
			other.colorTargets[i].format        = Format::Invalid;

			colorTargets[i].initialLayout       = other.colorTargets[i].initialLayout;
			other.colorTargets[i].initialLayout = Layout::Undefined;

			colorTargets[i].finalLayout         = other.colorTargets[i].finalLayout;
			other.colorTargets[i].finalLayout   = Layout::Undefined;
		}
		depthStencilTarget.format               = other.depthStencilTarget.format;
		other.depthStencilTarget.format         = Format::Invalid;

		depthStencilTarget.initialLayout        = other.depthStencilTarget.initialLayout;
		other.depthStencilTarget.initialLayout  = Layout::Undefined;

		depthStencilTarget.finalLayout          = other.depthStencilTarget.finalLayout;
		other.depthStencilTarget.finalLayout    = Layout::Undefined;
	}

	Framebuffer &operator=(Framebuffer &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!framebuffer);

		framebuffer       = other.framebuffer;
		width             = other.width;
		height            = other.height;
		numSamples        = other.numSamples;
		desc              = other.desc;
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorTargets[i].format        = other.colorTargets[i].format;
			colorTargets[i].initialLayout = other.colorTargets[i].initialLayout;
			colorTargets[i].finalLayout   = other.colorTargets[i].finalLayout;
		}
		depthStencilTarget.format         = other.depthStencilTarget.format;
		depthStencilTarget.initialLayout  = other.depthStencilTarget.initialLayout;
		depthStencilTarget.finalLayout    = other.depthStencilTarget.finalLayout;

		other.framebuffer = vk::Framebuffer();
		other.width       = 0;
		other.height      = 0;
		other.numSamples  = 0;
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			other.colorTargets[i].format        = Format::Invalid;
			other.colorTargets[i].initialLayout = Layout::Undefined;
			other.colorTargets[i].finalLayout   = Layout::Undefined;
		}
		other.depthStencilTarget.format         = Format::Invalid;
		other.depthStencilTarget.initialLayout  = Layout::Undefined;
		other.depthStencilTarget.finalLayout    = Layout::Undefined;

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
	bool               scissor  = false;


	Pipeline() noexcept {}

	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&other) noexcept
	: pipeline(other.pipeline)
	, layout(other.layout)
	, scissor(other.scissor)
	{
		other.pipeline = vk::Pipeline();
		other.layout   = vk::PipelineLayout();
		other.scissor  = false;
	}

	Pipeline &operator=(Pipeline &&other) noexcept {
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


	bool operator==(const Pipeline &other) const {
		return this->pipeline == other.pipeline;
	}


	size_t getHash() const {
		return VK_HASH(VkPipeline(pipeline));
	}
};


struct RenderPass {
	vk::RenderPass                                           renderPass;
	unsigned int                                             clearValueCount     = 0;
	std::array<vk::ClearValue, MAX_COLOR_RENDERTARGETS + 1>  clearValues;
	unsigned int                                             numColorAttachments = 0;
	RenderPassDesc                                           desc;


	RenderPass() noexcept
	{
	}

	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other) noexcept
	: renderPass(other.renderPass)
	, clearValueCount(other.clearValueCount)
	, numColorAttachments(other.numColorAttachments)
	, desc(other.desc)
	{
		for (unsigned int i = 0; i < other.clearValueCount; i++) {
			clearValues[i] = other.clearValues[i];
		}

		other.renderPass = vk::RenderPass();
		other.clearValueCount = 0;
		other.numColorAttachments = 0;
	}

	RenderPass &operator=(RenderPass &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!renderPass);

		renderPass       = other.renderPass;
		clearValueCount  = other.clearValueCount;
		desc             = other.desc;
		numColorAttachments = other.numColorAttachments;

		for (unsigned int i = 0; i < other.clearValueCount; i++) {
			clearValues[i] = other.clearValues[i];
		}

		other.renderPass = vk::RenderPass();
		other.clearValueCount = 0;
		other.numColorAttachments = 0;

		return *this;
	}

	~RenderPass() {
		assert(!renderPass);
		assert(clearValueCount == 0);
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
	unsigned int         width          = 0;
	unsigned int         height         = 0;
	unsigned int         numSamples     = 0;
	Layout               currentLayout  = Layout::Undefined;
	TextureHandle        texture;
	TextureHandle        additionalView;
	vk::Image            image;
	Format               format         = Format::Invalid;
	vk::ImageView        imageView;


	RenderTarget() noexcept {}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other) noexcept
	: width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, currentLayout(other.currentLayout)
	, texture(std::move(other.texture))
	, additionalView(std::move(other.additionalView))
	, image(other.image)
	, format(other.format)
	, imageView(other.imageView)
	{
		other.width          = 0;
		other.height         = 0;
		other.numSamples     = 0;
		other.currentLayout  = Layout::Undefined;
		other.image          = vk::Image();
		other.format         = Format::Invalid;
		other.imageView      = vk::ImageView();
	}

	RenderTarget &operator=(RenderTarget &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!image);
		assert(!imageView);

		width                = other.width;
		height               = other.height;
		numSamples           = other.numSamples;
		currentLayout        = other.currentLayout;
		texture              = other.texture;
		additionalView       = other.additionalView;
		image                = other.image;
		format               = other.format;
		imageView            = other.imageView;

		other.width          = 0;
		other.height         = 0;
		other.numSamples     = 0;
		other.currentLayout  = Layout::Undefined;
		other.image          = vk::Image();
		other.format         = Format::Invalid;
		other.imageView      = vk::ImageView();

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


	Sampler() noexcept
	{
	}

	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other) noexcept
	: sampler(other.sampler)
	{
		other.sampler = vk::Sampler();
	}

	Sampler &operator=(Sampler &&other) noexcept {
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
	unsigned int         width        = 0;
	unsigned int         height       = 0;
	bool                 renderTarget = false;
	vk::Image            image;
	vk::ImageView        imageView;
	VmaAllocation        memory       = nullptr;


	Texture() noexcept
	{
	}


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other) noexcept
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
		other.memory       = nullptr;
		other.renderTarget = false;
	}

	Texture &operator=(Texture &&other) noexcept {
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
		other.memory       = nullptr;

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


	VertexShader() noexcept
	{
	}

	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&other) noexcept
	: shaderModule(other.shaderModule)
	{
		other.shaderModule = vk::ShaderModule();
	}

	VertexShader &operator=(VertexShader &&other) noexcept {
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


struct PipelineLayoutKey {
	size_t                                                    layoutCount = 0;
	std::array<vk::DescriptorSetLayout, MAX_DESCRIPTOR_SETS>  layouts;

	void add(vk::DescriptorSetLayout l) {
		assert(layoutCount < MAX_DESCRIPTOR_SETS);

		layouts[layoutCount] = l;
		layoutCount++;
	}
};


using Resource = std::variant<Buffer, Framebuffer, Pipeline, RenderPass, RenderTarget, Sampler, Texture>;


}	// namespace renderer


namespace std {

	template <> struct hash<renderer::Resource> {
		size_t operator()(const renderer::Resource &r) const;
	};

}  // namespace std


namespace renderer {


struct UploadOp {
	vk::CommandBuffer                     cmdBuf;
	vk::Semaphore                         semaphore;
	vk::PipelineStageFlags                semWaitMask;
	vk::Buffer                            stagingBuffer;
	VmaAllocation                         memory                 = nullptr;
	VmaAllocationInfo                     allocationInfo;
	std::vector<vk::ImageMemoryBarrier>   imageAcquireBarriers;
	std::vector<vk::BufferMemoryBarrier>  bufferAcquireBarriers;
	bool                                  coherent               = false;


	UploadOp() noexcept
	{
		allocationInfo = {};
	}

	~UploadOp() noexcept {
		assert(!cmdBuf);
		assert(!semaphore);
		assert(!semWaitMask);
		assert(!stagingBuffer);
		assert(!memory);
		assert(!coherent);
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
	, imageAcquireBarriers(std::move(other.imageAcquireBarriers))
	, bufferAcquireBarriers(std::move(other.bufferAcquireBarriers))
	, coherent(other.coherent)
	{
		other.cmdBuf        = vk::CommandBuffer();
		other.semaphore     = vk::Semaphore();
		other.semWaitMask   = vk::PipelineStageFlags();
		other.stagingBuffer = vk::Buffer();
		other.memory        = nullptr;
		assert(other.imageAcquireBarriers.empty());
		assert(other.bufferAcquireBarriers.empty());
		other.coherent      = false;
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
		assert(imageAcquireBarriers.empty());
		assert(bufferAcquireBarriers.empty());
		assert(!coherent);

		cmdBuf                = other.cmdBuf;
		other.cmdBuf          = vk::CommandBuffer();

		semaphore             = other.semaphore;
		other.semaphore       = vk::Semaphore();

		semWaitMask           = other.semWaitMask;
		other.semWaitMask     = vk::PipelineStageFlags();

		stagingBuffer         = other.stagingBuffer;
		other.stagingBuffer   = vk::Buffer();

		memory                = other.memory;
		other.memory          = nullptr;

		imageAcquireBarriers  = std::move(other.imageAcquireBarriers);
		assert(other.imageAcquireBarriers.empty());

		bufferAcquireBarriers = std::move(other.bufferAcquireBarriers);
		assert(other.bufferAcquireBarriers.empty());

		coherent               = other.coherent;
		other.coherent         = false;

		allocationInfo         = other.allocationInfo;

		return *this;
	}
};


struct Frame : public FrameBase {
	enum class Status : uint8_t {
		  Ready
		, Pending
		, Done
	};

	Status                        status             = Status::Ready;
	unsigned int                  usedRingBufPtr     = 0;
	std::vector<BufferHandle>     ephemeralBuffers;
	vk::Fence                     fence;
	vk::Image                     image;
	vk::ImageView                 imageView;
	FramebufferHandle             framebuffer;
	RenderPassHandle              lastSwapchainRenderPass; // not owned
	vk::DescriptorPool            dsPool;
	vk::CommandPool               commandPool;
	vk::CommandBuffer             commandBuffer;
	vk::CommandBuffer             barrierCmdBuf;
	vk::Semaphore                 acquireSem;
	vk::Semaphore                 renderDoneSem;

	std::vector<Resource>         deleteResources;
	std::vector<UploadOp>         uploads;


	Frame() {}

	~Frame() {
		assert(ephemeralBuffers.empty());
		assert(!fence);
		assert(!image);
		assert(!imageView);
		assert(!framebuffer);
		assert(!lastSwapchainRenderPass);
		assert(!dsPool);
		assert(!commandPool);
		assert(!commandBuffer);
		assert(!barrierCmdBuf);
		assert(!acquireSem);
		assert(!renderDoneSem);
		assert(status == Status::Ready);
		assert(deleteResources.empty());
		assert(uploads.empty());
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other) noexcept
	: FrameBase(std::move(other))
	, status(other.status)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	, fence(other.fence)
	, image(other.image)
	, imageView(other.imageView)
	, framebuffer(std::move(other.framebuffer))
	, lastSwapchainRenderPass(std::move(other.lastSwapchainRenderPass))
	, dsPool(other.dsPool)
	, commandPool(other.commandPool)
	, commandBuffer(other.commandBuffer)
	, barrierCmdBuf(other.barrierCmdBuf)
	, acquireSem(other.acquireSem)
	, renderDoneSem(other.renderDoneSem)
	, deleteResources(std::move(other.deleteResources))
	, uploads(std::move(other.uploads))
	{
		other.image            = vk::Image();
		other.imageView        = vk::ImageView();
		other.lastSwapchainRenderPass.reset();
		other.fence            = vk::Fence();
		other.dsPool           = vk::DescriptorPool();
		other.commandPool      = vk::CommandPool();
		other.commandBuffer    = vk::CommandBuffer();
		other.barrierCmdBuf    = vk::CommandBuffer();
		other.acquireSem       = vk::Semaphore();
		other.renderDoneSem    = vk::Semaphore();
		other.status           = Status::Ready;
		other.lastFrameNum     = 0;
		other.usedRingBufPtr   = 0;
		assert(other.deleteResources.empty());
		assert(other.uploads.empty());
	}

	Frame &operator=(Frame &&other) noexcept
	{
		assert(!image);
		image                = other.image;
		other.image          = vk::Image();

		assert(!imageView);
		imageView            = other.imageView;
		other.imageView      = vk::ImageView();

		assert(!framebuffer);
		framebuffer          = std::move(other.framebuffer);

		assert(!lastSwapchainRenderPass);
		lastSwapchainRenderPass = std::move(other.lastSwapchainRenderPass);
		other.lastSwapchainRenderPass.reset();

		assert(!fence);
		fence                = other.fence;
		other.fence          = vk::Fence();

		assert(!dsPool);
		dsPool               = other.dsPool;
		other.dsPool         = vk::DescriptorPool();

		assert(!commandPool);
		commandPool          = other.commandPool;
		other.commandPool    = vk::CommandPool();

		assert(!commandBuffer);
		commandBuffer        = other.commandBuffer;
		other.commandBuffer  = vk::CommandBuffer();

		assert(!barrierCmdBuf);
		barrierCmdBuf        = other.barrierCmdBuf;
		other.barrierCmdBuf  = vk::CommandBuffer();

		assert(!acquireSem);
		acquireSem           = other.acquireSem;
		other.acquireSem     = vk::Semaphore();

		assert(!renderDoneSem);
		renderDoneSem        = other.renderDoneSem;
		other.renderDoneSem  = vk::Semaphore();

		assert(ephemeralBuffers.empty());
		ephemeralBuffers = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());

		status               = other.status;
		other.status         = Status::Ready;

		lastFrameNum         = other.lastFrameNum;
		other.lastFrameNum   = 0;

		usedRingBufPtr       = other.usedRingBufPtr;
		other.usedRingBufPtr = 0;

		assert(deleteResources.empty());
		deleteResources = std::move(other.deleteResources);
		assert(other.deleteResources.empty());

		assert(uploads.empty());
		uploads = std::move(other.uploads);
		assert(other.uploads.empty());

		return *this;
	}
};


struct RendererImpl : public RendererBase {
	SDL_Window                                              *window;

	vk::Semaphore                                           frameAcquireSem;

	std::vector<Frame>                                      frames;
	RenderTargetHandle                                      builtinDepthRT;

	ResourceContainer<Buffer>                               buffers;
	ResourceContainer<DescriptorSetLayout, uint32_t, true>  dsLayouts;
	ResourceContainer<FragmentShader, uint32_t, true>       fragmentShaders;
	ResourceContainer<Framebuffer>                          framebuffers;
	ResourceContainer<Pipeline>                             pipelines;
	ResourceContainer<RenderPass>                           renderPasses;
	ResourceContainer<RenderTarget>                         renderTargets;
	ResourceContainer<Sampler>                              samplers;
	ResourceContainer<Texture>                              textures;
	ResourceContainer<VertexShader, uint32_t, true>         vertexShaders;

	vk::DispatchLoaderDynamic                               dispatcher;

	vk::Instance                                            instance;
	vk::DebugReportCallbackEXT                              debugReportCallback;
	vk::DebugUtilsMessengerEXT                              debugUtilsCallback;
	unsigned int                                            physicalDeviceIndex     = 0;
	vk::PhysicalDevice                                      physicalDevice;
	vk::PhysicalDeviceProperties                            deviceProperties;
	vk::PhysicalDeviceFeatures                              deviceFeatures;
	bool                                                    pipelineExecutableInfo  = false;
	vk::Device                                              device;
	vk::SurfaceKHR                                          surface;
	vk::PhysicalDeviceMemoryProperties                      memoryProperties;
	uint32_t                                                graphicsQueueIndex      = 0;
	uint32_t                                                transferQueueIndex      = 0;
	HashSet<vk::Format>                                     surfaceFormats;
	vk::SurfaceCapabilitiesKHR                              surfaceCapabilities;
	HashSet<vk::PresentModeKHR>                             surfacePresentModes;
	vk::SwapchainKHR                                        swapchain;
	vk::PipelineCache                                       pipelineCache;
	vk::Queue                                               queue;
	vk::Queue                                               transferQueue;

	vk::CommandBuffer                                       currentCommandBuffer;
	vk::PipelineLayout                                      currentPipelineLayout;
	vk::Viewport                                            currentViewport;

	VmaAllocator                                            allocator;

	vk::CommandPool                                         transferCmdPool;
	std::vector<UploadOp>                                   uploads;
	size_t                                                  numUploads              = 0;

	std::vector<vk::Semaphore>                              freeSemaphores;

	bool                                                    debugMarkers            = false;
	bool                                                    portabilitySubset       = false;

	vk::Buffer                                              ringBuffer;
	VmaAllocation                                           ringBufferMem           = nullptr;
	char                                                    *persistentMapping      = nullptr;

	std::vector<Resource>                                   deleteResources;


	bool isRenderPassCompatible(const RenderPass &pass, const Framebuffer &fb);

	unsigned int bufferAlignment(BufferType type);

	void recreateSwapchain();
	void recreateRingBuffer(unsigned int newSize);
	unsigned int ringBufferAllocate(unsigned int size, unsigned int alignPower);

	void waitForFrame(unsigned int frameIdx);
	void cleanupFrame(unsigned int frameIdx);

	UploadOp allocateUploadOp(uint32_t size);
	void submitUploadOp(UploadOp &&op);

	vk::Semaphore allocateSemaphore();
	void freeSemaphore(vk::Semaphore sem);

	vk::PipelineLayout createPipelineLayout(const PipelineLayoutKey &key);

	void deleteBufferInternal(Buffer &b);
	void deleteFramebufferInternal(Framebuffer &fb);
	void deletePipelineInternal(Pipeline &p);
	void deleteRenderPassInternal(RenderPass &rp);
	void deleteRenderTargetInternal(RenderTarget &rt);
	void deleteResourceInternal(Resource &r);
	void deleteSamplerInternal(Sampler &s);
	void deleteTextureInternal(Texture &tex);
	void deleteFrameInternal(Frame &f);

	explicit RendererImpl(const RendererDesc &desc);

	~RendererImpl();


	struct ResourceDeleter final {
		RendererImpl *r;


		ResourceDeleter(RendererImpl *r_)
		: r(r_)
		{
		}

		ResourceDeleter(const ResourceDeleter &)            = default;
		ResourceDeleter(ResourceDeleter &&) noexcept        = default;

		ResourceDeleter &operator=(const ResourceDeleter &) = default;
		ResourceDeleter &operator=(ResourceDeleter &&) noexcept = default;

		~ResourceDeleter() {}

		void operator()(Buffer &b) const {
			r->deleteBufferInternal(b);
		}

		void operator()(Framebuffer &fb) const {
			r->deleteFramebufferInternal(fb);
		}

		void operator()(Pipeline &p) const {
			r->deletePipelineInternal(p);
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

	template <typename T> void debugNameObject(T h, const std::string &name);

	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros, ShaderLanguage shaderLanguage);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros, ShaderLanguage shaderLanguage);

	void waitForDeviceIdle();
};


}	// namespace renderer


#endif  // VULKANRENDERER_H
