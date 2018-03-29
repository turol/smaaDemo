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


#ifdef RENDERER_VULKAN


#include <SDL_vulkan.h>

#include "RendererInternal.h"
#include "utils/Utils.h"


// this part of the C++ bindings sucks...
// TODO: replace with DispatchLoaderDynamic

static PFN_vkCreateDebugReportCallbackEXT   pfn_vkCreateDebugReportCallbackEXT   = nullptr;
static PFN_vkDestroyDebugReportCallbackEXT  pfn_vkDestroyDebugReportCallbackEXT  = nullptr;
static PFN_vkDebugMarkerSetObjectNameEXT    pfn_vkDebugMarkerSetObjectNameEXT    = nullptr;
static PFN_vkGetShaderInfoAMD               pfn_vkGetShaderInfoAMD               = nullptr;


VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(
    VkInstance                                  instance,
    const VkDebugReportCallbackCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugReportCallbackEXT*                   pCallback)
{
	assert(pfn_vkCreateDebugReportCallbackEXT);
	return pfn_vkCreateDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback);
}


VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(
    VkInstance                                  instance,
    VkDebugReportCallbackEXT                    callback,
    const VkAllocationCallbacks*                pAllocator)
{
	assert(pfn_vkDestroyDebugReportCallbackEXT);
	return pfn_vkDestroyDebugReportCallbackEXT(instance, callback, pAllocator);
}


VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectNameInfoEXT*       pNameInfo)
{
	assert(pfn_vkDebugMarkerSetObjectNameEXT);
	return pfn_vkDebugMarkerSetObjectNameEXT(device, pNameInfo);
}


VKAPI_ATTR VkResult VKAPI_CALL vkGetShaderInfoAMD(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    VkShaderStageFlagBits                       shaderStage,
    VkShaderInfoTypeAMD                         infoType,
    size_t*                                     pInfoSize,
    void*                                       pInfo)
{
	return pfn_vkGetShaderInfoAMD(device, pipeline, shaderStage, infoType, pInfoSize, pInfo);
}

namespace renderer {


static const std::array<vk::DescriptorType, uint8_t(DescriptorType::Count) - 1> descriptorTypes =
{ {
	  vk::DescriptorType::eUniformBuffer
	, vk::DescriptorType::eStorageBuffer
	, vk::DescriptorType::eSampler
	, vk::DescriptorType::eSampledImage
	, vk::DescriptorType::eCombinedImageSampler
} };


static vk::Format vulkanVertexFormat(VtxFormat format, uint8_t count) {
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

		UNREACHABLE();
		return vk::Format::eUndefined;

	case VtxFormat::UNorm8:
		assert(count == 4);
		return vk::Format::eR8G8B8A8Unorm;

	}

	UNREACHABLE();
	return vk::Format::eUndefined;
}


static vk::Format vulkanFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		UNREACHABLE();
		return vk::Format::eUndefined;

	case Format::R8:
		return vk::Format::eR8Unorm;

	case Format::RG8:
		return vk::Format::eR8G8Unorm;

	case Format::RGB8:
		return vk::Format::eR8G8B8Unorm;

	case Format::RGBA8:
		return vk::Format::eR8G8B8A8Unorm;

	case Format::sRGBA8:
		return vk::Format::eR8G8B8A8Srgb;

	case Format::RGBA16Float:
		return vk::Format::eR16G16Sfloat;

	case Format::RGBA32Float:
		return vk::Format::eR32G32B32A32Sfloat;

	case Format::Depth16:
		return vk::Format::eD16Unorm;

	case Format::Depth16S8:
		return vk::Format::eD16UnormS8Uint;

	case Format::Depth24S8:
		return vk::Format::eD24UnormS8Uint;

	case Format::Depth24X8:
		return vk::Format::eX8D24UnormPack32;

	case Format::Depth32Float:
		return vk::Format::eD32Sfloat;

	}

	UNREACHABLE();
	return vk::Format::eUndefined;
}


vk::BufferUsageFlags bufferTypeUsage(BufferType type) {
	vk::BufferUsageFlags flags;
	switch (type) {
	case BufferType::Invalid:
		UNREACHABLE();
		break;

	case BufferType::Index:
		flags |= vk::BufferUsageFlagBits::eIndexBuffer;
		break;

	case BufferType::Uniform:
		flags |= vk::BufferUsageFlagBits::eUniformBuffer;
		break;

	case BufferType::Storage:
		flags |= vk::BufferUsageFlagBits::eStorageBuffer;
		break;

	case BufferType::Vertex:
		flags |= vk::BufferUsageFlagBits::eVertexBuffer;
		break;

	case BufferType::Everything:
		// not supposed to be called
		assert(false);
		break;

	}

	return flags;
}

static VkBool32 VKAPI_PTR debugCallbackFunc(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t /* messageCode */, const char * pLayerPrefix, const char * pMessage, void * /* pUserData*/) {
	LOG("layer %s %s object %lu type %s location %lu: %s\n", pLayerPrefix, vk::to_string(vk::DebugReportFlagBitsEXT(flags)).c_str(), static_cast<unsigned long>(object), vk::to_string(vk::DebugReportObjectTypeEXT(objectType)).c_str(), static_cast<unsigned long>(location), pMessage);
	// make errors fatal
	abort();

	return VK_FALSE;
}


