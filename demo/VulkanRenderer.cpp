#ifdef RENDERER_VULKAN


#define STUBBED(str) \
	{ \
		static bool seen = false; \
		if (!seen) { \
			printf("STUBBED: %s in %s at %s:%d\n", str, __PRETTY_FUNCTION__, __FILE__,  __LINE__); \
			seen = true; \
		} \
	}


// needs to be before RendererInternal.h
// TODO: have implementation in a separate file for compile time?
#define VMA_IMPLEMENTATION

#include "RendererInternal.h"
#include "Utils.h"


#ifndef SDL_VIDEO_VULKAN_SURFACE


// TODO: remove these when officially in SDL
#include <SDL_syswm.h>
#include <X11/Xlib-xcb.h>
#define SDL_WINDOW_VULKAN 0x10000000


/**
 *  \brief An opaque handle to a Vulkan instance.
 */
typedef void *SDL_vulkanInstance; /* VK_DEFINE_HANDLE(VkInstance) */

/**
 *  \brief An opaque handle to a Vulkan surface.
 */
typedef Uint64 SDL_vulkanSurface; /* VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR) */


static SDL_bool SDL_Vulkan_GetInstanceExtensions_Helper(unsigned *userCount, const char **userNames, unsigned nameCount, const char *const *names) {
    if(userNames)
    {
        if(*userCount != nameCount)
        {
            SDL_SetError(
                "count doesn't match count from previous call of SDL_Vulkan_GetInstanceExtensions");
            return SDL_FALSE;
        }
        for(unsigned i = 0; i < nameCount; i++)
        {
            userNames[i] = names[i];
        }
    }
    else
    {
        *userCount = nameCount;
    }
    return SDL_TRUE;
}

static SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window * /*window */, unsigned *count, const char **names) {
	static const char *const extensionsForXCB[] = {
		VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	};
	return SDL_Vulkan_GetInstanceExtensions_Helper(
		count, names, SDL_arraysize(extensionsForXCB), extensionsForXCB);
}


static SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window, SDL_vulkanInstance instance, SDL_vulkanSurface *surface) {
	SDL_SysWMinfo wminfo;
	memset(&wminfo, 0, sizeof(wminfo));
	SDL_VERSION(&wminfo.version);

	bool success = SDL_GetWindowWMInfo(window, &wminfo);
	if (!success) {
		printf("SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
		exit(1);
	}

	if (wminfo.subsystem != SDL_SYSWM_X11) {
		printf("unsupported wm subsystem\n");
		exit(1);
	}

	VkXcbSurfaceCreateInfoKHR createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	createInfo.connection = XGetXCBConnection(wminfo.info.x11.display);
	if(!createInfo.connection)
	{
		printf("XGetXCBConnection failed");
		exit(1);
	}
	createInfo.window = (xcb_window_t) wminfo.info.x11.window;

	VkSurfaceKHR outSurface = VK_NULL_HANDLE;
	auto result = vkCreateXcbSurfaceKHR(reinterpret_cast<VkInstance>(instance), &createInfo, nullptr, &outSurface);
	if (result != VK_SUCCESS)  {
		SDL_SetError("vkCreateXcbSurfaceKHR failed: %u", result);
		return SDL_FALSE;
	}

	*surface = reinterpret_cast<SDL_vulkanSurface>(outSurface);

	return SDL_TRUE;
}


#endif  // SDL_VIDEO_VULKAN_SURFACE


static vk::Format vulkanVertexFormat(VtxFormat::VtxFormat format, uint8_t count) {
	switch (format) {
	case VtxFormat::Float:
		switch (count) {
		case 2:
			return vk::Format::eR32G32Sfloat;

		case 3:
			return vk::Format::eR32G32B32Sfloat;

		case 4:
			return vk::Format::eR32G32B32A32Sfloat;

		}

		assert(false);
		return vk::Format::eUndefined;

	case VtxFormat::UNorm8:
		assert(count == 4);
		return vk::Format::eR8G8B8A8Unorm;

	}

	assert(false);
	return vk::Format::eUndefined;
}


RendererImpl::RendererImpl(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, savePreprocessedShaders(false)
, frameNum(0)
, ringBufSize(desc.ephemeralRingBufSize)
, ringBufPtr(0)
, instance(VK_NULL_HANDLE)
, physicalDevice(VK_NULL_HANDLE)
, surface(VK_NULL_HANDLE)
, graphicsQueueIndex(0)
, swapchain(VK_NULL_HANDLE)
, ringBufferMem(vk::MappedMemoryRange())
, persistentMapping(nullptr)
, inFrame(false)
, inRenderPass(false)
, validPipeline(false)
, pipelineDrawn(false)
{
	// TODO: get from desc.debug when this is finished
	bool enableValidation = true;

	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	printf("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		printf("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			printf("Display mode %i : width %i, height %i, BPP %i, refresh %u Hz\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
		}
	}

	int flags = SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_VULKAN;
	// TODO: fullscreen, resizable

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	// TODO: log stuff about window size, screen modes etc

	unsigned int numExtensions = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, NULL)) {
		printf("SDL_Vulkan_GetInstanceExtensions failed\n");
		exit(1);
	}

	std::vector<const char *> extensions(numExtensions, nullptr);

	if(!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, &extensions[0])) {
		printf("SDL_Vulkan_GetInstanceExtensions failed\n");
		exit(1);
	}

	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName    = "SMAA demo";
	appInfo.applicationVersion  = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName         = "SMAA demo";
	appInfo.engineVersion       = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion          = VK_MAKE_VERSION(1, 0, 24);

	vk::InstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.pApplicationInfo         = &appInfo;

	std::vector<const char *> validationLayers = { "VK_LAYER_LUNARG_standard_validation" };

	if (enableValidation) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		instanceCreateInfo.enabledLayerCount    = validationLayers.size();
		instanceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	instanceCreateInfo.enabledExtensionCount    = extensions.size();
	instanceCreateInfo.ppEnabledExtensionNames  = &extensions[0];

	instance = vk::createInstance(instanceCreateInfo);

	std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
	if (physicalDevices.empty()) {
		printf("No hysical Vulkan devices found\n");
		instance.destroy();
		instance = nullptr;
		SDL_DestroyWindow(window);
		window = nullptr;
		exit(1);
	}
	printf("%u physical devices\n", static_cast<unsigned int>(physicalDevices.size()));
	physicalDevice = physicalDevices[0];

	deviceProperties = physicalDevice.getProperties();
	printf("Device API version %u.%u.%u\n", VK_VERSION_MAJOR(deviceProperties.apiVersion), VK_VERSION_MINOR(deviceProperties.apiVersion), VK_VERSION_PATCH(deviceProperties.apiVersion));
	printf("Driver version %u.%u.%u (%u) (0x%08x)\n", VK_VERSION_MAJOR(deviceProperties.driverVersion), VK_VERSION_MINOR(deviceProperties.driverVersion), VK_VERSION_PATCH(deviceProperties.driverVersion), deviceProperties.driverVersion, deviceProperties.driverVersion);
	printf("VendorId 0x%x\n", deviceProperties.vendorID);
	printf("DeviceId 0x%x\n", deviceProperties.deviceID);
	printf("Type %s\n", vk::to_string(deviceProperties.deviceType).c_str());
	printf("Name \"%s\"\n", deviceProperties.deviceName);

	deviceFeatures = physicalDevice.getFeatures();

	if(!SDL_Vulkan_CreateSurface(window,
								 (SDL_vulkanInstance) instance,
								 (SDL_vulkanSurface *)&surface))
	{
		printf("failed to create Vulkan surface");
		// TODO: free instance, window etc...
		exit(1);
	}

	memoryProperties = physicalDevice.getMemoryProperties();
	printf("%u memory types\n", memoryProperties.memoryTypeCount);
	for (unsigned int i = 0; i < memoryProperties.memoryTypeCount; i++ ) {
		printf(" %u  heap %u  %s\n", i, memoryProperties.memoryTypes[i].heapIndex, vk::to_string(memoryProperties.memoryTypes[i].propertyFlags).c_str());
	}
	printf("%u memory heaps\n", memoryProperties.memoryHeapCount);
	for (unsigned int i = 0; i < memoryProperties.memoryHeapCount; i++ ) {
		printf(" %u  size %lu  %s\n", i, memoryProperties.memoryHeaps[i].size, vk::to_string(memoryProperties.memoryHeaps[i].flags).c_str());
	}

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	printf("%u queue families\n", static_cast<unsigned int>(queueProps.size()));

	graphicsQueueIndex = queueProps.size();
	for (uint32_t i = 0; i < queueProps.size(); i++) {
		const auto &q = queueProps[i];
		printf(" Queue family %u\n", i);
		printf("  Flags: %s\n", vk::to_string(q.queueFlags).c_str());
		printf("  Count: %u\n", q.queueCount);
		printf("  Timestamp valid bits: %u\n", q.timestampValidBits);
		printf("  Image transfer granularity: (%u, %u, %u)\n", q.minImageTransferGranularity.width, q.minImageTransferGranularity.height, q.minImageTransferGranularity.depth);

		if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
			if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
				printf("  Can present to our surface\n");
				graphicsQueueIndex = i;
			} else {
				printf("  Can't present to our surface\n");
			}
		}
	}

	if (graphicsQueueIndex == queueProps.size()) {
		printf("Error: no graphics queue\n");
		exit(1);
	}

	printf("Using queue %u for graphics\n", graphicsQueueIndex);

	std::array<float, 1> queuePriorities = { 0.0f };

	vk::DeviceQueueCreateInfo queueCreateInfo;
	queueCreateInfo.queueFamilyIndex  = graphicsQueueIndex;
	queueCreateInfo.queueCount        = 1;
	queueCreateInfo.pQueuePriorities  = &queuePriorities[0];;

	std::vector<const char *> deviceExtensions;
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	vk::DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.queueCreateInfoCount     = 1;
	deviceCreateInfo.pQueueCreateInfos        = &queueCreateInfo;
	// TODO: enable only features we need
	deviceCreateInfo.pEnabledFeatures         = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount    = deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames  = &deviceExtensions[0];
	if (enableValidation) {
		deviceCreateInfo.enabledLayerCount    = validationLayers.size();
		deviceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	device = physicalDevice.createDevice(deviceCreateInfo);

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device         = device;

	vmaCreateAllocator(&allocatorInfo, &allocator);

	queue = device.getQueue(graphicsQueueIndex, 0);

	surfaceFormats      = physicalDevice.getSurfaceFormatsKHR(surface);
	surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	printf("%u surface formats\n", static_cast<uint32_t>(surfaceFormats.size()));
	for (const auto &format : surfaceFormats) {
		printf(" %s\t%s\n", vk::to_string(format.format).c_str(), vk::to_string(format.colorSpace).c_str());
	}

	printf("%u present modes\n",   static_cast<uint32_t>(surfacePresentModes.size()));
	for (const auto &presentMode : surfacePresentModes) {
		printf(" %s\n", vk::to_string(presentMode).c_str());
	}

	recreateSwapchain(desc.swapchain);

	{
		vk::CommandPoolCreateInfo cp;
		cp.queueFamilyIndex = graphicsQueueIndex;
		commandPool = device.createCommandPool(cp);
	}

	// create ringbuffer
	{
		vk::BufferCreateInfo rbInfo;
		rbInfo.size  = ringBufSize;
		rbInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer;
		ringBuffer   = device.createBuffer(rbInfo);

		assert(ringBufferMem.memory == VK_NULL_HANDLE);
		assert(ringBufferMem.size   == 0);
		assert(ringBufferMem.offset == 0);

		VmaMemoryRequirements  req       = { true, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, false };
		uint32_t               typeIndex = 0;

		vmaAllocateMemoryForBuffer(allocator, ringBuffer, &req, &ringBufferMem, &typeIndex);
		printf("ringbuffer memory type index: %u\n",  typeIndex);
		printf("ringbuffer memory: %p\n",             ringBufferMem.memory);
		printf("ringbuffer memory offset: %u\n",      static_cast<unsigned int>(ringBufferMem.offset));
		printf("ringbuffer memory size: %u\n",        static_cast<unsigned int>(ringBufferMem.size));
		assert(ringBufferMem.memory != VK_NULL_HANDLE);
		assert(ringBufferMem.size   == ringBufSize);
		assert(ringBufferMem.offset == 0);

		device.bindBufferMemory(ringBuffer, ringBufferMem.memory, ringBufferMem.offset);

		persistentMapping = reinterpret_cast<char *>(device.mapMemory(ringBufferMem.memory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags()));
		assert(persistentMapping != nullptr);
	}

	// TODO: load pipeline cache
}


