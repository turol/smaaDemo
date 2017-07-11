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


struct Pipeline {
	PipelineDesc  desc;


	Pipeline(const Pipeline &)            = default;
	Pipeline &operator=(const Pipeline &) = default;

	Pipeline(Pipeline &&)            = default;
	Pipeline &operator=(Pipeline &&) = default;

	Pipeline() {}

	~Pipeline() {}
};


struct RendererBase {
	std::vector<char> ringBuffer;
	ResourceContainer<Buffer>              buffers;
	ResourceContainer<Pipeline>            pipelines;

	PipelineDesc  currentPipeline;

	unsigned int numBuffers;
	unsigned int numSamplers;
	unsigned int numTextures;

	std::vector<BufferHandle> ephemeralBuffers;


	RendererBase();

	~RendererBase();
};


#endif  // NULLRENDERER_H