RendererImpl::RendererImpl(const RendererDesc &desc)
: RendererBase(desc)
, graphicsQueueIndex(0)
, numUploads(0)
, amdShaderInfo(false)
, debugMarkers(false)
, ringBufferMem(nullptr)
, persistentMapping(nullptr)
{
	bool enableValidation = desc.debug;
	bool enableMarkers    = desc.tracing;

	// renderdoc crashes if SDL tries to init GL renderer so disable it
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	LOG("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int retval = SDL_GetDesktopDisplayMode(i, &mode);
		if (retval == 0) {
			LOG("Desktop mode for display %d: %dx%d, refresh %d Hz\n", i, mode.w, mode.h, mode.refresh_rate);
			currentRefreshRate = mode.refresh_rate;
		} else {
			LOG("Failed to get desktop display mode for display %d\n", i);
		}

		int numModes = SDL_GetNumDisplayModes(i);
		LOG("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			LOG("Display mode %i : width %i, height %i, BPP %i, refresh %u Hz\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
			maxRefreshRate = std::max(static_cast<unsigned int>(mode.refresh_rate), maxRefreshRate);
		}
	}

	int flags = SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_VULKAN;
	if (desc.swapchain.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	{
		auto extensions = vk::enumerateInstanceExtensionProperties();
		std::sort(extensions.begin(), extensions.end()
		  , [] (const vk::ExtensionProperties &a, const vk::ExtensionProperties &b) {
			  return strcmp(a.extensionName, b.extensionName) < 0;
		});


		size_t maxLen = 0;
		for (const auto &ext : extensions) {
			maxLen = std::max(strlen(ext.extensionName), maxLen);
		}

		LOG("Instance extensions:\n");
		std::vector<char> padding;
		padding.reserve(maxLen + 1);
		for (unsigned int i = 0; i < maxLen; i++) {
			padding.push_back(' ');
		}
		padding.push_back('\0');
		for (const auto &ext : extensions) {
			LOG(" %s %s %u\n", ext.extensionName, &padding[strnlen(ext.extensionName, maxLen)], ext.specVersion);
		}
	}

	unsigned int numExtensions = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, NULL)) {
		LOG("SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
		throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
	}

	std::vector<const char *> extensions(numExtensions, nullptr);

	if(!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, &extensions[0])) {
		LOG("SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
		throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
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
		instanceCreateInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	LOG("Active instance extensions:\n");
	for (const auto ext : extensions) {
		LOG(" %s\n", ext);
	}

	instanceCreateInfo.enabledExtensionCount    = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames  = &extensions[0];

	instance = vk::createInstance(instanceCreateInfo);

	if (enableValidation) {
		pfn_vkCreateDebugReportCallbackEXT   = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance.getProcAddr("vkCreateDebugReportCallbackEXT"));
		pfn_vkDestroyDebugReportCallbackEXT  = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance.getProcAddr("vkDestroyDebugReportCallbackEXT"));

		vk::DebugReportCallbackCreateInfoEXT callbackInfo;
		callbackInfo.flags       = vk::DebugReportFlagBitsEXT::eError;
		callbackInfo.pfnCallback = debugCallbackFunc;
		debugCallback = instance.createDebugReportCallbackEXT(callbackInfo);
	}

	if (enableMarkers) {
		pfn_vkDebugMarkerSetObjectNameEXT = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(instance.getProcAddr("vkDebugMarkerSetObjectNameEXT"));
	}

	std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
	if (physicalDevices.empty()) {
		LOG("No physical Vulkan devices found\n");
		instance.destroy();
		instance = nullptr;
		SDL_DestroyWindow(window);
		window = nullptr;
		throw std::runtime_error("No physical Vulkan devices found");
	}
	LOG("%u physical devices\n", static_cast<unsigned int>(physicalDevices.size()));
	physicalDevice = physicalDevices.at(0);

	deviceProperties = physicalDevice.getProperties();

	LOG("Device API version %u.%u.%u\n", VK_VERSION_MAJOR(deviceProperties.apiVersion), VK_VERSION_MINOR(deviceProperties.apiVersion), VK_VERSION_PATCH(deviceProperties.apiVersion));
	LOG("Driver version %u.%u.%u (%u) (0x%08x)\n", VK_VERSION_MAJOR(deviceProperties.driverVersion), VK_VERSION_MINOR(deviceProperties.driverVersion), VK_VERSION_PATCH(deviceProperties.driverVersion), deviceProperties.driverVersion, deviceProperties.driverVersion);
	LOG("VendorId 0x%x\n", deviceProperties.vendorID);
	LOG("DeviceId 0x%x\n", deviceProperties.deviceID);
	LOG("Type %s\n", vk::to_string(deviceProperties.deviceType).c_str());
	LOG("Name \"%s\"\n", deviceProperties.deviceName);
	LOG("uniform buffer alignment %u\n", static_cast<unsigned int>(deviceProperties.limits.minUniformBufferOffsetAlignment));
	LOG("storage buffer alignment %u\n", static_cast<unsigned int>(deviceProperties.limits.minStorageBufferOffsetAlignment));
	LOG("texel buffer alignment %u\n",   static_cast<unsigned int>(deviceProperties.limits.minTexelBufferOffsetAlignment));

	uboAlign  = static_cast<unsigned int>(deviceProperties.limits.minUniformBufferOffsetAlignment);
	ssboAlign = static_cast<unsigned int>(deviceProperties.limits.minStorageBufferOffsetAlignment);

	deviceFeatures = physicalDevice.getFeatures();

	if(!SDL_Vulkan_CreateSurface(window,
								 (SDL_vulkanInstance) instance,
								 (SDL_vulkanSurface *)&surface))
	{
		LOG("Failed to create Vulkan surface: %s\n", SDL_GetError());
		// TODO: free instance, window etc...
		throw std::runtime_error("Failed to create Vulkan surface");
	}

	memoryProperties = physicalDevice.getMemoryProperties();
	LOG("%u memory types\n", memoryProperties.memoryTypeCount);
	for (unsigned int i = 0; i < memoryProperties.memoryTypeCount; i++ ) {
		std::string tempString = vk::to_string(memoryProperties.memoryTypes[i].propertyFlags);
		LOG(" %u  heap %u  %s\n", i, memoryProperties.memoryTypes[i].heapIndex, tempString.c_str());
	}
	LOG("%u memory heaps\n", memoryProperties.memoryHeapCount);
	for (unsigned int i = 0; i < memoryProperties.memoryHeapCount; i++ ) {
		std::string tempString = vk::to_string(memoryProperties.memoryHeaps[i].flags);
		LOG(" %u  size %lu  %s\n", i, static_cast<unsigned long>(memoryProperties.memoryHeaps[i].size), tempString.c_str());
	}

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	LOG("%u queue families\n", static_cast<unsigned int>(queueProps.size()));

	graphicsQueueIndex = static_cast<uint32_t>(queueProps.size());
	for (uint32_t i = 0; i < queueProps.size(); i++) {
		const auto &q = queueProps.at(i);
		LOG(" Queue family %u\n", i);
		LOG("  Flags: %s\n", vk::to_string(q.queueFlags).c_str());
		LOG("  Count: %u\n", q.queueCount);
		LOG("  Timestamp valid bits: %u\n", q.timestampValidBits);
		LOG("  Image transfer granularity: (%u, %u, %u)\n", q.minImageTransferGranularity.width, q.minImageTransferGranularity.height, q.minImageTransferGranularity.depth);

		if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
			if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
				LOG("  Can present to our surface\n");
				graphicsQueueIndex = i;
			} else {
				LOG("  Can't present to our surface\n");
			}
		}
	}

	if (graphicsQueueIndex == queueProps.size()) {
		LOG("Error: no graphics queue\n");
		throw std::runtime_error("Error: no graphics queue");
	}

	LOG("Using queue %u for graphics\n", graphicsQueueIndex);

	std::array<float, 1> queuePriorities = { { 0.0f } };

	vk::DeviceQueueCreateInfo queueCreateInfo;
	queueCreateInfo.queueFamilyIndex  = graphicsQueueIndex;
	queueCreateInfo.queueCount        = 1;
	queueCreateInfo.pQueuePriorities  = &queuePriorities[0];;

	std::unordered_set<std::string> availableExtensions;
	{
		auto exts = physicalDevice.enumerateDeviceExtensionProperties();
		LOG("%u device extensions:\n", static_cast<unsigned int>(exts.size()));
		for (const auto &ext : exts) {
			LOG("%s\n", ext.extensionName);
			availableExtensions.insert(std::string(ext.extensionName));
		}
	}

	std::vector<const char *> deviceExtensions;

	auto checkExt = [&deviceExtensions, &availableExtensions] (const char *ext) {
		if (availableExtensions.find(std::string(ext)) != availableExtensions.end()) {
			LOG("Activating extension %s\n", ext);
			deviceExtensions.push_back(ext);
			return true;
		}
		return false;
	};

	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	bool dedicatedAllocation = true;
	dedicatedAllocation = checkExt(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) && dedicatedAllocation;
	dedicatedAllocation = checkExt(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)      && dedicatedAllocation;
	if (enableMarkers) {
		debugMarkers = checkExt(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	}

	amdShaderInfo = checkExt(VK_AMD_SHADER_INFO_EXTENSION_NAME);
	if (amdShaderInfo) {
		LOG("VK_AMD_shader_info found\n");
		pfn_vkGetShaderInfoAMD = reinterpret_cast<PFN_vkGetShaderInfoAMD>(instance.getProcAddr("vkGetShaderInfoAMD"));
	}

	vk::DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.queueCreateInfoCount     = 1;
	deviceCreateInfo.pQueueCreateInfos        = &queueCreateInfo;
	// TODO: enable only features we need
	deviceCreateInfo.pEnabledFeatures         = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount    = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames  = &deviceExtensions[0];
	if (enableValidation) {
		deviceCreateInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	device = physicalDevice.createDevice(deviceCreateInfo);

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device         = device;
	if (dedicatedAllocation) {
		LOG("Dedicated allocations enabled\n");
		allocatorInfo.flags      = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	}

	vmaCreateAllocator(&allocatorInfo, &allocator);

	queue = device.getQueue(graphicsQueueIndex, 0);

	{
		auto surfacePresentModes_ = physicalDevice.getSurfacePresentModesKHR(surface);
		surfacePresentModes.reserve(surfacePresentModes_.size());
		LOG("%u present modes\n",   static_cast<uint32_t>(surfacePresentModes_.size()));

		for (const auto &presentMode : surfacePresentModes_) {
			LOG(" %s\n", vk::to_string(presentMode).c_str());
			surfacePresentModes.insert(presentMode);
		}
	}

	{
		auto surfaceFormats_ = physicalDevice.getSurfaceFormatsKHR(surface);

		LOG("%u surface formats\n", static_cast<uint32_t>(surfaceFormats_.size()));
		for (const auto &format : surfaceFormats_) {
			LOG(" %s\t%s\n", vk::to_string(format.format).c_str(), vk::to_string(format.colorSpace).c_str());
			if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				surfaceFormats.insert(format.format);
			}
		}
	}

	uint32_t maxSamples = static_cast<uint32_t>(deviceProperties.limits.framebufferColorSampleCounts);
	maxSamples &= static_cast<uint32_t>(deviceProperties.limits.framebufferDepthSampleCounts);
	maxSamples &= static_cast<uint32_t>(deviceProperties.limits.framebufferStencilSampleCounts);
	// TODO: what about sampledImage*SamplerCounts?
	// those should probably go in a separate variable
	// we want to count the number of lowest bits set to get highest AA level
	// TODO: there are better ways to do this
	// foro example negate and count trailing zeros
	for (unsigned int i = 0; i < 7; i++) {
		uint32_t bit = 1 << i;
		if (uint32_t(maxSamples) & bit) {
			features.maxMSAASamples = bit;
		} else {
			break;
		}
	}
	features.SSBOSupported  = true;

	recreateSwapchain();
	recreateRingBuffer(desc.ephemeralRingBufSize);

	acquireSem    = device.createSemaphore(vk::SemaphoreCreateInfo());
	renderDoneSem = device.createSemaphore(vk::SemaphoreCreateInfo());

	vk::CommandPoolCreateInfo cp;
	// TODO: transferQueueIndex
	cp.queueFamilyIndex = graphicsQueueIndex;
	transferCmdPool = device.createCommandPool(cp);

	// TODO: load pipeline cache
}


void RendererImpl::recreateRingBuffer(unsigned int newSize) {
	assert(newSize > 0);

	// if buffer already exists, free it after it's no longer in use
	if (ringBuffer) {
		assert(ringBufSize       != 0);
		assert(persistentMapping != nullptr);

		// create a Buffer object which we can put into deleteResources
		Buffer buffer;
		buffer.buffer          = ringBuffer;
		ringBuffer             = vk::Buffer();

		buffer.ringBufferAlloc = false;

		buffer.memory          = ringBufferMem;
		ringBufferMem          = VK_NULL_HANDLE;
		persistentMapping      = nullptr;

		buffer.size            = ringBufSize;
		ringBufSize            = 0;

		buffer.type            = BufferType::Everything;

		buffer.offset          = ringBufPtr;
		ringBufPtr             = 0;

		buffer.lastUsedFrame   = frameNum;

		deleteResources.emplace(std::move(buffer));
	}

	assert(!ringBuffer);
	assert(ringBufSize       == 0);
	assert(ringBufPtr        == 0);
	assert(persistentMapping == nullptr);
	ringBufSize = newSize;

	// create ringbuffer
	vk::BufferCreateInfo rbInfo;
	rbInfo.size  = newSize;
	rbInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferSrc;
	ringBuffer   = device.createBuffer(rbInfo);

	assert(ringBufferMem == nullptr);

	VmaAllocationCreateInfo req = {};
	req.flags          = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
	req.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
	req.pUserData      = const_cast<char *>("Ringbuffer");

	VmaAllocationInfo  allocationInfo = {};
	auto result = vmaAllocateMemoryForBuffer(allocator, ringBuffer, &req, &ringBufferMem, &allocationInfo);

	if (result != VK_SUCCESS) {
		LOG("vmaAllocateMemoryForBuffer failed: %s\n", vk::to_string(vk::Result(result)).c_str());
		throw std::runtime_error("vmaAllocateMemoryForBuffer failed");
	}

	LOG("ringbuffer memory type: %u\n",    allocationInfo.memoryType);
	LOG("ringbuffer memory offset: %u\n",  static_cast<unsigned int>(allocationInfo.offset));
	LOG("ringbuffer memory size: %u\n",    static_cast<unsigned int>(allocationInfo.size));
	assert(ringBufferMem != nullptr);
	assert(allocationInfo.offset == 0);
	assert(allocationInfo.pMappedData != nullptr);

	device.bindBufferMemory(ringBuffer, allocationInfo.deviceMemory, allocationInfo.offset);

	persistentMapping = reinterpret_cast<char *>(allocationInfo.pMappedData);
	assert(persistentMapping != nullptr);
}


RendererImpl::~RendererImpl() {
	assert(instance);
	assert(device);
	assert(surface);
	assert(swapchain);
	assert(ringBuffer);
	assert(persistentMapping);
	assert(transferCmdPool);

	// TODO: save pipeline cache

	// TODO: if last frame is still pending we could add deleted resources to its list

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		if (f.outstanding) {
			// wait until complete
			waitForFrame(i);
		}
		deleteFrameInternal(f);
	}
	frames.clear();

	for (auto &r : deleteResources) {
		this->deleteResourceInternal(const_cast<Resource &>(r));
	}
	deleteResources.clear();

	device.destroySemaphore(renderDoneSem);
	renderDoneSem = vk::Semaphore();

	device.destroySemaphore(acquireSem);
	acquireSem = vk::Semaphore();

	vmaFreeMemory(allocator, ringBufferMem);
	ringBufferMem = nullptr;
	persistentMapping = nullptr;
	device.destroyBuffer(ringBuffer);
	ringBuffer = vk::Buffer();

	buffers.clearWith([this](Buffer &b) {
		deleteBufferInternal(b);
	} );

	samplers.clearWith([this](Sampler &s) {
		deleteSamplerInternal(s);
	} );

	pipelines.clearWith([this](Pipeline &p) {
		this->device.destroyPipelineLayout(p.layout);
		p.layout = vk::PipelineLayout();
		this->device.destroyPipeline(p.pipeline);
		p.pipeline = vk::Pipeline();
	} );

	framebuffers.clearWith([this](Framebuffer &fb) {
		deleteFramebufferInternal(fb);
	} );

	renderPasses.clearWith([this](RenderPass &r) {
		deleteRenderPassInternal(r);
	} );

	vertexShaders.clearWith([this](VertexShader &v) {
		device.destroyShaderModule(v.shaderModule);
		v.shaderModule = vk::ShaderModule();
	} );

	fragmentShaders.clearWith([this](FragmentShader &f) {
		device.destroyShaderModule(f.shaderModule);
		f.shaderModule = vk::ShaderModule();
	} );

	dsLayouts.clearWith([this](DescriptorSetLayout &l) {
		this->device.destroyDescriptorSetLayout(l.layout);
		l.layout = vk::DescriptorSetLayout();
	} );

	renderTargets.clearWith([this](RenderTarget &rt) {
		deleteRenderTargetInternal(rt);
	} );

	textures.clearWith([this](Texture &tex) {
		deleteTextureInternal(tex);
	} );

	device.destroySwapchainKHR(swapchain);
	swapchain = vk::SwapchainKHR();

	instance.destroySurfaceKHR(surface);
	surface = vk::SurfaceKHR();

	vmaDestroyAllocator(allocator);
	allocator = VK_NULL_HANDLE;

	device.destroyCommandPool(transferCmdPool);
	transferCmdPool = vk::CommandPool();

	device.destroy();
	device = vk::Device();

	if (debugCallback) {
		instance.destroyDebugReportCallbackEXT(debugCallback);
		debugCallback = vk::DebugReportCallbackEXT();
	}

	instance.destroy();
	instance = vk::Instance();

	SDL_DestroyWindow(window);

	SDL_Quit();
}