RendererImpl::~RendererImpl() {
	assert(instance);
	assert(device);
	assert(surface);
	assert(swapchain);
	assert(commandPool);
	assert(ringBuffer);
	assert(ringBufferMem.memory);
	assert(ringBufferMem.size   > 0);
	assert(persistentMapping);

	assert(ephemeralBuffers.empty());

	// TODO: save pipeline cache

	device.unmapMemory(ringBufferMem.memory);
	persistentMapping = nullptr;
	device.destroyBuffer(ringBuffer);
	ringBuffer = VK_NULL_HANDLE;
	vmaFreeMemory(this->allocator, &ringBufferMem);
	memset(&ringBufferMem, 0, sizeof(ringBufferMem));

	buffers.clearWith([this](struct Buffer &b) {
		assert(!b.ringBufferAlloc);
		this->device.destroyBuffer(b.buffer);
		assert(b.memory.memory != VK_NULL_HANDLE);
		assert(b.memory.size   >  0);
		vmaFreeMemory(this->allocator, &b.memory);
		memset(&b.memory, 0, sizeof(b.memory));
	} );

	samplers.clearWith([this](struct Sampler &s) {
		this->device.destroySampler(s.sampler);
	} );

	pipelines.clearWith([this](Pipeline &p) {
		this->device.destroyPipelineLayout(p.layout);
		this->device.destroyPipeline(p.pipeline);
	} );

	renderPasses.clearWith([this](RenderPass &r) {
		this->device.destroyFramebuffer(r.framebuffer);
		r.framebuffer = vk::Framebuffer();
		this->device.destroyRenderPass(r.renderPass);
		r.renderPass = vk::RenderPass();
	} );

	for (auto &v : vertexShaders) {
		device.destroyShaderModule(v.second.shaderModule);
		v.second.shaderModule = vk::ShaderModule();
	}

	for (auto &f : fragmentShaders) {
		device.destroyShaderModule(f.second.shaderModule);
		f.second.shaderModule = vk::ShaderModule();
	}

	dsLayouts.clearWith([this](DescriptorSetLayout &l) {
		this->device.destroyDescriptorSetLayout(l.layout);
	} );

	renderTargets.clearWith([this](RenderTarget &rt) {
		this->device.destroyImageView(rt.imageView);
		this->device.destroyImage(rt.image);
		assert(rt.memory.memory != VK_NULL_HANDLE);
		assert(rt.memory.size   >  0);
		vmaFreeMemory(this->allocator, &rt.memory);
		memset(&rt.memory, 0, sizeof(rt.memory));
	} );

	device.destroyCommandPool(commandPool);

	device.destroySwapchainKHR(swapchain);
	swapchain = VK_NULL_HANDLE;

	instance.destroySurfaceKHR(surface);
	surface = VK_NULL_HANDLE;

	vmaDestroyAllocator(allocator);
	allocator = VK_NULL_HANDLE;

	device.destroy();
	device = VK_NULL_HANDLE;

	instance.destroy();
	instance = VK_NULL_HANDLE;

	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle RendererImpl::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	vk::BufferCreateInfo info;
	info.size  = size;
	// TODO: usage flags should be parameters
	info.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer;

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer  = device.createBuffer(info);

	VmaMemoryRequirements  req       = { false, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0, false };
	uint32_t               typeIndex = 0;

	vmaAllocateMemoryForBuffer(allocator, buffer.buffer, &req, &buffer.memory, &typeIndex);
	printf("buffer memory type index: %u\n",  typeIndex);
	printf("buffer memory: %p\n",             buffer.memory.memory);
	printf("buffer memory offset: %u\n",      static_cast<unsigned int>(buffer.memory.offset));
	printf("buffer memory size: %u\n",        static_cast<unsigned int>(buffer.memory.size));
	device.bindBufferMemory(buffer.buffer, buffer.memory.memory, buffer.memory.offset);

	STUBBED("copy buffer contents");

	return BufferHandle(result.second);
}


