#ifndef RENDERER_H
#define RENDERER_H


#include <memory>
#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <SDL.h>


#ifdef RENDERER_OPENGL

#include <GL/glew.h>


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */);


#elif defined(RENDERER_VULKAN)

#include <vulkan/vulkan.hpp>


#elif defined(RENDERER_NULL)

#else

#error "No renderer specified"


#endif  // RENDERER


namespace ShaderDefines {

using namespace glm;

#include "../shaderDefines.h"

}  // namespace ShaderDefines


#define MAX_COLOR_RENDERTARGETS 2


class FragmentShader;
class Framebuffer;
class Shader;
class VertexShader;


typedef uint32_t BufferHandle;

class FramebufferHandle {
	uint32_t handle;

	friend class Renderer;

	FramebufferHandle(uint32_t h)
	: handle(h)
	{
	}

public:

	FramebufferHandle()
	: handle(0)
	{
	}

	FramebufferHandle(const FramebufferHandle &) = default;
	FramebufferHandle(FramebufferHandle &&)      = default;

	FramebufferHandle &operator=(const FramebufferHandle &) = default;
	FramebufferHandle &operator=(FramebufferHandle &&)      = default;



	operator bool() const {
		return handle;
	}
};

typedef uint32_t PipelineHandle;
typedef uint32_t RenderPassHandle;
typedef uint32_t RenderTargetHandle;
typedef uint32_t SamplerHandle;

class ShaderHandle {
	uint32_t handle;

	friend class Renderer;

	explicit ShaderHandle(uint32_t h)
	: handle(h)
	{
	}

public:
	ShaderHandle()
	: handle(0)
	{
	}
};

typedef uint32_t TextureHandle;
typedef uint32_t UniformBufferHandle;


struct SwapchainDesc {
	unsigned int width, height;
	unsigned int numFrames;
	bool vsync;
	bool fullscreen;


	SwapchainDesc()
	: width(0)
	, height(0)
	, numFrames(0)
	, vsync(true)
	, fullscreen(false)
	{
	}
};


#define MAX_TEXTURE_MIPLEVELS 14
#define MAX_TEXTURE_SIZE      (1 << (MAX_TEXTURE_MIPLEVELS - 1))


enum Format {
	  Invalid
	, R8
	, RG8
	, RGB8
	, RGBA8
	, Depth16
};


struct RenderTargetDesc {
	RenderTargetDesc()
	: width_(0)
	, height_(0)
	, format_(Invalid)
	{
	}

	~RenderTargetDesc() { }

	RenderTargetDesc(const RenderTargetDesc &) = default;
	RenderTargetDesc(RenderTargetDesc &&)      = default;

	RenderTargetDesc &operator=(const RenderTargetDesc &) = default;
	RenderTargetDesc &operator=(RenderTargetDesc &&)      = default;


	RenderTargetDesc &width(unsigned int w) {
		assert(w < MAX_TEXTURE_SIZE);
		width_ = w;
		return *this;
	}

	RenderTargetDesc &height(unsigned int h) {
		assert(h < MAX_TEXTURE_SIZE);
		height_ = h;
		return *this;
	}

	RenderTargetDesc &format(Format f) {
		format_ = f;
		return *this;
	}

private:

	unsigned int width_, height_;
	// TODO: unsigned int multisample;
	Format format_;

	friend class Renderer;
};


struct FramebufferDesc {
	FramebufferDesc()
	: depthStencil_(0)
	{
		std::fill(colors_.begin(), colors_.end(), 0);
	}

	~FramebufferDesc() { }

	FramebufferDesc(const FramebufferDesc &) = default;
	FramebufferDesc(FramebufferDesc &&)      = default;

	FramebufferDesc &operator=(const FramebufferDesc &) = default;
	FramebufferDesc &operator=(FramebufferDesc &&)      = default;

	FramebufferDesc &depthStencil(RenderTargetHandle ds) {
		depthStencil_ = ds;
		return *this;
	}

	FramebufferDesc &color(unsigned int index, RenderTargetHandle c) {
		assert(index < MAX_COLOR_RENDERTARGETS);
		colors_[index] = c;
		return *this;
	}

private:

	RenderTargetHandle depthStencil_;
	std::array<RenderTargetHandle, MAX_COLOR_RENDERTARGETS> colors_;

	friend class Renderer;
};


struct TextureDesc {
	TextureDesc()
	: width_(0)
	, height_(0)
	, numMips_(1)
	, format_(Invalid)
	{
		std::fill(mipData_.begin(), mipData_.end(), nullptr);
	}

	~TextureDesc() { }

	TextureDesc(const TextureDesc &) = default;
	TextureDesc(TextureDesc &&)      = default;

	TextureDesc &operator=(const TextureDesc &) = default;
	TextureDesc &operator=(TextureDesc &&)      = default;


	TextureDesc &width(unsigned int w) {
		assert(w < MAX_TEXTURE_SIZE);
		width_ = w;
		return *this;
	}

	TextureDesc &height(unsigned int h) {
		assert(h < MAX_TEXTURE_SIZE);
		height_ = h;
		return *this;
	}