bool RendererImpl::isRenderTargetFormatSupported(Format format) const {
	// TODO: cache these at startup
	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	if (isDepthFormat(format)) {
		flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	} else {
		flags |= vk::ImageUsageFlagBits::eColorAttachment;
	}
	vk::ImageFormatProperties prop;
	auto result = physicalDevice.getImageFormatProperties(vulkanFormat(format), vk::ImageType::e2D, vk::ImageTiling::eOptimal, flags, vk::ImageCreateFlags(), &prop);

	return (result == vk::Result::eSuccess);
}


BufferHandle RendererImpl::createBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	vk::BufferCreateInfo info;
	info.size  = size;
	info.usage = bufferTypeUsage(type) | vk::BufferUsageFlagBits::eTransferDst;

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer  = device.createBuffer(info);

	VmaAllocationCreateInfo req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForBuffer(allocator, buffer.buffer, &req, &buffer.memory, &allocationInfo);
	LOG("buffer memory type: %u\n",    allocationInfo.memoryType);
	LOG("buffer memory offset: %u\n",  static_cast<unsigned int>(allocationInfo.offset));
	LOG("buffer memory size: %u\n",    static_cast<unsigned int>(allocationInfo.size));
	assert(allocationInfo.size > 0);
	assert(allocationInfo.pMappedData == nullptr);
	device.bindBufferMemory(buffer.buffer, allocationInfo.deviceMemory, allocationInfo.offset);
	buffer.offset = static_cast<uint32_t>(allocationInfo.offset);
	buffer.size   = size;
	buffer.type   = type;

	// copy contents to GPU memory
	UploadOp op = allocateUploadOp(size);
	memcpy(static_cast<char *>(op.allocationInfo.pMappedData), contents, size);
	device.flushMappedMemoryRanges(vk::MappedMemoryRange(op.allocationInfo.deviceMemory, op.allocationInfo.offset, size));

	// TODO: reuse command buffer for multiple copies
	// TODO: use transfer queue instead of main queue
	vk::BufferCopy copyRegion;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size      = size;

	op.cmdBuf.copyBuffer(op.stagingBuffer, buffer.buffer, 1, &copyRegion);
	// TODO: share more of this stuff with createTexture
	op.cmdBuf.end();

	vk::SubmitInfo submit;
	submit.waitSemaphoreCount   = 0;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &op.cmdBuf;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &op.semaphore;

	queue.submit({ submit }, op.fence);

	uploads.emplace_back(std::move(op));

	return result.second;
}


BufferHandle RendererImpl::createEphemeralBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: separate ringbuffers based on type
	unsigned int beginPtr = ringBufferAllocate(size, bufferAlignment(type));

	memcpy(persistentMapping + beginPtr, contents, size);

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer          = ringBuffer;
	buffer.ringBufferAlloc = true;
	buffer.offset          = beginPtr;
	buffer.size            = size;
	buffer.type            = type;

	frames.at(currentFrameIdx).ephemeralBuffers.push_back(result.second);

	return result.second;
}


static vk::ImageLayout vulkanLayout(Layout l) {
	switch (l) {
	case Layout::Undefined:
		return vk::ImageLayout::eUndefined;

	case Layout::ShaderRead:
		return vk::ImageLayout::eShaderReadOnlyOptimal;

	case Layout::TransferSrc:
		return vk::ImageLayout::eTransferSrcOptimal;

	case Layout::TransferDst:
		return vk::ImageLayout::eTransferDstOptimal;
	}

	UNREACHABLE();
	return vk::ImageLayout::eUndefined;
}


FramebufferHandle RendererImpl::createFramebuffer(const FramebufferDesc &desc) {
	std::vector<vk::ImageView> attachmentViews;
	unsigned int width, height;

	// TODO: multiple render targets
	assert(desc.colors_[0]);
	assert(!desc.colors_[1]);
	// TODO: make sure renderPass formats match actual framebuffer attachments
	const auto &pass = renderPasses.get(desc.renderPass_);
	assert(pass.renderPass);
	{
		const auto &colorRT = renderTargets.get(desc.colors_[0]);
		assert(colorRT.width  > 0);
		assert(colorRT.height > 0);
		assert(colorRT.imageView);
		width  = colorRT.width;
		height = colorRT.height;
		attachmentViews.push_back(colorRT.imageView);
	}

	if (desc.depthStencil_) {
		const auto &depthRT = renderTargets.get(desc.depthStencil_);
		assert(depthRT.width  == width);
		assert(depthRT.height == height);
		assert(depthRT.imageView);
		attachmentViews.push_back(depthRT.imageView);
	}

	vk::FramebufferCreateInfo fbInfo;

	fbInfo.renderPass       = pass.renderPass;
	assert(!attachmentViews.empty());
	fbInfo.attachmentCount  = static_cast<uint32_t>(attachmentViews.size());
	fbInfo.pAttachments     = &attachmentViews[0];
	fbInfo.width            = width;
	fbInfo.height           = height;
	fbInfo.layers           = 1;  // TODO: multiple render targets?

	auto result     = framebuffers.add();
	Framebuffer &fb = result.first;
	fb.desc         = desc;
	fb.width        = width;
	fb.height       = height;
	fb.framebuffer  = device.createFramebuffer(fbInfo);

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerName;
		markerName.objectType  = vk::DebugReportObjectTypeEXT::eFramebuffer;
		markerName.object      = uint64_t(VkFramebuffer(fb.framebuffer));
		markerName.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerName);
	}

	return result.second;
}


static vk::SampleCountFlagBits sampleCountFlagsFromNum(unsigned int numSamples) {
	switch (numSamples) {
	case 1:
		return vk::SampleCountFlagBits::e1;

	case 2:
		return vk::SampleCountFlagBits::e2;

	case 4:
		return vk::SampleCountFlagBits::e4;

	case 8:
		return vk::SampleCountFlagBits::e8;

	case 16:
		return vk::SampleCountFlagBits::e16;

	case 32:
		return vk::SampleCountFlagBits::e32;

	case 64:
		return vk::SampleCountFlagBits::e64;

	}

	UNREACHABLE();
	return vk::SampleCountFlagBits::e1;
}


RenderPassHandle RendererImpl::createRenderPass(const RenderPassDesc &desc) {
	assert(!desc.name_.empty());

	vk::RenderPassCreateInfo info;
	vk::SubpassDescription subpass;

	std::vector<vk::AttachmentDescription> attachments;
	std::vector<vk::AttachmentReference> colorAttachments;

	auto result   = renderPasses.add();
	RenderPass &r = result.first;

	vk::SampleCountFlagBits samples = sampleCountFlagsFromNum(desc.numSamples_);

	// TODO: multiple render targets
	assert(desc.colorRTs_[0].format != Format::Invalid);
	assert(desc.colorRTs_[1].format == Format::Invalid);
	{
		uint32_t attachNum    = static_cast<uint32_t>(attachments.size());
		vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentDescription attach;
		attach.format         = vulkanFormat(desc.colorRTs_[0].format);
		attach.samples        = samples;
		switch (desc.colorRTs_[0].passBegin) {
		case PassBegin::DontCare:
			attach.loadOp         = vk::AttachmentLoadOp::eDontCare;
			attach.initialLayout  = vk::ImageLayout::eUndefined;
			break;

		case PassBegin::Keep:
			attach.loadOp         = vk::AttachmentLoadOp::eLoad;
			// TODO: should come from desc
			attach.initialLayout  = vk::ImageLayout::eTransferDstOptimal;
			break;

		case PassBegin::Clear:
			attach.loadOp         = vk::AttachmentLoadOp::eClear;
			attach.initialLayout  = vk::ImageLayout::eUndefined;
			std::array<float, 4> color = { { desc.colorRTs_[0].clearValue.x, desc.colorRTs_[0].clearValue.y, desc.colorRTs_[0].clearValue.z, desc.colorRTs_[0].clearValue.a } };
			r.clearValueCount = attachNum + 1;
			assert(r.clearValueCount <= 2);
			r.clearValues[attachNum].color = vk::ClearColorValue(color);
			break;
		}

		assert(desc.colorRTs_[1].passBegin == PassBegin::DontCare);
		assert(desc.colorRTs_[1].finalLayout == Layout::Undefined);

		attach.storeOp        = vk::AttachmentStoreOp::eStore;
		attach.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
		attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attach.finalLayout    = vulkanLayout(desc.colorRTs_[0].finalLayout);
		attachments.push_back(attach);

		vk::AttachmentReference ref;
		ref.attachment = attachNum;
		ref.layout     = layout;
		colorAttachments.push_back(ref);
	}
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	subpass.pColorAttachments    = &colorAttachments[0];

	bool hasDepthStencil = (desc.depthStencilFormat_ != Format::Invalid);
	vk::AttachmentReference depthAttachment;
	if (hasDepthStencil) {
		vk::ImageLayout layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::AttachmentDescription attach;
		uint32_t attachNum    = static_cast<uint32_t>(attachments.size());
		attach.format         = vulkanFormat(desc.depthStencilFormat_);
		attach.samples        = samples;
		// TODO: these should be customizable via RenderPassDesc
		attach.loadOp         = vk::AttachmentLoadOp::eDontCare;
		if (desc.clearDepthAttachment) {
			attach.loadOp     = vk::AttachmentLoadOp::eClear;
			r.clearValueCount = attachNum + 1;
			assert(r.clearValueCount <= 2);
			r.clearValues[attachNum].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
		}
		attach.storeOp        = vk::AttachmentStoreOp::eStore;
		// TODO: stencil
		attach.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
		attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attach.initialLayout  = vk::ImageLayout::eUndefined;
		// TODO: finalLayout should come from desc
		attach.finalLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments.push_back(attach);

		depthAttachment.attachment = attachNum;
		depthAttachment.layout     = layout;
		subpass.pDepthStencilAttachment = &depthAttachment;
	}

	info.attachmentCount = static_cast<uint32_t>(attachments.size());
	info.pAttachments    = &attachments[0];

	// no input attachments
	// no resolved attachments (multisample TODO)
	// no preserved attachments
	info.subpassCount    = 1;
	info.pSubpasses      = &subpass;

	// subpass dependencies (external)
	std::array<vk::SubpassDependency, 2> dependencies;
	{
		// access from before the pass
		vk::SubpassDependency &d = dependencies[0];
		d.srcSubpass       = VK_SUBPASS_EXTERNAL;
		d.dstSubpass       = 0;

		// TODO: should come from desc
		// depends on whether previous thing was rendering or msaa resolve
		d.srcStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eTransfer;
		d.srcAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eTransferWrite;

		d.dstStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		// TODO: shouldn't need read unless we load the attachment and use blending
		d.dstAccessMask    = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

		d.dependencyFlags  = vk::DependencyFlagBits::eByRegion;

		if (hasDepthStencil) {
			// TODO: should come from desc
			// depends on whether previous thing was rendering or msaa resolve
			d.srcStageMask    |= vk::PipelineStageFlagBits::eLateFragmentTests | vk::PipelineStageFlagBits::eTransfer;
			d.srcAccessMask   |= vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eTransferWrite;

			d.dstStageMask    |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
			d.dstAccessMask   |= vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		}
	}
	{
		// access after the pass
		vk::SubpassDependency &d = dependencies[1];
		d.srcSubpass       = 0;
		d.dstSubpass       = VK_SUBPASS_EXTERNAL;

		d.srcStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		d.srcAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite;

		assert(desc.colorRTs_[0].finalLayout != Layout::Undefined);
		assert(desc.colorRTs_[0].finalLayout != Layout::TransferDst);
		if (desc.colorRTs_[0].finalLayout == Layout::TransferSrc) {
			d.dstStageMask   = vk::PipelineStageFlagBits::eTransfer;
			d.dstAccessMask  = vk::AccessFlagBits::eTransferRead;
		} else {
			assert(desc.colorRTs_[0].finalLayout == Layout::ShaderRead);
			d.dstStageMask   = vk::PipelineStageFlagBits::eFragmentShader;
			d.dstAccessMask  = vk::AccessFlagBits::eShaderRead;
		}

		d.dependencyFlags  = vk::DependencyFlagBits::eByRegion;

		if (hasDepthStencil) {
			d.srcStageMask   |= vk::PipelineStageFlagBits::eLateFragmentTests;
			d.srcAccessMask  |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;

			d.dstStageMask   |= vk::PipelineStageFlagBits::eFragmentShader;
			d.dstAccessMask  |= vk::AccessFlagBits::eShaderRead;
		}
	}
	info.dependencyCount = 2;
	info.pDependencies   = &dependencies[0];

	r.renderPass  = device.createRenderPass(info);
	r.numSamples  = desc.numSamples_;
	r.desc        = desc;

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerName;
		markerName.objectType  = vk::DebugReportObjectTypeEXT::eRenderPass;
		markerName.object      = uint64_t(VkRenderPass(r.renderPass));
		markerName.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerName);
	}

	return result.second;
}


