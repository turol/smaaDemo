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


#ifndef NULLRENDERER_H
#define NULLRENDERER_H


namespace renderer {


struct Buffer {
	bool          ringBufferAlloc;
	unsigned int  beginOffs;
	unsigned int  size;
	// TODO: usage flags for debugging


	Buffer()
	: ringBufferAlloc(false)
	, beginOffs(0)
	, size(0)
	{
	}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other)
	: ringBufferAlloc(other.ringBufferAlloc)
	, beginOffs(other.beginOffs)
	, size(other.size)
	{
		other.ringBufferAlloc = false;
		other.beginOffs       = 0;
		other.size            = 0;
	}

	Buffer &operator=(Buffer &&other) {
		if (this == &other) {
			return *this;
		}

		ringBufferAlloc       = other.ringBufferAlloc;
		beginOffs             = other.beginOffs;
		size                  = other.size;

		other.ringBufferAlloc = false;
		other.beginOffs       = 0;
		other.size            = 0;

		return *this;
	}

	~Buffer() {
	}
};


struct DescriptorSetLayout {
	std::vector<DescriptorLayout> layout;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other)
	: layout(std::move(other.layout))
	{
		assert(other.layout.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) {
		if (this == &other) {
			return *this;
		}

		assert(layout.empty());
		layout = std::move(other.layout);
		assert(other.layout.empty());

		return *this;
	}

	~DescriptorSetLayout() {}
};


struct FragmentShader {
	std::string      name;


	FragmentShader()
	{
	}

	FragmentShader(const FragmentShader &)            = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&other)
	: name(std::move(other.name))
	{
		assert(other.name.empty());
	}

	FragmentShader &operator=(FragmentShader &&other) {
		if (this == &other) {
			return *this;
		}

		name            = std::move(other.name);

		assert(other.name.empty());

		return *this;
	}

	~FragmentShader() {
	}
};


struct Framebuffer {
	RenderPassHandle  renderPass;


	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other)
	: renderPass(other.renderPass)
	{
		other.renderPass = RenderPassHandle();
	}

	Framebuffer &operator=(Framebuffer &&other) {
		if (this == &other) {
			return *this;
		}

		assert(!renderPass);

		renderPass       = other.renderPass;

		other.renderPass = RenderPassHandle();

		return *this;
	}

	Framebuffer() {}

	~Framebuffer() {
	}
};


struct Pipeline {
	PipelineDesc  desc;


	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&other)
	: desc(other.desc)
	{
		other.desc = PipelineDesc();
	}

	Pipeline &operator=(Pipeline &&other) {
		if (this == &other) {
			return *this;
		}

		desc       = other.desc;

		other.desc = PipelineDesc();

		return *this;
	}

	Pipeline() {}

	~Pipeline() {}
};


struct RenderPass {
	RenderPassDesc  desc;


	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other)
	: desc(other.desc)
	{
		other.desc = RenderPassDesc();
	}

	RenderPass &operator=(RenderPass &&other) {
		if (this == &other) {
			return *this;
		}

		desc       = other.desc;

		other.desc = RenderPassDesc();

		return *this;
	}

	RenderPass() {}

	~RenderPass() {}
};


struct RenderTarget {
	RenderTargetDesc  desc;


	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other)
	: desc(other.desc)
	{
		other.desc = RenderTargetDesc();
	}

	RenderTarget &operator=(RenderTarget &&other) {
		if (this == &other) {
			return *this;
		}

		desc       = other.desc;

		other.desc = RenderTargetDesc();

		return *this;
	}

	RenderTarget() {}

	~RenderTarget() {}
};


struct Sampler {
	SamplerDesc desc;


	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other)
	: desc(other.desc)
	{
		other.desc = SamplerDesc();
	}

	Sampler &operator=(Sampler &&other) {
		if (this == &other) {
			return *this;
		}

		desc       = other.desc;

		other.desc = SamplerDesc();

		return *this;
	}

	Sampler() {}

	~Sampler() {}
};


struct Texture {
	TextureDesc desc;


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other)
	: desc(other.desc)
	{
		other.desc = TextureDesc();
	}

	Texture &operator=(Texture &&other)
	{
		if (this == &other) {
			return *this;
		}

		desc = other.desc;

		other.desc = TextureDesc();

		return *this;
	}

	Texture() {}

	~Texture() {}
};


struct VertexShader {
	std::string name;


	VertexShader()
	{
	}


	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&other)
	: name(std::move(other.name))
	{
		assert(other.name.empty());
	}

	VertexShader &operator=(VertexShader &&other) {
		if (this == &other) {
			return *this;
		}

		name            = std::move(other.name);

		assert(other.name.empty());

		return *this;
	}

	~VertexShader() {
	}
};


struct Frame {
	bool                      outstanding;
	uint32_t                  lastFrameNum;
	unsigned int              usedRingBufPtr;
	std::vector<BufferHandle> ephemeralBuffers;


	Frame()
	: outstanding(false)
	, lastFrameNum(0)
	, usedRingBufPtr(0)
	{}

	~Frame() {
		assert(ephemeralBuffers.empty());
		assert(!outstanding);
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other)
	: outstanding(other.outstanding)
	, lastFrameNum(other.lastFrameNum)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	{
		other.outstanding      = false;
		other.lastFrameNum     = 0;
		other.usedRingBufPtr   = 0;
	}

	Frame &operator=(Frame &&other) {
		assert(ephemeralBuffers.empty());
		ephemeralBuffers = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());

		outstanding = other.outstanding;
		other.outstanding = false;

		lastFrameNum = other.lastFrameNum;
		other.lastFrameNum = 0;

		usedRingBufPtr       = other.usedRingBufPtr;
		other.usedRingBufPtr = 0;

		return *this;
	}
};


struct RendererImpl : public RendererBase {
	std::vector<char> ringBuffer;

	std::vector<Frame>                       frames;

	ResourceContainer<Buffer>              buffers;
	ResourceContainer<DescriptorSetLayout>  dsLayouts;
	ResourceContainer<FragmentShader>        fragmentShaders;
	ResourceContainer<Framebuffer>         framebuffers;
	ResourceContainer<Pipeline>            pipelines;
	ResourceContainer<RenderPass>          renderpasses;
	ResourceContainer<RenderTarget>        rendertargets;
	ResourceContainer<Sampler>             samplers;
	ResourceContainer<Texture>             textures;
	ResourceContainer<VertexShader>          vertexShaders;

	PipelineDesc  currentPipeline;


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
	BufferHandle         createBuffer(BufferType type, uint32_t size, const void *contents);
	BufferHandle         createEphemeralBuffer(BufferType type, uint32_t size, const void *contents);
	SamplerHandle        createSampler(const SamplerDesc &desc);
	TextureHandle        createTexture(const TextureDesc &desc);

	DSLayoutHandle       createDescriptorSetLayout(const DescriptorLayout *layout);

	TextureHandle        getRenderTargetTexture(RenderTargetHandle handle);
	TextureHandle        getRenderTargetView(RenderTargetHandle handle, Format f);

	void deleteBuffer(BufferHandle handle);
	void deleteFramebuffer(FramebufferHandle fbo);
	void deletePipeline(PipelineHandle handle);
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

	void blit(RenderTargetHandle source, RenderTargetHandle target);
	void resolveMSAA(RenderTargetHandle source, RenderTargetHandle target);
	void resolveMSAA(FramebufferHandle source, FramebufferHandle target, unsigned int n);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int minIndex, unsigned int maxIndex);
};


} // namespace renderer


#endif  // NULLRENDERER_H
