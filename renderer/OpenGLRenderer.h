/*
Copyright (c) 2015-2022 Alternative Games Ltd / Turo Lamminen

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

#include <variant>


namespace spirv_cross {


class CompilerGLSL;


}  // namespace spirv_cross


namespace renderer {


struct ShaderResources {
	std::vector<DSIndex>        ubos;
	std::vector<DSIndex>        ssbos;
	std::vector<DSIndex>        textures;
	std::vector<DSIndex>        samplers;

	// TODO: debug only, hide when NDEBUG
	std::vector<uint32_t>       uboSizes;


	ShaderResources() {}
	~ShaderResources() {}

	ShaderResources(const ShaderResources &)                = default;
	ShaderResources(ShaderResources &&) noexcept            = default;

	ShaderResources &operator=(const ShaderResources &)     = default;
	ShaderResources &operator=(ShaderResources &&) noexcept = default;

};


struct Buffer {
	bool           ringBufferAlloc;
	uint32_t       size;
	uint32_t       offset;
	GLuint         buffer;
	BufferType     type;


	Buffer()
	: ringBufferAlloc(false)
	, size(0)
	, offset(0)
	, buffer(0)
	, type(BufferType::Invalid)
	{}

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&other) noexcept
	: ringBufferAlloc(other.ringBufferAlloc)
	, size(other.size)
	, offset(other.offset)
	, buffer(other.buffer)
	, type(other.type)
	{
		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = 0;
		other.type            = BufferType::Invalid;
	}

	Buffer &operator=(Buffer &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!buffer);

		ringBufferAlloc       = other.ringBufferAlloc;
		size                  = other.size;
		offset                = other.offset;
		buffer                = other.buffer;
		type                  = other.type;

		other.ringBufferAlloc = false;
		other.size            = 0;
		other.offset          = 0;
		other.buffer          = 0;
		other.type            = BufferType::Invalid;

		return *this;
	}

	~Buffer() {
		assert(!ringBufferAlloc);
		assert(size   == 0);
		assert(offset == 0);
		assert(!buffer);
		assert(type == BufferType::Invalid);
	}
};


struct DescriptorSetLayout {
	std::vector<DescriptorLayout>  descriptors;


	DescriptorSetLayout() {}

	DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other) noexcept
	: descriptors(std::move(other.descriptors))
	{
		assert(other.descriptors.empty());
	}

	DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		descriptors  = std::move(other.descriptors);
		assert(other.descriptors.empty());

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

	FragmentShader(FragmentShader &&other) noexcept
	: name(std::move(other.name))
	, spirv(std::move(other.spirv))
	, macros(std::move(other.macros))
	{
		assert(other.name.empty());
		assert(other.spirv.empty());
	}

	FragmentShader &operator=(FragmentShader &&other) noexcept {
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
	GLuint                                                   fbo;
	unsigned int                                             width, height;
	unsigned int                                             numSamples;
	bool                                                     sRGB;
	RenderPassHandle                                         renderPass;
	FramebufferDesc                                          desc;
	std::array<Format, MAX_COLOR_RENDERTARGETS>              colorFormats;
	Format                                                   depthStencilFormat;


	Framebuffer(const Framebuffer &)            = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other) noexcept
	: fbo(other.fbo)
	, width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, sRGB(other.sRGB)
	, renderPass(std::move(other.renderPass))
	, desc(other.desc)
	, depthStencilFormat(other.depthStencilFormat)
	{
		other.fbo          = 0;
		other.width        = 0;
		other.height       = 0;
		other.numSamples   = 0;
		other.sRGB         = false;
		other.desc         = FramebufferDesc();
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorFormats[i]       = other.colorFormats[i];
			other.colorFormats[i] = Format::Invalid;
		}
		other.depthStencilFormat = Format::Invalid;
	}

	Framebuffer &operator=(Framebuffer &&other) noexcept = delete;

	Framebuffer()
	: fbo(0)
	, width(0)
	, height(0)
	, numSamples(0)
	, sRGB(false)
	, depthStencilFormat(Format::Invalid)
	{
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorFormats[i] = Format::Invalid;
		}
	}

	~Framebuffer() {
		assert(fbo == 0);
		assert(numSamples == 0);
		assert(depthStencilFormat == Format::Invalid);
	}
};


struct Pipeline {
	PipelineDesc     desc;
	GLuint           shader;
	GLenum           srcBlend;
	GLenum           destBlend;
	ShaderResources  resources;


	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	Pipeline(Pipeline &&other) noexcept
	: desc(other.desc)
	, shader(other.shader)
	, srcBlend(other.srcBlend)
	, destBlend(other.destBlend)
	, resources(other.resources)
	{
		other.desc      = PipelineDesc();
		other.shader    = 0;
		other.srcBlend  = GL_NONE;
		other.destBlend = GL_NONE;
		other.resources = ShaderResources();
	}

	Pipeline &operator=(Pipeline &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		desc            = other.desc;
		shader          = other.shader;
		srcBlend        = other.srcBlend;
		destBlend       = other.destBlend;
		resources       = other.resources;

		other.desc      = PipelineDesc();
		other.shader    = 0;
		other.srcBlend  = GL_NONE;
		other.destBlend = GL_NONE;
		other.resources = ShaderResources();

		return *this;
	}

	Pipeline()
	: shader(0)
	, srcBlend(GL_ONE)
	, destBlend(GL_ZERO)
	{
	}


	~Pipeline() {
		assert(shader == 0);
	}
};


struct RenderPass {
	RenderPassDesc                                  desc;
	std::array<glm::vec4, MAX_COLOR_RENDERTARGETS>  colorClearValues;
	float                                           depthClearValue;
	GLbitfield                                      clearMask;


	RenderPass()
	: depthClearValue(1.0f)
	, clearMask(0)
	{}

	RenderPass(const RenderPass &) = delete;
	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass(RenderPass &&other) noexcept
	: desc(other.desc)
	, depthClearValue(other.depthClearValue)
	, clearMask(other.clearMask)
	{
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorClearValues[i] = other.colorClearValues[i];
		}
	}

	RenderPass &operator=(RenderPass &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		desc            = other.desc;
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			colorClearValues[i] = other.colorClearValues[i];
		}
		depthClearValue = other.depthClearValue;
		clearMask       = other.clearMask;

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
	GLuint         helperFBO;
	Format         format;


	RenderTarget()
	: width(0)
	, height(0)
	, numSamples(0)
	, currentLayout(Layout::Undefined)
	, helperFBO(0)
	, format(Format::Invalid)
	{
	}

	RenderTarget(const RenderTarget &)            = delete;
	RenderTarget &operator=(const RenderTarget &) = delete;

	RenderTarget(RenderTarget &&other) noexcept
	: width(other.width)
	, height(other.height)
	, numSamples(other.numSamples)
	, currentLayout(other.currentLayout)
	, texture(std::move(other.texture))
	, additionalView(std::move(other.additionalView))
	, helperFBO(other.helperFBO)
	, format(other.format)
	{
		other.width           = 0;
		other.height          = 0;
		other.numSamples      = 0;
		other.currentLayout   = Layout::Undefined;
		other.texture.reset();
		other.additionalView.reset();
		other.helperFBO       = 0;
		other.format          = Format::Invalid;
	}

	RenderTarget &operator=(RenderTarget &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		assert(!helperFBO);
		assert(!texture);

		width                 = other.width;
		height                = other.height;
		numSamples            = other.numSamples;
		currentLayout         = other.currentLayout;
		texture               = other.texture;
		additionalView        = other.additionalView;
		helperFBO             = other.helperFBO;
		format                = other.format;

		other.width           = 0;
		other.height          = 0;
		other.numSamples      = 0;
		other.currentLayout   = Layout::Undefined;
		other.helperFBO       = 0;
		other.format          = Format::Invalid;

		return *this;
	}

	~RenderTarget() {
		assert(numSamples == 0);
		assert(helperFBO  == 0);
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

	Sampler(Sampler &&other) noexcept
	: sampler(other.sampler)
	{
		other.sampler = 0;
	}

	Sampler &operator=(Sampler &&other) noexcept {
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
	Format        format;


	Texture()
	: width(0)
	, height(0)
	, renderTarget(false)
	, tex(0)
	, target(GL_NONE)
	, format(Format::Invalid)
	{
	}


	Texture(const Texture &)            = delete;
	Texture &operator=(const Texture &) = delete;

	Texture(Texture &&other) noexcept
	: width(other.width)
	, height(other.height)
	, renderTarget(other.renderTarget)
	, tex(other.tex)
	, target(other.target)
	, format(other.format)
	{
		other.tex          = 0;
		other.width        = 0;
		other.height       = 0;
		other.renderTarget = false;
		other.target       = GL_NONE;
		other.format       = Format::Invalid;
	}

	Texture &operator=(Texture &&other) noexcept {
		if (this == &other) {
			return *this;
		}

		width              = other.width;
		height             = other.height;
		renderTarget       = other.renderTarget;
		tex                = other.tex;
		target             = other.target;
		format             = other.format;

		other.width        = 0;
		other.height       = 0;
		other.renderTarget = false;
		other.tex          = 0;
		other.target       = GL_NONE;
		other.format       = Format::Invalid;

		return *this;
	}

	~Texture() {
		// it should have been deleted by Renderer before destroying this
		assert(tex == 0);
		assert(!renderTarget);
		assert(target == GL_NONE);
		assert(format == Format::Invalid);
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

	VertexShader(VertexShader &&other) noexcept
	: name(std::move(other.name))
	, spirv(std::move(other.spirv))
	, macros(other.macros)
	{
		assert(other.name.empty());
		assert(other.spirv.empty());
	}

	VertexShader &operator=(VertexShader &&other) noexcept {
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


using Descriptor = std::variant<BufferHandle, CSampler, SamplerHandle, TextureHandle>;


struct Frame : public FrameBase {
	bool                      outstanding;
	unsigned int              usedRingBufPtr;
	std::vector<BufferHandle> ephemeralBuffers;
	GLsync                    fence;


	Frame()
	: outstanding(false)
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

	Frame(Frame &&other) noexcept
	: FrameBase(std::move(other))
	, outstanding(other.outstanding)
	, usedRingBufPtr(other.usedRingBufPtr)
	, ephemeralBuffers(std::move(other.ephemeralBuffers))
	, fence(other.fence)
	{
		other.outstanding     = false;
		other.fence           = nullptr;
		other.usedRingBufPtr  = 0;
		assert(other.ephemeralBuffers.empty());
	}

	Frame &operator=(Frame &&other) noexcept {
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
	SDL_Window                                              *window;
	SDL_GLContext                                           context;

	HashMap<GLenum, int>                                    glValues;

	std::vector<Frame>                                      frames;

	ResourceContainer<Buffer>                               buffers;
	ResourceContainer<DescriptorSetLayout, uint32_t, true>  dsLayouts;
	ResourceContainer<FragmentShader, uint32_t, true>       fragmentShaders;
	ResourceContainer<Framebuffer>                          framebuffers;
	ResourceContainer<Pipeline>                             pipelines;
	ResourceContainer<RenderPass>                           renderPasses;
	ResourceContainer<RenderTarget>                         renderTargets;
	ResourceContainer<Sampler>                              samplers;
	ResourceContainer<Texture>                              textures;
	ResourceContainer<VertexShader, uint32_t, true>         vertexShaders;

	GLuint                                                  ringBuffer;
	bool                                                    persistentMapInUse;
	char                                                    *persistentMapping;

	PipelineHandle                                          currentPipeline;

	bool                                                    decriptorSetsDirty;
	HashMap<DSIndex, Descriptor>                            descriptors;

	bool                                                    debug;
	bool                                                    tracing;
	GLuint                                                  vao;
	IndexFormat                                             indexFormat;
	unsigned int                                            indexBufByteOffset;


	bool isRenderPassCompatible(const RenderPass &pass, const Framebuffer &fb);

	void rebindDescriptorSets();

	void recreateSwapchain();
	void recreateRingBuffer(unsigned int newSize);
	unsigned int ringBufferAllocate(unsigned int size, unsigned int alignPower);

	void waitForFrame(unsigned int frameIdx);
	void deleteFrameInternal(Frame &f);

	void createRTHelperFBO(RenderTarget &rt);

	explicit RendererImpl(const RendererDesc &desc);

	~RendererImpl();

	std::vector<char> spirv2glsl(std::string_view name, const ShaderMacros &macros, spirv_cross::CompilerGLSL &glsl);

	GLuint createShader(GLenum type, const std::string &name, const ShaderMacros &macros, spirv_cross::CompilerGLSL &glsl);

	VertexShaderHandle   createVertexShader(const std::string &name, const ShaderMacros &macros);
	FragmentShaderHandle createFragmentShader(const std::string &name, const ShaderMacros &macros);

	void waitForDeviceIdle();
};


} // namespace renderer


#endif  // OPENGLRENDERER_H