PipelineHandle RendererImpl::createPipeline(const PipelineDesc &desc) {
	vk::GraphicsPipelineCreateInfo info;

	const auto &v = vertexShaders.get(desc.vertexShader_);
	const auto &f = fragmentShaders.get(desc.fragmentShader_);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
	stages[0].stage  = vk::ShaderStageFlagBits::eVertex;
	stages[0].module = v.shaderModule;
	stages[0].pName  = "main";
	stages[1].stage  = vk::ShaderStageFlagBits::eFragment;
	stages[1].module = f.shaderModule;
	stages[1].pName  = "main";

	info.stageCount = 2;
	info.pStages = &stages[0];

	vk::PipelineVertexInputStateCreateInfo vinput;
	std::vector<vk::VertexInputAttributeDescription> attrs;
	std::vector<vk::VertexInputBindingDescription> bindings;
	if (desc.vertexAttribMask) {
		uint32_t bufmask = 0;

		uint32_t mask = desc.vertexAttribMask;
#ifdef __GNUC__
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
#else
		while (true) {
			unsigned long bit = 0;
			if (!_BitScanForward(&bit, mask)) {
				break;
			}
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
		assert(mask == 0);
#endif

		// currently we support only 1 buffer, TODO: need more?
		assert(bufmask == 1);
		assert(desc.vertexBuffers[0].stride != 0);
		vk::VertexInputBindingDescription bind;
		bind.binding   = 0;
		bind.stride    = desc.vertexBuffers[0].stride;
		bind.inputRate = vk::VertexInputRate::eVertex;
		bindings.push_back(bind);

		vinput.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
		vinput.pVertexBindingDescriptions      = &bindings[0];
		vinput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
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
	multisample.rasterizationSamples = sampleCountFlagsFromNum(desc.numSamples_);
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
			cb.blendEnable          = true;
			// TODO: get from Pipeline
			cb.srcColorBlendFactor  = vk::BlendFactor::eSrcAlpha;
			cb.dstColorBlendFactor  = vk::BlendFactor::eOneMinusSrcAlpha;
			cb.colorBlendOp         = vk::BlendOp::eAdd;
			cb.srcAlphaBlendFactor  = vk::BlendFactor::eOne;
			cb.dstAlphaBlendFactor  = vk::BlendFactor::eOne;
			cb.alphaBlendOp         = vk::BlendOp::eAdd;
		}
		cb.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendStates.push_back(cb);
	}
	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = static_cast<uint32_t>(colorBlendStates.size());
	blendInfo.pAttachments    = &colorBlendStates[0];
	info.pColorBlendState     = &blendInfo;

	vk::PipelineDynamicStateCreateInfo dyn;
	std::vector<vk::DynamicState> dynStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	dyn.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
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
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts    = &layouts[0];

	auto layout = device.createPipelineLayout(layoutInfo);
	info.layout = layout;

	const auto &renderPass = renderPasses.get(desc.renderPass_);
	info.renderPass = renderPass.renderPass;

	auto result = device.createGraphicsPipeline(vk::PipelineCache(), info);

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerName;
		markerName.objectType  = vk::DebugReportObjectTypeEXT::ePipeline;
		markerName.object      = uint64_t(VkPipeline(result));
		markerName.pObjectName = desc.name_.c_str();

		device.debugMarkerSetObjectNameEXT(&markerName);
	}

	if (amdShaderInfo) {
		vk::ShaderStatisticsInfoAMD stats;
		size_t dataSize = sizeof(stats);
		// TODO: other stages

		device.getShaderInfoAMD(result, vk::ShaderStageFlagBits::eVertex, vk::ShaderInfoTypeAMD::eStatistics, &dataSize, &stats);
		LOG("pipeline \"%s\" vertex SGPR %u VGPR %u\n", desc.name_.c_str(), stats.resourceUsage.numUsedSgprs, stats.resourceUsage.numUsedVgprs);

		device.getShaderInfoAMD(result, vk::ShaderStageFlagBits::eFragment, vk::ShaderInfoTypeAMD::eStatistics, &dataSize, &stats);
		LOG("pipeline \"%s\" fragment SGPR %u VGPR %u\n", desc.name_.c_str(), stats.resourceUsage.numUsedSgprs, stats.resourceUsage.numUsedVgprs);
	}

	auto id = pipelines.add();
	Pipeline &p = id.first;
	p.pipeline = result;
	p.layout   = layout;
	p.scissor  = desc.scissorTest_;

	return id.second;
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Format::Invalid);
	assert(isPow2(desc.numSamples_));
	assert(!desc.name_.empty());

	// TODO: use NV_dedicated_allocation when available

	vk::Format format = vulkanFormat(desc.format_);
	vk::ImageCreateInfo info;
	if (desc.additionalViewFormat_ != Format::Invalid) {
		info.flags   = vk::ImageCreateFlagBits::eMutableFormat;
	}
	info.imageType   = vk::ImageType::e2D;
	info.format      = format;
	info.extent      = vk::Extent3D(desc.width_, desc.height_, 1);
	info.mipLevels   = 1;
	info.arrayLayers = 1;
	info.samples     = sampleCountFlagsFromNum(desc.numSamples_);
	// TODO: usage should come from desc
	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	if (isDepthFormat(desc.format_)) {
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

	auto texResult   = textures.add();
	Texture &tex     = texResult.first;
	tex.width        = desc.width_;
	tex.height       = desc.height_;
	tex.image        = rt.image;
	tex.renderTarget = true;

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerNameImage;
		markerNameImage.objectType  = vk::DebugReportObjectTypeEXT::eImage;
		markerNameImage.object      = uint64_t(VkImage(tex.image));
		markerNameImage.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerNameImage);
	}

	VmaAllocationCreateInfo req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	req.flags          = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
	req.pUserData      = const_cast<char *>(desc.name_.c_str());
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForImage(allocator, rt.image, &req, &tex.memory, &allocationInfo);
	device.bindImageMemory(rt.image, allocationInfo.deviceMemory, allocationInfo.offset);

	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image    = rt.image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format   = format;
	if (isDepthFormat(desc.format_)) {
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	} else {
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	}
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	rt.imageView = device.createImageView(viewInfo);
	tex.imageView    = rt.imageView;

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerNameImageView;
		markerNameImageView.objectType  = vk::DebugReportObjectTypeEXT::eImageView;
		markerNameImageView.object      = uint64_t(VkImageView(tex.imageView));
		markerNameImageView.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerNameImageView);
	}

	// TODO: std::move ?
	rt.texture = texResult.second;

	if (desc.additionalViewFormat_ != Format::Invalid) {
		assert(isDepthFormat(desc.format_) == isDepthFormat(desc.additionalViewFormat_));
		auto viewResult   = textures.add();
		Texture &view     = viewResult.first;
		rt.additionalView = viewResult.second;
		view.width        = desc.width_;
		view.height       = desc.height_;
		view.image        = rt.image;
		view.renderTarget = true;

		viewInfo.format   = vulkanFormat(desc.additionalViewFormat_);
		view.imageView    = device.createImageView(viewInfo);

		if (debugMarkers) {
			std::string viewName = desc.name_ + " " + formatName(desc.additionalViewFormat_) + " view";
			vk::DebugMarkerObjectNameInfoEXT markerNameImageView;
			markerNameImageView.objectType  = vk::DebugReportObjectTypeEXT::eImageView;
			markerNameImageView.object      = uint64_t(VkImageView(view.imageView));
			markerNameImageView.pObjectName = viewName.c_str();
			device.debugMarkerSetObjectNameEXT(&markerNameImageView);
		}
	}

	return result.second;
}


