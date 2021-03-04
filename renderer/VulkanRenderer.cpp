/*
Copyright (c) 2015-2021 Alternative Games Ltd / Turo Lamminen

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


namespace renderer {


struct ResourceHasher final {
	size_t operator()(const Buffer &b) const {
		return b.getHash();
	}

	size_t operator()(const Framebuffer &fb) const {
		return fb.getHash();
	}

	size_t operator()(const Pipeline &p) const {
		return p.getHash();
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


template <typename T> void RendererImpl::debugNameObject(T handle, const std::string &name) {
	if (debugMarkers) {
		vk::DebugMarkerObjectNameInfoEXT markerName;
		markerName.objectType  = T::debugReportObjectType;
		markerName.object      = uint64_t(typename T::CType(handle));
		markerName.pObjectName = name.c_str();
		device.debugMarkerSetObjectNameEXT(&markerName, dispatcher);
	}
}


static const std::array<vk::DescriptorType, DescriptorType::_size()> descriptorTypes =
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

		HEDLEY_UNREACHABLE();
		return vk::Format::eUndefined;

	case VtxFormat::UNorm8:
		assert(count == 4);
		return vk::Format::eR8G8B8A8Unorm;

	}

	HEDLEY_UNREACHABLE();
	return vk::Format::eUndefined;
}


static vk::Format vulkanFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();
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

	case Format::BGRA8:
		return vk::Format::eB8G8R8A8Unorm;

	case Format::sBGRA8:
		return vk::Format::eB8G8R8A8Srgb;

	case Format::RG16Float:
		return vk::Format::eR16G16Sfloat;

	case Format::RGBA16Float:
		return vk::Format::eR16G16B16A16Sfloat;

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

	HEDLEY_UNREACHABLE();
	return vk::Format::eUndefined;
}


vk::BufferUsageFlags bufferTypeUsage(BufferType type) {
	vk::BufferUsageFlags flags;
	switch (type) {
	case BufferType::Invalid:
		HEDLEY_UNREACHABLE();
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


static VkBool32 VKAPI_PTR debugMessengerFunc(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /* messageTypes */, const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void * /* pUserData*/) {
	LOG("error of severity \"{}\" {} \"{}\" \"{}\"", vk::to_string(vk::DebugUtilsMessageSeverityFlagBitsEXT(severity)), callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
	// TODO: log other parts of VkDebugUtilsMessengerCallbackDataEXT
	logFlush();

	// make errors fatal
	switch (vk::DebugUtilsMessageSeverityFlagBitsEXT(severity)) {
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
		abort();

	default:
		break;
	}

	return VK_FALSE;
}


static VkBool32 VKAPI_PTR debugCallbackFunc(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t /* messageCode */, const char * pLayerPrefix, const char * pMessage, void * /* pUserData*/) {
	LOG("layer {} {} object {} type {} location {}: {}", pLayerPrefix, vk::to_string(vk::DebugReportFlagBitsEXT(flags)), object, vk::to_string(vk::DebugReportObjectTypeEXT(objectType)), location, pMessage);
	logFlush();

	// make errors fatal
	switch (vk::DebugReportFlagBitsEXT(flags)) {
	case vk::DebugReportFlagBitsEXT::eWarning:
	case vk::DebugReportFlagBitsEXT::eError:
		abort();

	default:
		break;
	}

	return VK_FALSE;
}


static uint32_t makeVulkanVersion(const Version &v) {
	return VK_MAKE_VERSION(v.major, v.minor, v.patch);
};


RendererImpl::RendererImpl(const RendererDesc &desc)
: RendererBase(desc)
, physicalDeviceIndex(0)
, graphicsQueueIndex(0)
, transferQueueIndex(0)
, numUploads(0)
, amdShaderInfo(false)
, debugMarkers(false)
, portabilitySubset(false)
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
	LOG("Number of displays detected: {}", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int retval = SDL_GetDesktopDisplayMode(i, &mode);
		if (retval == 0) {
			LOG("Desktop mode for display {}: {}x{}, refresh {} Hz", i, mode.w, mode.h, mode.refresh_rate);
			currentRefreshRate = mode.refresh_rate;
		} else {
			LOG("Failed to get desktop display mode for display {}", i);
		}

		int numModes = SDL_GetNumDisplayModes(i);
		LOG("Number of display modes for display {} : {}", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			LOG("Display mode {} : width {}, height {}, BPP {}, refresh {} Hz", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
			maxRefreshRate = std::max(static_cast<unsigned int>(mode.refresh_rate), maxRefreshRate);
		}
	}

	int flags = SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_VULKAN;
	if (desc.swapchain.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow(desc.applicationName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	if (!window) {
		LOG("SDL_CreateWindow failed: {}", SDL_GetError());
		throw std::runtime_error("SDL_CreateWindow failed");
	}

	{
		uint32_t instanceVersion = 0;
		auto result = vk::enumerateInstanceVersion(&instanceVersion);
		if (result != vk::Result::eSuccess) {
			LOG("Failed to get Vulkan instance version: {}", vk::to_string(result));
			throw std::runtime_error("Failed to get Vulkan instance version");
		}

		LOG("Vulkan instance version {}.{}.{}", VK_VERSION_MAJOR(instanceVersion), VK_VERSION_MINOR(instanceVersion), VK_VERSION_PATCH(instanceVersion));
	}

	HashSet<std::string>  instanceExtensions;
	{
		auto extensions = vk::enumerateInstanceExtensionProperties();
		std::sort(extensions.begin(), extensions.end()
		  , [] (const vk::ExtensionProperties &a, const vk::ExtensionProperties &b) {
			  return strcmp(a.extensionName, b.extensionName) < 0;
		});

		instanceExtensions.reserve(extensions.size());

		size_t maxLen = 0;
		for (const auto &ext : extensions) {
			maxLen = std::max(strlen(ext.extensionName), maxLen);
			instanceExtensions.insert(ext.extensionName);
		}

		LOG("Instance extensions:");
		std::vector<char> padding;
		padding.reserve(maxLen + 1);
		for (unsigned int i = 0; i < maxLen; i++) {
			padding.push_back(' ');
		}
		padding.push_back('\0');
		for (const auto &ext : extensions) {
			LOG(" {} {} {}", ext.extensionName.data(), &padding[strnlen(ext.extensionName, maxLen)], ext.specVersion);
		}
	}

	HashSet<std::string>  instanceLayers;
	{
		auto layers = vk::enumerateInstanceLayerProperties();
		std::sort(layers.begin(), layers.end()
		  , [] (const vk::LayerProperties &a, const vk::LayerProperties &b) {
			  return strcmp(a.layerName, b.layerName) < 0;
		});

		instanceLayers.reserve(layers.size());

		size_t maxLen = 0;
		for (const auto &l : layers) {
			maxLen = std::max(strlen(l.layerName), maxLen);
			instanceLayers.insert(l.layerName);
		}

		LOG("Instance layers:");
		std::vector<char> padding;
		padding.reserve(maxLen + 1);
		for (unsigned int i = 0; i < maxLen; i++) {
			padding.push_back(' ');
		}
		padding.push_back('\0');
		for (const auto &l : layers) {
			LOG(" {} {} (version {},\tspec {}.{}.{})", l.layerName.data(), &padding[strnlen(l.layerName, maxLen)], l.implementationVersion, VK_VERSION_MAJOR(l.specVersion), VK_VERSION_MINOR(l.specVersion), VK_VERSION_PATCH(l.specVersion));
		}
	}

	unsigned int numExtensions = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, NULL)) {
		LOG("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
		throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
	}

	std::vector<const char *> extensions(numExtensions, nullptr);

	if (instanceExtensions.find(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) != instanceExtensions.end()) {
		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	if(!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, &extensions[0])) {
		LOG("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
		throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
	}

	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName    = desc.applicationName.c_str();
	appInfo.applicationVersion  = makeVulkanVersion(desc.applicationVersion);
	appInfo.pEngineName         = desc.engineName.c_str();
	appInfo.engineVersion       = makeVulkanVersion(desc.engineVersion);
	appInfo.apiVersion          = VK_MAKE_VERSION(1, 0, 24);

	vk::InstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.pApplicationInfo         = &appInfo;

	std::vector<const char *> validationLayers;

	bool newDebugExtension = false;
	if (enableValidation) {
		const char *khronosValidation = "VK_LAYER_KHRONOS_validation";
		const char *lunargValidation  = "VK_LAYER_LUNARG_standard_validation";
		if (instanceLayers.find(khronosValidation) != instanceLayers.end()) {
			validationLayers.push_back(khronosValidation);
			LOG("Using KHRONOS validation layer");
		} else if (instanceLayers.find(lunargValidation) != instanceLayers.end()) {
			validationLayers.push_back(lunargValidation);
			LOG("Using LUNARG validation layer");
		} else {
			LOG("Validation requested but no validation layer available");
			throw std::runtime_error("Validation requested but no validation layer available");
		}

		if (instanceExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != instanceExtensions.end()) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			newDebugExtension = true;
		} else if (instanceExtensions.find(VK_EXT_DEBUG_REPORT_EXTENSION_NAME) != instanceExtensions.end()) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		} else {
			LOG("Validation requested but no debug reporting extension available");
			throw std::runtime_error("Validation requested but no debug reporting extension available");
		}
		instanceCreateInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	LOG("Active instance extensions:");
	for (const auto ext : extensions) {
		LOG(" {}", ext);
	}

	instanceCreateInfo.enabledExtensionCount    = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames  = &extensions[0];

	instance = vk::createInstance(instanceCreateInfo);

#if VK_HEADER_VERSION < 99

	dispatcher.init(instance);

#else  // VK_HEADER_VERSION

	auto getInstanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
	dispatcher.init(instance, getInstanceProc);

#endif  // VK_HEADER_VERSION

	if (enableValidation) {
		if (newDebugExtension) {
			vk::DebugUtilsMessengerCreateInfoEXT messengerInfo;
			messengerInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
			messengerInfo.messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
			messengerInfo.pfnUserCallback = debugMessengerFunc;

			debugUtilsCallback = instance.createDebugUtilsMessengerEXT(messengerInfo, nullptr, dispatcher);
		} else {
			vk::DebugReportCallbackCreateInfoEXT callbackInfo;
			callbackInfo.flags       = vk::DebugReportFlagBitsEXT::eError;
			callbackInfo.pfnCallback = debugCallbackFunc;
			debugReportCallback      = instance.createDebugReportCallbackEXT(callbackInfo, nullptr, dispatcher);
		}
	}

	if(!SDL_Vulkan_CreateSurface(window,
								 (SDL_vulkanInstance) instance,
								 (SDL_vulkanSurface *)&surface))
	{
		LOG("Failed to create Vulkan surface: {}", SDL_GetError());
		// TODO: free instance, window etc...
		throw std::runtime_error("Failed to create Vulkan surface");
	}

	std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
	if (physicalDevices.empty()) {
		LOG("No physical Vulkan devices found");
		instance.destroy();
		instance = nullptr;
		SDL_DestroyWindow(window);
		window = nullptr;
		throw std::runtime_error("No physical Vulkan devices found");
	}
	LOG("{} physical devices", physicalDevices.size());

	std::vector<vk::PhysicalDeviceProperties> physicalDevicesProperties;
	physicalDevicesProperties.reserve(physicalDevices.size());

	for (unsigned int i = 0; i < physicalDevices.size(); i++) {
		physicalDevice = physicalDevices.at(i);
		deviceProperties = physicalDevice.getProperties();
		physicalDevicesProperties.push_back(deviceProperties);

		LOG(" {}: \"{}\"", i, deviceProperties.deviceName.data());
		LOG("  Device API version {}.{}.{}", VK_VERSION_MAJOR(deviceProperties.apiVersion), VK_VERSION_MINOR(deviceProperties.apiVersion), VK_VERSION_PATCH(deviceProperties.apiVersion));
		LOG("  Driver version {}.{}.{} ({}) ({:#08x})", VK_VERSION_MAJOR(deviceProperties.driverVersion), VK_VERSION_MINOR(deviceProperties.driverVersion), VK_VERSION_PATCH(deviceProperties.driverVersion), deviceProperties.driverVersion, deviceProperties.driverVersion);
		LOG("  VendorId {:#x}", deviceProperties.vendorID);
		LOG("  DeviceId {:#x}", deviceProperties.deviceID);
		LOG("  Type {}", vk::to_string(deviceProperties.deviceType));
		LOG("  Name \"{}\"", deviceProperties.deviceName.data());
		LOG("  uniform buffer alignment {}", deviceProperties.limits.minUniformBufferOffsetAlignment);
		LOG("  storage buffer alignment {}", deviceProperties.limits.minStorageBufferOffsetAlignment);
		LOG("  texel buffer alignment {}",   deviceProperties.limits.minTexelBufferOffsetAlignment);
		std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
		LOG("  {} queue families", queueProps.size());

		bool canPresent = false;
		for (uint32_t j = 0; j < queueProps.size(); j++) {
			if (physicalDevice.getSurfaceSupportKHR(j, surface)) {
				canPresent = true;
				break;
			}
		}
		LOG("  {} present to our surface\n", canPresent ? "can" : "can NOT");
	}

	assert(physicalDevicesProperties.size() == physicalDevices.size());

	if (!desc.vulkanDeviceFilter.empty()) {
		LOG("Filtering vulkan device list for \"{}\"", desc.vulkanDeviceFilter);
		bool found = false;
		for (unsigned int i = 0; i < physicalDevicesProperties.size(); i++) {
			if (desc.vulkanDeviceFilter == physicalDevicesProperties[i].deviceName.data()) {
				physicalDeviceIndex = i;
				found = true;
				break;
			}
		}

		if (!found) {
			LOG("Didn't find physical device matching filter");
			logFlush();
			throw std::runtime_error("Didn't find physical device matching filter");
		}
	}

	physicalDevice = physicalDevices.at(physicalDeviceIndex);
	deviceProperties = physicalDevicesProperties.at(physicalDeviceIndex);

	LOG("Using physical device {} \"{}\"", physicalDeviceIndex, deviceProperties.deviceName.data());

	uboAlign  = static_cast<unsigned int>(deviceProperties.limits.minUniformBufferOffsetAlignment);
	ssboAlign = static_cast<unsigned int>(deviceProperties.limits.minStorageBufferOffsetAlignment);

	deviceFeatures = physicalDevice.getFeatures();

	memoryProperties = physicalDevice.getMemoryProperties();
	LOG("{} memory types", memoryProperties.memoryTypeCount);
	for (unsigned int i = 0; i < memoryProperties.memoryTypeCount; i++ ) {
		LOG(" {}  heap {}  {}", i, memoryProperties.memoryTypes[i].heapIndex, vk::to_string(memoryProperties.memoryTypes[i].propertyFlags));
	}
	LOG("{} memory heaps", memoryProperties.memoryHeapCount);
	for (unsigned int i = 0; i < memoryProperties.memoryHeapCount; i++ ) {
		LOG(" {}  size {}  {}", i, memoryProperties.memoryHeaps[i].size, vk::to_string(memoryProperties.memoryHeaps[i].flags));
	}

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	LOG("{} queue families", queueProps.size());

	graphicsQueueIndex = static_cast<uint32_t>(queueProps.size());
	bool graphicsQueueFound = false;
	for (uint32_t i = 0; i < queueProps.size(); i++) {
		const auto &q = queueProps.at(i);
		LOG(" Queue family {}", i);
		LOG("  Flags: {}", vk::to_string(q.queueFlags));
		LOG("  Count: {}", q.queueCount);
		LOG("  Timestamp valid bits: {}", q.timestampValidBits);
		LOG("  Image transfer granularity: ({}, {}, {})", q.minImageTransferGranularity.width, q.minImageTransferGranularity.height, q.minImageTransferGranularity.depth);

		if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
			LOG("  Can present to our surface");
			if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
				if (!graphicsQueueFound) {
					graphicsQueueIndex = i;
					graphicsQueueFound = true;
				}
			}
		} else {
			LOG("  Can't present to our surface");
		}
	}

	if (graphicsQueueIndex == queueProps.size()) {
		LOG("Error: no graphics queue");
		throw std::runtime_error("Error: no graphics queue");
	}

	LOG("Using queue {} for graphics", graphicsQueueIndex);

	std::array<float, 1> queuePriorities = { { 0.0f } };

	std::array<vk::DeviceQueueCreateInfo, 2> queueCreateInfos;
	unsigned int numQueues = 0;
	queueCreateInfos[numQueues].queueFamilyIndex  = graphicsQueueIndex;
	queueCreateInfos[numQueues].queueCount        = 1;
	queueCreateInfos[numQueues].pQueuePriorities  = &queuePriorities[0];
	numQueues++;

	// get a transfer queue if there is one
	transferQueueIndex          = graphicsQueueIndex;
	if (desc.transferQueue) {
		uint32_t currentFlags       = static_cast<uint32_t>(queueProps[graphicsQueueIndex].queueFlags);
		for (uint32_t i = 0; i < queueProps.size(); i++) {
			// never the same as graphics queue
			if (i == graphicsQueueIndex) {
				continue;
			}

			const auto &q = queueProps.at(i);
			if (!(q.queueFlags & vk::QueueFlagBits::eTransfer)) {
				// no transfer in this queue
				continue;
			}

			// is it a smaller set of flags than the currently chosen queue?
			// TODO: slight abuse of comparison here, should compare count of set bits
			if (static_cast<uint32_t>(q.queueFlags) < currentFlags) {
				transferQueueIndex = i;
				currentFlags       = static_cast<uint32_t>(q.queueFlags);
			}
		}
	}

	if (transferQueueIndex != graphicsQueueIndex) {
		LOG("Using queue {} for transfer", transferQueueIndex);
		queueCreateInfos[numQueues].queueFamilyIndex  = transferQueueIndex;
		queueCreateInfos[numQueues].queueCount        = 1;
		queueCreateInfos[numQueues].pQueuePriorities  = &queuePriorities[0];
		numQueues++;
	} else {
		LOG("No separate transfer queue");
	}

	HashSet<std::string> availableExtensions;
	{
		auto exts = physicalDevice.enumerateDeviceExtensionProperties();
		LOG("{} device extensions:", exts.size());
		for (const auto &ext : exts) {
			LOG("{}", ext.extensionName.data());
			availableExtensions.insert(std::string(ext.extensionName.data()));
		}
	}

	std::vector<const char *> deviceExtensions;

	auto checkExt = [&deviceExtensions, &availableExtensions] (const char *ext) {
		if (availableExtensions.find(std::string(ext)) != availableExtensions.end()) {
			LOG("Activating extension {}", ext);
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

	if (desc.tracing) {
		// this disables pipeline caching on radv so only enable when tracing
		amdShaderInfo = checkExt(VK_AMD_SHADER_INFO_EXTENSION_NAME);
		if (amdShaderInfo) {
			LOG("VK_AMD_shader_info found");
		}
	}

	if (!checkExt(VK_KHR_MAINTENANCE1_EXTENSION_NAME)) {
		throw std::runtime_error("Missing required extension VK_KHR_maintenance1");
	}

	portabilitySubset = checkExt(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);

	vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDevicePortabilitySubsetFeaturesKHR> deviceCreateInfoChain;
	if (!portabilitySubset) {
		deviceCreateInfoChain.unlink<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
	}
	auto &deviceCreateInfo = deviceCreateInfoChain.get<vk::DeviceCreateInfo>();

	assert(numQueues <= queueCreateInfos.size());
	deviceCreateInfo.queueCreateInfoCount     = numQueues;
	deviceCreateInfo.pQueueCreateInfos        = queueCreateInfos.data();

	vk::PhysicalDeviceFeatures enabledFeatures;
	if (desc.robustness) {
		LOG("Robust buffer access requested");
		if (deviceFeatures.robustBufferAccess) {
			enabledFeatures.robustBufferAccess = true;
			LOG(" enabled");
		} else {
			LOG(" not supported");
		}
	}
	deviceCreateInfo.pEnabledFeatures         = &enabledFeatures;

	deviceCreateInfo.enabledExtensionCount    = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames  = &deviceExtensions[0];
	if (enableValidation) {
		deviceCreateInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames  = &validationLayers[0];
	}

	device = physicalDevice.createDevice(deviceCreateInfo);

#if VK_HEADER_VERSION < 99

	dispatcher.init(instance, device);

#else  // VK_HEADER_VERSION

	dispatcher.init(instance, getInstanceProc, device, vkGetDeviceProcAddr);

#endif  // VK_HEADER_VERSION

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
	allocatorInfo.physicalDevice   = physicalDevice;
	allocatorInfo.device           = device;
	allocatorInfo.instance         = instance;
	if (dedicatedAllocation) {
		LOG("Dedicated allocations enabled");
		allocatorInfo.flags      = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	}

	vmaCreateAllocator(&allocatorInfo, &allocator);

	queue = device.getQueue(graphicsQueueIndex, 0);
	transferQueue = device.getQueue(transferQueueIndex, 0);

	{
		auto surfacePresentModes_ = physicalDevice.getSurfacePresentModesKHR(surface);
		surfacePresentModes.reserve(surfacePresentModes_.size());
		LOG("{} present modes",   surfacePresentModes_.size());

		for (const auto &presentMode : surfacePresentModes_) {
			LOG(" {}", vk::to_string(presentMode));
			surfacePresentModes.insert(presentMode);
		}
	}

	{
		auto surfaceFormats_ = physicalDevice.getSurfaceFormatsKHR(surface);

		LOG("{} surface formats", surfaceFormats_.size());
		for (const auto &format : surfaceFormats_) {
			LOG(" {}\t{}", vk::to_string(format.format), vk::to_string(format.colorSpace));
			if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				surfaceFormats.insert(format.format);
			}
		}
	}

	LOG("framebufferColorSampleCounts: {}",   vk::to_string(deviceProperties.limits.framebufferColorSampleCounts));
	LOG("framebufferDepthSampleCounts: {}",   vk::to_string(deviceProperties.limits.framebufferDepthSampleCounts));
	LOG("framebufferStencilSampleCounts: {}", vk::to_string(deviceProperties.limits.framebufferStencilSampleCounts));

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
	LOG("maxMSAASamples: {}", features.maxMSAASamples);
	features.SSBOSupported  = true;

	recreateSwapchain();
	recreateRingBuffer(desc.ephemeralRingBufSize);

	vk::CommandPoolCreateInfo cp;
	cp.queueFamilyIndex = transferQueueIndex;
	transferCmdPool = device.createCommandPool(cp);

	vk::PipelineCacheCreateInfo cacheInfo;
	std::vector<char> cacheData;
	std::string plCacheFile =  spirvCacheDir + "pipeline.cache";
	if (!desc.skipShaderCache && fileExists(plCacheFile)) {
		cacheData                 = readFile(plCacheFile);
		cacheInfo.initialDataSize = cacheData.size();
		cacheInfo.pInitialData    = cacheData.data();
	}

	pipelineCache = device.createPipelineCache(cacheInfo);
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

		deleteResources.emplace_back(std::move(buffer));
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
		LOG("vmaAllocateMemoryForBuffer failed: {}", vk::to_string(vk::Result(result)));
		throw std::runtime_error("vmaAllocateMemoryForBuffer failed");
	}

	LOG("ringbuffer memory type: {}",    allocationInfo.memoryType);
	LOG("ringbuffer memory offset: {}",  allocationInfo.offset);
	LOG("ringbuffer memory size: {}",    allocationInfo.size);
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
	assert(pipelineCache);

	assert(!frameAcquireSem);

	// save pipeline cache
	auto cacheData = device.getPipelineCacheData(pipelineCache);
	writeFile(spirvCacheDir + "pipeline.cache", cacheData.data(), cacheData.size());
	device.destroyPipelineCache(pipelineCache);
	pipelineCache = vk::PipelineCache();

	if (builtinDepthRT) {
		renderTargets.removeWith(builtinDepthRT, [this](struct RenderTarget &rt) {
			deleteResources.emplace_back(std::move(rt));
		} );
		builtinDepthRT = RenderTargetHandle();
	}

	waitForDeviceIdle();

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		assert(f.status == Frame::Status::Ready);
		deleteFrameInternal(f);
	}
	frames.clear();

	// must have been deleted by waitForDeviceIdle
	assert(deleteResources.empty());

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
		deletePipelineInternal(p);
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

	for (auto &sem : freeSemaphores) {
		device.destroySemaphore(sem);
		sem = vk::Semaphore();
	}

	device.destroyCommandPool(transferCmdPool);
	transferCmdPool = vk::CommandPool();

	device.destroy();
	device = vk::Device();

	if (debugReportCallback) {
		instance.destroyDebugReportCallbackEXT(debugReportCallback, nullptr, dispatcher);
		debugReportCallback = vk::DebugReportCallbackEXT();
	}

	if (debugUtilsCallback) {
		instance.destroyDebugUtilsMessengerEXT(debugUtilsCallback, nullptr, dispatcher);
		debugUtilsCallback = vk::DebugUtilsMessengerEXT();
	}

	instance.destroy();
	instance = vk::Instance();

	SDL_DestroyWindow(window);

	SDL_Quit();
}


