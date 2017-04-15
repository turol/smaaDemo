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

#error "No renderer specififued"


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
typedef uint32_t FramebufferHandle;
typedef uint32_t PipelineHandle;
typedef uint32_t RenderPassHandle;
typedef uint32_t RenderTargetHandle;
typedef uint32_t SamplerHandle;
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
#if 0   // clang warns...
	std::string shaderName;
	ShaderMacros macros;

	FramebufferHandle framebuffer;
	// depthstencil
	// blending
#endif // 0
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


	// render target
	RenderTargetHandle  createRenderTarget(const RenderTargetDesc &desc);
	// FramebufferHandle   createFramebuffer(const FramebufferDesc &desc);
	std::unique_ptr<Framebuffer> createFramebuffer(const FramebufferDesc &desc);
	PipelineHandle      createPipeline(const PipelineDesc &desc);
	// TODO: add buffer usage flags
	BufferHandle        createBuffer(uint32_t size, const void *contents);
	BufferHandle        createEphemeralBuffer(uint32_t size, const void *contents);
	// image ?
	// descriptor set
	SamplerHandle       createSampler(const SamplerDesc &desc);
	TextureHandle       createTexture(const TextureDesc &desc);

	void deleteBuffer(BufferHandle handle);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);


	void recreateSwapchain(const SwapchainDesc &desc);

	// rendering
	void beginFrame();
	void presentFrame();

	void beginRenderPass(RenderPassHandle);
	void endRenderPass();

	void bindFramebuffer(const std::unique_ptr<Framebuffer> &fbo);
	void bindFramebuffer(FramebufferHandle);
	void bindPipeline(PipelineHandle);
	void bindIndexBuffer();
	void bindVertexBuffer();
	// descriptor set

	void draw();
};


class Framebuffer {
	// TODO: need a proper Render object to control the others
	friend class Renderer;
	friend class SMAADemo;

#ifdef RENDERER_OPENGL

	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;

#endif  // RENDERER_OPENGL

	Framebuffer() = delete;
	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&) = delete;
	Framebuffer &operator=(Framebuffer &&) = delete;

public:

#ifdef RENDERER_OPENGL

	explicit Framebuffer(GLuint fbo_)
	: fbo(fbo_)
	, colorTex(0)
	, depthTex(0)
	, width(0)
	, height(0)
	{
	}

#endif  // RENDERER_OPENGL


	~Framebuffer();

	void blitTo(Framebuffer &target);
};


class VertexShader {
#ifdef RENDERER_OPENGL

	GLuint shader;

#endif  // RENDERER_OPENGL

	VertexShader() = delete;

	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	friend class Shader;

public:

	VertexShader(const std::string &name, const ShaderMacros &macros);

	~VertexShader();
};


class FragmentShader {
#ifdef RENDERER_OPENGL

	GLuint shader;

#endif  // RENDERER_OPENGL

	FragmentShader() = delete;

	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&) = delete;
	FragmentShader &operator=(FragmentShader &&) = delete;

	friend class Shader;

public:

	FragmentShader(const std::string &name, const ShaderMacros &macros);

	~FragmentShader();
};


class Shader {
#ifdef RENDERER_OPENGL

    GLuint program;

#endif  // RENDERER_OPENGL

	Shader() = delete;
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&) = delete;
	Shader &operator=(Shader &&) = delete;

public:
	Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader);

	~Shader();

	void bind();
};


#endif  // RENDERER_H