static vk::Filter vulkanFiltermode(FilterMode m) {
	switch (m) {
	case FilterMode::Nearest:
        return vk::Filter::eNearest;

	case FilterMode::Linear:
        return vk::Filter::eLinear;
	}

	UNREACHABLE();

	return vk::Filter::eNearest;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc &desc) {
	vk::SamplerCreateInfo info;

	info.magFilter = vulkanFiltermode(desc.mag);
	info.minFilter = vulkanFiltermode(desc.min);

	vk::SamplerAddressMode m = vk::SamplerAddressMode::eClampToEdge;
	if (desc.wrapMode == WrapMode::Wrap) {
		m = vk::SamplerAddressMode::eRepeat;
	}
	info.addressModeU = m;
	info.addressModeV = m;
	info.addressModeW = m;

	auto result = samplers.add();
	struct Sampler &sampler = result.first;
	sampler.sampler    = device.createSampler(info);

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerName;
		markerName.objectType  = vk::DebugReportObjectTypeEXT::eSampler;
		markerName.object      = uint64_t(VkSampler(sampler.sampler));
		markerName.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerName);
	}

	return result.second;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

	ShaderMacros macros_(macros);
	macros_.emplace("VULKAN_FLIP", "1");

	std::vector<uint32_t> spirv = compileSpirv(vertexShaderName, macros_, shaderc_glsl_vertex_shader);

	auto result_ = vertexShaders.add();

	VertexShader &v = result_.first;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	v.shaderModule = device.createShaderModule(info);

	return result_.second;
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName   = name + ".frag";

	ShaderMacros macros_(macros);
	macros_.emplace("VULKAN_FLIP", "1");

	std::vector<uint32_t> spirv = compileSpirv(fragmentShaderName, macros_, shaderc_glsl_fragment_shader);

	auto result_ = fragmentShaders.add();

	FragmentShader &f = result_.first;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	f.shaderModule = device.createShaderModule(info);

	return result_.second;
}


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	// TODO: check PhysicalDeviceFormatProperties

	vk::Format format = vulkanFormat(desc.format_);

	vk::ImageCreateInfo info;
	info.imageType   = vk::ImageType::e2D;
	info.format      = format;
	info.extent      = vk::Extent3D(desc.width_, desc.height_, 1);
	info.mipLevels   = desc.numMips_;
	info.arrayLayers = 1;

	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	assert(!isDepthFormat(desc.format_));
	info.usage       = flags;

	auto result = textures.add();
	Texture &tex = result.first;
	tex.width  = desc.width_;
	tex.height = desc.height_;
	tex.image  = device.createImage(info);

	VmaAllocationCreateInfo  req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	req.flags          = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
	req.pUserData      = const_cast<char *>(desc.name_.c_str());
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForImage(allocator, tex.image, &req, &tex.memory, &allocationInfo);
	LOG("texture image memory type: %u\n",   allocationInfo.memoryType);
	LOG("texture image memory offset: %u\n", static_cast<unsigned int>(allocationInfo.offset));
	LOG("texture image memory size: %u\n",   static_cast<unsigned int>(allocationInfo.size));
	device.bindImageMemory(tex.image, allocationInfo.deviceMemory, allocationInfo.offset);

	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image    = tex.image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format   = format;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.levelCount = desc.numMips_;
	viewInfo.subresourceRange.layerCount = 1;
	tex.imageView = device.createImageView(viewInfo);

	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerNameImage;
		markerNameImage.objectType  = vk::DebugReportObjectTypeEXT::eImage;
		markerNameImage.object      = uint64_t(VkImage(tex.image));
		markerNameImage.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerNameImage);

		vk::DebugMarkerObjectNameInfoEXT markerNameImageView;
		markerNameImageView.objectType  = vk::DebugReportObjectTypeEXT::eImageView;
		markerNameImageView.object      = uint64_t(VkImageView(tex.imageView));
		markerNameImageView.pObjectName = desc.name_.c_str();
		device.debugMarkerSetObjectNameEXT(&markerNameImageView);
	}

	// TODO: reuse command buffer for multiple copies
	// TODO: use transfer queue instead of main queue
	// TODO: share more of this stuff with createBuffer
	unsigned int w = desc.width_, h = desc.height_;
	unsigned int bufferSize = 0;
	uint32_t align = std::max(formatSize(desc.format_), static_cast<uint32_t>(deviceProperties.limits.optimalBufferCopyOffsetAlignment));
	std::vector<vk::BufferImageCopy> regions;
	regions.reserve(desc.numMips_);
	for (unsigned int i = 0; i < desc.numMips_; i++) {
		assert(desc.mipData_[i].data != nullptr);
		assert(desc.mipData_[i].size != 0);
		unsigned int size = desc.mipData_[i].size;

		vk::ImageSubresourceLayers layers;
		layers.aspectMask = vk::ImageAspectFlagBits::eColor;
		layers.layerCount = 1;

		vk::BufferImageCopy region;
		region.bufferOffset     = bufferSize;
		// leave row length and image height 0 for tight packing
		region.imageSubresource = layers;
		region.imageExtent      = vk::Extent3D(w, h, 1);
		regions.push_back(region);

		// round size up for proper alignment
		size = size + align - 1;
		size = size / align;
		size = size * align;
		bufferSize += size;

		w = std::max(w / 2, 1u);
		h = std::max(h / 2, 1u);
	}
	UploadOp op = allocateUploadOp(bufferSize);

	// transition to transfer destination
	{
		vk::ImageSubresourceRange range;
		range.aspectMask            = vk::ImageAspectFlagBits::eColor;
		range.baseMipLevel          = 0;
		range.levelCount            = VK_REMAINING_MIP_LEVELS;
		range.baseArrayLayer        = 0;
		range.layerCount            = VK_REMAINING_ARRAY_LAYERS;

		vk::ImageMemoryBarrier barrier;
		barrier.srcAccessMask        = vk::AccessFlagBits();
		barrier.dstAccessMask        = vk::AccessFlagBits::eTransferWrite;
		barrier.oldLayout            = vk::ImageLayout::eUndefined;
		barrier.newLayout            = vk::ImageLayout::eTransferDstOptimal;
		barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
		barrier.image                = tex.image;
		barrier.subresourceRange     = range;

		// TODO: do we need a barrier for the staging buffer?
		// TODO: relax stage flag bits
		op.cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { barrier });

		w = desc.width_;
		h = desc.height_;
		char *mappedPtr = static_cast<char *>(op.allocationInfo.pMappedData);
		for (unsigned int i = 0; i < desc.numMips_; i++) {
			// copy contents to GPU memory
			memcpy(mappedPtr + regions[i].bufferOffset, desc.mipData_[i].data, desc.mipData_[i].size);
		}
		device.flushMappedMemoryRanges(vk::MappedMemoryRange(op.allocationInfo.deviceMemory, op.allocationInfo.offset, bufferSize));
		op.cmdBuf.copyBufferToImage(op.stagingBuffer, tex.image, vk::ImageLayout::eTransferDstOptimal, regions);

		// transition to shader use
		barrier.srcAccessMask       = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask       = vk::AccessFlagBits::eShaderRead;
		barrier.oldLayout           = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal;
		// TODO: relax stage flag bits
		op.cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { barrier });
	}

	// TODO: move this to helper method, shared with uploadBuffer
	op.cmdBuf.end();

	vk::SubmitInfo submit;
	submit.waitSemaphoreCount   = 0;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &op.cmdBuf;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &op.semaphore;

	queue.submit({ submit }, op.fence);

	uploads.emplace_back(std::move(op));

	return result.second;
}


DSLayoutHandle RendererImpl::createDescriptorSetLayout(const DescriptorLayout *layout) {
	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	unsigned int i = 0;
	std::vector<DescriptorLayout> descriptors;
	while (layout->type != DescriptorType::End) {
		vk::DescriptorSetLayoutBinding b;

		b.binding         = i;
		// TODO: make layout End last in enum so this is nicer
		b.descriptorType  = descriptorTypes[uint8_t(layout->type) - 1];
		b.descriptorCount = 1;
		// TODO: should specify stages in layout
		b.stageFlags      = vk::ShaderStageFlagBits::eAll;

		bindings.push_back(b);
		descriptors.push_back(*layout);

		layout++;
		i++;
	}
	assert(layout->offset == 0);

	vk::DescriptorSetLayoutCreateInfo info;
	info.bindingCount = static_cast<uint32_t>(bindings.size());
	info.pBindings    = &bindings[0];

	auto result = dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;
	dsLayout.layout = device.createDescriptorSetLayout(info);
	dsLayout.descriptors = std::move(descriptors);

	return result.second;
}


TextureHandle RendererImpl::getRenderTargetTexture(RenderTargetHandle handle) {
	const auto &rt = renderTargets.get(handle);

	return rt.texture;
}


TextureHandle RendererImpl::getRenderTargetView(RenderTargetHandle handle, Format /* f */) {
	const auto &rt = renderTargets.get(handle);

	const auto &tex = textures.get(rt.additionalView);
	assert(tex.renderTarget);
	//assert(tex.format == f);

	return rt.additionalView;
}


void RendererImpl::deleteBuffer(BufferHandle handle) {
	buffers.removeWith(handle, [this](struct Buffer &b) {
		// TODO: if b.lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(b));
	} );
}


void RendererImpl::deleteFramebuffer(FramebufferHandle handle) {
	framebuffers.removeWith(handle, [this](Framebuffer &fb) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(fb));
	} );
}


void RendererImpl::deleteRenderPass(RenderPassHandle handle) {
	renderPasses.removeWith(handle, [this](RenderPass &rp) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(rp));
	} );
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &handle) {
	renderTargets.removeWith(handle, [this](struct RenderTarget &rt) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(rt));
	} );
}


void RendererImpl::deleteSampler(SamplerHandle handle) {
	samplers.removeWith(handle, [this](struct Sampler &s) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(s));
	} );
}


void RendererImpl::deleteTexture(TextureHandle handle) {
	textures.removeWith(handle, [this](Texture &tex) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		this->deleteResources.emplace(std::move(tex));
	} );
}


void RendererImpl::setSwapchainDesc(const SwapchainDesc &desc) {
	bool changed = false;

	if (swapchainDesc.fullscreen != desc.fullscreen) {
		changed = true;
		if (desc.fullscreen) {
			// TODO: check return val?
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			LOG("Fullscreen\n");
		} else {
			SDL_SetWindowFullscreen(window, 0);
			LOG("Windowed\n");
		}
	}

	if (swapchainDesc.vsync != desc.vsync) {
		changed = true;
	}

	if (swapchainDesc.numFrames != desc.numFrames) {
		changed = true;
	}

	int w = -1, h = -1;
	SDL_Vulkan_GetDrawableSize(window, &w, &h);
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	if (static_cast<unsigned int>(w) != drawableSize.x
	 || static_cast<unsigned int>(h) != drawableSize.y) {
		changed = true;
	}

	if (changed) {
		wantedSwapchain = desc;
		swapchainDirty  = true;
		drawableSize    = glm::uvec2(w, h);
	}
}


static const unsigned int numPresentModes = 4;
static const std::array<vk::PresentModeKHR, numPresentModes> vsyncModes
= {{ vk::PresentModeKHR::eFifo
   , vk::PresentModeKHR::eMailbox
   , vk::PresentModeKHR::eFifoRelaxed
   , vk::PresentModeKHR::eImmediate }};

static const std::array<vk::PresentModeKHR, numPresentModes> lateSwapModes
= {{ vk::PresentModeKHR::eFifoRelaxed
   , vk::PresentModeKHR::eFifo
   , vk::PresentModeKHR::eMailbox
   , vk::PresentModeKHR::eImmediate }};

static const std::array<vk::PresentModeKHR, numPresentModes> nonVSyncModes
= {{ vk::PresentModeKHR::eImmediate
   , vk::PresentModeKHR::eMailbox
   , vk::PresentModeKHR::eFifoRelaxed
   , vk::PresentModeKHR::eFifo }};