bool Renderer::isRenderTargetFormatSupported(Format format) const {
	// TODO: cache these at startup
	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	if (isDepthFormat(format)) {
		flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	} else {
		flags |= vk::ImageUsageFlagBits::eColorAttachment;
	}
	vk::ImageFormatProperties prop;
	auto result = impl->physicalDevice.getImageFormatProperties(vulkanFormat(format), vk::ImageType::e2D, vk::ImageTiling::eOptimal, flags, vk::ImageCreateFlags(), &prop);

	return (result == vk::Result::eSuccess);
}


BufferHandle Renderer::createBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != +BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	vk::BufferCreateInfo info;
	info.size  = size;
	info.usage = bufferTypeUsage(type) | vk::BufferUsageFlagBits::eTransferDst;

	auto result    = impl->buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer  = impl->device.createBuffer(info);

	VmaAllocationCreateInfo req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForBuffer(impl->allocator, buffer.buffer, &req, &buffer.memory, &allocationInfo);
	LOG("buffer memory type: {}",    allocationInfo.memoryType);
	LOG("buffer memory offset: {}",  allocationInfo.offset);
	LOG("buffer memory size: {}",    allocationInfo.size);
	assert(allocationInfo.size > 0);
	assert(allocationInfo.pMappedData == nullptr);
	impl->device.bindBufferMemory(buffer.buffer, allocationInfo.deviceMemory, allocationInfo.offset);
	buffer.offset = static_cast<uint32_t>(allocationInfo.offset);
	buffer.size   = size;
	buffer.type   = type;

	// copy contents to GPU memory
	UploadOp op = impl->allocateUploadOp(size);
	switch (type) {
	case BufferType::Invalid:
		HEDLEY_UNREACHABLE();
		break;

	case BufferType::Index:
	case BufferType::Vertex:
	case BufferType::Everything:
		op.semWaitMask = vk::PipelineStageFlagBits::eVertexInput;
		break;

	case BufferType::Uniform:
	case BufferType::Storage:
		op.semWaitMask = vk::PipelineStageFlagBits::eVertexShader;
		break;

	}

	memcpy(static_cast<char *>(op.allocationInfo.pMappedData), contents, size);
    vmaFlushAllocation(impl->allocator, buffer.memory, 0, size);

	// TODO: reuse command buffer for multiple copies
	vk::BufferCopy copyRegion;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size      = size;

	op.cmdBuf.copyBuffer(op.stagingBuffer, buffer.buffer, 1, &copyRegion);

	vk::BufferMemoryBarrier barrier;
	barrier.srcAccessMask       = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryRead;
	if (impl->transferQueueIndex != impl->graphicsQueueIndex) {
		barrier.srcQueueFamilyIndex = impl->transferQueueIndex;
		barrier.dstQueueFamilyIndex = impl->graphicsQueueIndex;
	} else {
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}
	barrier.buffer              = buffer.buffer;
	barrier.offset              = 0;
	barrier.size                = size;

	op.cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { barrier }, {});

	if (impl->transferQueueIndex != impl->graphicsQueueIndex) {
		op.bufferAcquireBarriers.push_back(barrier);
	}

	impl->submitUploadOp(std::move(op));

	return result.second;
}


