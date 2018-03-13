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


#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H


#include <GL/glew.h>

// TODO: use std::variant if the compiler has C++17
#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>


namespace renderer {


struct DSIndex {
	uint8_t set;
	uint8_t binding;


	bool operator==(const DSIndex &other) const {
		return (set == other.set) && (binding == other.binding);
	}

	bool operator!=(const DSIndex &other) const {
		return (set != other.set) || (binding != other.binding);
	}
};


}  // namespace renderer


namespace std {

	template <> struct hash<renderer::DSIndex> {
		size_t operator()(const renderer::DSIndex &ds) const {
			uint16_t val = (uint16_t(ds.set) << 8) | ds.binding;
			return hash<uint16_t>()(val);
		}
	};

}  // namespace std


namespace renderer {


struct ShaderResources {
	std::vector<DSIndex>        ubos;
	std::vector<DSIndex>        ssbos;
	std::vector<DSIndex>        textures;
	std::vector<DSIndex>        samplers;


	ShaderResources() {}
	~ShaderResources() {}

	ShaderResources(const ShaderResources &)            = default;
	ShaderResources(ShaderResources &&)                 = default;

	ShaderResources &operator=(const ShaderResources &) = default;
	ShaderResources &operator=(ShaderResources &&)      = default;

};


struct Buffer {
	bool           ringBufferAlloc;
	uint32_t       size;
	uint32_t       offset;
	GLuint         buffer;
	// TODO: access type bits (for debugging)


	Buffer()
	: ringBufferAlloc(false)
	, size(0)
	, offset(0)
	, buffer(0)
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other)
	: ringBufferAlloc(other.ringBufferAlloc)
	, size(other.size)
	, offset(other.offset)
	, buffer(other.buffer)
	{
		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = 0;
	}

	Buffer &operator=(Buffer &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!buffer);

		ringBufferAlloc       = other.ringBufferAlloc;
		size                  = other.size;
		offset                = other.offset;
		buffer                = other.buffer;

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = 0;

		return *this;
	}

	~Buffer() {
		assert(!ringBufferAlloc);
		assert(size   == 0);
		assert(offset == 0);
		assert(!buffer);
	}
};


struct DescriptorSetLayout {
	std::vector<DescriptorLayout>  descriptors;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other)
	: descriptors(std::move(other.descriptors))
	{
		assert(descriptors.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) {
		if (this == &other) {
			return *this;
		}

		descriptors  = std::move(other.descriptors);
		assert(descriptors.empty());

		return *this;
	}

	~DescriptorSetLayout() {
	}
};


struct FragmentShader {
	std::string            name;
	std::vector<uint32_t>  spirv;
	ShaderMacros           macros;


	FragmentShader()
	{
	}

	FragmentShader(const FragmentShader &)            = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&other)
	: name(std::move(other.name))
	, spirv(std::move(other.spirv))
	, macros(std::move(other.macros))
	{
		assert(other.name.empty());
		assert(other.spirv.empty());
	}

	FragmentShader &operator=(FragmentShader &&other) {
		if (this == &other) {
			return *this;
		}

		name            = std::move(other.name);
		spirv           = std::move(other.spirv);
		macros          = std::move(other.macros);

		assert(other.name.empty());
		assert(other.spirv.empty());

		return *this;
	}

	~FragmentShader() {
	}
};


struct Framebuffer {
	unsigned int                                             width, height;
	unsigned int                                             numSamples;
	bool                                                     sRGB;
	GLuint                                                   fbo;
	RenderTargetHandle                                       depthStencil;
	std::array<RenderTargetHandle, MAX_COLOR_RENDERTARGETS>  colors;
	RenderPassHandle                                         renderPass;


	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other)
	: width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, sRGB(other.sRGB)
	, fbo(other.fbo)
	, depthStencil(other.depthStencil)
	, renderPass(other.renderPass)
	{
		other.width        = 0;
		other.height       = 0;
		other.numSamples   = 0;
		other.sRGB         = false;
		other.fbo          = 0;
		// TODO: use std::move
		assert(!other.colors[1]);
		this->colors[0]    = other.colors[0];
		other.colors[0]    = RenderTargetHandle();
		other.renderPass   = RenderPassHandle();
		other.depthStencil = RenderTargetHandle();
	}

	Framebuffer &operator=(Framebuffer &&other) = delete;

	Framebuffer()
	: width(0)
	, height(0)
	, numSamples(0)
	, sRGB(false)
	, fbo(0)
	{
	}

	~Framebuffer() {
		assert(fbo == 0);
		assert(numSamples == 0);
	}
};


struct Pipeline {
	PipelineDesc    desc;
	GLuint          shader;
	ShaderResources  resources;


	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&other)
	: desc(other.desc)
	, shader(other.shader)
	, resources(other.resources)
	{
		other.desc      = PipelineDesc();
		other.shader    = 0;
		other.resources = ShaderResources();
	}

	Pipeline &operator=(Pipeline &&other) {
		if (this == &other) {
			return *this;
		}

		desc            = other.desc;
		shader          = other.shader;
		resources       = other.resources;

		other.desc      = PipelineDesc();
		other.shader    = 0;
		other.resources = ShaderResources();

		return *this;
	}

	Pipeline()
	: shader(0)
	{
	}


	~Pipeline() {
		assert(shader == 0);
	}
};