static const std::array<vk::PresentModeKHR, numPresentModes> &vsyncMode(VSync mode) {
	switch (mode) {
	case VSync::On:
		return vsyncModes;

	case VSync::Off:
		return nonVSyncModes;

	case VSync::LateSwapTear:
		return lateSwapModes;

	}

	UNREACHABLE();
}


unsigned int RendererImpl::bufferAlignment(BufferType type) {
	switch (type) {
	case BufferType::Invalid:
		UNREACHABLE();
		break;

	case BufferType::Index:
		// TODO: can we find something more accurate?
		return 4;
		break;

	case BufferType::Uniform:
		return uboAlign;
		break;

	case BufferType::Storage:
		return ssboAlign;
		break;

	case BufferType::Vertex:
		// TODO: can we find something more accurate?
		return 16;
		break;

	case BufferType::Everything:
		// not supposed to be called
		assert(false);
		break;

	}

	UNREACHABLE();

	return 64;
}


void RendererImpl::recreateSwapchain() {
	assert(swapchainDirty);

	surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	LOG("image count min-max %u - %u\n", surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
	LOG("image extent min-max %ux%u - %ux%u\n", surfaceCapabilities.minImageExtent.width, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.width, surfaceCapabilities.maxImageExtent.height);
	LOG("current image extent %ux%u\n", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height);
	LOG("supported surface transforms: %s\n", vk::to_string(surfaceCapabilities.supportedTransforms).c_str());
	LOG("supported surface alpha composite flags: %s\n", vk::to_string(surfaceCapabilities.supportedCompositeAlpha).c_str());
	LOG("supported surface usage flags: %s\n", vk::to_string(surfaceCapabilities.supportedUsageFlags).c_str());

	int tempW = -1, tempH = -1;
	SDL_Vulkan_GetDrawableSize(window, &tempW, &tempH);
	if (tempW <= 0 || tempH <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	// this is nasty but apparently surface might not have resized yet
	// FIXME: find a better way
	unsigned int w = std::max(surfaceCapabilities.minImageExtent.width,  std::min(static_cast<unsigned int>(tempW), surfaceCapabilities.maxImageExtent.width));
	unsigned int h = std::max(surfaceCapabilities.minImageExtent.height, std::min(static_cast<unsigned int>(tempH), surfaceCapabilities.maxImageExtent.height));

	drawableSize = glm::uvec2(w, h);

	swapchainDesc.width  = w;
	swapchainDesc.height = h;

	unsigned int numImages = wantedSwapchain.numFrames;
	numImages = std::max(numImages, surfaceCapabilities.minImageCount);
	if (surfaceCapabilities.maxImageCount != 0) {
		numImages = std::min(numImages, surfaceCapabilities.maxImageCount);
	}

	LOG("Want %u images, using %u images\n", wantedSwapchain.numFrames, numImages);

	swapchainDesc.fullscreen = wantedSwapchain.fullscreen;
	swapchainDesc.numFrames  = numImages;
	swapchainDesc.vsync      = wantedSwapchain.vsync;

	if (frames.size() != numImages) {
		if (numImages < frames.size()) {
			// decreasing, delete old and resize
			for (unsigned int i = numImages; i < frames.size(); i++) {
				auto &f = frames.at(i);
				if (f.outstanding) {
					// wait until complete
					waitForFrame(i);
				}
				assert(!f.outstanding);
				// delete contents of Frame
				deleteFrameInternal(f);
			}
			frames.resize(numImages);
		} else {
			// increasing, resize and initialize new
			unsigned int oldSize = static_cast<unsigned int>(frames.size());
			frames.resize(numImages);

			// descriptor pool
			// TODO: these limits are arbitrary, find better ones
			std::vector<vk::DescriptorPoolSize> poolSizes;
			for (const auto t : descriptorTypes ) {
				vk::DescriptorPoolSize s;
				s.type            = t;
				s.descriptorCount = 32;
				poolSizes.push_back(s);
			}

			vk::DescriptorPoolCreateInfo dsInfo;
			dsInfo.maxSets       = 256;
			dsInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			dsInfo.pPoolSizes    = &poolSizes[0];

			vk::CommandPoolCreateInfo cp;
			cp.queueFamilyIndex = graphicsQueueIndex;

			for (unsigned int i = oldSize; i < frames.size(); i++) {
				auto &f = frames.at(i);
				assert(!f.fence);
				f.fence = device.createFence(vk::FenceCreateInfo());

				// we fill this in after we've created the swapchain
				assert(!f.image);

				assert(!f.dsPool);
				f.dsPool = device.createDescriptorPool(dsInfo);

				assert(!f.commandPool);
				f.commandPool = device.createCommandPool(cp);

				assert(!f.commandBuffer);
				assert(!f.presentCmdBuf);
				// create command buffer
				vk::CommandBufferAllocateInfo info(f.commandPool, vk::CommandBufferLevel::ePrimary, 2);
				auto bufs = device.allocateCommandBuffers(info);
				assert(bufs.size() == 2);
				f.commandBuffer = bufs.at(0);
				f.presentCmdBuf = bufs.at(1);
			}
		}
	}

	vk::Extent2D imageExtent;
	// TODO: check against min and max
	imageExtent.width  = swapchainDesc.width;
	imageExtent.height = swapchainDesc.height;

	if (!(surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)) {
		LOG("warning: identity transform not supported\n");
	}

	if (surfaceCapabilities.currentTransform != vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		LOG("warning: current transform is not identity\n");
	}

	if (!(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
		LOG("warning: opaque alpha not supported\n");
	}

	// FIFO is guaranteed to be supported
	vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
	// pick from the supported modes based on a prioritized
	// list depending on whether we want vsync or not
	// TODO: check against
	// https://timothylottes.github.io/20180202.html
	for (const auto presentMode : vsyncMode(swapchainDesc.vsync)) {
		if (surfacePresentModes.find(presentMode) != surfacePresentModes.end()) {
			swapchainPresentMode = presentMode;
			break;
		}
	}

	LOG("Using present mode %s\n", vk::to_string(swapchainPresentMode).c_str());

	// TODO: should fallback to Unorm and communicate back to demo
	vk::Format surfaceFormat = vk::Format::eB8G8R8A8Srgb;
	if (surfaceFormats.find(surfaceFormat) == surfaceFormats.end()) {
		throw std::runtime_error("No sRGB format backbuffer support");
	}
	features.sRGBFramebuffer = true;

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.flags                 = vk::SwapchainCreateFlagBitsKHR();
	swapchainCreateInfo.surface               = surface;
	swapchainCreateInfo.minImageCount         = numImages;
	swapchainCreateInfo.imageFormat           = surfaceFormat;
	swapchainCreateInfo.imageColorSpace       = vk::ColorSpaceKHR::eSrgbNonlinear;
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

	std::vector<vk::Image> swapchainImages = device.getSwapchainImagesKHR(swapchain);
	// TODO: implementation doesn't have to obey size, should resize here
	assert(swapchainImages.size() == frames.size());
	assert(swapchainImages.size() == numImages);

	for (unsigned int i = 0; i < numImages; i++) {
		frames.at(i).image = swapchainImages.at(i);
	}

	swapchainDirty = false;
}


MemoryStats RendererImpl::getMemStats() const {
	VmaStats vmaStats;
	memset(&vmaStats, 0, sizeof(VmaStats));
	vmaCalculateStats(allocator, &vmaStats);
	MemoryStats stats;
	stats.allocationCount    = vmaStats.total.allocationCount;
	stats.subAllocationCount = vmaStats.total.unusedRangeCount;
	stats.usedBytes          = vmaStats.total.usedBytes;
	stats.unusedBytes        = vmaStats.total.unusedBytes;
	return stats;
}


void RendererImpl::beginFrame() {
	assert(!inFrame);
	inFrame       = true;
	inRenderPass  = false;
	validPipeline = false;
	pipelineDrawn = true;

	if (swapchainDirty) {
		recreateSwapchain();
		assert(!swapchainDirty);
	}

	// acquire next image
	auto imageIdx_         = device.acquireNextImageKHR(swapchain, UINT64_MAX, acquireSem, vk::Fence());
	if (imageIdx_.result == vk::Result::eSuccess) {
		// nothing to do
	} else if (imageIdx_.result == vk::Result::eErrorOutOfDateKHR) {
		// swapchain went out of date during acquire, recreate and try again
		LOG("swapchain out of date during acquireNextImageKHR, recreating...\n");
		swapchainDirty = true;
		recreateSwapchain();
		assert(!swapchainDirty);

		imageIdx_ = device.acquireNextImageKHR(swapchain, UINT64_MAX, acquireSem, vk::Fence());
		if (imageIdx_.result != vk::Result::eSuccess) {
			// nope, still wrong
			LOG("acquireNextImageKHR failed: %s\n", vk::to_string(imageIdx_.result).c_str());
			throw std::runtime_error("acquireNextImageKHR failed");
		}
		LOG("swapchain recreated\n");
	} else {
		LOG("acquireNextImageKHR failed: %s\n", vk::to_string(imageIdx_.result).c_str());
		throw std::runtime_error("acquireNextImageKHR failed");
	}

	currentFrameIdx        = imageIdx_.value;
	assert(currentFrameIdx < frames.size());
	auto &frame            = frames.at(currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	if (frame.outstanding) {
		waitForFrame(currentFrameIdx);
	}
	assert(!frame.outstanding);

	device.resetFences( { frame.fence } );

	// set command buffer to recording
	currentCommandBuffer = frame.commandBuffer;
	currentCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	currentPipelineLayout = vk::PipelineLayout();

	// mark buffers deleted during gap between frames to be deleted when this frame has synced
	// TODO: we could move this earlier and add these to the previous frame's list
	if (!deleteResources.empty()) {
		assert(frame.deleteResources.empty());
		frame.deleteResources = std::move(deleteResources);
		assert(deleteResources.empty());
	}
}


void RendererImpl::presentFrame(RenderTargetHandle rtHandle) {
	assert(inFrame);
	inFrame = false;

	const auto &rt = renderTargets.get(rtHandle);

	auto &frame = frames.at(currentFrameIdx);
	device.resetFences( { frame.fence } );

	currentCommandBuffer.end();
	// TODO: this could be a baked buffer
	frame.presentCmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	vk::Image image        = frame.image;
	vk::ImageLayout layout = vk::ImageLayout::eTransferDstOptimal;

	// transition image to transfer dst optimal
	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask       = vk::AccessFlagBits();
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

	// TODO: add eComputeShader when implementing cs
	// TODO: reduce wait mask
	vk::PipelineStageFlags acquireWaitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	frame.presentCmdBuf.pipelineBarrier(acquireWaitStage, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, {}, {}, { barrier });

	vk::ImageBlit blit;
	blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[1]             = vk::Offset3D(rt.width, rt.height, 1);
	blit.dstSubresource            = blit.srcSubresource;
	blit.dstOffsets[1]             = blit.srcOffsets[1];

	// blit draw image to presentation image
	frame.presentCmdBuf.blitImage(rt.image, vk::ImageLayout::eTransferSrcOptimal, image, layout, { blit }, vk::Filter::eNearest);

	// transition to present
	barrier.srcAccessMask       = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask       = vk::AccessFlags();
	barrier.oldLayout           = layout;
	barrier.newLayout           = vk::ImageLayout::ePresentSrcKHR;
	barrier.image               = image;
	frame.presentCmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits::eByRegion, {}, {}, { barrier });
	frame.presentCmdBuf.end();

	// submit command buffers
	vk::SubmitInfo submit;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &currentCommandBuffer;
	std::vector<vk::Semaphore>          uploadSemaphores;
	std::vector<vk::PipelineStageFlags> semWaitMasks;
	if (!uploads.empty()) {
		LOG("%u uploads pending\n", static_cast<unsigned int>(uploads.size()));

		// use semaphores to make sure draw doesn't proceed until uploads are ready
		uploadSemaphores.reserve(uploads.size());
		semWaitMasks.reserve(uploads.size());
		for (auto &op : uploads) {
			uploadSemaphores.push_back(op.semaphore);
			semWaitMasks.push_back(vk::PipelineStageFlagBits::eTopOfPipe);
		}
		submit.waitSemaphoreCount   = uploadSemaphores.size();
		submit.pWaitSemaphores      = uploadSemaphores.data();
		submit.pWaitDstStageMask    = semWaitMasks.data();
	}

	vk::SubmitInfo submit2;
	submit2.waitSemaphoreCount   = 1;
	submit2.pWaitSemaphores      = &acquireSem;
	submit2.pWaitDstStageMask    = &acquireWaitStage;
	submit2.commandBufferCount   = 1;
	submit2.pCommandBuffers      = &frame.presentCmdBuf;
	submit2.signalSemaphoreCount = 1;
	submit2.pSignalSemaphores    = &renderDoneSem;

	queue.submit({ submit, submit2 }, frame.fence);

	// present
	vk::PresentInfoKHR presentInfo;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = &renderDoneSem;
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains        = &swapchain;
	presentInfo.pImageIndices      = &currentFrameIdx;

	auto presentResult = queue.presentKHR(&presentInfo);
	if (presentResult == vk::Result::eSuccess) {
		// nothing to do
	} else if (presentResult == vk::Result::eErrorOutOfDateKHR) {
		LOG("swapchain out of date during presentKHR, marking dirty\n");
		// swapchain went out of date during present, mark it dirty
		swapchainDirty = true;
	} else {
		LOG("presentKHR failed: %s\n", vk::to_string(presentResult).c_str());
		throw std::runtime_error("presentKHR failed");
	}
	frame.usedRingBufPtr = ringBufPtr;
	frame.outstanding = true;
	frame.lastFrameNum = frameNum;

	// mark buffers deleted during frame to be deleted when the frame has synced
	if (!deleteResources.empty()) {
		assert(frame.deleteResources.empty());
		frame.deleteResources = std::move(deleteResources);
		assert(deleteResources.empty());
	}

	if (!uploads.empty()) {
		assert(frame.uploads.empty());
		frame.uploads = std::move(uploads);
	}
	frameNum++;
}


void RendererImpl::waitForFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.outstanding);

	if (frame.uploads.empty()) {
	auto waitResult = device.waitForFences({ frame.fence }, true, 1000000000ull);
	if (waitResult != vk::Result::eSuccess) {
		// TODO: handle these somehow
		LOG("wait result is not success: %s\n", vk::to_string(waitResult).c_str());
		throw std::runtime_error("wait result is not success");
	}
	} else {
		std::vector<vk::Fence> fences;
		fences.reserve(frame.uploads.size() + 1);
		fences.push_back(frame.fence);
		for (auto &op : frame.uploads) {
			fences.push_back(op.fence);
		}

		vk::Result result;
		do {
			result = device.waitForFences(fences, true, 1000000000);
		} while (result == vk::Result::eTimeout);

		for (auto &op : frame.uploads) {
			device.destroyFence(op.fence);
			device.freeCommandBuffers(transferCmdPool, { op.cmdBuf } );
			device.destroySemaphore(op.semaphore);

			op.fence     = vk::Fence();
			op.cmdBuf    = vk::CommandBuffer();
			op.semaphore = vk::Semaphore();

			if (op.stagingBuffer) {
				assert(op.memory);

				device.destroyBuffer(op.stagingBuffer);
				vmaFreeMemory(allocator, op.memory);

				op.stagingBuffer = vk::Buffer();
				op.memory        = VK_NULL_HANDLE;
			} else {
				assert(!op.memory);
			}
		}

		assert(numUploads >= frame.uploads.size());
		numUploads -= frame.uploads.size();
		frame.uploads.clear();

		// if all pending uploads are complete, reset the command pool
		// TODO: should use multiple command pools
		if (numUploads == 0) {
			device.resetCommandPool(transferCmdPool, vk::CommandPoolResetFlags());
		}
	}

	frame.outstanding    = false;
	lastSyncedFrame      = std::max(lastSyncedFrame, frame.lastFrameNum);
	lastSyncedRingBufPtr = std::max(lastSyncedRingBufPtr, frame.usedRingBufPtr);

	// reset per-frame pools
	device.resetCommandPool(frame.commandPool, vk::CommandPoolResetFlags());
	device.resetDescriptorPool(frame.dsPool);

	for (auto &r : frame.deleteResources) {
		this->deleteResourceInternal(const_cast<Resource &>(r));
	}
	frame.deleteResources.clear();

	for (auto handle : frame.ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		assert(buffer.size   >  0);

		buffer.buffer          = vk::Buffer();
		buffer.ringBufferAlloc = false;
		buffer.memory          = 0;
		buffer.size            = 0;
		buffer.offset          = 0;
		buffer.lastUsedFrame   = 0;

		buffers.remove(handle);
	}
	frame.ephemeralBuffers.clear();
}


UploadOp RendererImpl::allocateUploadOp(uint32_t size) {
	UploadOp op;

	// TODO: have a free list of fences and semaphores instead of creating new ones all the time
	op.semaphore = device.createSemaphore(vk::SemaphoreCreateInfo());

	vk::FenceCreateInfo fci;
	op.fence = device.createFence(fci);

	vk::CommandBufferAllocateInfo cmdInfo(transferCmdPool, vk::CommandBufferLevel::ePrimary, 1);
	op.cmdBuf = device.allocateCommandBuffers(cmdInfo)[0];
	op.cmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	// TODO: move staging buffer, memory and command buffer allocation to allocateUploadOp
	vk::BufferCreateInfo bufInfo;
	bufInfo.size      = size;
	bufInfo.usage     = vk::BufferUsageFlagBits::eTransferSrc;
	op.stagingBuffer  = device.createBuffer(bufInfo);

	VmaAllocationCreateInfo req = {};
	req.usage         = VMA_MEMORY_USAGE_CPU_ONLY;
	req.flags         = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	req.pUserData     = nullptr;
	vmaAllocateMemoryForBuffer(allocator, op.stagingBuffer, &req, &op.memory, &op.allocationInfo);
	assert(op.allocationInfo.pMappedData);
	device.bindBufferMemory(op.stagingBuffer, op.allocationInfo.deviceMemory, op.allocationInfo.offset);

	numUploads++;

	return op;
}


void RendererImpl::deleteBufferInternal(Buffer &b) {
	assert(!b.ringBufferAlloc);
	assert(b.lastUsedFrame <= lastSyncedFrame);
	this->device.destroyBuffer(b.buffer);
	assert(b.memory != nullptr);
	vmaFreeMemory(this->allocator, b.memory);
	assert(b.type   != BufferType::Invalid);

	b.buffer          = vk::Buffer();
	b.ringBufferAlloc = false;
	b.memory          = 0;
	b.size            = 0;
	b.offset          = 0;
	b.lastUsedFrame   = 0;
	b.type            = BufferType::Invalid;
}


void RendererImpl::deleteFramebufferInternal(Framebuffer &fb) {
	device.destroyFramebuffer(fb.framebuffer);
	fb.framebuffer = vk::Framebuffer();
	fb.width       = 0;
	fb.height      = 0;
}


void RendererImpl::deleteRenderTargetInternal(RenderTarget &rt) {
	assert(rt.texture);
	auto &tex = this->textures.get(rt.texture);
	assert(tex.image == rt.image);
	assert(tex.imageView == rt.imageView);

	if (rt.additionalView) {
		auto &view = this->textures.get(rt.additionalView);
		assert(view.image     == rt.image);
		assert(view.imageView != tex.imageView);
		assert(view.imageView);
		assert(view.renderTarget);

		this->device.destroyImageView(view.imageView);

		view.image        = vk::Image();
		view.imageView    = vk::ImageView();
		view.renderTarget = false;

		this->textures.remove(rt.additionalView);
		rt.additionalView = TextureHandle();
	}

	tex.image        = vk::Image();
	tex.imageView    = vk::ImageView();
	tex.renderTarget = false;

	assert(tex.memory != nullptr);
	vmaFreeMemory(this->allocator, tex.memory);
	tex.memory = nullptr;

	this->textures.remove(rt.texture);
	rt.texture = TextureHandle();

	this->device.destroyImageView(rt.imageView);
	this->device.destroyImage(rt.image);
	rt.imageView = vk::ImageView();
	rt.image     = vk::Image();
}


void RendererImpl::deleteRenderPassInternal(RenderPass &rp) {
	this->device.destroyRenderPass(rp.renderPass);
	rp.renderPass = vk::RenderPass();
	rp.clearValueCount = 0;
	rp.numSamples      = 0;
}


void RendererImpl::deleteSamplerInternal(Sampler &s) {
	assert(s.sampler);
	this->device.destroySampler(s.sampler);
	s.sampler = vk::Sampler();
}


void RendererImpl::deleteTextureInternal(Texture &tex) {
	assert(!tex.renderTarget);
	this->device.destroyImageView(tex.imageView);
	this->device.destroyImage(tex.image);
	tex.imageView = vk::ImageView();
	tex.image     = vk::Image();
	assert(tex.memory != nullptr);
	vmaFreeMemory(this->allocator, tex.memory);
	tex.memory = nullptr;
}


void RendererImpl::deleteResourceInternal(Resource &r) {
	boost::apply_visitor(ResourceDeleter(this), r);
}


void RendererImpl::deleteFrameInternal(Frame &f) {
	assert(!f.outstanding);
	assert(f.fence);
	device.destroyFence(f.fence);
	f.fence = vk::Fence();

	// owned by swapchain, don't delete
	f.image = vk::Image();

	assert(f.dsPool);
	device.destroyDescriptorPool(f.dsPool);
	f.dsPool = vk::DescriptorPool();

	assert(f.commandBuffer);
	assert(f.presentCmdBuf);
	device.freeCommandBuffers(f.commandPool, { f.commandBuffer, f.presentCmdBuf });
	f.commandBuffer = vk::CommandBuffer();
	f.presentCmdBuf = vk::CommandBuffer();

	assert(f.commandPool);
	device.destroyCommandPool(f.commandPool);
	f.commandPool = vk::CommandPool();

	assert(f.deleteResources.empty());
}


void RendererImpl::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
	assert(inFrame);
	assert(!inRenderPass);
	inRenderPass  = true;
	validPipeline = false;

	const auto &pass = renderPasses.get(rpHandle);
	assert(pass.renderPass);
	const auto &fb   = framebuffers.get(fbHandle);
	assert(fb.framebuffer);
	assert(fb.width  > 0);
	assert(fb.height > 0);
	// clear image

	vk::RenderPassBeginInfo info;
	info.renderPass                = pass.renderPass;
	info.framebuffer               = fb.framebuffer;
	info.renderArea.extent.width   = fb.width;
	info.renderArea.extent.height  = fb.height;
	info.clearValueCount           = pass.clearValueCount;
	info.pClearValues              = &pass.clearValues[0];

	currentCommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);

	currentPipelineLayout = vk::PipelineLayout();
	currentRenderPass  = rpHandle;
	currentFramebuffer = fbHandle;
}


