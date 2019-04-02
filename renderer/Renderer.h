/*
Copyright (c) 2015-2019 Alternative Games Ltd / Turo Lamminen

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


#ifndef RENDERER_H
#define RENDERER_H


#include <string>
#include <unordered_map>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>

#include <SDL.h>

#include "utils/Utils.h"  // for isPow2


namespace renderer {


#define MAX_COLOR_RENDERTARGETS 2
#define MAX_VERTEX_ATTRIBS      4
#define MAX_VERTEX_BUFFERS      1
#define MAX_DESCRIPTOR_SETS     2  // per pipeline
#define MAX_TEXTURE_MIPLEVELS   14
#define MAX_TEXTURE_SIZE        (1 << (MAX_TEXTURE_MIPLEVELS - 1))


struct Buffer;
struct DescriptorSetLayout;
struct Framebuffer;
struct Pipeline;
struct RenderPass;
struct RenderTarget;
struct Sampler;
struct Texture;


template <class T> class ResourceContainer;


template <class T>
class Handle {
	friend class ResourceContainer<T>;

	uint32_t handle;


	explicit Handle(uint32_t handle_)
	: handle(handle_)
	{
	}

public:


	Handle()
	: handle(0)
	{
	}


	~Handle() {
	};


	Handle(const Handle<T> &other)                     = default;
	Handle &operator=(const Handle<T> &other) noexcept = default;

	Handle(Handle<T> &&other) noexcept                 = default;
	Handle<T> &operator=(Handle<T> &&other) noexcept   = default;


	bool operator==(const Handle<T> &other) const {
		return handle == other.handle;
	}


	bool operator!=(const Handle<T> &other) const {
		return handle != other.handle;
	}


	explicit operator bool() const {
		return handle != 0;
	}
};


typedef Handle<Buffer>               BufferHandle;
typedef Handle<DescriptorSetLayout>  DSLayoutHandle;
typedef Handle<Framebuffer>          FramebufferHandle;
typedef Handle<Pipeline>             PipelineHandle;
typedef Handle<RenderPass>           RenderPassHandle;
typedef Handle<RenderTarget>         RenderTargetHandle;
typedef Handle<Sampler>              SamplerHandle;
typedef Handle<Texture>              TextureHandle;


enum class BlendFunc : uint8_t {
	  Zero
	, One
	, Constant
	, SrcAlpha
	, OneMinusSrcAlpha
};


enum class BufferType : uint8_t {
	  Invalid
	, Index
	, Uniform
	, Storage
	, Vertex
	, Everything
};


enum class DescriptorType : uint8_t {
	  End
	, UniformBuffer
	, StorageBuffer
	, Sampler
	, Texture
	, CombinedSampler
	, Count
};


enum class FilterMode : uint8_t{
	  Nearest
	, Linear
};


enum class Format : uint8_t {
	  Invalid
	, R8
	, RG8
	, RGB8
	, RGBA8
	, sRGBA8
	, RG16Float
	, RGBA16Float
	, RGBA32Float
	, Depth16
	, Depth16S8
	, Depth24S8
	, Depth24X8
	, Depth32Float
};


enum class Layout : uint8_t {
	  Undefined
	, ShaderRead
	, TransferSrc
	, TransferDst
	, ColorAttachment
};


// rendertarget behavior when RenderPass begins
enum class PassBegin : uint8_t {
	  DontCare
	, Keep
	, Clear
};


enum class VSync : uint8_t {
	  Off
	, On
	, LateSwapTear
};


enum class VtxFormat : uint8_t {
	  Float
	, UNorm8
};


enum class WrapMode : uint8_t {
	  Clamp
	, Wrap
};


// CombinedSampler helper
struct CSampler {
	TextureHandle tex;
	SamplerHandle sampler;


	CSampler()
	{
	}

	CSampler(const CSampler &other)                = default;
	CSampler(CSampler &&other) noexcept            = default;

	CSampler &operator=(const CSampler &other)     = default;
	CSampler &operator=(CSampler &&other) noexcept = default;

	~CSampler() {
	}
};


struct DescriptorLayout {
	DescriptorType  type;
	unsigned int    offset;
	// TODO: stage flags
};


struct MemoryStats {
	uint32_t allocationCount;
	uint32_t subAllocationCount;
	uint64_t usedBytes;
	uint64_t unusedBytes;


	MemoryStats()
	: allocationCount(0)
	, subAllocationCount(0)
	, usedBytes(0)
	, unusedBytes(0)
	{
	}

	~MemoryStats() {}

	MemoryStats(const MemoryStats &stats)            = default;
	MemoryStats(MemoryStats &&stats) noexcept            = default;

	MemoryStats &operator=(const MemoryStats &stats) = default;
	MemoryStats &operator=(MemoryStats &&stats) noexcept = default;
};


typedef std::unordered_map<std::string, std::string> ShaderMacros;


const char *layoutName(Layout layout);
const char *formatName(Format format);
const char *passBeginName(PassBegin pb);
uint32_t formatSize(Format format);


struct FramebufferDesc {
	FramebufferDesc()
	{
	}

	~FramebufferDesc() { }

	FramebufferDesc(const FramebufferDesc &)            = default;
	FramebufferDesc(FramebufferDesc &&) noexcept            = default;

	FramebufferDesc &operator=(const FramebufferDesc &) = default;
	FramebufferDesc &operator=(FramebufferDesc &&) noexcept = default;


	FramebufferDesc &renderPass(RenderPassHandle rp) {
		renderPass_ = rp;
		return *this;
	}

	FramebufferDesc &depthStencil(RenderTargetHandle ds) {
		depthStencil_ = ds;
		return *this;
	}

	FramebufferDesc &color(unsigned int index, RenderTargetHandle c) {
		assert(index < MAX_COLOR_RENDERTARGETS);
		colors_[index] = c;
		return *this;
	}

	FramebufferDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}


private:

	RenderPassHandle                                         renderPass_;
	RenderTargetHandle                                       depthStencil_;
	std::array<RenderTargetHandle, MAX_COLOR_RENDERTARGETS>  colors_;
	std::string                                              name_;

	friend struct RendererImpl;
};


class PipelineDesc {
	std::string           vertexShaderName;
	std::string           fragmentShaderName;
	RenderPassHandle      renderPass_;
	ShaderMacros          shaderMacros_;
	uint32_t              vertexAttribMask;
	unsigned int          numSamples_;
	bool                  depthWrite_;
	bool                  depthTest_;
	bool                  cullFaces_;
	bool                  scissorTest_;
	bool                  blending_;
	BlendFunc             sourceBlend_;
	BlendFunc             destinationBlend_;
	// TODO: blend equation
	// TODO: per-MRT blending

	struct VertexAttr {
		uint8_t bufBinding;
		uint8_t count;
		VtxFormat format;
		uint8_t offset;

		bool operator==(const VertexAttr &other) const;
		bool operator!=(const VertexAttr &other) const;
	};

	struct VertexBuf {
		uint32_t stride;

		bool operator==(const VertexBuf &other) const;
		bool operator!=(const VertexBuf &other) const;
	};

	std::array<VertexAttr,     MAX_VERTEX_ATTRIBS>   vertexAttribs;
	std::array<VertexBuf,      MAX_VERTEX_BUFFERS>   vertexBuffers;
	std::array<DSLayoutHandle, MAX_DESCRIPTOR_SETS>  descriptorSetLayouts;

	std::string                                      name_;


public:

	PipelineDesc &vertexShader(const std::string &name) {
		assert(!name.empty());
		vertexShaderName = name;
		return *this;
	}

	PipelineDesc &fragmentShader(const std::string &name) {
		assert(!name.empty());
		fragmentShaderName = name;
		return *this;
	}

	PipelineDesc &shaderMacros(const ShaderMacros &m) {
		shaderMacros_ = m;
		return *this;
	}

	PipelineDesc &renderPass(RenderPassHandle h) {
		renderPass_ = h;
		return *this;
	}

	PipelineDesc &vertexAttrib(uint32_t attrib, uint8_t bufBinding, uint8_t count, VtxFormat format, uint8_t offset) {
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

	PipelineDesc &descriptorSetLayout(unsigned int index, DSLayoutHandle handle) {
		assert(index < MAX_DESCRIPTOR_SETS);
		descriptorSetLayouts[index] = handle;
		return *this;
	}

	template <typename T> PipelineDesc &descriptorSetLayout(unsigned int index) {
		assert(index < MAX_DESCRIPTOR_SETS);
		descriptorSetLayouts[index] = T::layoutHandle;
		return *this;
	}

	PipelineDesc &blending(bool b) {
		blending_ = b;
		return *this;
	}

	PipelineDesc &sourceBlend(BlendFunc b) {
		assert(blending_);
		sourceBlend_ = b;
		return *this;
	}

	PipelineDesc &destinationBlend(BlendFunc b) {
		assert(blending_);
		destinationBlend_ = b;
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

	PipelineDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}

	PipelineDesc &numSamples(unsigned int n) {
		assert(n != 0);
		assert(isPow2(n));
		numSamples_ = n;
		return *this;
	}

	PipelineDesc()
	: vertexAttribMask(0)
	, numSamples_(1)
	, depthWrite_(false)
	, depthTest_(false)
	, cullFaces_(false)
	, scissorTest_(false)
	, blending_(false)
	, sourceBlend_(BlendFunc::One)
	, destinationBlend_(BlendFunc::Zero)
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

	PipelineDesc(const PipelineDesc &desc)                = default;
	PipelineDesc(PipelineDesc &&desc)                     = default;

	PipelineDesc &operator=(const PipelineDesc &desc)     = default;
	PipelineDesc &operator=(PipelineDesc &&desc) noexcept = default;

	bool operator==(const PipelineDesc &other) const;


	friend struct RendererImpl;
};


struct RenderPassDesc {
	RenderPassDesc()
	: depthStencilFormat_(Format::Invalid)
	, numSamples_(1)
	, clearDepthAttachment(false)
	, depthClearValue(1.0f)
	{
		for (auto &rt : colorRTs_) {
			rt.format        = Format::Invalid;
			rt.passBegin     = PassBegin::DontCare;
			rt.initialLayout = Layout::Undefined;
			rt.finalLayout   = Layout::Undefined;
			rt.clearValue    = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	~RenderPassDesc() { }

	RenderPassDesc(const RenderPassDesc &)            = default;
	RenderPassDesc(RenderPassDesc &&) noexcept            = default;

	RenderPassDesc &operator=(const RenderPassDesc &) = default;
	RenderPassDesc &operator=(RenderPassDesc &&) noexcept = default;

	RenderPassDesc &depthStencil(Format ds, PassBegin) {
		depthStencilFormat_ = ds;
		return *this;
	}

	RenderPassDesc &color(unsigned int index, Format c, PassBegin pb, Layout initial, Layout final, glm::vec4 clear = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)) {
		assert(index < MAX_COLOR_RENDERTARGETS);
		colorRTs_[index].format         = c;
		colorRTs_[index].passBegin      = pb;
		colorRTs_[index].initialLayout  = initial;
		colorRTs_[index].finalLayout    = final;
		if (pb == PassBegin::Clear) {
			colorRTs_[index].clearValue = clear;
		}
		return *this;
	}

	RenderPassDesc &clearDepth(float v) {
		clearDepthAttachment  = true;
		depthClearValue       = v;
		return *this;
	}

	RenderPassDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}

	RenderPassDesc &numSamples(unsigned int n) {
		numSamples_ = n;
		return *this;
	}


	struct RTInfo {
		Format     format;
		PassBegin  passBegin;
		Layout     initialLayout;
		Layout     finalLayout;
		glm::vec4  clearValue;
	};


	const RTInfo &color(unsigned int index) const {
		assert(index < MAX_COLOR_RENDERTARGETS);
		return colorRTs_[index];
	}


private:

	Format                                       depthStencilFormat_;
	std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorRTs_;
	unsigned int                                 numSamples_;
	std::string                                  name_;
	bool                                         clearDepthAttachment;
	float                                        depthClearValue;


	friend struct RendererImpl;
};


struct RenderTargetDesc {
	RenderTargetDesc()
	: width_(0)
	, height_(0)
	, numSamples_(1)
	, format_(Format::Invalid)
	, additionalViewFormat_(Format::Invalid)
	{
	}

	~RenderTargetDesc() { }

	RenderTargetDesc(const RenderTargetDesc &)            = default;
	RenderTargetDesc(RenderTargetDesc &&) noexcept            = default;

	RenderTargetDesc &operator=(const RenderTargetDesc &) = default;
	RenderTargetDesc &operator=(RenderTargetDesc &&) noexcept = default;


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

	RenderTargetDesc &numSamples(unsigned int s) {
		assert(s > 0);
		numSamples_ = s;
		return *this;
	}

	RenderTargetDesc &format(Format f) {
		format_ = f;
		return *this;
	}

	RenderTargetDesc &additionalViewFormat(Format f) {
		additionalViewFormat_ = f;
		return *this;
	}

	RenderTargetDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}

	unsigned int width()      const  { return width_; }
	unsigned int height()     const  { return height_; }
	unsigned int numSamples() const  { return numSamples_; }
	Format       format()     const  { return format_; }
	Format       additionalViewFormat() const  { return additionalViewFormat_; }

private:

	unsigned int   width_, height_;
	unsigned int   numSamples_;
	Format         format_;
	Format         additionalViewFormat_;
	std::string    name_;

	friend struct RendererImpl;
};


struct SamplerDesc {
	SamplerDesc()
	: min(FilterMode::Nearest)
	, mag(FilterMode::Nearest)
	, wrapMode(WrapMode::Clamp)
	{
	}

	SamplerDesc(const SamplerDesc &desc)            = default;
	SamplerDesc(SamplerDesc &&desc) noexcept            = default;

	SamplerDesc &operator=(const SamplerDesc &desc) = default;
	SamplerDesc &operator=(SamplerDesc &&desc) noexcept = default;

	~SamplerDesc() {}

	SamplerDesc &minFilter(FilterMode m) {
		min = m;
		return *this;
	}

	SamplerDesc &magFilter(FilterMode m) {
		mag = m;
		return *this;
	}

	SamplerDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}

private:

	FilterMode  min, mag;
	WrapMode    wrapMode;
	std::string name_;

	friend struct RendererImpl;
};


struct SwapchainDesc {
	unsigned int  width, height;
	unsigned int  numFrames;
	VSync         vsync;
	bool          fullscreen;


	SwapchainDesc()
	: width(0)
	, height(0)
	, numFrames(3)
	, vsync(VSync::On)
	, fullscreen(false)
	{
	}
};


struct TextureDesc {
	TextureDesc()
	: width_(0)
	, height_(0)
	, numMips_(1)
	, format_(Format::Invalid)
	{
		std::fill(mipData_.begin(), mipData_.end(), MipLevel());
	}

	~TextureDesc() { }

	TextureDesc(const TextureDesc &)            = default;
	TextureDesc(TextureDesc &&) noexcept            = default;

	TextureDesc &operator=(const TextureDesc &) = default;
	TextureDesc &operator=(TextureDesc &&) noexcept = default;


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

	TextureDesc &mipLevelData(unsigned int level, const void *data, unsigned int size) {
		assert(level < numMips_);
		mipData_[level].data = data;
		mipData_[level].size = size;
		return *this;
	}

	TextureDesc &name(const std::string &str) {
		name_ = str;
		return *this;
	}


private:

	struct MipLevel {
		const void   *data;
		unsigned int  size;

		MipLevel()
		: data(nullptr)
		, size(0)
		{
		}
	};

	unsigned int                                 width_, height_;
	unsigned int                                 numMips_;
	Format                                       format_;
	std::array<MipLevel, MAX_TEXTURE_MIPLEVELS>  mipData_;
	std::string                                  name_;

	friend struct RendererImpl;
};


struct RendererDesc {
	bool           debug;
	bool           robustness;
	bool           tracing;
	bool           skipShaderCache;
	bool           optimizeShaders;
	bool           validateShaders;
	bool           transferQueue;
	unsigned int   ephemeralRingBufSize;
	SwapchainDesc  swapchain;


	RendererDesc()
	: debug(false)
	, robustness(false)
	, tracing(false)
	, skipShaderCache(false)
	, optimizeShaders(true)
	, validateShaders(false)
	, transferQueue(true)
	, ephemeralRingBufSize(1 * 1048576)
	{
	}
};


struct RendererFeatures {
	uint32_t  maxMSAASamples;
	bool      sRGBFramebuffer;
	bool      SSBOSupported;


	RendererFeatures()
	: maxMSAASamples(1)
	, sRGBFramebuffer(false)
	, SSBOSupported(false)
	{
	}
};


struct RendererImpl;


class Renderer {
	RendererImpl *impl;

	explicit Renderer(RendererImpl *impl_)
	: impl(impl_)
	{
		assert(impl != nullptr);
	}


public:

	static Renderer createRenderer(const RendererDesc &desc);

	Renderer(const Renderer &)            = delete;
	Renderer &operator=(const Renderer &) = delete;

	Renderer &operator=(Renderer &&other)  noexcept
	{
		if (this == &other) {
			return *this;
		}

		assert(!impl);

		impl       = other.impl;
		other.impl = nullptr;

		return *this;
	}

	Renderer(Renderer &&other) noexcept
	: impl(other.impl)
	{
		other.impl = nullptr;
	}

	Renderer()
	: impl(nullptr)
	{
	}

	~Renderer();




	bool isRenderTargetFormatSupported(Format format) const;
	unsigned int getCurrentRefreshRate() const;
	unsigned int getMaxRefreshRate() const;

	const RendererFeatures &getFeatures() const;

	// TODO: add buffer usage flags
	BufferHandle          createBuffer(BufferType type, uint32_t size, const void *contents);
	BufferHandle          createEphemeralBuffer(BufferType type, uint32_t size, const void *contents);
	FramebufferHandle     createFramebuffer(const FramebufferDesc &desc);
	PipelineHandle        createPipeline(const PipelineDesc &desc);
	RenderPassHandle      createRenderPass(const RenderPassDesc &desc);
	RenderTargetHandle    createRenderTarget(const RenderTargetDesc &desc);
	SamplerHandle         createSampler(const SamplerDesc &desc);
	TextureHandle         createTexture(const TextureDesc &desc);
	// TODO: non-ephemeral descriptor set

	DSLayoutHandle createDescriptorSetLayout(const DescriptorLayout *layout);
	template <typename T> void registerDescriptorSetLayout() {
		T::layoutHandle = createDescriptorSetLayout(T::layout);
	}

	// gets the textures of a rendertarget to be used for sampling
	// might be ephemeral, don't store
	TextureHandle        getRenderTargetView(RenderTargetHandle handle, Format f);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deletePipeline(PipelineHandle handle);
	void deleteRenderPass(RenderPassHandle fbo);
	void deleteRenderTarget(RenderTargetHandle &fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);


	void setSwapchainDesc(const SwapchainDesc &desc);
	bool isSwapchainDirty() const;
	glm::uvec2 getDrawableSize() const;
	MemoryStats getMemStats() const;

	bool waitForDeviceIdle() WARN_UNUSED_RESULT;

	// rendering
	bool beginFrame();
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle);
	void endRenderPass();

	void layoutTransition(RenderTargetHandle image, Layout src, Layout dest);

	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void bindPipeline(PipelineHandle pipeline);
	void bindDescriptorSet(unsigned int index, const DSLayoutHandle layout, const void *data);
	template <typename T> void bindDescriptorSet(unsigned int index, const T &data) {
		bindDescriptorSet(index, T::layoutHandle, &data);
	}

	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	void blit(RenderTargetHandle source, RenderTargetHandle target);
	void resolveMSAA(RenderTargetHandle source, RenderTargetHandle target);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int minIndex, unsigned int maxIndex);
};


}  // namespace renderer


#endif  // RENDERER_H