struct RenderPass {
	RenderPassDesc  desc;
	glm::vec4       colorClearValue;
	float           depthClearValue;
	GLbitfield      clearMask;
	unsigned int    numSamples;


	RenderPass()
	: depthClearValue(1.0f)
	, clearMask(0)
	, numSamples(0)
	{}

	RenderPass(const RenderPass &) = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other)
	: desc(other.desc)
	, colorClearValue(other.colorClearValue)
	, depthClearValue(other.depthClearValue)
	, clearMask(other.clearMask)
	, numSamples(other.numSamples)
	{
	}

	RenderPass &operator=(RenderPass &&other) {
		if (this == &other) {
			return *this;
		}

		desc            = other.desc;
		colorClearValue = other.colorClearValue;
		depthClearValue = other.depthClearValue;
		clearMask       = other.clearMask;
		numSamples      = other.numSamples;

		return *this;
	}

	~RenderPass() {
	}
};


struct RenderTarget {
	unsigned int   width, height;
	unsigned int   numSamples;
	Layout         currentLayout;
	TextureHandle  texture;
	TextureHandle  additionalView;
	GLuint         readFBO;
	Format         format;


	RenderTarget()
	: width(0)
	, height(0)
	, numSamples(0)
	, currentLayout(Layout::Undefined)
	, readFBO(0)
	, format(Format::Invalid)
	{
	}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other)
	: width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, currentLayout(other.currentLayout)
	, texture(other.texture)   // TODO: use std::move
	, additionalView(other.additionalView)
	, readFBO(other.readFBO)
	, format(other.format)
	{
		other.width         = 0;
		other.height        = 0;
		other.numSamples    = 0;
		other.currentLayout = Layout::Undefined;
		other.texture       = TextureHandle();
		other.additionalView = TextureHandle();
		other.readFBO       = 0;
		other.format        = Format::Invalid;
	}

	RenderTarget &operator=(RenderTarget &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!readFBO);
		assert(!texture);

		width         = other.width;
		height        = other.height;
		numSamples     = other.numSamples;
		currentLayout = other.currentLayout;
		texture       = other.texture;
		additionalView = other.additionalView;
		readFBO       = other.readFBO;
		format        = other.format;

		other.width         = 0;
		other.height        = 0;
		other.numSamples     = 0;
		other.currentLayout = Layout::Undefined;
		other.texture       = TextureHandle();
		other.additionalView = TextureHandle();
		other.readFBO       = 0;
		other.format        = Format::Invalid;

		return *this;
	};

	~RenderTarget() {
		assert(numSamples == 0);
		assert(readFBO  == 0);
		assert(!texture);
	}

};


struct Sampler {
	GLuint sampler;


	Sampler()
	: sampler(0)
	{
	}

	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other)
	: sampler(other.sampler)
	{
		other.sampler = 0;
	}

	Sampler &operator=(Sampler &&other) {
		if (this == &other) {
			return *this;
		}

		sampler       = other.sampler;

		other.sampler = 0;

		return *this;
	}

	~Sampler()
	{
		assert(!sampler);
	}
};


struct Texture {
	unsigned int  width, height;
	bool          renderTarget;
	GLuint        tex;
	GLenum        target;


	Texture()
	: width(0)
	, height(0)
	, renderTarget(false)
	, tex(0)
	, target(GL_NONE)
	{
	}


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other)
	: width(other.width)
	, height(other.height)
	, renderTarget(other.renderTarget)
	, tex(other.tex)
	, target(other.target)
	{
		other.tex          = 0;
		other.width        = 0;
		other.height       = 0;
		other.renderTarget = false;
		other.target       = GL_NONE;
	}

	Texture &operator=(Texture &&other) {
		if (this == &other) {
			return *this;
		}

		width              = other.width;
		height             = other.height;
		renderTarget       = other.renderTarget;
		tex                = other.tex;
		target             = other.target;

		other.width        = 0;
		other.height       = 0;
		other.renderTarget = false;
		other.tex          = 0;
		other.target       = GL_NONE;

		return *this;
	}

	~Texture() {
		// it should have been deleted by Renderer before destroying this
		assert(tex == 0);
		assert(!renderTarget);
		assert(target == GL_NONE);
	}
};


struct VertexShader {
	std::string            name;
	std::vector<uint32_t>  spirv;
	ShaderMacros           macros;


	VertexShader()
	{
	}


	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&other)
	: name(std::move(other.name))
	, spirv(std::move(other.spirv))
	, macros(other.macros)
	{
		assert(other.name.empty());
		assert(other.spirv.empty());
	}

	VertexShader &operator=(VertexShader &&other) {
		if (this == &other) {
			return *this;
		}

		spirv           = std::move(other.spirv);
		name            = std::move(other.name);
		macros          = std::move(other.macros);

		assert(other.spirv.empty());
		assert(other.name.empty());

		return *this;
	}

	~VertexShader() {
	}
};