BufferHandle Renderer::createEphemeralBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != +BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: separate ringbuffers based on type
	unsigned int beginPtr = impl->ringBufferAllocate(size, impl->bufferAlignment(type));

	memcpy(impl->persistentMapping + beginPtr, contents, size);

	auto result    = impl->buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer          = impl->ringBuffer;
	buffer.ringBufferAlloc = true;
	buffer.offset          = beginPtr;
	buffer.size            = size;
	buffer.type            = type;

	impl->frames.at(impl->currentFrameIdx).ephemeralBuffers.push_back(result.second);

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

	case Layout::ColorAttachment:
		return vk::ImageLayout::eColorAttachmentOptimal;

	case Layout::Present:
		return vk::ImageLayout::ePresentSrcKHR;
	}

	HEDLEY_UNREACHABLE();
	return vk::ImageLayout::eUndefined;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	assert(!desc.name_.empty());
	assert(desc.renderPass_);

	auto &renderPass = impl->renderPasses.get(desc.renderPass_);
	assert(renderPass.renderPass);

	std::vector<vk::ImageView> attachmentViews;
	unsigned int width = 0, height = 0;
	unsigned int numSamples = 0;

	std::array<Format, MAX_COLOR_RENDERTARGETS> colorFormats;
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (!desc.colors_[i]) {
			colorFormats[i] = Format::Invalid;
			continue;
		}

		const auto &colorRT = impl->renderTargets.get(desc.colors_[i]);

		if (width == 0) {
			assert(height == 0);
			assert(numSamples == 0);
			width  = colorRT.width;
			height = colorRT.height;
			numSamples = colorRT.numSamples;
		} else {
			assert(width  == colorRT.width);
			assert(height == colorRT.height);
			assert(numSamples == colorRT.numSamples);
		}

		assert(colorRT.width  > 0);
		assert(colorRT.height > 0);
		assert(colorRT.numSamples > 0);
		assert(colorRT.imageView);
		// TODO: make sure renderPass formats match actual framebuffer attachments
		assert(colorRT.format == renderPass.desc.colorRTs_[i].format);
		colorFormats[i] = colorRT.format;
		attachmentViews.push_back(colorRT.imageView);
	}

	auto depthStencilFormat = Format::Invalid;
	if (desc.depthStencil_) {
		const auto &depthRT = impl->renderTargets.get(desc.depthStencil_);
		assert(depthRT.width  == width);
		assert(depthRT.height == height);
		assert(depthRT.numSamples == numSamples);
		assert(depthRT.imageView);
		attachmentViews.push_back(depthRT.imageView);
		depthStencilFormat = depthRT.format;
	} else {
		assert(renderPass.desc.depthStencilFormat_ == +Format::Invalid);
	}

	vk::FramebufferCreateInfo fbInfo;

	fbInfo.renderPass       = renderPass.renderPass;
	assert(!attachmentViews.empty());
	fbInfo.attachmentCount  = static_cast<uint32_t>(attachmentViews.size());
	fbInfo.pAttachments     = &attachmentViews[0];
	fbInfo.width            = width;
	fbInfo.height           = height;
	fbInfo.layers           = 1;

	auto result     = impl->framebuffers.add();
	Framebuffer &fb = result.first;
	fb.desc         = desc;
	fb.width        = width;
	fb.height       = height;
	fb.numSamples   = numSamples;
	fb.framebuffer  = impl->device.createFramebuffer(fbInfo);
	fb.depthStencilFormat = depthStencilFormat;
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		fb.colorFormats[i] = colorFormats[i];
	}

	impl->debugNameObject<vk::Framebuffer>(fb.framebuffer, desc.name_);

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

	HEDLEY_UNREACHABLE();
	return vk::SampleCountFlagBits::e1;
}