BufferHandle RendererImpl::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	// sub-allocate from persistent coherent buffer
	// round current pointer up to necessary alignment
	// TODO: UBOs need alignment queried from implementation
	// TODO: need buffer usage flags for that
	const unsigned int align = 8;
	const unsigned int add   = (1 << align) - 1;
	const unsigned int mask  = ~add;
	unsigned int alignedPtr  = (ringBufPtr + add) & mask;
	assert(ringBufPtr <= alignedPtr);
	// TODO: ring buffer size should be pow2, se should use add & mask here too
	unsigned int beginPtr    =  alignedPtr % ringBufSize;

	if (beginPtr + size >= ringBufSize) {
		// we went past the end and have to go back to beginning
		// TODO: add and mask here too
		ringBufPtr = (ringBufPtr / ringBufSize + 1) * ringBufSize;
		assert((ringBufPtr & ~mask) == 0);
		alignedPtr  = (ringBufPtr + add) & mask;
		beginPtr    =  alignedPtr % ringBufSize;
		assert(beginPtr + size < ringBufSize);
		assert(beginPtr == 0);
	}
	ringBufPtr = alignedPtr + size;

	memcpy(persistentMapping + beginPtr, contents, size);

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer          = ringBuffer;
	buffer.ringBufferAlloc = true;
	buffer.memory.offset   = beginPtr;
	buffer.memory.size     = size;

	ephemeralBuffers.push_back(result.second);

	return result.second;
}


RenderPassHandle RendererImpl::createRenderPass(const RenderPassDesc &desc) {
	vk::RenderPassCreateInfo info;
	vk::SubpassDescription subpass;

	unsigned int width = 0, height = 0;

	std::vector<vk::AttachmentDescription> attachments;
	std::vector<vk::AttachmentReference> colorAttachments;
	std::vector<vk::ImageView>             attachmentViews;

	// TODO: multiple render targets
	{
		const auto &colorRT = renderTargets.get(desc.colors_[0]);
		assert(colorRT.width  > 0);
		assert(colorRT.height > 0);
		width  = colorRT.width;
		height = colorRT.height;
		vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentDescription attach;
		attach.format         = colorRT.format;
		// TODO: these should be customizable via RenderPassDesc
		attach.loadOp         = vk::AttachmentLoadOp::eClear;
		attach.storeOp        = vk::AttachmentStoreOp::eStore;
		attach.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
		attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attach.initialLayout  = layout;
		// TODO: finalLayout should be transfer dst for final render pass
		attach.finalLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments.push_back(attach);

		vk::AttachmentReference ref;
		ref.attachment = attachments.size() - 1;
		ref.layout     = layout;
		colorAttachments.push_back(ref);
		attachmentViews.push_back(colorRT.imageView);
	}
	subpass.colorAttachmentCount = colorAttachments.size();
	subpass.pColorAttachments    = &colorAttachments[0];

	vk::AttachmentReference depthAttachment;
	if (desc.depthStencil_) {
		const auto &depthRT = renderTargets.get(desc.depthStencil_);
		assert(depthRT.width  == width);
		assert(depthRT.height == height);
		vk::ImageLayout layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::AttachmentDescription attach;
		attach.format         = depthRT.format;
		// TODO: these should be customizable via RenderPassDesc
		attach.loadOp         = vk::AttachmentLoadOp::eClear;
		attach.storeOp        = vk::AttachmentStoreOp::eStore;
		// TODO: stencil
		attach.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
		attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attach.initialLayout  = layout;
		attach.finalLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments.push_back(attach);
		attachmentViews.push_back(depthRT.imageView);

		depthAttachment.attachment = attachments.size() - 1;
		depthAttachment.layout     = layout;
		subpass.pDepthStencilAttachment = &depthAttachment;
	}

	info.attachmentCount = attachments.size();
	info.pAttachments    = &attachments[0];

	// no input attachments
	// no resolved attachments (multisample TODO)
	// no preserved attachments
	info.subpassCount    = 1;
	info.pSubpasses      = &subpass;

	// subpass dependencies (external)
	// TODO: are these really necessary?
	std::vector<vk::SubpassDependency> dependencies;
	dependencies.reserve(desc.depthStencil_ ? 4 : 2);
	{
		vk::SubpassDependency d;
		d.srcSubpass       = VK_SUBPASS_EXTERNAL;
		d.dstSubpass       = 0;
		d.srcStageMask     = vk::PipelineStageFlagBits::eBottomOfPipe;
		d.dstStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		d.srcAccessMask    = vk::AccessFlagBits::eMemoryRead;
		d.dstAccessMask    = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		d.dependencyFlags  = vk::DependencyFlagBits::eByRegion;
		dependencies.push_back(d);

		if (desc.depthStencil_) {
			d.dstStageMask     = vk::PipelineStageFlagBits::eEarlyFragmentTests;
			d.dstAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			dependencies.push_back(d);
		}
	}
	{
		vk::SubpassDependency d;
		d.srcSubpass       = 0;
		d.dstSubpass       = VK_SUBPASS_EXTERNAL;
		d.srcStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		d.dstStageMask     = vk::PipelineStageFlagBits::eBottomOfPipe;
		d.srcAccessMask    = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		d.dstAccessMask    = vk::AccessFlagBits::eMemoryRead;
		d.dependencyFlags  = vk::DependencyFlagBits::eByRegion;
		dependencies.push_back(d);

		if (desc.depthStencil_) {
			d.srcStageMask     = vk::PipelineStageFlagBits::eEarlyFragmentTests;
			d.srcAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			dependencies.push_back(d);
		}
	}
	info.dependencyCount = dependencies.size();
	info.pDependencies   = &dependencies[0];

	auto result   = renderPasses.add();
	RenderPass &r = result.first;
	r.desc        = desc;
	r.width       = width;
	r.height      = height;
	r.renderPass  = device.createRenderPass(info);;
	{
		vk::FramebufferCreateInfo fbInfo;
		fbInfo.renderPass       = r.renderPass;
		assert(!attachmentViews.empty());
		fbInfo.attachmentCount  = attachmentViews.size();
		fbInfo.pAttachments     = &attachmentViews[0];
		fbInfo.width            = width;
		fbInfo.height           = height;
		fbInfo.layers           = 1;  // TODO: multiple render targets?

		r.framebuffer = device.createFramebuffer(fbInfo);
	}

	return RenderPassHandle(result.second);
}