void RendererImpl::endRenderPass() {
	assert(inFrame);
	assert(inRenderPass);
	inRenderPass = false;

	currentCommandBuffer.endRenderPass();

	const auto &pass = renderPasses.get(currentRenderPass);
	const auto &fb = framebuffers.get(currentFramebuffer);

	// TODO: track depthstencil layout too
	auto &rt = renderTargets.get(fb.desc.colors_[0]);
	rt.currentLayout = pass.desc.colorRTs_[0].finalLayout;

	currentRenderPass = RenderPassHandle();
	currentFramebuffer = FramebufferHandle();
}


void RendererImpl::layoutTransition(RenderTargetHandle image, Layout src, Layout dest) {
	assert(image);
	assert(dest != Layout::Undefined);
	assert(src != dest);

	auto &rt = renderTargets.get(image);
	assert(src == Layout::Undefined || rt.currentLayout == src);
	rt.currentLayout = dest;

	vk::ImageMemoryBarrier b;
	// TODO: should allow user to specify access flags
	b.srcAccessMask               = vk::AccessFlagBits::eColorAttachmentWrite;
	b.dstAccessMask               = vk::AccessFlagBits::eMemoryRead;
	b.oldLayout                   = vulkanLayout(src);
	b.newLayout                   = vulkanLayout(dest);
	b.image                       = rt.image;
	b.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	b.subresourceRange.levelCount = 1;
	b.subresourceRange.layerCount = 1;

	// TODO: should allow user to specify stage masks
	currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { b });
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(inFrame);
	assert(inRenderPass);
	assert(pipelineDrawn);
	pipelineDrawn = false;
	validPipeline = true;
	scissorSet = false;

	// TODO: make sure current renderpass matches the one in pipeline

	const auto &p = pipelines.get(pipeline);
	currentCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p.pipeline);
	currentPipelineLayout = p.layout;

	if (!p.scissor) {
		// Vulkan always requires a scissor rect
		// if we don't use scissor set default here
		// TODO: shouldn't need this is previous pipeline didn't use scissor
		// except for first pipeline of the command buffer
		vk::Rect2D rect;
		rect.offset.x      = static_cast<int32_t>(currentViewport.x);
		rect.offset.y      = static_cast<int32_t>(currentViewport.y);
		rect.extent.width  = static_cast<uint32_t>(currentViewport.width);
		rect.extent.height = static_cast<uint32_t>(currentViewport.height);

		currentCommandBuffer.setScissor(0, 1, &rect);
		scissorSet = true;
	}
}


