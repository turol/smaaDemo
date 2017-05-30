#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <vector>

#include <spirv_glsl.hpp>

#include "Renderer.h"
#include "Utils.h"
#include "RendererInternal.h"


static GLuint createShader(GLenum type, const std::string &name, const std::vector<char> &src) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);

	const char *sourcePointer = &src[0];
	GLint sourceLen = src.size();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &sourcePointer, &sourceLen);
	glCompileShader(shader);

	// TODO: defer checking to enable multithreaded shader compile
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	{
		GLint infoLogLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
		if (infoLogLen != 0) {
			std::vector<char> infoLog(infoLogLen + 1, '\0');
			// TODO: better logging
			glGetShaderInfoLog(shader, infoLogLen, NULL, &infoLog[0]);
			if (infoLog[0] != '\0') {
				printf("shader \"%s\" info log:\n%s\ninfo log end\n", name.c_str(), &infoLog[0]); fflush(stdout);
			}
		}
	}

	if (status != GL_TRUE) {
		glDeleteShader(shader);
		throw std::runtime_error("shader compile failed");
	}

	return shader;
}


static GLenum glTexFormat(Format format) {
	switch (format) {
	case Invalid:
		__builtin_unreachable();

	case R8:
		return GL_R8;

	case RG8:
		return GL_RG8;

	case RGB8:
		return GL_RGB8;

	case RGBA8:
		return GL_RGBA8;

	case Depth16:
		return GL_DEPTH_COMPONENT16;

	}

	__builtin_unreachable();
}


static GLenum glTexBaseFormat(Format format) {
	switch (format) {
	case Invalid:
		__builtin_unreachable();

	case R8:
		return GL_RED;

	case RG8:
		return GL_RG;

	case RGB8:
		return GL_RGB;

	case RGBA8:
		return GL_RGBA;

	case Depth16:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	}

	__builtin_unreachable();
}


VertexShader::VertexShader()
: shader(0)
{
}


VertexShader::~VertexShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


FragmentShader::FragmentShader()
: shader(0)
{
}


FragmentShader::~FragmentShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


Shader::Shader(GLuint program_)
: program(program_)
{
}


Shader::~Shader() {
	if (program != 0) {
		glDeleteProgram(program);
		program = 0;
	}
}


Framebuffer::~Framebuffer() {
	if (colorTex != 0) {
		glDeleteTextures(1, &colorTex);
		colorTex = 0;
	}

	if (depthTex != 0) {
		glDeleteTextures(1, &depthTex);
		depthTex = 0;
	}

	if (fbo != 0) {
		glDeleteFramebuffers(1, &fbo);
		fbo = 0;
	}
}


static const char *errorSource(GLenum source)
{
	switch (source)
	{
	case GL_DEBUG_SOURCE_API:
		return "API";
		break;

	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return "window system";
		break;

	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return "shader compiler";
		break;

	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return "third party";
		break;

	case GL_DEBUG_SOURCE_APPLICATION:
		return "application";
		break;

	case GL_DEBUG_SOURCE_OTHER:
		return "other";
		break;

	default:
		break;
	}

	return "unknown source";
}


static const char *errorType(GLenum type)
{
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
	case GL_DEBUG_CATEGORY_API_ERROR_AMD:
		return "error";
		break;

	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
		return "deprecated behavior";
		break;

	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
		return "undefined behavior";
		break;

	case GL_DEBUG_TYPE_PORTABILITY:
		return "portability";
		break;

	case GL_DEBUG_TYPE_PERFORMANCE:
	case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
		return "performance";
		break;

	case GL_DEBUG_TYPE_OTHER:
	case GL_DEBUG_CATEGORY_OTHER_AMD:
		return "other";
		break;

	case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
		return "window system error";
		break;

	case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
		return "shader compiler error";
		break;

	case GL_DEBUG_CATEGORY_APPLICATION_AMD:
		return "application error";
		break;

	default:
		break;

	}

	return "unknown type";
}


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH_ARB:
		printf("GL error from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		printf("GL warning from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_LOW_ARB:
		printf("GL debug from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	default:
		printf("GL error of unknown severity %x from %s type %s: (%d) %s\n", severity, errorSource(source), errorType(type), id, message);
		break;
	}
}


