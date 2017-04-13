#ifndef RENDERER_H
#define RENDERER_H


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


enum Format {
};


struct RenderTargetDesc {
	unsigned int width, height;
	unsigned int multisample;
	Format format;
};


struct FramebufferDesc {
	RenderTargetHandle depthStencil;
	RenderTargetHandle colors[MAX_COLOR_RENDERTARGETS];
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
	FilterMode  min, mag;
	uint32_t    anisotropy;
	WrapMode    wrapMode;
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


	explicit Renderer(const RendererDesc &desc);
	Renderer(const Renderer &)            = delete;
	Renderer(Renderer &&)                 = delete;

	Renderer &operator=(const Renderer &) = delete;
	Renderer &operator=(Renderer &&)      = delete;


public:

	static Renderer *createRenderer(const RendererDesc &desc);

	~Renderer();


	// render target
	RenderTargetHandle  createFramebuffer(const RenderTargetDesc &desc);
	FramebufferHandle   createFramebuffer(const FramebufferDesc &desc);
	PipelineHandle      createPipeline(const PipelineDesc &desc);
	BufferHandle        createBuffer(uint32_t size, const char *contents);
	BufferHandle        createEphemeralBuffer(uint32_t size, const char *contents);
	// texture
	// image ?
	// descriptor set

	void deleteBuffer(BufferHandle handle);
	void deleteSampler(SamplerHandle handle);


	void recreateSwapchain(const SwapchainDesc &desc);

	// rendering
	void beginFrame();
	void presentFrame();

	void beginRenderPass(RenderPassHandle);
	void endRenderPass();

	void bindFramebuffer(FramebufferHandle);
	void bindPipeline(PipelineHandle);
	void bindIndexBuffer();
	void bindVertexBuffer();
	// descriptor set

	void draw();
};


class Framebuffer {
	// TODO: need a proper Render object to control the others
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

	void bind();

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