RenderPassHandle Renderer::createRenderPass(const RenderPassDesc &desc) {
	assert(!desc.name_.empty());

	vk::RenderPassCreateInfo info;
	vk::SubpassDescription subpass;

	std::vector<vk::AttachmentDescription> attachments;
	std::vector<vk::AttachmentReference> colorAttachments;

	auto result   = impl->renderPasses.add();
	RenderPass &r = result.first;

	vk::SampleCountFlagBits samples = sampleCountFlagsFromNum(desc.numSamples_);

	unsigned int numColorAttachments = 0;
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (desc.colorRTs_[i].format == +Format::Invalid) {
			assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);
			// TODO: could be break, it's invalid to have holes in attachment list
			// but should check that
			continue;
		}
		numColorAttachments++;

		const auto &colorRT = desc.colorRTs_[i];

		uint32_t attachNum    = static_cast<uint32_t>(attachments.size());
		vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentDescription attach;
		attach.format         = vulkanFormat(colorRT.format);
		attach.samples        = samples;
		switch (colorRT.passBegin) {
		case PassBegin::DontCare:
			assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);

			attach.loadOp         = vk::AttachmentLoadOp::eDontCare;
			attach.initialLayout  = vk::ImageLayout::eUndefined;
			break;

		case PassBegin::Keep:
			assert(desc.colorRTs_[i].initialLayout != +Layout::Undefined);

			attach.loadOp         = vk::AttachmentLoadOp::eLoad;
			attach.initialLayout  = vulkanLayout(desc.colorRTs_[i].initialLayout);
			break;

		case PassBegin::Clear:
			assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);

			attach.loadOp         = vk::AttachmentLoadOp::eClear;
			attach.initialLayout  = vk::ImageLayout::eUndefined;
			std::array<float, 4> color = { { colorRT.clearValue.x, colorRT.clearValue.y, colorRT.clearValue.z, colorRT.clearValue.a } };
			r.clearValueCount = attachNum + 1;
			assert(r.clearValueCount <= 2);
			r.clearValues[attachNum].color = vk::ClearColorValue(color);
			break;
		}

		attach.storeOp        = vk::AttachmentStoreOp::eStore;
		attach.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
		attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attach.finalLayout    = vulkanLayout(colorRT.finalLayout);
		attachments.push_back(attach);

		vk::AttachmentReference ref;
		ref.attachment = attachNum;
		ref.layout     = layout;
		colorAttachments.push_back(ref);
	}
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	subpass.pColorAttachments    = &colorAttachments[0];

	bool hasDepthStencil = (desc.depthStencilFormat_ != +Format::Invalid);
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
            assert(attachNum < r.clearValues.size());
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

		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			if (desc.colorRTs_[i].format == +Format::Invalid) {
				assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);
				// TODO: could be break, it's invalid to have holes in attachment list
				// but should check that
				continue;
			}

			switch (desc.colorRTs_[i].finalLayout) {
			case Layout::Undefined:
			case Layout::TransferDst:
				assert(false);
				break;

			case Layout::ShaderRead:
				d.dstStageMask   |= vk::PipelineStageFlagBits::eFragmentShader;
				d.dstAccessMask  |= vk::AccessFlagBits::eShaderRead;
				break;

			case Layout::TransferSrc:
				d.dstStageMask   |= vk::PipelineStageFlagBits::eTransfer;
				d.dstAccessMask  |= vk::AccessFlagBits::eTransferRead;
				break;

			case Layout::ColorAttachment:
				d.dstStageMask   |= vk::PipelineStageFlagBits::eColorAttachmentOutput;
				d.dstAccessMask  |= vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
				break;

			case Layout::Present:
				d.dstStageMask   |= vk::PipelineStageFlagBits::eBottomOfPipe;
				break;

			}
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

	r.renderPass  = impl->device.createRenderPass(info);
	r.numSamples  = desc.numSamples_;
	r.numColorAttachments = numColorAttachments;
	r.desc        = desc;

	impl->debugNameObject<vk::RenderPass>(r.renderPass, desc.name_);

	return result.second;
}