PipelineHandle RendererImpl::createPipeline(const PipelineDesc &desc) {
	vk::GraphicsPipelineCreateInfo info;

	auto vit = vertexShaders.find(desc.vertexShader_.handle);
	assert(vit != vertexShaders.end());

	auto fit = fragmentShaders.find(desc.fragmentShader_.handle);
	assert(fit != fragmentShaders.end());

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
	stages[0].stage  = vk::ShaderStageFlagBits::eVertex;
	stages[0].module = vit->second.shaderModule;
	stages[0].pName  = "main";
	stages[1].stage  = vk::ShaderStageFlagBits::eFragment;
	stages[1].module = fit->second.shaderModule;
	stages[1].pName  = "main";

	info.stageCount = 2;
	info.pStages = &stages[0];

	vk::PipelineVertexInputStateCreateInfo vinput;
	std::vector<vk::VertexInputAttributeDescription> attrs;
	std::vector<vk::VertexInputBindingDescription> bindings;
	if (desc.vertexAttribMask) {
		uint32_t bufmask = 0;

		uint32_t mask = desc.vertexAttribMask;
		while (mask) {
			int bit = __builtin_ctz(mask);
			vk::VertexInputAttributeDescription attr;
			const auto &attrDesc = desc.vertexAttribs.at(bit);
			attr.location = bit;
			attr.binding  = attrDesc.bufBinding;
			attr.format   = vulkanVertexFormat(attrDesc.format, attrDesc.count);
			attr.offset   = attrDesc.offset;
			attrs.push_back(attr);
			mask    &= ~(1 << bit);
			bufmask |=  (1 << attrDesc.bufBinding);
		}

		// currently we support only 1 buffer, TODO: need more?
		assert(bufmask == 1);
		assert(desc.vertexBuffers[0].stride != 0);
		vk::VertexInputBindingDescription bind;
		bind.binding   = 0;
		bind.stride    = desc.vertexBuffers[0].stride;
		bind.inputRate = vk::VertexInputRate::eVertex;
		bindings.push_back(bind);

		vinput.vertexBindingDescriptionCount   = bindings.size();
		vinput.pVertexBindingDescriptions      = &bindings[0];
		vinput.vertexAttributeDescriptionCount = attrs.size();
		vinput.pVertexAttributeDescriptions    = &attrs[0];

	}
	info.pVertexInputState = &vinput;

	vk::PipelineInputAssemblyStateCreateInfo inputAsm;
	inputAsm.topology               = vk::PrimitiveTopology::eTriangleList;
	inputAsm.primitiveRestartEnable = false;
	info.pInputAssemblyState        = &inputAsm;

	vk::PipelineViewportStateCreateInfo vp;
	vp.viewportCount = 1;
	vp.scissorCount  = 1;
	// leave pointers null, we use dynamic states for them
	info.pViewportState = &vp;

	vk::PipelineRasterizationStateCreateInfo raster;
	if (desc.cullFaces_) {
		raster.cullMode = vk::CullModeFlagBits::eBack;
	}
	raster.lineWidth = 1.0;
	info.pRasterizationState        = &raster;

	vk::PipelineMultisampleStateCreateInfo multisample;
	info.pMultisampleState = &multisample;

	vk::PipelineDepthStencilStateCreateInfo ds;
	ds.depthTestEnable  = desc.depthTest_;
	ds.depthWriteEnable = desc.depthWrite_;
	ds.depthCompareOp   = vk::CompareOp::eLess;
	info.pDepthStencilState = &ds;

	std::vector<vk::PipelineColorBlendAttachmentState> colorBlendStates;
	{
		// TODO: for all color render targets
		vk::PipelineColorBlendAttachmentState cb;
		if (desc.blending_) {
			STUBBED("blending");
		}
		cb.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendStates.push_back(cb);
	}
	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = colorBlendStates.size();
	blendInfo.pAttachments    = &colorBlendStates[0];
	info.pColorBlendState     = &blendInfo;

	vk::PipelineDynamicStateCreateInfo dyn;
	std::vector<vk::DynamicState> dynStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	dyn.dynamicStateCount = dynStates.size();
	dyn.pDynamicStates    = &dynStates[0];
	info.pDynamicState    = &dyn;

	std::vector<vk::DescriptorSetLayout> layouts;
	for (unsigned int i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
		if (desc.descriptorSetLayouts[i]) {
			const auto &layout = dsLayouts.get(desc.descriptorSetLayouts[i]);
			layouts.push_back(layout.layout);
		}
	}

	vk::PipelineLayoutCreateInfo layoutInfo;
	layoutInfo.setLayoutCount = layouts.size();
	layoutInfo.pSetLayouts    = &layouts[0];

	auto layout = device.createPipelineLayout(layoutInfo);
	info.layout = layout;

	const auto &renderPass = renderPasses.get(desc.renderPass_.handle);
	info.renderPass = renderPass.renderPass;

	auto result = device.createGraphicsPipeline(vk::PipelineCache(), info);

	auto id = pipelines.add();
	Pipeline &p = id.first;
	p.pipeline = result;
	p.layout   = layout;

	return PipelineHandle(id.second);
}