void RendererImpl::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	assert(inFrame);
	assert(validPipeline);

	auto &b = buffers.get(buffer);
	b.lastUsedFrame = frameNum;
	assert(b.type == BufferType::Index);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.offset;
	}
	currentCommandBuffer.bindIndexBuffer(b.buffer, offset, bit16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
}


void RendererImpl::bindVertexBuffer(unsigned int binding, BufferHandle buffer) {
	assert(inFrame);
	assert(validPipeline);

	auto &b = buffers.get(buffer);
	b.lastUsedFrame = frameNum;
	assert(b.type == BufferType::Vertex);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.offset;
	}
	currentCommandBuffer.bindVertexBuffers(binding, 1, &b.buffer, &offset);
}


void RendererImpl::bindDescriptorSet(unsigned int dsIndex, DSLayoutHandle layoutHandle, const void *data_) {
	assert(inFrame);
	assert(validPipeline);

	const DescriptorSetLayout &layout = dsLayouts.get(layoutHandle);

	vk::DescriptorSetAllocateInfo dsInfo;
	dsInfo.descriptorPool      = frames.at(currentFrameIdx).dsPool;
	dsInfo.descriptorSetCount  = 1;
	dsInfo.pSetLayouts         = &layout.layout;

	vk::DescriptorSet ds = device.allocateDescriptorSets(dsInfo)[0];

	std::vector<vk::WriteDescriptorSet>   writes;
	std::vector<vk::DescriptorBufferInfo> bufferWrites;
	std::vector<vk::DescriptorImageInfo>  imageWrites;

	unsigned int numWrites = static_cast<unsigned int>(layout.descriptors.size());
	writes.reserve(numWrites);
	bufferWrites.reserve(numWrites);
	imageWrites.reserve(numWrites);

	const char *data = reinterpret_cast<const char *>(data_);
	unsigned int index = 0;
	for (const auto &l : layout.descriptors) {
		vk::WriteDescriptorSet write;
		write.dstSet          = ds;
		write.dstBinding      = index;
		write.descriptorCount = 1;
		// TODO: move to a helper function
		write.descriptorType  = descriptorTypes[uint8_t(l.type) - 1];

		switch (l.type) {
		case DescriptorType::End:
			// can't happen because createDesciptorSetLayout doesn't let it
			UNREACHABLE();
			break;

		case DescriptorType::UniformBuffer:
		case DescriptorType::StorageBuffer: {
			// this is part of the struct, we know it's correctly aligned and right type
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			Buffer &buffer = buffers.get(handle);
			assert(buffer.size > 0);
			buffer.lastUsedFrame = frameNum;
			assert((buffer.type == BufferType::Uniform && l.type == DescriptorType::UniformBuffer)
			    || (buffer.type == BufferType::Storage && l.type == DescriptorType::StorageBuffer));

			vk::DescriptorBufferInfo  bufWrite;
			bufWrite.buffer = buffer.buffer;
			bufWrite.offset = buffer.offset;
			bufWrite.range  = buffer.size;

			// we trust that reserve() above makes sure this doesn't reallocate the storage
			bufferWrites.push_back(bufWrite);

			write.pBufferInfo = &bufferWrites.back();

			writes.push_back(write);
		} break;

		case DescriptorType::Sampler: {
			const auto &sampler = samplers.get(*reinterpret_cast<const SamplerHandle *>(data + l.offset));
			assert(sampler.sampler);

			vk::DescriptorImageInfo imgWrite;
			imgWrite.sampler = sampler.sampler;

			// we trust that reserve() above makes sure this doesn't reallocate the storage
			imageWrites.push_back(imgWrite);

			write.pImageInfo = &imageWrites.back();

			writes.push_back(write);
		} break;

		case DescriptorType::Texture: {
			TextureHandle texHandle = *reinterpret_cast<const TextureHandle *>(data + l.offset);
			const auto &tex = textures.get(texHandle);
			assert(tex.image);
			assert(tex.imageView);

			vk::DescriptorImageInfo imgWrite;
			imgWrite.imageView   = tex.imageView;
			imgWrite.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

			// we trust that reserve() above makes sure this doesn't reallocate the storage
			imageWrites.push_back(imgWrite);

			write.pImageInfo = &imageWrites.back();

			writes.push_back(write);
		} break;

		case DescriptorType::CombinedSampler: {
			const CSampler &combined = *reinterpret_cast<const CSampler *>(data + l.offset);

			const Texture &tex = textures.get(combined.tex);
			assert(tex.image);
			assert(tex.imageView);
			const Sampler &s   = samplers.get(combined.sampler);
			assert(s.sampler);

			vk::DescriptorImageInfo  imgWrite;
			imgWrite.sampler      = s.sampler;
			imgWrite.imageView    = tex.imageView;
			imgWrite.imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal;

			// we trust that reserve() above makes sure this doesn't reallocate the storage
			imageWrites.push_back(imgWrite);

			write.pImageInfo = &imageWrites.back();

			writes.push_back(write);
		} break;

		case DescriptorType::Count:
			UNREACHABLE(); // shouldn't happen
			break;

		}

		index++;
	}

	device.updateDescriptorSets(writes, {});
	currentCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, currentPipelineLayout, dsIndex, { ds }, {});
}


void RendererImpl::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(inFrame);

	currentViewport.x        = static_cast<float>(x);
	// TODO: check viewport y direction when not using full height
	currentViewport.y        = static_cast<float>(y);
	currentViewport.width    = static_cast<float>(width);
	currentViewport.height   = static_cast<float>(height);
	currentViewport.maxDepth = 1.0f;
	currentCommandBuffer.setViewport(0, 1, &currentViewport);
}


void RendererImpl::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(validPipeline);
	scissorSet = true;

	vk::Rect2D rect;
	rect.offset.x      = x;
	rect.offset.y      = y;
	rect.extent.width  = width;
	rect.extent.height = height;

	currentCommandBuffer.setScissor(0, 1, &rect);
}


void RendererImpl::resolveMSAA(FramebufferHandle source, FramebufferHandle target) {
	assert(source);
	assert(target);

	assert(!inRenderPass);

	const auto &srcFb = framebuffers.get(source);
	assert(srcFb.width       >  0);
	assert(srcFb.height      >  0);

	const auto &destFb = framebuffers.get(target);
	assert(destFb.width      >  0);
	assert(destFb.height     >  0);

	assert(srcFb.width       == destFb.width);
	assert(srcFb.height      == destFb.height);

	// must have exactly 1 color target
	// TODO: more targets
	// or allow picking which one
	assert(srcFb.desc.colors_[0]);
	assert(!srcFb.desc.colors_[1]);
	assert(destFb.desc.colors_[0]);
	assert(!destFb.desc.colors_[1]);

	auto &srcColor  = renderTargets.get(srcFb.desc.colors_[0]);
	assert(srcColor.currentLayout == Layout::TransferSrc);
	auto &destColor = renderTargets.get(destFb.desc.colors_[0]);
	assert(destColor.currentLayout == Layout::TransferDst);

	vk::ImageResolve r;
	r.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	r.srcSubresource.layerCount = 1;
	r.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	r.dstSubresource.layerCount = 1;
	r.extent.width              = srcFb.width;
	r.extent.height             = srcFb.height;
	r.extent.depth              = 1;
	currentCommandBuffer.resolveImage(srcColor.image, vk::ImageLayout::eTransferSrcOptimal, destColor.image, vk::ImageLayout::eTransferDstOptimal, { r } );
}


void RendererImpl::draw(unsigned int firstVertex, unsigned int vertexCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	pipelineDrawn = true;

	currentCommandBuffer.draw(vertexCount, 1, firstVertex, 0);
}


void RendererImpl::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(instanceCount > 0);
	pipelineDrawn = true;

	currentCommandBuffer.drawIndexed(vertexCount, instanceCount, 0, 0, 0);
}


void RendererImpl::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	pipelineDrawn = true;

	currentCommandBuffer.drawIndexed(vertexCount, 1, firstIndex, 0, 0);
}


} // namespace renderer


#endif  // RENDERER_VULKAN