	TextureDesc &format(Format f) {
		format_ = f;
		return *this;
	}

	TextureDesc &mipLevelData(unsigned int level, const void *data) {
		assert(level < numMips_);
		mipData_[level] = data;
		return *this;
	}


private:

	unsigned int  width_, height_;
	unsigned int  numMips_;
	Format        format_;
	std::array<const void *, MAX_TEXTURE_MIPLEVELS> mipData_;

	friend class Renderer;
};


typedef std::unordered_map<std::string, std::string> ShaderMacros;


enum FilterMode {
	  Nearest
	, Linear
};


enum WrapMode {
	  Clamp
	, Wrap
};


struct SamplerDesc {
	SamplerDesc()
	: min(Nearest)
	, mag(Nearest)
	, anisotropy(0)
	, wrapMode(Wrap)
	{
	}

	SamplerDesc(const SamplerDesc &desc) = default;
	SamplerDesc(SamplerDesc &&desc)      = default;

	SamplerDesc &operator=(const SamplerDesc &desc) = default;
	SamplerDesc &operator=(SamplerDesc &&desc)      = default;

	~SamplerDesc() {}

	SamplerDesc &minFilter(FilterMode m) {
		min = m;
		return *this;
	}

	SamplerDesc &magFilter(FilterMode m) {
		mag = m;
		return *this;
	}

private:

	FilterMode  min, mag;
	uint32_t    anisotropy;
	WrapMode    wrapMode;

	friend class Renderer;
};


class PipelineDesc {
	ShaderHandle shader_;
	bool depthWrite_;
	bool depthTest_;
	bool cullFaces_;


public:

	PipelineDesc &shader(ShaderHandle h) {
		shader_ = h;
		return *this;
	}

	PipelineDesc &depthWrite(bool d) {
		depthWrite_ = d;
		return *this;
	}

	PipelineDesc &depthTest(bool d) {
		depthTest_ = d;
		return *this;
	}

	PipelineDesc &cullFaces(bool c) {
		cullFaces_ = c;
		return *this;
	}

	PipelineDesc()
	: depthWrite_(false)
	, depthTest_(false)
	, cullFaces_(false)
	{
	}

	~PipelineDesc() {}

	PipelineDesc(const PipelineDesc &desc) = default;
	PipelineDesc(PipelineDesc &&desc)      = default;

	PipelineDesc &operator=(const PipelineDesc &desc) = default;
	PipelineDesc &operator=(PipelineDesc &&desc)      = default;

	friend class Renderer;
};


struct RendererDesc {
	bool debug;
	SwapchainDesc swapchain;


	RendererDesc()
	: debug(false)
	{
	}
};


class Renderer {
#ifdef RENDERER_OPENGL

	SDL_Window *window;
	SDL_GLContext context;

	SwapchainDesc swapchainDesc;

	GLuint vao;

	std::unordered_map<GLuint, std::unique_ptr<Framebuffer> > framebuffers;
	std::unordered_map<GLuint, std::unique_ptr<Shader> > shaders;
	std::unordered_map<uint32_t, PipelineDesc>                 pipelines;

#endif  // RENDERER_OPENGL

	std::unordered_map<RenderTargetHandle, RenderTargetDesc> renderTargets;


	explicit Renderer(const RendererDesc &desc);
	Renderer(const Renderer &)            = delete;
	Renderer(Renderer &&)                 = delete;

	Renderer &operator=(const Renderer &) = delete;
	Renderer &operator=(Renderer &&)      = delete;


public:

	static Renderer *createRenderer(const RendererDesc &desc);

	~Renderer();


	RenderTargetHandle  createRenderTarget(const RenderTargetDesc &desc);
	FramebufferHandle   createFramebuffer(const FramebufferDesc &desc);
	ShaderHandle        createShader(const std::string &name, const ShaderMacros &macros);
	PipelineHandle      createPipeline(const PipelineDesc &desc);
	// TODO: add buffer usage flags
	BufferHandle        createBuffer(uint32_t size, const void *contents);
	BufferHandle        createEphemeralBuffer(uint32_t size, const void *contents);
	// image ?
	// descriptor set
	SamplerHandle       createSampler(const SamplerDesc &desc);
	TextureHandle       createTexture(const TextureDesc &desc);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);
	void deleteRenderTarget(RenderTargetHandle &fbo);


	void recreateSwapchain(const SwapchainDesc &desc);

	// rendering
	void beginFrame();
	void presentFrame(FramebufferHandle fbo);

	void beginRenderPass(RenderPassHandle, FramebufferHandle);
	void endRenderPass();

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void blitFBO(FramebufferHandle src, FramebufferHandle dest);

	void bindFramebuffer(FramebufferHandle fbo);
	void bindPipeline(PipelineHandle pipeline);
	void bindShader(ShaderHandle shader);
	void bindIndexBuffer();
	void bindVertexBuffer();

	// TODO: replace with descriptor set stuff
	void bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
};


#endif  // RENDERER_H