static vk::Format vulkanFormat(Format format) {
	switch (format) {
	case Invalid:
		assert(false);
		return vk::Format::eUndefined;

	case R8:
		return vk::Format::eR8Unorm;

	case RG8:
		return vk::Format::eR8G8Unorm;

	case RGB8:
		return vk::Format::eR8G8B8Unorm;

	case RGBA8:
		return vk::Format::eR8G8B8A8Unorm;

	case Depth16:
		return vk::Format::eD16Unorm;

	}

	assert(false);
	return vk::Format::eUndefined;
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	// TODO: use NV_dedicated_allocation when available

	vk::Format format = vulkanFormat(desc.format_);
	vk::ImageCreateInfo info;
	info.imageType   = vk::ImageType::e2D;
	info.format      = format;
	info.extent      = vk::Extent3D(desc.width_, desc.height_, 1);
	info.mipLevels   = 1;
	info.arrayLayers = 1;
	// TODO: samples when multisampling
	// TODO: usage should come from desc
	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	if (desc.format_ == Depth16) {
		flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	} else {
		flags |= vk::ImageUsageFlagBits::eColorAttachment;
	}
	info.usage       = flags;

	auto result = renderTargets.add();
	RenderTarget &rt = result.first;
	rt.width  = desc.width_;
	rt.height = desc.height_;
	rt.image = device.createImage(info);
	rt.format = format;

	VmaMemoryRequirements  req       = { false, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0, false };
	uint32_t               typeIndex = 0;

	vmaAllocateMemoryForImage(allocator, rt.image, &req, &rt.memory, &typeIndex);
	printf("image memory type index: %u\n",  typeIndex);
	printf("image memory: %p\n",             rt.memory.memory);
	printf("image memory offset: %u\n",      static_cast<unsigned int>(rt.memory.offset));
	printf("image memory size: %u\n",        static_cast<unsigned int>(rt.memory.size));
	device.bindImageMemory(rt.image, rt.memory.memory, rt.memory.offset);

	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image    = rt.image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format   = format;
	if (desc.format_ == Depth16) {
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	} else {
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	}
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	rt.imageView = device.createImageView(viewInfo);

	return result.second;
}


static vk::Filter vulkanFiltermode(FilterMode m) {
	switch (m) {
	case Nearest:
        return vk::Filter::eNearest;

	case Linear:
        return vk::Filter::eLinear;
	}

	assert(false);

	return vk::Filter::eNearest;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc &desc) {
	vk::SamplerCreateInfo info;

	info.magFilter = vulkanFiltermode(desc.mag);
	info.minFilter = vulkanFiltermode(desc.min);

	vk::SamplerAddressMode m = vk::SamplerAddressMode::eClampToEdge;
	if (desc.wrapMode == Wrap) {
		m = vk::SamplerAddressMode::eRepeat;
	}
	info.addressModeU = m;
	info.addressModeV = m;
	info.addressModeW = m;

	auto result = samplers.add();
	struct Sampler &sampler = result.first;
	sampler.sampler    = device.createSampler(info);

	return SamplerHandle(result.second);
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

	auto vertexSrc = loadSource(vertexShaderName);

	shaderc::CompileOptions options;
	// TODO: optimization level?
	// TODO: cache includes globally
	options.SetIncluder(std::make_unique<Includer>());

	for (const auto &p : macros) {
		options.AddMacroDefinition(p.first, p.second);
	}

	auto result = compiler.CompileGlslToSpv(&vertexSrc[0], vertexSrc.size(), shaderc_glsl_vertex_shader, vertexShaderName.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		printf("Shader %s compile failed: %s\n", vertexShaderName.c_str(), result.GetErrorMessage().c_str());
		exit(1);
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());

	if (savePreprocessedShaders) {
		// TODO: save SPIR-V?
	}

	VertexShader v;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	v.shaderModule = device.createShaderModule(info);
	auto id = vertexShaders.size() + 1;

	auto temp = vertexShaders.emplace(id, std::move(v));
	assert(temp.second);

	return VertexShaderHandle(id);
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName   = name + ".frag";

	auto fragmentSrc = loadSource(fragmentShaderName);

	shaderc::CompileOptions options;
	// TODO: optimization level?
	// TODO: cache includes globally
	options.SetIncluder(std::make_unique<Includer>());

	for (const auto &p : macros) {
		options.AddMacroDefinition(p.first, p.second);
	}

	auto result = compiler.CompileGlslToSpv(&fragmentSrc[0], fragmentSrc.size(), shaderc_glsl_fragment_shader, fragmentShaderName.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		printf("Shader %s compile failed: %s\n", fragmentShaderName.c_str(), result.GetErrorMessage().c_str());
		exit(1);
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());

	if (savePreprocessedShaders) {
		// TODO: save SPIR-V?
	}

	FragmentShader f;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	f.shaderModule = device.createShaderModule(info);
	auto id = fragmentShaders.size() + 1;

	auto temp = fragmentShaders.emplace(id, std::move(f));
	assert(temp.second);

	return FragmentShaderHandle(id);
}


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	STUBBED("");

	return 0;
}


static const std::array<vk::DescriptorType, Count - 1> descriptorTypes =
{
	  vk::DescriptorType::eUniformBuffer
	, vk::DescriptorType::eStorageBuffer
	, vk::DescriptorType::eSampler
	, vk::DescriptorType::eSampledImage
	, vk::DescriptorType::eCombinedImageSampler
};


DescriptorSetLayoutHandle RendererImpl::createDescriptorSetLayout(const DescriptorLayout *layout) {
	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	unsigned int i = 0;
	while (layout->type != End) {
		vk::DescriptorSetLayoutBinding b;

		b.binding         = i;
		// TODO: make layout End last in enum so this is nicer
		b.descriptorType  = descriptorTypes[layout->type - 1];
		b.descriptorCount = 1;
		// TODO: should specify stages in layout
		b.stageFlags      = vk::ShaderStageFlagBits::eAll;

		bindings.push_back(b);

		layout++;
		i++;
	}
	assert(layout->offset == 0);

	vk::DescriptorSetLayoutCreateInfo info;
	info.bindingCount = bindings.size();
	info.pBindings    = &bindings[0];

	auto result = dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;
	dsLayout.layout = device.createDescriptorSetLayout(info);

	return DescriptorSetLayoutHandle(result.second);
}