RendererImpl::RendererImpl(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, savePreprocessedShaders(false)
, window(nullptr)
, context(nullptr)
, vao(0)
, idxBuf16Bit(false)
, inRenderPass(false)
{

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)

	unsigned int glMajor = 4;
	unsigned int glMinor = 5;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	if (desc.debug) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	}

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	printf("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		printf("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			printf("Display mode %i : width %i, height %i, BPP %i, refresh %u Hz\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
		}
	}

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (desc.swapchain.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	context = SDL_GL_CreateContext(window);

	if (desc.swapchain.vsync) {
		// enable vsync, using late swap tearing if possible
		int retval = SDL_GL_SetSwapInterval(-1);
		if (retval != 0) {
			// TODO: check return val
			SDL_GL_SetSwapInterval(1);
		}
		printf("VSync is on\n");
	}

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	glewExperimental = true;
	glewInit();

	// TODO: check extensions
	// at least direct state access, texture storage

	if (!GLEW_ARB_direct_state_access) {
		printf("ARB_direct_state_access not found\n");
		exit(1);
	}

	if (desc.debug) {
		if (GLEW_KHR_debug) {
			printf("KHR_debug found\n");

			glDebugMessageCallback(glDebugCallback, NULL);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		} else {
			printf("KHR_debug not found\n");
		}
	}

	printf("GL vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("GL renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("GL version: \"%s\"\n", glGetString(GL_VERSION));
	printf("GLSL version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// swap once to get better traces
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
}


RendererImpl::~RendererImpl() {
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle RendererImpl::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);

	GLuint buffer = 0;
	glCreateBuffers(1, &buffer);
	glNamedBufferData(buffer, size, contents, GL_STATIC_DRAW);

	return buffer;
}


BufferHandle RendererImpl::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: sub-allocate from persistent coherent buffer
	GLuint buffer = 0;
	glCreateBuffers(1, &buffer);
	glNamedBufferData(buffer, size, contents, GL_STREAM_DRAW);

	ephemeralBuffers.push_back(buffer);

	return buffer;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

	auto vertexSrc = loadSource(vertexShaderName);

	shaderc::CompileOptions options;
	// TODO: optimization level?
	// TODO: cache includes globally
	options.SetIncluder(std::make_unique<Includer>());

	for (const auto &p : macros) {
		options.AddMacroDefinition(p.first, p.second);
	}

	auto result = compiler.CompileGlslToSpv(&vertexSrc[0], vertexSrc.size(), shaderc_glsl_vertex_shader, vertexShaderName.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		printf("Shader %s compile failed: %s\n", vertexShaderName.c_str(), result.GetErrorMessage().c_str());
		exit(1);
	}

	spirv_cross::CompilerGLSL glsl(std::vector<uint32_t>(result.cbegin(), result.cend()));
	spirv_cross::CompilerGLSL::Options glslOptions;
	glslOptions.vertex.fixup_clipspace = false;
	glsl.set_options(glslOptions);
	std::string src_ = glsl.compile();

	std::vector<char> src(src_.begin(), src_.end());

	if (savePreprocessedShaders) {
		// FIXME: name not really accurate
		writeFile(vertexShaderName + ".prep", src);
	}

	auto v = std::make_unique<VertexShader>();
	auto id = createShader(GL_VERTEX_SHADER, vertexShaderName, src);
	v->shader = id;

	vertexShaders.emplace(id, std::move(v));

	return VertexShaderHandle(id);
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName = name + ".frag";

	auto fragSrc = loadSource(fragmentShaderName);

	shaderc::CompileOptions options;
	// TODO: optimization level?
	// TODO: cache includes globally
	options.SetIncluder(std::make_unique<Includer>());

	for (const auto &p : macros) {
		options.AddMacroDefinition(p.first, p.second);
	}

	auto result = compiler.CompileGlslToSpv(&fragSrc[0], fragSrc.size(), shaderc_glsl_fragment_shader, fragmentShaderName.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		printf("Shader %s compile failed: %s\n", fragmentShaderName.c_str(), result.GetErrorMessage().c_str());
		exit(1);
	}

	spirv_cross::CompilerGLSL glsl(std::vector<uint32_t>(result.cbegin(), result.cend()));
	spirv_cross::CompilerGLSL::Options glslOptions;
	glslOptions.vertex.fixup_clipspace = false;
	glsl.set_options(glslOptions);
	std::string src_ = glsl.compile();

	std::vector<char> src(src_.begin(), src_.end());

	if (savePreprocessedShaders) {
		// FIXME: name not really accurate
		writeFile(fragmentShaderName + ".prep", src);
	}

	auto f = std::make_unique<FragmentShader>();
	auto id = createShader(GL_FRAGMENT_SHADER, name, src);
	f->shader = id;

	fragmentShaders.emplace(id, std::move(f));

	return FragmentShaderHandle(id);
}