static vk::BlendFactor vulkanBlendFactor(BlendFunc b) {
	switch (b) {
	case BlendFunc::Zero:
		return vk::BlendFactor::eZero;

	case BlendFunc::One:
		return vk::BlendFactor::eOne;

	case BlendFunc::Constant:
		return vk::BlendFactor::eConstantAlpha;

	case BlendFunc::SrcAlpha:
		return vk::BlendFactor::eSrcAlpha;

	case BlendFunc::OneMinusSrcAlpha:
		return vk::BlendFactor::eOneMinusSrcAlpha;

	}

	HEDLEY_UNREACHABLE();
	return vk::BlendFactor::eZero;
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	vk::GraphicsPipelineCreateInfo info;

	ShaderMacros macros_(desc.shaderMacros_);
	macros_.emplace("VULKAN_FLIP", "1");

	auto vshaderHandle = impl->createVertexShader(desc.vertexShaderName, macros_);
	const auto &v = impl->vertexShaders.get(vshaderHandle);
	auto fshaderHandle = impl->createFragmentShader(desc.fragmentShaderName, macros_);
	const auto &f = impl->fragmentShaders.get(fshaderHandle);

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
		forEachSetBit(desc.vertexAttribMask, [&attrs, &bufmask, &desc] (uint32_t bit, uint32_t /* mask */) {
			vk::VertexInputAttributeDescription attr;
			const auto &attrDesc = desc.vertexAttribs.at(bit);
			attr.location = bit;
			attr.binding  = attrDesc.bufBinding;
			attr.format   = vulkanVertexFormat(attrDesc.format, attrDesc.count);
			attr.offset   = attrDesc.offset;
			attrs.push_back(attr);
			bufmask |=  (1 << attrDesc.bufBinding);
		});

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

	const auto &renderPass = impl->renderPasses.get(desc.renderPass_);
	info.renderPass = renderPass.renderPass;

	vk::PipelineMultisampleStateCreateInfo multisample;
	multisample.rasterizationSamples = sampleCountFlagsFromNum(desc.numSamples_);
	info.pMultisampleState = &multisample;

	vk::PipelineDepthStencilStateCreateInfo ds;
	ds.depthTestEnable  = desc.depthTest_;
	ds.depthWriteEnable = desc.depthWrite_;
	ds.depthCompareOp   = vk::CompareOp::eLess;
	info.pDepthStencilState = &ds;

	std::vector<vk::PipelineColorBlendAttachmentState> colorBlendStates;
	for (unsigned int i = 0; i < renderPass.numColorAttachments; i++) {
		vk::PipelineColorBlendAttachmentState cb;
		if (desc.blending_) {
			cb.blendEnable          = true;
			cb.srcColorBlendFactor  = vulkanBlendFactor(desc.sourceBlend_);
			cb.dstColorBlendFactor  = vulkanBlendFactor(desc.destinationBlend_);
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
	if (desc.blending_ && (desc.sourceBlend_ == +BlendFunc::Constant || desc.destinationBlend_ == +BlendFunc::Constant)) {
		// TODO: get from desc
		for (unsigned int i = 0; i < 4; i++) {
			blendInfo.blendConstants[i] = 0.5f;
		}
	}

	info.pColorBlendState     = &blendInfo;

	vk::PipelineDynamicStateCreateInfo dyn;
	std::vector<vk::DynamicState> dynStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	dyn.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
	dyn.pDynamicStates    = &dynStates[0];
	info.pDynamicState    = &dyn;

	std::vector<vk::DescriptorSetLayout> layouts;
	for (unsigned int i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
		if (desc.descriptorSetLayouts[i]) {
			const auto &layout = impl->dsLayouts.get(desc.descriptorSetLayouts[i]);
			layouts.push_back(layout.layout);
		}
	}

	vk::PipelineLayoutCreateInfo layoutInfo;
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts    = &layouts[0];

	auto device = impl->device;

	auto layout = device.createPipelineLayout(layoutInfo);
	info.layout = layout;

	auto result = device.createGraphicsPipeline(impl->pipelineCache, info);
	// TODO: check success instead of implicitly using result.value

	impl->debugNameObject<vk::Pipeline>(result.value, desc.name_);

	if (impl->amdShaderInfo) {
		vk::ShaderStatisticsInfoAMD stats;
		size_t dataSize = sizeof(stats);
		// TODO: other stages

		device.getShaderInfoAMD(result.value, vk::ShaderStageFlagBits::eVertex, vk::ShaderInfoTypeAMD::eStatistics, &dataSize, &stats, impl->dispatcher);
		LOG("pipeline \"{}\" vertex SGPR {} VGPR {}", desc.name_, stats.resourceUsage.numUsedSgprs, stats.resourceUsage.numUsedVgprs);

		device.getShaderInfoAMD(result.value, vk::ShaderStageFlagBits::eFragment, vk::ShaderInfoTypeAMD::eStatistics, &dataSize, &stats, impl->dispatcher);
		LOG("pipeline \"{}\" fragment SGPR {} VGPR {}", desc.name_, stats.resourceUsage.numUsedSgprs, stats.resourceUsage.numUsedVgprs);
	}

	auto id = impl->pipelines.add();
	Pipeline &p = id.first;
	p.pipeline = result.value;
	p.layout   = layout;
	p.scissor  = desc.scissorTest_;

	return id.second;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != +Format::Invalid);
	assert(isPow2(desc.numSamples_));
	assert(!desc.name_.empty());

	// TODO: use NV_dedicated_allocation when available

	vk::Format format = vulkanFormat(desc.format_);
	vk::ImageCreateInfo info;
	if (desc.additionalViewFormat_ != +Format::Invalid) {
		info.flags   = vk::ImageCreateFlagBits::eMutableFormat;
	}
	info.imageType   = vk::ImageType::e2D;
	info.format      = format;
	info.extent      = vk::Extent3D(desc.width_, desc.height_, 1);
	info.mipLevels   = 1;
	info.arrayLayers = 1;
	// FIXME: validate samples against format-specific limits
	// validation layer says: vkCreateImage(): samples VK_SAMPLE_COUNT_16_BIT is not supported by format 0x0000000F. The Vulkan spec states: samples must be a bit value that is set in imageCreateSampleCounts (as defined in Image Creation Limits).
	// (https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-VkImageCreateInfo-samples-02258)
	info.samples     = sampleCountFlagsFromNum(desc.numSamples_);
	// TODO: usage should come from desc
	vk::ImageUsageFlags flags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
	if (isDepthFormat(desc.format_)) {
		flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	} else {
		flags |= vk::ImageUsageFlagBits::eColorAttachment;
	}
	info.usage       = flags;

	auto device = impl->device;

	auto result = impl->renderTargets.add();
	RenderTarget &rt = result.first;
	rt.width  = desc.width_;
	rt.height = desc.height_;
	rt.numSamples = desc.numSamples_;
	rt.image = device.createImage(info);
	rt.format = desc.format_;

	auto texResult   = impl->textures.add();
	Texture &tex     = texResult.first;
	tex.width        = desc.width_;
	tex.height       = desc.height_;
	tex.image        = rt.image;
	tex.renderTarget = true;

	impl->debugNameObject<vk::Image>(tex.image, desc.name_);

	VmaAllocationCreateInfo req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	req.flags          = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
	req.pUserData      = const_cast<char *>(desc.name_.c_str());
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForImage(impl->allocator, rt.image, &req, &tex.memory, &allocationInfo);
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

	impl->debugNameObject<vk::ImageView>(tex.imageView, desc.name_);

	// TODO: std::move ?
	rt.texture = texResult.second;

	if (desc.additionalViewFormat_ != +Format::Invalid) {
		assert(isDepthFormat(desc.format_) == isDepthFormat(desc.additionalViewFormat_));
		auto viewResult   = impl->textures.add();
		Texture &view     = viewResult.first;
		rt.additionalView = viewResult.second;
		view.width        = desc.width_;
		view.height       = desc.height_;
		view.image        = rt.image;
		view.renderTarget = true;

		viewInfo.format   = vulkanFormat(desc.additionalViewFormat_);
		view.imageView    = device.createImageView(viewInfo);

		std::string viewName = desc.name_ + " " + desc.additionalViewFormat_._to_string() + " view";
		impl->debugNameObject<vk::ImageView>(view.imageView, viewName);
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

	HEDLEY_UNREACHABLE();

	return vk::Filter::eNearest;
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	vk::SamplerCreateInfo info;

	info.magFilter = vulkanFiltermode(desc.mag);
	info.minFilter = vulkanFiltermode(desc.min);

	vk::SamplerAddressMode m = vk::SamplerAddressMode::eClampToEdge;
	if (desc.wrapMode == +WrapMode::Wrap) {
		m = vk::SamplerAddressMode::eRepeat;
	}
	info.addressModeU = m;
	info.addressModeV = m;
	info.addressModeW = m;

	auto result = impl->samplers.add();
	struct Sampler &sampler = result.first;
	sampler.sampler    = impl->device.createSampler(info);

	impl->debugNameObject<vk::Sampler>(sampler.sampler, desc.name_);

	return result.second;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

	std::vector<uint32_t> spirv = compileSpirv(vertexShaderName, macros, ShaderKind::Vertex);

	auto result_ = vertexShaders.add();

	VertexShader &v = result_.first;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	v.shaderModule = device.createShaderModule(info);

		// TODO: add macros to name
	debugNameObject<vk::ShaderModule>(v.shaderModule, vertexShaderName);

	return result_.second;
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName   = name + ".frag";

	std::vector<uint32_t> spirv = compileSpirv(fragmentShaderName, macros, ShaderKind::Fragment);

	auto result_ = fragmentShaders.add();

	FragmentShader &f = result_.first;
	vk::ShaderModuleCreateInfo info;
	info.codeSize = spirv.size() * 4;
	info.pCode    = &spirv[0];
	f.shaderModule = device.createShaderModule(info);

		// TODO: add macros to name
	debugNameObject<vk::ShaderModule>(f.shaderModule, fragmentShaderName);

	return result_.second;
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
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

	auto device = impl->device;

	auto result = impl->textures.add();
	Texture &tex = result.first;
	tex.width  = desc.width_;
	tex.height = desc.height_;
	tex.image  = device.createImage(info);

	VmaAllocationCreateInfo  req = {};
	req.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
	req.flags          = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
	req.pUserData      = const_cast<char *>(desc.name_.c_str());
	VmaAllocationInfo  allocationInfo = {};

	vmaAllocateMemoryForImage(impl->allocator, tex.image, &req, &tex.memory, &allocationInfo);
	LOG("texture image memory type: {}",   allocationInfo.memoryType);
	LOG("texture image memory offset: {}", allocationInfo.offset);
	LOG("texture image memory size: {}",   allocationInfo.size);
	device.bindImageMemory(tex.image, allocationInfo.deviceMemory, allocationInfo.offset);

	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image    = tex.image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format   = format;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.levelCount = desc.numMips_;
	viewInfo.subresourceRange.layerCount = 1;
	tex.imageView = device.createImageView(viewInfo);

	impl->debugNameObject<vk::Image>(tex.image, desc.name_);
	impl->debugNameObject<vk::ImageView>(tex.imageView, desc.name_);

	// TODO: reuse command buffer for multiple copies
	unsigned int w = desc.width_, h = desc.height_;
	unsigned int bufferSize = 0;
	uint32_t align = std::max(formatSize(desc.format_), static_cast<uint32_t>(impl->deviceProperties.limits.optimalBufferCopyOffsetAlignment));
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

	UploadOp op = impl->allocateUploadOp(bufferSize);
	op.semWaitMask = vk::PipelineStageFlagBits::eFragmentShader;

	// transition to transfer destination
	{
		vk::ImageSubresourceRange range;
		range.aspectMask            = vk::ImageAspectFlagBits::eColor;
		range.baseMipLevel          = 0;
		range.levelCount            = VK_REMAINING_MIP_LEVELS;
		range.baseArrayLayer        = 0;
		range.layerCount            = VK_REMAINING_ARRAY_LAYERS;

		vk::ImageMemoryBarrier barrier;
		// TODO: should this be eHostWrite?
		barrier.srcAccessMask        = vk::AccessFlagBits();
		barrier.dstAccessMask        = vk::AccessFlagBits::eTransferWrite;
		barrier.oldLayout            = vk::ImageLayout::eUndefined;
		barrier.newLayout            = vk::ImageLayout::eTransferDstOptimal;
		barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
		barrier.image                = tex.image;
		barrier.subresourceRange     = range;

		// TODO: relax stage flag bits
		op.cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { barrier });

		char *mappedPtr = static_cast<char *>(op.allocationInfo.pMappedData);
		for (unsigned int i = 0; i < desc.numMips_; i++) {
			// copy contents to GPU memory
			memcpy(mappedPtr + regions[i].bufferOffset, desc.mipData_[i].data, desc.mipData_[i].size);
		}

		vmaFlushAllocation(impl->allocator, op.memory, 0, bufferSize);

		op.cmdBuf.copyBufferToImage(op.stagingBuffer, tex.image, vk::ImageLayout::eTransferDstOptimal, regions);

		// transition to shader use
		barrier.srcAccessMask       = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryRead;
		barrier.oldLayout           = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal;
		if (impl->transferQueueIndex != impl->graphicsQueueIndex) {
			barrier.srcQueueFamilyIndex = impl->transferQueueIndex;
			barrier.dstQueueFamilyIndex = impl->graphicsQueueIndex;
		} else {
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}

		op.cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { barrier });

		if (impl->transferQueueIndex != impl->graphicsQueueIndex) {
			op.imageAcquireBarriers.push_back(barrier);
		}
	}

	impl->submitUploadOp(std::move(op));

	return result.second;
}


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	unsigned int i = 0;
	std::vector<DescriptorLayout> descriptors;
	while (layout->type != +DescriptorType::End) {
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

	auto result = impl->dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;
	dsLayout.layout = impl->device.createDescriptorSetLayout(info);
	dsLayout.descriptors = std::move(descriptors);

	return result.second;
}


TextureHandle Renderer::getRenderTargetView(RenderTargetHandle handle, Format f) {
	const auto &rt = impl->renderTargets.get(handle);

	TextureHandle result;
	if (f == rt.format) {
		result = rt.texture;

#ifndef NDEBUG
		const auto &tex = impl->textures.get(result);
		assert(tex.renderTarget);
#endif //  NDEBUG
	} else {
		result = rt.additionalView;

#ifndef NDEBUG
		const auto &tex = impl->textures.get(result);
		assert(tex.renderTarget);
		//assert(tex.format == f);
#endif //  NDEBUG
	}

	return result;
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->buffers.removeWith(handle, [this](struct Buffer &b) {
		// TODO: if b.lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(b));
	} );
}


void Renderer::deleteFramebuffer(FramebufferHandle handle) {
	impl->framebuffers.removeWith(handle, [this](Framebuffer &fb) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(fb));
	} );
}


void Renderer::deletePipeline(PipelineHandle handle) {
	impl->pipelines.removeWith(handle, [this](Pipeline &p) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(p));
	} );
}


void Renderer::deleteRenderPass(RenderPassHandle handle) {
	impl->renderPasses.removeWith(handle, [this](RenderPass &rp) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(rp));
	} );
}


void Renderer::deleteRenderTarget(RenderTargetHandle &handle) {
	impl->renderTargets.removeWith(handle, [this](struct RenderTarget &rt) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(rt));
	} );
}


void Renderer::deleteSampler(SamplerHandle handle) {
	impl->samplers.removeWith(handle, [this](struct Sampler &s) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(s));
	} );
}


void Renderer::deleteTexture(TextureHandle handle) {
	impl->textures.removeWith(handle, [this](Texture &tex) {
		// TODO: if lastUsedFrame has already been synced we could delete immediately
		impl->deleteResources.emplace_back(std::move(tex));
	} );
}


