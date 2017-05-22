#ifdef RENDERER_VULKAN


#define STUBBED(str) \
	{ \
		static bool seen = false; \
		if (!seen) { \
			printf("STUBBED: %s in %s at %s:%d\n", str, __PRETTY_FUNCTION__, __FILE__,  __LINE__); \
			seen = true; \
		} \
	}



#include "Renderer.h"
#include "Utils.h"


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


Renderer *Renderer::createRenderer(const RendererDesc &desc) {
	return new Renderer(desc);
}


Renderer::Renderer(const RendererDesc &desc)
: instance(VK_NULL_HANDLE)
, physicalDevice(VK_NULL_HANDLE)
, surface(VK_NULL_HANDLE)
, inRenderPass(false)
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

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	printf("%u queue families\n", static_cast<unsigned int>(queueProps.size()));
	for (uint32_t i = 0; i < queueProps.size(); i++) {
		const auto &queue = queueProps[i];
		printf(" Queue family %u\n", i);
		printf("  Flags: %s\n", vk::to_string(queue.queueFlags).c_str());
		printf("  Count: %u\n", queue.queueCount);
		printf("  Timestamp valid bits: %u\n", queue.timestampValidBits);
		printf("  Image transfer granularity: (%u, %u, %u)\n", queue.minImageTransferGranularity.width, queue.minImageTransferGranularity.height, queue.minImageTransferGranularity.depth);
	}

	if(!SDL_Vulkan_CreateSurface(window,
								 (SDL_vulkanInstance) instance,
								 (SDL_vulkanSurface *)&surface))
	{
		printf("failed to create Vulkan surface");
		// TODO: free instance, window etc...
		exit(1);
	}

	STUBBED("");
}


Renderer::~Renderer() {
	assert(instance);
	assert(surface);

	instance.destroySurfaceKHR(surface);
	surface = VK_NULL_HANDLE;

	instance.destroy();
	instance = VK_NULL_HANDLE;

	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	STUBBED("");

	return 0;
}


BufferHandle Renderer::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	STUBBED("");

	return 0;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc & /* desc */) {
	STUBBED("");

	return 0;
}


PipelineHandle Renderer::createPipeline(const PipelineDesc & /* desc */) {
	STUBBED("");

	return 0;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	STUBBED("");

	return 0;
}


SamplerHandle Renderer::createSampler(const SamplerDesc & /* desc */) {
	STUBBED("");

	return 0;
}


ShaderHandle Renderer::createShader(const std::string & /* name */, const ShaderMacros & /* macros */) {
	STUBBED("");

	return ShaderHandle (0);
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	STUBBED("");

	return 0;
}


void Renderer::deleteBuffer(BufferHandle /* handle */) {
	STUBBED("");
}


void Renderer::deleteFramebuffer(FramebufferHandle /* fbo */) {
	STUBBED("");
}


void Renderer::deleteRenderTarget(RenderTargetHandle &) {
	STUBBED("");
}


void Renderer::deleteSampler(SamplerHandle /* handle */) {
	STUBBED("");
}


void Renderer::deleteTexture(TextureHandle /* handle */) {
	STUBBED("");
}


void Renderer::recreateSwapchain(const SwapchainDesc & /* desc */) {
	STUBBED("");
}


void Renderer::blitFBO(FramebufferHandle /* src */, FramebufferHandle /* dest */) {
	STUBBED("");
}


void Renderer::beginFrame() {
	STUBBED("");
}


void Renderer::presentFrame(FramebufferHandle /* fbo */) {
	STUBBED("");
}


void Renderer::beginRenderPass(FramebufferHandle /* fbo */) {
	assert(!inRenderPass);
	inRenderPass = true;

	STUBBED("");
}


void Renderer::endRenderPass() {
	assert(inRenderPass);
	inRenderPass = false;

	STUBBED("");
}


void Renderer::bindFramebuffer(FramebufferHandle fbo) {
	assert(fbo.handle);

	STUBBED("");
}


void Renderer::bindShader(ShaderHandle shader) {
	assert(shader.handle != 0);

	STUBBED("");
}


void Renderer::bindPipeline(PipelineHandle /* pipeline */) {
	// assert(pipeline != 0);

	STUBBED("");
}


void Renderer::bindIndexBuffer(BufferHandle /* buffer */, bool /* bit16 */ ) {
	STUBBED("");
}


void Renderer::bindVertexBuffer(unsigned int /* binding */, BufferHandle /* buffer */, unsigned int /* stride */) {
	STUBBED("");
}


void Renderer::bindTexture(unsigned int /* unit */, TextureHandle /* tex */, SamplerHandle /* sampler */) {
	STUBBED("");
}


void Renderer::bindUniformBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
	STUBBED("");
}


void Renderer::bindStorageBuffer(unsigned int /* index */, BufferHandle /* buffer */) {
	STUBBED("");
}


void Renderer::setViewport(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	STUBBED("");
}


void Renderer::setScissorRect(unsigned int /* x */, unsigned int /* y */, unsigned int /* width */, unsigned int /* height */) {
	STUBBED("");
}


void Renderer::draw(unsigned int /* firstVertex */, unsigned int /* vertexCount */) {
	assert(inRenderPass);

	STUBBED("");
}


void Renderer::drawIndexedInstanced(unsigned int /* vertexCount */, unsigned int /* instanceCount */) {
	assert(inRenderPass);

	STUBBED("");
}


#endif  // RENDERER_VULKAN