RenderPassHandle RendererImpl::createRenderPass(FramebufferHandle /* fbo */, const RenderPassDesc & /* desc */) {
	return RenderPassHandle(0);
}


PipelineHandle RendererImpl::createPipeline(const PipelineDesc &desc) {
	// TODO: something better
	uint32_t handle = pipelines.size() + 1;
	auto it = pipelines.find(handle);
	while (it != pipelines.end()) {
		handle++;
		it = pipelines.find(handle);
	}

	// TODO: cache shaders

	assert(desc.vertexShader_.handle != 0);
	assert(desc.fragmentShader_.handle != 0);

	GLuint program = glCreateProgram();

	glAttachShader(program, desc.vertexShader_.handle);
	glAttachShader(program, desc.fragmentShader_.handle);
	glLinkProgram(program);

	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetProgramInfoLog(program, status, NULL, &infoLog[0]);
		printf("info log: %s\n", &infoLog[0]); fflush(stdout);
		throw std::runtime_error("shader link failed");
	}
	glUseProgram(program);

	shaders.emplace(program, std::make_unique<Shader>(program));

	Pipeline pipeline(desc, program);

	pipelines.emplace(handle, pipeline);

	return handle;
}


FramebufferHandle RendererImpl::createFramebuffer(const FramebufferDesc &desc) {
	GLuint fbo = 0;

	glCreateFramebuffers(1, &fbo);
	auto fb = std::make_unique<Framebuffer>(fbo);

	auto it = renderTargets.find(desc.colors_[0]);
	assert(it != renderTargets.end());

	fb->colorTex = desc.colors_[0];
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, fb->colorTex, 0);
	assert(desc.colors_[1] == 0);

	if (desc.depthStencil_ != 0) {
		fb->depthTex = desc.depthStencil_;
		glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, fb->depthTex, 0);
	}

	const auto &colorDesc = renderTargets[desc.colors_[0]];
	assert(colorDesc.width_ > 0);
	assert(colorDesc.height_ > 0);
	fb->width  = colorDesc.width_;
	fb->height = colorDesc.height_;

	framebuffers.emplace(fbo, std::move(fb));

	return FramebufferHandle(fbo);
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	GLuint rt = 0;

	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Invalid);

	glCreateTextures(GL_TEXTURE_2D, 1, &rt);
	glTextureStorage2D(rt, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(rt, GL_TEXTURE_MAX_LEVEL, 0);

	renderTargets.emplace(rt, desc);

	return rt;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc &desc) {
	GLuint sampler = 0;
	glCreateSamplers(1, &sampler);

	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (desc.min == Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (desc.mag == Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S,     (desc.wrapMode == Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T,     (desc.wrapMode == Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

	return sampler;
}


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	GLuint texture = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureStorage2D(texture, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, desc.numMips_ - 1);
	unsigned int w = desc.width_, h = desc.height_;

	for (unsigned int i = 0; i < desc.numMips_; i++) {
		assert(desc.mipData_[i] != nullptr);
		glTextureSubImage2D(texture, i, 0, 0, w, h, glTexBaseFormat(desc.format_), GL_UNSIGNED_BYTE, desc.mipData_[i]);

		w = std::max(w / 2, 1u);
		h = std::max(h / 2, 1u);
	}

	return texture;
}


void RendererImpl::deleteBuffer(BufferHandle handle) {
	glDeleteBuffers(1, &handle);
}


void RendererImpl::deleteFramebuffer(FramebufferHandle fbo) {
	assert(fbo.handle != 0);
	auto it = framebuffers.find(fbo.handle);
	assert(it != framebuffers.end());

	framebuffers.erase(it);
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &) {
	// TODO...
}


void RendererImpl::deleteSampler(SamplerHandle handle) {
	glDeleteSamplers(1, &handle);
}


void RendererImpl::deleteTexture(TextureHandle handle) {
	glDeleteTextures(1, &handle);
}


void RendererImpl::recreateSwapchain(const SwapchainDesc &desc) {
	if (swapchainDesc.fullscreen != desc.fullscreen) {
		if (desc.fullscreen) {
			// TODO: check return val?
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			printf("Fullscreen\n");
		} else {
			SDL_SetWindowFullscreen(window, 0);
			printf("Windowed\n");
		}
	}

	if (swapchainDesc.vsync != desc.vsync) {
		if (desc.vsync) {
			// enable vsync, using late swap tearing if possible
			int retval = SDL_GL_SetSwapInterval(-1);
			if (retval != 0) {
				// TODO: check return val
				SDL_GL_SetSwapInterval(1);
			}
			printf("VSync is on\n");
		} else {
			// TODO: check return val
			SDL_GL_SetSwapInterval(0);
			printf("VSync is off\n");
		}
	}

	// we currently don't touch window width and height

	swapchainDesc = desc;
}


void RendererImpl::beginFrame() {
	// TODO: some asserting here

	// TODO: reset all relevant state in case some 3rd-party program fucked them up
	glDepthMask(GL_TRUE);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// TODO: only clear depth/stencil if we have it
	// TODO: set color/etc write masks if necessary
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void RendererImpl::presentFrame(FramebufferHandle fbo) {
	auto it = framebuffers.find(fbo.handle);
	assert(it != framebuffers.end());

	unsigned int width  = it->second->width;
	unsigned int height = it->second->height;

	// TODO: only if enabled
	glDisable(GL_SCISSOR_TEST);

	// TODO: necessary? should do linear blit?
	assert(width  == swapchainDesc.width);
	assert(height == swapchainDesc.height);

	assert(width > 0);
	assert(height > 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	SDL_GL_SwapWindow(window);

	// TODO: multiple frames, only delete after no longer in use by GPU
	// TODO: use persistent coherent buffer
	for (const auto &buffer : ephemeralBuffers) {
		glDeleteBuffers(1, &buffer);
	}
	ephemeralBuffers.clear();
}


void RendererImpl::beginRenderPass(RenderPassHandle /* pass */, FramebufferHandle fbo) {
	assert(!inRenderPass);
	inRenderPass = true;

	// TODO: should get clear bits from RenderPass object
	assert(fbo != 0);
	auto it = framebuffers.find(fbo);
	assert(it != framebuffers.end());

	const auto &fb = it->second;

	GLbitfield mask = GL_COLOR_BUFFER_BIT;
	if (fb->depthTex != 0) {
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	bindFramebuffer(fbo);
	glClear(mask);
}


void RendererImpl::endRenderPass() {
	assert(inRenderPass);
	inRenderPass = false;
}


void RendererImpl::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	glViewport(x, y, width, height);
}


void RendererImpl::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	// TODO: should use current FB height
	glScissor(x, swapchainDesc.height - y, width, height);
}


void RendererImpl::blitFBO(FramebufferHandle src, FramebufferHandle dest) {
	assert(src.handle);
	assert(dest.handle);

	auto itSrc    = framebuffers.find(src.handle);
	assert(itSrc != framebuffers.end());
	auto itDest    = framebuffers.find(dest.handle);
	assert(itDest != framebuffers.end());

	unsigned int srcWidth   = itSrc->second->width;
	unsigned int srcHeight  = itSrc->second->height;
	unsigned int dstWidth   = itDest->second->width;
	unsigned int dstHeight  = itDest->second->height;

	assert(srcWidth  == dstWidth);
	assert(srcHeight == dstHeight);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, src.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest.handle);
	glBlitFramebuffer(0, 0, srcWidth, srcHeight, 0, 0, dstWidth, dstHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


void RendererImpl::bindFramebuffer(FramebufferHandle fbo) {
	assert(fbo.handle);

	auto it = framebuffers.find(fbo.handle);
	assert(it != framebuffers.end());

	assert(it->second->width > 0);
	assert(it->second->height > 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.handle);
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(pipeline != 0);

	auto it = pipelines.find(pipeline);
	assert(it != pipelines.end());

	const auto &p = it->second;

	// TODO: shadow state, set only necessary
	glUseProgram(p.shader);
	if (p.depthWrite_) {
		glDepthMask(GL_TRUE);
	} else {
		glDepthMask(GL_FALSE);
	}

	if (p.depthTest_) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}

	if (p.cullFaces_) {
		glEnable(GL_CULL_FACE);
	} else {
		glDisable(GL_CULL_FACE);
	}

	if (p.scissorTest_) {
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}

	if (p.blending_) {
		glEnable(GL_BLEND);
		// TODO: get from Pipeline
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}

	uint32_t oldMask = currentPipeline.vertexAttribMask;
	uint32_t newMask = p.vertexAttribMask;

	// enable/disable changed attributes
	uint32_t vattrChanged = oldMask ^ newMask;
	while (vattrChanged != 0) {
		int bit = __builtin_ctz(vattrChanged);
		uint32_t mask = 1 << bit;

		if (newMask & mask) {
			glEnableVertexAttribArray(bit);
		} else {
			glDisableVertexAttribArray(bit);
		}

		vattrChanged &= ~mask;
	}

	// set format on new attributes
	const auto &attribs = p.vertexAttribs;
	while (newMask) {
		int bit = __builtin_ctz(newMask);
		uint32_t mask = 1 << bit;

		const auto &attr = attribs[bit];
		bool normalized = false;
		GLenum format = GL_NONE;
		switch (attr.format) {
		case VtxFormat::Float:
			format = GL_FLOAT;
			break;

		case VtxFormat::UNorm8:
			format = GL_UNSIGNED_BYTE;
			normalized = true;
			break;
		}

		glVertexAttribFormat(bit, attr.count, format, normalized ? GL_TRUE : GL_FALSE, attr.offset);
		glVertexAttribBinding(bit, attr.bufBinding);
		newMask &= ~mask;
	}

	currentPipeline = p;
}


void RendererImpl::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
	idxBuf16Bit = bit16;
}


void RendererImpl::bindVertexBuffer(unsigned int binding, BufferHandle buffer) {
	glBindVertexBuffer(binding, buffer, 0, currentPipeline.vertexBuffers[binding].stride);
}


void RendererImpl::bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler) {
	glBindTextureUnit(unit, tex);
	glBindSampler(unit, sampler);
}


void RendererImpl::bindUniformBuffer(unsigned int index, BufferHandle buffer) {
	glBindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
}

void RendererImpl::bindStorageBuffer(unsigned int index, BufferHandle buffer) {
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, buffer);
}

void RendererImpl::draw(unsigned int firstVertex, unsigned int vertexCount) {
	assert(inRenderPass);

	// TODO: get primitive from current pipeline
	glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
}


void RendererImpl::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	assert(inRenderPass);

	// TODO: get primitive from current pipeline
	GLenum format = idxBuf16Bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT ;
	glDrawElementsInstanced(GL_TRIANGLES, vertexCount, format, NULL, instanceCount);
}


#endif //  RENDERER_OPENGL
