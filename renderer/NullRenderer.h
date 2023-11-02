/*
Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

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
	bool          ringBufferAlloc  = false;
	unsigned int  beginOffs        = 0;
	unsigned int  size             = 0;
	// TODO: usage flags for debugging


	Buffer()
	{
	}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other) noexcept
	: ringBufferAlloc(other.ringBufferAlloc)
	, beginOffs(other.beginOffs)
	, size(other.size)
	{
		other.ringBufferAlloc = false;
		other.beginOffs       = 0;
		other.size            = 0;
	}

	Buffer &operator=(Buffer &&other) noexcept {
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
	std::vector<DescriptorLayout>  layout;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other) noexcept
	: layout(std::move(other.layout))
	{
		assert(other.layout.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept {
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

	FragmentShader(FragmentShader &&other) noexcept
	: name(std::move(other.name))
	{
		assert(other.name.empty());
	}

	FragmentShader &operator=(FragmentShader &&other) noexcept {
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

	Framebuffer(Framebuffer &&other) noexcept
	: renderPass(std::move(other.renderPass))
	{
	}

	Framebuffer &operator=(Framebuffer &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!renderPass);

		renderPass       = std::move(other.renderPass);

		return *this;
	}

	Framebuffer() {}

	~Framebuffer() {
	}
};


struct GraphicsPipeline {
	GraphicsPipelineDesc  desc;


	GraphicsPipeline(const GraphicsPipeline &)            = delete;
	GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

	GraphicsPipeline(GraphicsPipeline &&other) noexcept
	: desc(other.desc)
	{
		other.desc = GraphicsPipelineDesc();
	}

	GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		desc       = other.desc;

		other.desc = GraphicsPipelineDesc();

		return *this;
	}

	GraphicsPipeline() {}

	~GraphicsPipeline() {}
};


struct RenderPass {
	RenderPassDesc  desc;


	RenderPass(const RenderPass &)            = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other) noexcept
	: desc(other.desc)
	{
		other.desc = RenderPassDesc();
	}

	RenderPass &operator=(RenderPass &&other) noexcept {
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

	RenderTarget(RenderTarget &&other) noexcept
	: desc(other.desc)
	{
		other.desc = RenderTargetDesc();
	}

	RenderTarget &operator=(RenderTarget &&other) noexcept {
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
	SamplerDesc  desc;


	Sampler(const Sampler &)            = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other) noexcept
	: desc(other.desc)
	{
		other.desc = SamplerDesc();
	}

	Sampler &operator=(Sampler &&other) noexcept {
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
	TextureDesc  desc;


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other) noexcept
	: desc(other.desc)
	{
		other.desc = TextureDesc();
	}

	Texture &operator=(Texture &&other) noexcept
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
	std::string  name;


	VertexShader()
	{
	}


	VertexShader(const VertexShader &)            = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&other) noexcept
	: name(std::move(other.name))
	{
		assert(other.name.empty());
	}

	VertexShader &operator=(VertexShader &&other) noexcept {
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


struct Frame : public FrameBase {
	bool                       outstanding     = false;
	unsigned int               usedRingBufPtr  = 0;
	std::vector<BufferHandle>  ephemeralBuffers;


	Frame()
	{}

	~Frame() {
		assert(ephemeralBuffers.empty());
		assert(!outstanding);
	}

	Frame(const Frame &)            = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&other) noexcept
	: FrameBase(std::move(other))
	, outstanding(other.outstanding)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	{
		other.outstanding     = false;
		other.lastFrameNum    = 0;
		other.usedRingBufPtr  = 0;
	}

	Frame &operator=(Frame &&other) noexcept {
		assert(ephemeralBuffers.empty());
		ephemeralBuffers = std::move(other.ephemeralBuffers);
		assert(other.ephemeralBuffers.empty());

		outstanding           = other.outstanding;
		other.outstanding     = false;

		lastFrameNum          = other.lastFrameNum;
		other.lastFrameNum    = 0;

		usedRingBufPtr        = other.usedRingBufPtr;
		other.usedRingBufPtr  = 0;

		return *this;
	}
};


struct RendererImpl : public RendererBase {
	std::vector<char>                                       ringBuffer;

	std::vector<Frame>                                      frames;

	ResourceContainer<Buffer>                               buffers;
	ResourceContainer<DescriptorSetLayout, uint32_t, true>  dsLayouts;
	ResourceContainer<FragmentShader, uint32_t, true>       fragmentShaders;
	ResourceContainer<Framebuffer>                          framebuffers;
	ResourceContainer<GraphicsPipeline>                     graphicsPipelines;
	ResourceContainer<RenderPass>                           renderpasses;
	ResourceContainer<RenderTarget>                         rendertargets;
	ResourceContainer<Sampler>                              samplers;
	ResourceContainer<Texture>                              textures;
	ResourceContainer<VertexShader, uint32_t, true>         vertexShaders;


	void recreateRingBuffer(unsigned int newSize);
	unsigned int ringBufferAllocate(unsigned int size, unsigned int alignPower);

	void waitForFrame(unsigned int frameIdx);
	void deleteFrameInternal(Frame &f);

	explicit RendererImpl(const RendererDesc &desc);

	~RendererImpl();


	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);

	void waitForDeviceIdle();
};


} // namespace renderer


#endif  // NULLRENDERER_H