void RendererImpl::deleteBuffer(BufferHandle /* handle */) {
	STUBBED("");
}


void RendererImpl::deleteRenderPass(RenderPassHandle /* pass */) {
	STUBBED("");
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &) {
	STUBBED("");
}


void RendererImpl::deleteSampler(SamplerHandle /* handle */) {
	STUBBED("");
}


void RendererImpl::deleteTexture(TextureHandle /* handle */) {
	STUBBED("");
}


void RendererImpl::recreateSwapchain(const SwapchainDesc &desc) {
	surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	printf("image count min-max %u - %u\n", surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
	printf("image extent min-max %ux%u - %ux%u\n", surfaceCapabilities.minImageExtent.width, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.width, surfaceCapabilities.maxImageExtent.height);
	printf("current image extent %ux%u\n", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height);
	printf("supported surface transforms: %s\n", vk::to_string(surfaceCapabilities.supportedTransforms).c_str());
	printf("supported surface alpha composite flags: %s\n", vk::to_string(surfaceCapabilities.supportedCompositeAlpha).c_str());
	printf("supported surface usage flags: %s\n", vk::to_string(surfaceCapabilities.supportedUsageFlags).c_str());

	unsigned int numImages = desc.numFrames;
	numImages = std::max(numImages, surfaceCapabilities.minImageCount);
	if (surfaceCapabilities.maxImageCount != 0) {
		numImages = std::min(numImages, surfaceCapabilities.maxImageCount);
	}

	printf("Want %u images, using %u images\n", desc.numFrames, numImages);

	vk::Extent2D imageExtent;
	if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
		assert(surfaceCapabilities.currentExtent.height == 0xFFFFFFFF);
		// TODO: check against min and max
		imageExtent.width  = desc.width;
		imageExtent.height = desc.height;
	} else {
		if ((surfaceCapabilities.currentExtent.width != desc.width) || (surfaceCapabilities.currentExtent.height != desc.height)) {
			printf("warning: surface current extent (%ux%u) differs from requested (%ux%u)\n", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height, desc.width, desc.height);
			// TODO: should we use requested? can we? spec says platform-specific behavior
		}
		imageExtent = surfaceCapabilities.currentExtent;
	}

	if (!(surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)) {
		printf("warning: identity transform not supported\n");
	}

	if (surfaceCapabilities.currentTransform != vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		printf("warning: current transform is not identity\n");
	}

	if (!(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
		printf("warning: opaque alpha not supported\n");
	}

	// FIFO is guaranteed to be supported
	vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
	if (desc.vsync) {
		for (const auto &mode : surfacePresentModes) {
			if (mode == vk::PresentModeKHR::eMailbox) {
				swapchainPresentMode = vk::PresentModeKHR::eMailbox;
				// mailbox is "best", get out
				break;
			} else if (mode == vk::PresentModeKHR::eFifoRelaxed) {
				swapchainPresentMode = vk::PresentModeKHR::eFifoRelaxed;
				// keep looking in case we find a better one
			}
		}
	} else {
		for (const auto &mode : surfacePresentModes) {
			if (mode == vk::PresentModeKHR::eImmediate) {
				swapchainPresentMode = vk::PresentModeKHR::eImmediate;
				break;
			}
		}
	}

	printf("using present mode %s\n", vk::to_string(swapchainPresentMode).c_str());

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.flags                 = vk::SwapchainCreateFlagBitsKHR();
	swapchainCreateInfo.surface               = surface;
	swapchainCreateInfo.minImageCount         = numImages;
	// TODO: better way to choose a format, should care about sRGB
	swapchainCreateInfo.imageFormat           = surfaceFormats[0].format;
	swapchainCreateInfo.imageColorSpace       = surfaceFormats[0].colorSpace;
	swapchainCreateInfo.imageExtent           = imageExtent;
	swapchainCreateInfo.imageArrayLayers      = 1;
	swapchainCreateInfo.imageUsage            = vk::ImageUsageFlagBits::eTransferDst;

	// no concurrent access
	swapchainCreateInfo.imageSharingMode      = vk::SharingMode::eExclusive;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices   = nullptr;

	swapchainCreateInfo.preTransform          = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	swapchainCreateInfo.compositeAlpha        = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainCreateInfo.presentMode           = swapchainPresentMode;
	swapchainCreateInfo.clipped               = true;
	swapchainCreateInfo.oldSwapchain          = swapchain;

	vk::SwapchainKHR newSwapchain = device.createSwapchainKHR(swapchainCreateInfo);

	if (swapchain) {
		device.destroySwapchainKHR(swapchain);
	}
	swapchain = newSwapchain;

	swapchainImages = device.getSwapchainImagesKHR(swapchain);
    printf("Got %u swapchain images\n", static_cast<unsigned int>(swapchainImages.size()));

}


void RendererImpl::beginFrame() {
	assert(!inFrame);
	inFrame       = true;
	inRenderPass  = false;
	validPipeline = false;
	pipelineDrawn = true;

	// TODO: check how many frames are outstanding, wait if maximum
	// here or in presentFrame?

	// TODO: acquire next image here or in presentFrame?

	// create command buffer
	// TODO: should have multiple sets of these ready and just reset
	// the appropriate pool
	vk::CommandBufferAllocateInfo info(commandPool, vk::CommandBufferLevel::ePrimary, 1);
	auto bufs = device.allocateCommandBuffers(info);

	currentCommandBuffer = bufs[0];

	// set command buffer to recording
	currentCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	STUBBED("");
}


void RendererImpl::presentFrame(RenderTargetHandle /* rt */) {
	assert(inFrame);
	inFrame = false;

	// TODO: shouldn't recreate constantly...
	vk::Fence fence = device.createFence(vk::FenceCreateInfo());

	auto imageIdx_         = device.acquireNextImageKHR(swapchain, UINT64_MAX, vk::Semaphore(), fence);
	uint32_t imageIdx      = imageIdx_.value;
	vk::Image image        = swapchainImages[imageIdx];
	vk::ImageLayout layout = vk::ImageLayout::eTransferDstOptimal;

	// transition image to transfer dst optimal
	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask       = vk::AccessFlagBits::eMemoryWrite;
	barrier.dstAccessMask       = vk::AccessFlagBits::eTransferWrite;
	barrier.oldLayout           = vk::ImageLayout::eUndefined;
	barrier.newLayout           = layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = image;

	vk::ImageSubresourceRange range;
	range.aspectMask            = vk::ImageAspectFlagBits::eColor;
	range.baseMipLevel          = 0;
	range.levelCount            = VK_REMAINING_MIP_LEVELS;
	range.baseArrayLayer        = 0;
	range.layerCount            = VK_REMAINING_ARRAY_LAYERS;
	barrier.subresourceRange    = range;

	currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { barrier });

	// clear image
	STUBBED("blit real draw image to presentation image");
	double crap = 0.0;
	float c = modf(frameNum / 60.0f, &crap);
	std::array<float, 4> color = { c, c, c, c };
	currentCommandBuffer.clearColorImage(image, layout, vk::ClearColorValue(color), { range });

	// transition to present
	barrier.srcAccessMask       = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryRead;
	barrier.oldLayout           = layout;
	barrier.newLayout           = vk::ImageLayout::ePresentSrcKHR;
	currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { barrier });

	// submit command buffer
	currentCommandBuffer.end();
	vk::SubmitInfo submit;
	submit.waitSemaphoreCount   = 0;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &currentCommandBuffer;
	submit.signalSemaphoreCount = 0;
	queue.submit({ submit }, vk::Fence());

	// present
	device.waitForFences({ fence }, true, UINT64_MAX);
	vk::PresentInfoKHR presentInfo;
	presentInfo.waitSemaphoreCount = 0;
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains        = &swapchain;
	presentInfo.pImageIndices      = &imageIdx;

	queue.presentKHR(presentInfo);

	// wait until complete
	// TODO: don't
	queue.waitIdle();

	// delete command buffer
	// TODO: shouldn't do that, reuse it
	device.freeCommandBuffers(commandPool, {currentCommandBuffer} );

	// reset command pool
	device.resetCommandPool(commandPool, vk::CommandPoolResetFlags());

	device.destroyFence(fence);

	// TODO: multiple frames, only delete after no longer in use by GPU
	for (auto handle : ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		assert(buffer.buffer == ringBuffer);
		assert(buffer.ringBufferAlloc);
		assert(buffer.memory.size   >  0);
		buffers.remove(handle);
	}
	ephemeralBuffers.clear();

	frameNum++;
}


