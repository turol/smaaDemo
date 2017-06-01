#ifndef RENDERER_H
#define RENDERER_H


#include <string>
#include <unordered_map>
#include <array>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <SDL.h>


namespace ShaderDefines {

using namespace glm;

#include "../shaderDefines.h"

}  // namespace ShaderDefines


#define MAX_COLOR_RENDERTARGETS 2
#define MAX_VERTEX_ATTRIBS      4
#define MAX_VERTEX_BUFFERS      1


struct RendererImpl;
struct FragmentShader;
struct Framebuffer;
struct Shader;
struct VertexShader;


typedef uint32_t BufferHandle;

class FramebufferHandle {
	uint32_t handle;

	friend struct RendererImpl;

	explicit FramebufferHandle(uint32_t h)
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


class VertexShaderHandle {
	uint32_t handle;

	friend struct RendererImpl;

	explicit VertexShaderHandle(uint32_t h)
	: handle(h)
	{
	}

public:
	VertexShaderHandle()
	: handle(0)
	{
	}
};


class FragmentShaderHandle {
	uint32_t handle;

	friend struct RendererImpl;

	explicit FragmentShaderHandle(uint32_t h)
	: handle(h)
	{
	}

public:
	FragmentShaderHandle()
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


namespace VtxFormat {


enum VtxFormat {
	  Float
	, UNorm8
};

}  // namespace VtxFormat

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

	friend struct RendererImpl;
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

	friend struct RendererImpl;
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

	friend struct RendererImpl;
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
	WrapMode    wrapMode;

	friend struct RendererImpl;
};


class RenderPassDesc {
};


class PipelineDesc {
	VertexShaderHandle vertexShader_;
	FragmentShaderHandle fragmentShader_;
	uint32_t vertexAttribMask;
	bool depthWrite_;
	bool depthTest_;
	bool cullFaces_;
	bool scissorTest_;
	bool blending_;
	// TODO: blend equation and function
	// TODO: per-MRT blending

	struct VertexAttr {
		uint8_t bufBinding;
		uint8_t count;
		VtxFormat::VtxFormat format;
		uint8_t offset;
	};

	struct VertexBuf {
		uint32_t stride;
	};

	std::array<VertexAttr, MAX_VERTEX_ATTRIBS> vertexAttribs;
	std::array<VertexBuf,  MAX_VERTEX_BUFFERS> vertexBuffers;


public:

	PipelineDesc &vertexShader(VertexShaderHandle h) {
		vertexShader_ = h;
		return *this;
	}

	PipelineDesc &fragmentShader(FragmentShaderHandle h) {
		fragmentShader_ = h;
		return *this;
	}

	PipelineDesc &vertexAttrib(uint32_t attrib, uint8_t bufBinding, uint8_t count, VtxFormat::VtxFormat format, uint8_t offset) {
		assert(attrib < MAX_VERTEX_ATTRIBS);

		vertexAttribs[attrib].bufBinding = bufBinding;
		vertexAttribs[attrib].count      = count;
		vertexAttribs[attrib].format     = format;
		vertexAttribs[attrib].offset     = offset;


		vertexAttribMask |= (1 << attrib);
		return *this;
	}

	PipelineDesc &vertexBufferStride(uint8_t buf, uint32_t stride) {
		assert(buf < MAX_VERTEX_BUFFERS);
		vertexBuffers[buf].stride = stride;

		return *this;
	}

	PipelineDesc &blending(bool b) {
		blending_ = b;
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

	PipelineDesc &scissorTest(bool s) {
		scissorTest_ = s;
		return *this;
	}

	PipelineDesc()
	: vertexAttribMask(0)
	, depthWrite_(false)
	, depthTest_(false)
	, cullFaces_(false)
	, scissorTest_(false)
	, blending_(false)
	{
		for (unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; i++) {
			vertexAttribs[i].bufBinding = 0;
			vertexAttribs[i].count      = 0;
			vertexAttribs[i].offset     = 0;
		}

		for (unsigned int i = 0; i < MAX_VERTEX_BUFFERS; i++) {
			vertexBuffers[i].stride = 0;
		}
	}

	~PipelineDesc() {}

	PipelineDesc(const PipelineDesc &desc) = default;
	PipelineDesc(PipelineDesc &&desc)      = default;

	PipelineDesc &operator=(const PipelineDesc &desc) = default;
	PipelineDesc &operator=(PipelineDesc &&desc)      = default;

	friend struct RendererImpl;
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
	RendererImpl *impl;

	explicit Renderer(RendererImpl *impl_);


public:

	static Renderer createRenderer(const RendererDesc &desc);

	Renderer(const Renderer &)            = delete;
	Renderer &operator=(const Renderer &) = delete;

	Renderer &operator=(Renderer &&);
	Renderer(Renderer &&);

	Renderer();
	~Renderer();


	RenderTargetHandle  createRenderTarget(const RenderTargetDesc &desc);
	FramebufferHandle   createFramebuffer(const FramebufferDesc &desc);
	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);
	RenderPassHandle     createRenderPass(FramebufferHandle fbo, const RenderPassDesc &desc);
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
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle pass, FramebufferHandle fbo);
	void endRenderPass();

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void blitFBO(FramebufferHandle src, FramebufferHandle dest);

	void bindFramebuffer(FramebufferHandle fbo);
	void bindPipeline(PipelineHandle pipeline);
	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	// TODO: replace with descriptor set stuff
	void bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler);
	void bindUniformBuffer(unsigned int index, BufferHandle buffer);
	void bindStorageBuffer(unsigned int index, BufferHandle buffer);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
};


#endif  // RENDERER_H