typedef boost::variant<BufferHandle, CSampler, SamplerHandle, TextureHandle> Descriptor;


struct Frame {
	bool                      outstanding;
	uint32_t                  lastFrameNum;
	unsigned int              usedRingBufPtr;
	std::vector<BufferHandle> ephemeralBuffers;
	GLsync                    fence;


	Frame()
	: outstanding(false)
	, lastFrameNum(0)
	, usedRingBufPtr(0)
	, fence(nullptr)
	{}

	~Frame() {
		assert(!outstanding);
		assert(!fence);
		assert(ephemeralBuffers.empty());
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other)
	: outstanding(other.outstanding)
	, lastFrameNum(other.lastFrameNum)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	, fence(other.fence)
	{
		other.outstanding     = false;
		other.fence           = nullptr;
		other.usedRingBufPtr  = 0;
		assert(other.ephemeralBuffers.empty());
	}

	Frame &operator=(Frame &&other) {
		assert(!outstanding);
		outstanding            = other.outstanding;
		other.outstanding      = false;

		lastFrameNum           = other.lastFrameNum;

		usedRingBufPtr         = other.usedRingBufPtr;
		other.usedRingBufPtr   = 0;

		assert(!fence);
		fence                  = other.fence;
		other.fence            = nullptr;

		assert(ephemeralBuffers.empty());
		ephemeralBuffers       = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());

		return *this;
	}
};


struct RendererImpl : public RendererBase {
	SDL_Window                               *window;
	SDL_GLContext                            context;

	std::unordered_map<GLenum, int>          glValues;

	std::vector<Frame>                       frames;

	ResourceContainer<Buffer>                buffers;
	ResourceContainer<DescriptorSetLayout>   dsLayouts;
	ResourceContainer<FragmentShader>        fragmentShaders;
	ResourceContainer<Framebuffer>           framebuffers;
	ResourceContainer<Pipeline>              pipelines;
	ResourceContainer<RenderPass>            renderPasses;
	ResourceContainer<RenderTarget>          renderTargets;
	ResourceContainer<Sampler>               samplers;
	ResourceContainer<Texture>               textures;
	ResourceContainer<VertexShader>          vertexShaders;

	GLuint                                   ringBuffer;
	bool                                     persistentMapInUse;
	char                                     *persistentMapping;

	PipelineHandle                           currentPipeline;
	RenderPassHandle                         currentRenderPass;
	FramebufferHandle                        currentFramebuffer;

	bool                                     decriptorSetsDirty;
	std::unordered_map<DSIndex, Descriptor>  descriptors;

	bool                                     debug;
	bool                                     tracing;
	GLuint                                   vao;
	bool                                     idxBuf16Bit;
	unsigned int                             indexBufByteOffset;


	bool isRenderPassCompatible(const RenderPass &pass, const Framebuffer &fb);

	void rebindDescriptorSets();

	void recreateSwapchain();
	void recreateRingBuffer(unsigned int newSize);
	unsigned int ringBufferAllocate(unsigned int size, unsigned int alignPower);

	void waitForFrame(unsigned int frameIdx);
	void deleteFrameInternal(Frame &f);

	explicit RendererImpl(const RendererDesc &desc);

	~RendererImpl();


	bool isRenderTargetFormatSupported(Format format) const;

	RenderTargetHandle   createRenderTarget(const RenderTargetDesc &desc);
	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);
	FramebufferHandle    createFramebuffer(const FramebufferDesc &desc);
	RenderPassHandle     createRenderPass(const RenderPassDesc &desc);
	PipelineHandle       createPipeline(const PipelineDesc &desc);
	BufferHandle         createBuffer(uint32_t size, const void *contents);
	BufferHandle         createEphemeralBuffer(uint32_t size, const void *contents);
	SamplerHandle        createSampler(const SamplerDesc &desc);
	TextureHandle        createTexture(const TextureDesc &desc);

	DSLayoutHandle       createDescriptorSetLayout(const DescriptorLayout *layout);

	TextureHandle        getRenderTargetTexture(RenderTargetHandle handle);
	TextureHandle        getRenderTargetView(RenderTargetHandle handle, Format f);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deleteRenderPass(RenderPassHandle fbo);
	void deleteSampler(SamplerHandle handle);
	void deleteTexture(TextureHandle handle);
	void deleteRenderTarget(RenderTargetHandle &fbo);


	void setSwapchainDesc(const SwapchainDesc &desc);
	MemoryStats getMemStats() const;

	void beginFrame();
	void presentFrame(RenderTargetHandle image);

	void beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle);
	void endRenderPass();

	void layoutTransition(RenderTargetHandle image, Layout src, Layout dest);

	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void bindPipeline(PipelineHandle pipeline);
	void bindIndexBuffer(BufferHandle buffer, bool bit16);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	void bindDescriptorSet(unsigned int index, DSLayoutHandle layout, const void *data);

	void resolveMSAA(FramebufferHandle source, FramebufferHandle target);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex);
};


} // namespace renderer


#endif  // OPENGLRENDERER_H