void RendererImpl::beginRenderPass(RenderPassHandle /* pass */) {
	assert(inFrame);
	assert(!inRenderPass);
	inRenderPass  = true;
	validPipeline = false;

	STUBBED("");
}


void RendererImpl::endRenderPass() {
	assert(inFrame);
	assert(inRenderPass);
	inRenderPass = false;

	STUBBED("");
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(inFrame);
	assert(inRenderPass);
	assert(pipelineDrawn);
	pipelineDrawn = false;
	validPipeline = true;

	// TODO: make sure current renderpass matches the one in pipeline

	const auto &p = pipelines.get(pipeline);
	currentCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p.pipeline);
}


void RendererImpl::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	assert(inFrame);
	assert(validPipeline);

	auto &b = buffers.get(buffer);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.memory.offset;
	}
	currentCommandBuffer.bindIndexBuffer(b.buffer, offset, bit16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
}


void RendererImpl::bindVertexBuffer(unsigned int binding, BufferHandle buffer) {
	assert(inFrame);
	assert(validPipeline);

	auto &b = buffers.get(buffer);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.memory.offset;
	}
	currentCommandBuffer.bindVertexBuffers(binding, 1, &b.buffer, &offset);
}


void RendererImpl::bindDescriptorSet(unsigned int /* index */, DescriptorSetLayoutHandle /* layout */, const void * /* data_ */) {
	assert(validPipeline);

	STUBBED("");
}


void RendererImpl::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(inFrame);

	vk::Viewport vp;
	vp.x        = x;
	STUBBED("check viewport y direction");
	vp.y        = y;
	vp.width    = width;
	vp.height   = height;
	vp.maxDepth = 1.0f;
	currentCommandBuffer.setViewport(0, 1, &vp);
}


void RendererImpl::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(validPipeline);
	// TODO: check current pipeline has scissor enabled

	vk::Rect2D rect;
	rect.offset.x      = x;
	rect.offset.y      = y;
	rect.extent.width  = width;
	rect.extent.height = height;

	currentCommandBuffer.setScissor(0, 1, &rect);
}


void RendererImpl::draw(unsigned int /* firstVertex */, unsigned int /* vertexCount */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;

	STUBBED("");
}


void RendererImpl::drawIndexedInstanced(unsigned int /* vertexCount */, unsigned int /* instanceCount */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;

	STUBBED("");
}


void RendererImpl::drawIndexedOffset(unsigned int /* vertexCount */, unsigned int /* firstIndex */) {
	assert(inRenderPass);
	assert(validPipeline);
	pipelineDrawn = true;

	STUBBED("");
}


#endif  // RENDERER_VULKAN
