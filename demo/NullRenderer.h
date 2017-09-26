#ifndef NULLRENDERER_H
#define NULLRENDERER_H


struct Buffer {
	bool          ringBufferAlloc;
	unsigned int  beginOffs;
	unsigned int  size;
	// TODO: usage flags for debugging


	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&)            = default;
	Buffer &operator=(Buffer &&) = default;

	Buffer();

	~Buffer();
};


struct Framebuffer {
	RenderPassHandle  renderPass;


	Framebuffer(const Framebuffer &)            = default;
	Framebuffer &operator=(const Framebuffer &) = default;

	Framebuffer(Framebuffer &&)                 = default;
	Framebuffer &operator=(Framebuffer &&)      = default;

	Framebuffer() {}

	~Framebuffer() {}
};


struct Pipeline {
	PipelineDesc  desc;


	Pipeline(const Pipeline &)            = default;
	Pipeline &operator=(const Pipeline &) = default;

	Pipeline(Pipeline &&)            = default;
	Pipeline &operator=(Pipeline &&) = default;

	Pipeline() {}

	~Pipeline() {}
};


struct Sampler {
	SamplerDesc desc;


	Sampler(const Sampler &)            = default;
	Sampler &operator=(const Sampler &) = default;

	Sampler(Sampler &&)                 = default;
	Sampler &operator=(Sampler &&)      = default;

	Sampler() {}

	~Sampler() {}
};


struct RendererBase {
	std::vector<char> ringBuffer;
	ResourceContainer<Buffer>              buffers;
	ResourceContainer<Framebuffer>         framebuffers;
	ResourceContainer<Pipeline>            pipelines;
	ResourceContainer<Sampler>             samplers;

	PipelineDesc  currentPipeline;

	unsigned int numBuffers;
	unsigned int numTextures;

	std::vector<BufferHandle> ephemeralBuffers;


	RendererBase();

	~RendererBase();
};


#endif  // NULLRENDERER_H