void Renderer::setSwapchainDesc(const SwapchainDesc &desc) {
	bool changed = false;

	if (impl->swapchainDesc.fullscreen != desc.fullscreen) {
		changed = true;
		if (desc.fullscreen) {
			// TODO: check return val?
			SDL_SetWindowFullscreen(impl->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			LOG("Fullscreen");
		} else {
			SDL_SetWindowFullscreen(impl->window, 0);
			LOG("Windowed");
		}
	}

	if (impl->swapchainDesc.vsync != desc.vsync) {
		changed = true;
	}

	if (impl->swapchainDesc.numFrames != desc.numFrames) {
		changed = true;
	}

	if (impl->swapchainDesc.width     != desc.width) {
		changed = true;
	}

	if (impl->swapchainDesc.height    != desc.height) {
		changed = true;
	}

	if (changed) {
		impl->wantedSwapchain = desc;
		impl->swapchainDirty  = true;
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

	HEDLEY_UNREACHABLE();
}


unsigned int RendererImpl::bufferAlignment(BufferType type) {
	switch (type) {
	case BufferType::Invalid:
		HEDLEY_UNREACHABLE();
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

	HEDLEY_UNREACHABLE();

	return 64;
}


glm::uvec2 Renderer::getDrawableSize() const {
	int w = -1, h = -1;
	SDL_Vulkan_GetDrawableSize(impl->window, &w, &h);
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	return glm::uvec2(w, h);
}


void RendererImpl::recreateSwapchain() {
	assert(swapchainDirty);

	if (builtinDepthRT) {
		renderTargets.removeWith(builtinDepthRT, [this](struct RenderTarget &rt) {
			deleteResources.emplace_back(std::move(rt));
		} );
		builtinDepthRT = RenderTargetHandle();
	}

	waitForDeviceIdle();

	surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	LOG("image count min-max {} - {}", surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
	LOG("image extent min-max {}x{} - {}x{}", surfaceCapabilities.minImageExtent.width, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.width, surfaceCapabilities.maxImageExtent.height);
	LOG("current image extent {}x{}", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height);
	LOG("supported surface transforms: {}", vk::to_string(surfaceCapabilities.supportedTransforms));
	LOG("supported surface alpha composite flags: {}", vk::to_string(surfaceCapabilities.supportedCompositeAlpha));
	LOG("supported surface usage flags: {}", vk::to_string(surfaceCapabilities.supportedUsageFlags));

	int tempW = -1, tempH = -1;
	SDL_Vulkan_GetDrawableSize(window, &tempW, &tempH);
	if (tempW <= 0 || tempH <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	// this is nasty but apparently surface might not have resized yet
	// FIXME: find a better way
	unsigned int w = std::max(surfaceCapabilities.minImageExtent.width,  std::min(static_cast<unsigned int>(tempW), surfaceCapabilities.maxImageExtent.width));
	unsigned int h = std::max(surfaceCapabilities.minImageExtent.height, std::min(static_cast<unsigned int>(tempH), surfaceCapabilities.maxImageExtent.height));

	swapchainDesc.width  = w;
	swapchainDesc.height = h;

	unsigned int numImages = wantedSwapchain.numFrames;
	numImages = std::max(numImages, surfaceCapabilities.minImageCount);
	if (surfaceCapabilities.maxImageCount != 0) {
		numImages = std::min(numImages, surfaceCapabilities.maxImageCount);
	}

	LOG("Want {} images, using {} images", wantedSwapchain.numFrames, numImages);

	swapchainDesc.fullscreen = wantedSwapchain.fullscreen;
	swapchainDesc.numFrames  = numImages;
	swapchainDesc.vsync      = wantedSwapchain.vsync;

	// always destroy the old image views and framebuffers
	for (Frame &f : frames) {
		assert(f.imageView);
		device.destroyImageView(f.imageView);
		f.imageView = vk::ImageView();

		if (f.framebuffer) {
			framebuffers.removeWith(f.framebuffer, [this](Framebuffer &fb) {
				deleteResources.emplace_back(std::move(fb));
			} );

			f.framebuffer = FramebufferHandle();
		}
	}

	if (frames.size() != numImages) {
		if (numImages < frames.size()) {
			// decreasing, delete old and resize
			for (unsigned int i = numImages; i < frames.size(); i++) {
				auto &f = frames.at(i);
				assert(f.status == Frame::Status::Ready);

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

				// we fill these in after we've created the swapchain
				assert(!f.image);
				assert(!f.imageView);
				assert(!f.framebuffer);

				assert(!f.dsPool);
				f.dsPool = device.createDescriptorPool(dsInfo);

				assert(!f.commandPool);
				f.commandPool = device.createCommandPool(cp);

				assert(!f.commandBuffer);
				assert(!f.barrierCmdBuf);
				// create command buffer
				vk::CommandBufferAllocateInfo info(f.commandPool, vk::CommandBufferLevel::ePrimary, 2);
				auto bufs = device.allocateCommandBuffers(info);
				assert(bufs.size() == 2);
				f.commandBuffer = bufs.at(0);
				f.barrierCmdBuf = bufs.at(1);

				assert(!f.renderDoneSem);
				f.renderDoneSem = allocateSemaphore();
			}
		}
	}

	vk::Extent2D imageExtent;
	// TODO: check against min and max
	imageExtent.width  = swapchainDesc.width;
	imageExtent.height = swapchainDesc.height;

	if (!(surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)) {
		LOG("warning: identity transform not supported");
	}

	if (surfaceCapabilities.currentTransform != vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		LOG("warning: current transform is not identity");
	}

	if (!(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
		LOG("warning: opaque alpha not supported");
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

	LOG("Using present mode {}", vk::to_string(swapchainPresentMode));

	// TODO: should fallback to Unorm and communicate back to demo
	// TODO: should iterate through supported formats and pick an appropriate one
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
	// TODO: need eStorage when implementing compute shaders
	swapchainCreateInfo.imageUsage            = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;

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

	vk::ImageViewCreateInfo info;
	info.viewType                    = vk::ImageViewType::e2D;
	info.format                      = surfaceFormat;
	info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;
	for (unsigned int i = 0; i < numImages; i++) {
		Frame &f = frames.at(i);
		f.image     = swapchainImages.at(i);
		info.image  = f.image;
		assert(!f.imageView);
		f.imageView = device.createImageView(info);
	}

	swapchainFormat = Format::sBGRA8;
	swapchainDirty = false;
}


MemoryStats Renderer::getMemStats() const {
	VmaStats vmaStats;
	memset(&vmaStats, 0, sizeof(VmaStats));
	vmaCalculateStats(impl->allocator, &vmaStats);
	MemoryStats stats;
	stats.allocationCount    = vmaStats.total.allocationCount;
	stats.subAllocationCount = vmaStats.total.unusedRangeCount;
	stats.usedBytes          = vmaStats.total.usedBytes;
	stats.unusedBytes        = vmaStats.total.unusedBytes;
	return stats;
}


void RendererImpl::waitForDeviceIdle() {
	std::vector<vk::Fence> fences;

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		switch (f.status) {
		case Frame::Status::Ready:
            break;

		case Frame::Status::Pending:
			fences.push_back(f.fence);

		case Frame::Status::Done:
			break;
		}
	}

	if (!fences.empty()) {
		auto waitResult = device.waitForFences( fences, true, frameTimeoutNanos);
		if (waitResult != vk::Result::eSuccess) {
			// TODO: better exception types
			std::string s = vk::to_string(waitResult);
			LOG("wait result is not success: {}", s);
			throw std::runtime_error("wait result is not success " + s);
		}

		unsigned int DEBUG_ASSERTED count = 0;
		for (unsigned int i = 0; i < frames.size(); i++) {
			auto &f = frames.at(i);
			switch (f.status) {
			case Frame::Status::Ready:
				break;

			case Frame::Status::Pending:
				f.status = Frame::Status::Done;
				count++;
				// fallthrough
			case Frame::Status::Done:
				cleanupFrame(i);
				break;
			}
		}
		assert(count == fences.size());
	}

	device.waitIdle();

	for (auto &r : deleteResources) {
		this->deleteResourceInternal(const_cast<Resource &>(r));
	}
	deleteResources.clear();
}


void Renderer::beginFrame() {
#ifndef NDEBUG
	assert(!impl->inFrame);
#endif  // NDEBUG

	auto device = impl->device;

		assert(!impl->frameAcquireSem);

		if (impl->swapchainDirty) {
			impl->recreateSwapchain();
			assert(!impl->swapchainDirty);
		}

		// acquire next image
		uint32_t imageIdx = 0xFFFFFFFFU;

		impl->frameAcquireSem = impl->allocateSemaphore();

		vk::Result result = device.acquireNextImageKHR(impl->swapchain, impl->frameTimeoutNanos, impl->frameAcquireSem, vk::Fence(), &imageIdx);
		while (result != vk::Result::eSuccess) {
			if (result == vk::Result::eSuboptimalKHR) {
				LOG("swapchain suboptimal during acquireNextImageKHR, recreating on next frame...");
				logFlush();
				impl->swapchainDirty = true;
				break;
			} else if (result == vk::Result::eErrorOutOfDateKHR) {
				// swapchain went out of date during acquire, recreate and try again
				LOG("swapchain out of date during acquireNextImageKHR, recreating immediately...");
				logFlush();
				impl->swapchainDirty = true;
				impl->recreateSwapchain();
				assert(!impl->swapchainDirty);
			} else {
				LOG("acquireNextImageKHR failed: {}", vk::to_string(result));
				logFlush();
				throw std::runtime_error("acquireNextImageKHR failed");
			}

			result = device.acquireNextImageKHR(impl->swapchain, impl->frameTimeoutNanos, impl->frameAcquireSem, vk::Fence(), &imageIdx);
		}

		assert(imageIdx < impl->frames.size());
		impl->currentFrameIdx        = imageIdx;

	auto &frame            = impl->frames.at(impl->currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	// if not done, return false and let caller deal with calling us again
	switch (frame.status) {
	case Frame::Status::Ready:
		break;

	case Frame::Status::Pending:
		impl->waitForFrame(impl->currentFrameIdx);
		assert(frame.status == Frame::Status::Done);
		impl->cleanupFrame(impl->currentFrameIdx);

		break;

	case Frame::Status::Done:
		break;
	}

	assert(frame.status == Frame::Status::Ready);

#ifndef NDEBUG
	impl->inFrame       = true;
	impl->inRenderPass  = false;
	impl->validPipeline = false;
	impl->pipelineDrawn = true;
#endif  // NDEBUG
	device.resetFences( { frame.fence } );

	assert(!frame.acquireSem);
	frame.acquireSem             = impl->frameAcquireSem;
	impl->frameAcquireSem        = vk::Semaphore();

	// set command buffer to recording
	impl->currentCommandBuffer = frame.commandBuffer;
	impl->currentCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	impl->currentPipelineLayout = vk::PipelineLayout();

	// mark buffers deleted during gap between frames to be deleted when this frame has synced
	// TODO: we could move this earlier and add these to the previous frame's list
	if (!impl->deleteResources.empty()) {
		assert(frame.deleteResources.empty());
		frame.deleteResources = std::move(impl->deleteResources);
		assert(impl->deleteResources.empty());
	}
}


void Renderer::presentFrame(RenderTargetHandle rtHandle) {
#ifndef NDEBUG
	assert(impl->inFrame);
#endif  // NDEBUG

	const auto &rt = impl->renderTargets.get(rtHandle);
	unsigned int width  = rt.width;
	unsigned int height = rt.height;

	if (width != impl->swapchainDesc.width || height != impl->swapchainDesc.height) {
		LOG("warning: rendertarget size mismatch at presentFrame, is ({}x{}) should be ({}x{})", width, height, impl->swapchainDesc.width, impl->swapchainDesc.height);
		width  = std::min(width,  impl->swapchainDesc.width);
		height = std::min(height, impl->swapchainDesc.height);
		impl->swapchainDirty  = true;
	}

	auto &frame = impl->frames.at(impl->currentFrameIdx);

	vk::Image image        = frame.image;
	vk::ImageLayout layout = vk::ImageLayout::eTransferDstOptimal;

	// transition image to transfer dst optimal
	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask       = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
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

	impl->currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlagBits::eByRegion, {}, {}, { barrier });

	vk::ImageBlit blit;
	blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[1u]            = vk::Offset3D(width, height, 1);
	blit.dstSubresource            = blit.srcSubresource;
	blit.dstOffsets[1u]            = blit.srcOffsets[1u];

	// blit draw image to presentation image
	impl->currentCommandBuffer.blitImage(rt.image, vk::ImageLayout::eTransferSrcOptimal, image, layout, { blit }, vk::Filter::eNearest);

	// transition to present
	barrier.srcAccessMask       = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	barrier.oldLayout           = layout;
	barrier.newLayout           = vk::ImageLayout::ePresentSrcKHR;
	barrier.image               = image;
	impl->currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlagBits::eByRegion, {}, {}, { barrier });

	presentFrame();
}


void Renderer::presentFrame() {
#ifndef NDEBUG
	assert(impl->inFrame);
	impl->inFrame = false;
#endif  // NDEBUG

	auto &frame = impl->frames.at(impl->currentFrameIdx);

	assert(frame.acquireSem);
	assert(frame.renderDoneSem);
	impl->device.resetFences( { frame.fence } );

	impl->currentCommandBuffer.end();

	// TODO: this should be all the stages that access the backbuffer
	// currently transfer and/or color output
	// in the future also compute shader
	// should be a parameter to this function?
	vk::PipelineStageFlags acquireWaitStage = vk::PipelineStageFlagBits::eAllCommands;

	// submit command buffers
	vk::SubmitInfo submit;

	std::array<vk::CommandBuffer, 2> submitBuffers;

	std::vector<vk::Semaphore>          waitSemaphores;
	std::vector<vk::PipelineStageFlags> semWaitMasks;
	std::vector<vk::ImageMemoryBarrier> imageAcquireBarriers;
	std::vector<vk::BufferMemoryBarrier> bufferAcquireBarriers;
	if (!impl->uploads.empty()) {
		LOG("{} uploads pending", impl->uploads.size());

		// use semaphores to make sure draw doesn't proceed until uploads are ready
		waitSemaphores.reserve(impl->uploads.size() + 1);
		semWaitMasks.reserve(impl->uploads.size() + 1);
		for (auto &op : impl->uploads) {
			waitSemaphores.push_back(op.semaphore);
			semWaitMasks.push_back(op.semWaitMask);

			imageAcquireBarriers.insert(imageAcquireBarriers.end()
			                          , op.imageAcquireBarriers.begin()
			                          , op.imageAcquireBarriers.end());
			bufferAcquireBarriers.insert(bufferAcquireBarriers.end()
			                           , op.bufferAcquireBarriers.begin()
			                           , op.bufferAcquireBarriers.end());
		}
		LOG("Gathered {} image and {} buffer acquire barriers from {} upload ops"
		   , imageAcquireBarriers.size()
		   , bufferAcquireBarriers.size()
		   , impl->uploads.size());

		if (!imageAcquireBarriers.empty() || !bufferAcquireBarriers.empty()) {
			LOG("submitting acquire barriers");
			auto barrierCmdBuf = frame.barrierCmdBuf;
			barrierCmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
			barrierCmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, bufferAcquireBarriers, imageAcquireBarriers);
			barrierCmdBuf.end();

			submitBuffers[0] = barrierCmdBuf;
			submitBuffers[1] = impl->currentCommandBuffer;
			submit.commandBufferCount = 2;
		} else {
			submitBuffers[0]            = impl->currentCommandBuffer;
			submit.commandBufferCount   = 1;
		}

		submit.pCommandBuffers      = submitBuffers.data();
	} else {
		submitBuffers[0]            = impl->currentCommandBuffer;
		submit.pCommandBuffers      = submitBuffers.data();
		submit.commandBufferCount   = 1;
	}

	waitSemaphores.push_back(frame.acquireSem);
	semWaitMasks.push_back(acquireWaitStage);

	submit.waitSemaphoreCount   = static_cast<uint32_t>(waitSemaphores.size());
	submit.pWaitSemaphores      = waitSemaphores.data();
	submit.pWaitDstStageMask    = semWaitMasks.data();

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &frame.renderDoneSem;

	impl->queue.submit({ submit }, frame.fence);

	// present
	vk::PresentInfoKHR presentInfo;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = &frame.renderDoneSem;
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains        = &impl->swapchain;
	presentInfo.pImageIndices      = &impl->currentFrameIdx;

	auto presentResult = impl->queue.presentKHR(&presentInfo);
	if (presentResult == vk::Result::eSuccess) {
		// nothing to do
	} else if (presentResult == vk::Result::eErrorOutOfDateKHR) {
		LOG("swapchain out of date during presentKHR, marking dirty");
		// swapchain went out of date during present, mark it dirty
		impl->swapchainDirty = true;
	} else {
		LOG("presentKHR failed: {}", vk::to_string(presentResult));
		throw std::runtime_error("presentKHR failed");
	}
	frame.usedRingBufPtr = impl->ringBufPtr;
	frame.status         = Frame::Status::Pending;
	frame.lastFrameNum   = impl->frameNum;

	// mark buffers deleted during frame to be deleted when the frame has synced
	if (!impl->deleteResources.empty()) {
		if (frame.deleteResources.empty()) {
			// frame.deleteResources is empty, easy case
			frame.deleteResources = std::move(impl->deleteResources);
		} else {
			// there's stuff already in frame.deleteResources
			// from deleting things "between" frames
			do {
				frame.deleteResources.emplace_back(std::move(impl->deleteResources.back()));
				impl->deleteResources.pop_back();
			} while (!impl->deleteResources.empty());
		}

		assert(impl->deleteResources.empty());
	}

	if (!impl->uploads.empty()) {
		assert(frame.uploads.empty());
		frame.uploads = std::move(impl->uploads);
	}
	impl->frameNum++;
}


void RendererImpl::waitForFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.status == Frame::Status::Pending);

	auto waitResult = device.waitForFences({ frame.fence }, true, frameTimeoutNanos);
	switch (waitResult) {
	case vk::Result::eSuccess:
		// nothing
		break;

	default: {
		// TODO: better exception types
		std::string s = vk::to_string(waitResult);
		LOG("wait result is not success: {}", s);
		throw std::runtime_error("wait result is not success " + s);
	}
	}

	frame.status = Frame::Status::Done;
}


void RendererImpl::cleanupFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.status == Frame::Status::Done);

	if (!frame.uploads.empty()) {
		for (auto &op : frame.uploads) {
			device.freeCommandBuffers(transferCmdPool, { op.cmdBuf } );
			freeSemaphore(op.semaphore);

			op.cmdBuf    = vk::CommandBuffer();
			op.semaphore = vk::Semaphore();
			op.semWaitMask = vk::PipelineStageFlags();

			if (op.stagingBuffer) {
				assert(op.memory);

				device.destroyBuffer(op.stagingBuffer);
				vmaFreeMemory(allocator, op.memory);

				op.stagingBuffer = vk::Buffer();
				op.memory        = VK_NULL_HANDLE;
				op.coherent      = false;
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

	assert(frame.acquireSem);
	freeSemaphore(frame.acquireSem);
	frame.acquireSem = vk::Semaphore();

	frame.status         = Frame::Status::Ready;
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

	// TODO: have a free list of semaphores instead of creating new ones all the time
	op.semaphore = allocateSemaphore();

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

	assert(op.allocationInfo.memoryType < memoryProperties.memoryTypeCount);
	op.coherent = !!(memoryProperties.memoryTypes[op.allocationInfo.memoryType].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent);

	return op;
}


void RendererImpl::submitUploadOp(UploadOp &&op) {
	op.cmdBuf.end();

	vk::SubmitInfo submit;
	submit.waitSemaphoreCount   = 0;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &op.cmdBuf;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &op.semaphore;

	transferQueue.submit({ submit }, vk::Fence());

	uploads.emplace_back(std::move(op));
}


vk::Semaphore RendererImpl::allocateSemaphore() {
	if (!freeSemaphores.empty()) {
		auto ret = freeSemaphores.back();
		freeSemaphores.pop_back();
		return ret;
	}

	return device.createSemaphore(vk::SemaphoreCreateInfo());
}


void RendererImpl::freeSemaphore(vk::Semaphore sem) {
	assert(sem);

	freeSemaphores.push_back(sem);
}


void RendererImpl::deleteBufferInternal(Buffer &b) {
	assert(!b.ringBufferAlloc);
	assert(b.lastUsedFrame <= lastSyncedFrame);
	this->device.destroyBuffer(b.buffer);
	assert(b.memory != nullptr);
	vmaFreeMemory(this->allocator, b.memory);
	assert(b.type   != +BufferType::Invalid);

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
	fb.depthStencilFormat = Format::Invalid;
}


void RendererImpl::deletePipelineInternal(Pipeline &p) {
	device.destroyPipelineLayout(p.layout);
	p.layout = vk::PipelineLayout();
	device.destroyPipeline(p.pipeline);
	p.pipeline = vk::Pipeline();
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
	rp.numColorAttachments = 0;
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
	mpark::visit(ResourceDeleter(this), r);
}


void RendererImpl::deleteFrameInternal(Frame &f) {
	assert(f.status == Frame::Status::Ready);
	assert(f.fence);
	device.destroyFence(f.fence);
	f.fence = vk::Fence();

	// owned by swapchain, don't delete
	f.image = vk::Image();

	if (f.imageView) {
		device.destroyImageView(f.imageView);
		f.imageView = vk::ImageView();
	}

	if (f.framebuffer) {
		framebuffers.removeWith(f.framebuffer, [this](Framebuffer &fb) {
			// since we synced the frame this is idle and can be removed immediately
			// also if we're shutting down we can't put any more stuff into deleteResources
			device.destroyFramebuffer(fb.framebuffer);
			fb.framebuffer = vk::Framebuffer();
			fb.width       = 0;
			fb.height      = 0;
			fb.depthStencilFormat = Format::Invalid;
		} );

		f.framebuffer = FramebufferHandle();
	}

	assert(f.dsPool);
	device.destroyDescriptorPool(f.dsPool);
	f.dsPool = vk::DescriptorPool();

	assert(f.commandBuffer);
	assert(f.barrierCmdBuf);
	device.freeCommandBuffers(f.commandPool, { f.commandBuffer, f.barrierCmdBuf });
	f.commandBuffer = vk::CommandBuffer();
	f.barrierCmdBuf = vk::CommandBuffer();

	assert(!f.acquireSem);

	assert(f.renderDoneSem);
	freeSemaphore(f.renderDoneSem);
	f.renderDoneSem = vk::Semaphore();

	assert(f.commandPool);
	device.destroyCommandPool(f.commandPool);
	f.commandPool = vk::CommandPool();

	assert(f.deleteResources.empty());
}


void Renderer::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(!impl->inRenderPass);
	impl->inRenderPass  = true;
	impl->validPipeline = false;
#endif  // NDEBUG

	assert(!impl->renderingToSwapchain);

	const auto &pass = impl->renderPasses.get(rpHandle);
	assert(pass.renderPass);
	const auto &fb   = impl->framebuffers.get(fbHandle);
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

	impl->currentCommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);

	impl->currentPipelineLayout = vk::PipelineLayout();
	impl->currentRenderPass  = rpHandle;
	impl->currentFramebuffer = fbHandle;
}


void Renderer::endRenderPass() {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(impl->inRenderPass);
	impl->inRenderPass = false;
#endif  // NDEBUG

	assert(impl->currentRenderPass);

	impl->currentCommandBuffer.endRenderPass();

	if (impl->renderingToSwapchain) {
		assert(!impl->currentFramebuffer);
		impl->renderingToSwapchain = false;
	} else {
		assert(impl->currentFramebuffer);

		const auto &pass = impl->renderPasses.get(impl->currentRenderPass);
		const auto &fb   = impl->framebuffers.get(impl->currentFramebuffer);

		// TODO: track depthstencil layout too
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			if (fb.desc.colors_[i]) {
				auto &rt = impl->renderTargets.get(fb.desc.colors_[i]);
				rt.currentLayout = pass.desc.colorRTs_[i].finalLayout;
			}
		}
		impl->currentFramebuffer = FramebufferHandle();
	}

	impl->currentRenderPass  = RenderPassHandle();
}


