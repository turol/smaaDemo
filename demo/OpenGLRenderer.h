#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H


#include <GL/glew.h>


struct ShaderResource {
	unsigned int    set;
	unsigned int    binding;
	DescriptorType  type;
};


struct DescriptorSetLayout {
	std::vector<DescriptorLayout> layout;
};


struct Pipeline {
	PipelineDesc  desc;
	GLuint        shader;


	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&)            = default;
	Pipeline &operator=(Pipeline &&) = default;

	Pipeline();

	~Pipeline();
};


struct Buffer {
	GLuint        buffer;
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


struct VertexShader {
	GLuint shader;
	std::string name;
	std::vector<ShaderResource> resources;


	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&)            = default;
	VertexShader &operator=(VertexShader &&) = default;

	VertexShader();

	~VertexShader();
};


struct FragmentShader {
	GLuint shader;
	std::string name;
	std::vector<ShaderResource> resources;


	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&)            = default;
	FragmentShader &operator=(FragmentShader &&) = default;


	FragmentShader();

	~FragmentShader();
};


struct RenderTarget {
	GLuint readFBO;
	unsigned int width, height;
	Layout               currentLayout;
	TextureHandle        texture;


	RenderTarget()
	: readFBO(0)
	, width(0)
	, height(0)
	, currentLayout(InvalidLayout)
	{
	}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other)
	: readFBO(other.readFBO)
	, width(other.width)
	, height(other.height)
	, currentLayout(other.currentLayout)
	, texture(other.texture)   // TODO: use std::move
	{
		other.readFBO = 0;
		other.width  = 0;
		other.height = 0;
		other.currentLayout = Layout::InvalidLayout;
		other.texture       = TextureHandle();
	}

	RenderTarget &operator=(RenderTarget &&other) {
		if (this == &other) {
			return *this;
		}

		std::swap(readFBO, other.readFBO);
		std::swap(width,  other.width);
		std::swap(height, other.height);
		std::swap(currentLayout, other.currentLayout);
		std::swap(texture,       other.texture);

		return *this;
	};

	~RenderTarget();
};


struct RenderPass {
	RenderPassDesc  desc;
	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;


	RenderPass(const RenderPass &) = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other)
	: fbo(other.fbo)
	, colorTex(other.colorTex)
	, depthTex(other.depthTex)
	, width(other.width)
	, height(other.height)
	{
		other.fbo      = 0;
		other.colorTex = 0;
		other.depthTex = 0;
		other.width    = 0;
		other.height   = 0;
	}

	RenderPass &operator=(RenderPass &&other) = delete;

	RenderPass()
	: fbo(0)
	, colorTex(0)
	, depthTex(0)
	, width(0)
	, height(0)
	{
	}


	~RenderPass();
};


struct Sampler {
	GLuint sampler;


	Sampler(const Sampler &) = delete;
	Sampler &operator=(const Sampler &) = delete;

	Sampler(Sampler &&other)
	: sampler(other.sampler)
	{
		other.sampler = 0;
	}

	Sampler &operator=(Sampler &&other) {
		std::swap(sampler, other.sampler);

		return *this;
	}

	Sampler()
	: sampler(0)
	{
	}


	~Sampler()
	{
		assert(sampler == 0);
	}
};


struct Texture {
	// TODO: need target for anything?
	GLuint tex;
	unsigned int width, height;
	bool         renderTarget;


	Texture(const Texture &) = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other)
	: tex(other.tex)
	, width(other.width)
	, height(other.height)
	, renderTarget(other.renderTarget)
	{
		other.tex      = 0;
		other.width    = 0;
		other.height   = 0;
		other.renderTarget = false;
	}

	Texture &operator=(Texture &&other) {
		std::swap(tex,    other.tex);
		std::swap(width,  other.width);
		std::swap(height, other.height);
		std::swap(renderTarget, other.renderTarget);

		return *this;
	}

	Texture()
	: tex(0)
	, width(0)
	, height(0)
	, renderTarget(false)
	{
	}


	~Texture();
};


struct RendererBase {
	GLuint        ringBuffer;
	bool          persistentMapInUse;
	char         *persistentMapping;

	PipelineDesc  currentPipeline;
	RenderPassHandle  currentRenderPass;

	SDL_Window *window;
	SDL_GLContext context;

	bool           debug;
	GLuint vao;
	bool idxBuf16Bit;
	unsigned int  indexBufByteOffset;

	ResourceContainer<Buffer>               buffers;
	ResourceContainer<DescriptorSetLayout>  dsLayouts;
	ResourceContainer<FragmentShader>       fragmentShaders;
	ResourceContainer<Pipeline>             pipelines;
	ResourceContainer<RenderPass>           renderPasses;
	ResourceContainer<RenderTarget>         renderTargets;
	ResourceContainer<Sampler>              samplers;
	ResourceContainer<Texture>              textures;
	ResourceContainer<VertexShader>         vertexShaders;

	std::vector<BufferHandle> ephemeralBuffers;


	RendererBase();

	~RendererBase();
};


#endif  // OPENGLRENDERER_H