void Renderer::layoutTransition(RenderTargetHandle image, Layout src, Layout dest) {
	assert(image);
	assert(dest != +Layout::Undefined);
	assert(src != dest);

	auto &rt = impl->renderTargets.get(image);
	assert(src == +Layout::Undefined || rt.currentLayout == src);
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
	impl->currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { b });
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(impl->inRenderPass);
	assert(impl->pipelineDrawn);
	impl->pipelineDrawn = false;
	impl->validPipeline = true;
	impl->scissorSet = false;
#endif  // NDEBUG

	// TODO: make sure current renderpass matches the one in pipeline

	const auto &p = impl->pipelines.get(pipeline);
	impl->currentCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p.pipeline);
	impl->currentPipelineLayout = p.layout;

	if (!p.scissor) {
		// Vulkan always requires a scissor rect
		// if we don't use scissor set default here
		// TODO: shouldn't need this is previous pipeline didn't use scissor
		// except for first pipeline of the command buffer
		vk::Rect2D rect;
		rect.offset.x      = static_cast<int32_t>(impl->currentViewport.x);
		rect.offset.y      = static_cast<int32_t>(impl->currentViewport.y);
		rect.extent.width  = static_cast<uint32_t>(impl->currentViewport.width);
		rect.extent.height = static_cast<uint32_t>(impl->currentViewport.height);

		impl->currentCommandBuffer.setScissor(0, 1, &rect);
#ifndef NDEBUG
		impl->scissorSet = true;
#endif  // NDEBUG
	}
}


void Renderer::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	assert(impl->inFrame);
	assert(impl->validPipeline);

	auto &b = impl->buffers.get(buffer);
	b.lastUsedFrame = impl->frameNum;
	assert(b.type == +BufferType::Index);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.offset;
	}
	impl->currentCommandBuffer.bindIndexBuffer(b.buffer, offset, bit16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
}


void Renderer::bindVertexBuffer(unsigned int binding, BufferHandle buffer) {
	assert(impl->inFrame);
	assert(impl->validPipeline);

	auto &b = impl->buffers.get(buffer);
	b.lastUsedFrame = impl->frameNum;
	assert(b.type == +BufferType::Vertex);
	// "normal" buffers begin from beginning of buffer
	vk::DeviceSize offset = 0;
	if (b.ringBufferAlloc) {
		// but ephemeral buffers use the ringbuffer and an offset
		offset = b.offset;
	}
	impl->currentCommandBuffer.bindVertexBuffers(binding, 1, &b.buffer, &offset);
}


void Renderer::bindDescriptorSet(unsigned int dsIndex, DSLayoutHandle layoutHandle, const void *data_) {
	assert(impl->inFrame);
	assert(impl->validPipeline);

	const DescriptorSetLayout &layout = impl->dsLayouts.get(layoutHandle);

	vk::DescriptorSetAllocateInfo dsInfo;
	dsInfo.descriptorPool      = impl->frames.at(impl->currentFrameIdx).dsPool;
	dsInfo.descriptorSetCount  = 1;
	dsInfo.pSetLayouts         = &layout.layout;

	auto device = impl->device;

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
			HEDLEY_UNREACHABLE();
			break;

		case DescriptorType::UniformBuffer:
		case DescriptorType::StorageBuffer: {
			// this is part of the struct, we know it's correctly aligned and right type
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			Buffer &buffer = impl->buffers.get(handle);
			assert(buffer.size > 0);
			buffer.lastUsedFrame = impl->frameNum;
			assert((buffer.type == +BufferType::Uniform && l.type == +DescriptorType::UniformBuffer)
			    || (buffer.type == +BufferType::Storage && l.type == +DescriptorType::StorageBuffer));

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
			const auto &sampler = impl->samplers.get(*reinterpret_cast<const SamplerHandle *>(data + l.offset));
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
			const auto &tex = impl->textures.get(texHandle);
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

			const Texture &tex = impl->textures.get(combined.tex);
			assert(tex.image);
			assert(tex.imageView);
			const Sampler &s   = impl->samplers.get(combined.sampler);
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

		}

		index++;
	}

	device.updateDescriptorSets(writes, {});
	impl->currentCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, impl->currentPipelineLayout, dsIndex, { ds }, {});
}


bool RendererImpl::isRenderPassCompatible(const RenderPass &pass, const Framebuffer &fb) {
	if (pass.numSamples != fb.numSamples) {
		return false;
	}

	if (fb.desc.depthStencil_) {
		const auto &depthRT = renderTargets.get(fb.desc.depthStencil_);
		assert(depthRT.format == fb.depthStencilFormat);

		if (pass.desc.depthStencilFormat_ != fb.depthStencilFormat) {
			return false;
		}
	} else {
		assert(fb.depthStencilFormat == +Format::Invalid);
		if (pass.desc.depthStencilFormat_ != +Format::Invalid) {
			return false;
		}
	}

	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (fb.desc.colors_[i]) {
			const auto &colorRT = renderTargets.get(fb.desc.colors_[i]);
			assert(colorRT.format == fb.colorFormats[i]);

			if (pass.desc.colorRTs_[i].format != fb.colorFormats[i]) {
				return false;
			}
		} else {
			assert(fb.colorFormats[i] == +Format::Invalid);
			if (pass.desc.colorRTs_[i].format != +Format::Invalid) {
				return false;
			}
		}
	}

	return true;
}


void Renderer::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(impl->inFrame);

	impl->currentViewport.x        = static_cast<float>(x);
	// TODO: check viewport y direction when not using full height
	impl->currentViewport.y        = static_cast<float>(y);
	impl->currentViewport.width    = static_cast<float>(width);
	impl->currentViewport.height   = static_cast<float>(height);
	impl->currentViewport.maxDepth = 1.0f;

	// use VK_KHR_maintenance1 negative viewport to flip it
	// so we don't need flip in shader
	vk::Viewport realViewport =  impl->currentViewport;
	realViewport.y            += realViewport.height;
	realViewport.height       =  -realViewport.height;

	impl->currentCommandBuffer.setViewport(0, 1, &realViewport);
}


void Renderer::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
#ifndef NDEBUG
	assert(impl->validPipeline);
	impl->scissorSet = true;
#endif  // NDEBUG

	vk::Rect2D rect;
	rect.offset.x      = x;
	rect.offset.y      = y;
	rect.extent.width  = width;
	rect.extent.height = height;

	impl->currentCommandBuffer.setScissor(0, 1, &rect);
}


void Renderer::blit(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);

	// TODO: check numSamples is 1 for both

	const auto &srcRT = impl->renderTargets.get(source);
	assert(srcRT.width       >  0);
	assert(srcRT.height      >  0);
	assert(srcRT.currentLayout == +Layout::TransferSrc);

	const auto &destRT = impl->renderTargets.get(target);
	assert(destRT.width      >  0);
	assert(destRT.height     >  0);
	assert(destRT.currentLayout == +Layout::TransferDst);

	assert(srcRT.width       == destRT.width);
	assert(srcRT.height      == destRT.height);

	// TODO: check they're both color targets
	// or implement depth blit

	vk::ImageBlit b;
	b.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	b.srcSubresource.layerCount = 1;
	b.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	b.dstSubresource.layerCount = 1;
	b.srcOffsets[1u].x          = srcRT.width;
	b.srcOffsets[1u].y          = srcRT.height;
	b.srcOffsets[1u].z          = 1;
	b.dstOffsets[1u].x          = srcRT.width;
	b.dstOffsets[1u].y          = srcRT.height;
	b.dstOffsets[1u].z          = 1;
	impl->currentCommandBuffer.blitImage(srcRT.image, vk::ImageLayout::eTransferSrcOptimal, destRT.image, vk::ImageLayout::eTransferDstOptimal, { b }, vk::Filter::eNearest );
}


void Renderer::resolveMSAA(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);

	// TODO: check numSamples make sense
	// TODO: check they're both color targets

	const auto &srcRT = impl->renderTargets.get(source);
	assert(srcRT.width       >  0);
	assert(srcRT.height      >  0);
	assert(srcRT.currentLayout == +Layout::TransferSrc);

	const auto &destRT = impl->renderTargets.get(target);
	assert(destRT.width      >  0);
	assert(destRT.height     >  0);
	assert(destRT.currentLayout == +Layout::TransferDst);

	assert(srcRT.width       == destRT.width);
	assert(srcRT.height      == destRT.height);

	vk::ImageResolve r;
	r.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	r.srcSubresource.layerCount = 1;
	r.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	r.dstSubresource.layerCount = 1;
	r.extent.width              = srcRT.width;
	r.extent.height             = srcRT.height;
	r.extent.depth              = 1;
	impl->currentCommandBuffer.resolveImage(srcRT.image, vk::ImageLayout::eTransferSrcOptimal, destRT.image, vk::ImageLayout::eTransferDstOptimal, { r } );
}


void Renderer::draw(unsigned int firstVertex, unsigned int vertexCount) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	impl->pipelineDrawn = true;
#endif  // NDEBUG

	impl->currentCommandBuffer.draw(vertexCount, 1, firstVertex, 0);
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	assert(instanceCount > 0);
	impl->pipelineDrawn = true;
#endif //  NDEBUG

	impl->currentCommandBuffer.drawIndexed(vertexCount, instanceCount, 0, 0, 0);
}


void Renderer::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int /* minIndex */, unsigned int /* maxIndex */) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	impl->pipelineDrawn = true;
#endif //  NDEBUG

	impl->currentCommandBuffer.drawIndexed(vertexCount, 1, firstIndex, 0, 0);
}


} // namespace renderer


namespace std {

	size_t hash<renderer::Resource>::operator()(const renderer::Resource &r) const {
		return mpark::visit(renderer::ResourceHasher(), r);
	}

}  // namespace std


#endif  // RENDERER_VULKAN
