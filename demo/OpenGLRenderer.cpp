#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <vector>

#include "Renderer.h"
#include "Utils.h"


class VertexShader {
#ifdef RENDERER_OPENGL

	GLuint shader;

#endif  // RENDERER_OPENGL

	VertexShader() = delete;

	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	friend class Shader;

public:

	VertexShader(const std::string &name, const ShaderMacros &macros);

	~VertexShader();
};


class FragmentShader {
#ifdef RENDERER_OPENGL

	GLuint shader;

#endif  // RENDERER_OPENGL

	FragmentShader() = delete;

	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&) = delete;
	FragmentShader &operator=(FragmentShader &&) = delete;

	friend class Shader;

public:

	FragmentShader(const std::string &name, const ShaderMacros &macros);

	~FragmentShader();
};


static std::vector<char> processShaderIncludes(std::vector<char> shaderSource, const ShaderMacros &macros) {
	std::vector<char> output(shaderSource);

	auto includePos = output.begin();
	std::string::size_type lastExtPos = 0;

	while (true) {
		// find an #include
		while (includePos < output.end()) {
			// is it a comment?
			if (*includePos == '/') {
				includePos++;
				if (includePos == output.end()) {
					break;
				}

				if (*includePos == '/') {
					// until line end
					includePos = std::find(includePos, output.end(), '\n');
				} else if (*includePos == '*') {
					// until "*/"
					while (true) {
						includePos = std::find(includePos + 1, output.end(), '*');
						if (includePos == output.end()) {
							break;
						}

						includePos++;
						if (includePos == output.end()) {
							break;
						}

						if (*includePos == '/') {
							includePos++;
							break;
						} else if (*includePos == '*') {
							// handle "**/"
							includePos--;
						}
					}
				}
			} else if (*includePos == '#' ) {
				std::string directive(includePos + 1, std::min(includePos + 8, output.end()));
				if (directive == "include") {
					// we have an "#include"
					break;
				} else if (directive == "version" || directive == "extensi") {
					lastExtPos = std::distance(output.begin(), includePos);
				}
				includePos++;
			} else {
				includePos++;
			}
		}

		if (includePos == output.end()) {
			// not found, we're done
			break;
		}

		// find first of either " or <
		auto filenamePos = std::min(std::find(includePos, output.end(), '"'), std::find(includePos, output.end(), '<')) + 1;
		auto filenameEnd = std::min(std::find(filenamePos, output.end(), '"'), std::find(filenamePos, output.end(), '>'));
		std::string filename(filenamePos, filenameEnd);

		// we don't want a terminating '\0'
		auto includeContents = readFile(filename);
		// TODO: strip other errant '\0's

		std::vector<char> newOutput;
		// TODO: could reduce this a bit
		newOutput.reserve(output.size() + includeContents.size());
		newOutput.insert(newOutput.end(), output.begin(), includePos);
		newOutput.insert(newOutput.end(), includeContents.begin(), includeContents.end());
		newOutput.insert(newOutput.end(), filenameEnd + 1, output.end());

		auto dist = std::distance(output.begin(), includePos);
		std::swap(output, newOutput);
		includePos = output.begin() + dist;
		// go again in case of recursive includes
	}

	// add macros after last #version and #extension
	if (!macros.empty()) {
		std::vector<char> defines;
		for (const auto &p : macros) {
			std::string macro = std::string("#define ") + p.first + " " + p.second + "\n";
			defines.insert(defines.end(), macro.begin(), macro.end());
		}

		auto nextLine = std::find(output.begin() + lastExtPos, output.end(), '\n') + 1;
		output.insert(nextLine, defines.begin(), defines.end());
	}

	return output;
}


static GLuint createShader(GLenum type, const std::string &name, const std::vector<char> &rawSrc, const ShaderMacros &macros) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
	auto src = processShaderIncludes(rawSrc, macros);

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


VertexShader::VertexShader(const std::string &name, const ShaderMacros &macros)
: shader(0)
{
	auto source = readTextFile(name);
	shader = createShader(GL_VERTEX_SHADER, name, source, macros);
}


VertexShader::~VertexShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


FragmentShader::FragmentShader(const std::string &name, const ShaderMacros &macros)
: shader(0)
{
	auto source = readTextFile(name);
	shader = createShader(GL_FRAGMENT_SHADER, name, source, macros);
}


FragmentShader::~FragmentShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


Shader::Shader(const std::string &name, const ShaderMacros &macros)
: program(0)
{
	VertexShader   vertexShader(name + ".vert", macros);
	FragmentShader fragmentShader(name + ".frag", macros);

	program = glCreateProgram();

	glAttachShader(program, vertexShader.shader);
	glAttachShader(program, fragmentShader.shader);
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


Renderer::Renderer(const RendererDesc &desc)
: window(nullptr)
, context(nullptr)
, vao(0)
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
			printf("Display mode %i : width %i, height %i, BPP %i\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format));
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


Renderer *Renderer::createRenderer(const RendererDesc &desc) {
	return new Renderer(desc);
}


Renderer::~Renderer() {
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	GLuint buffer = 0;
	glCreateBuffers(1, &buffer);
	glNamedBufferData(buffer, size, contents, GL_STATIC_DRAW);

	return buffer;
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	GLuint fbo = 0;

	glCreateFramebuffers(1, &fbo);
	auto fb = std::make_unique<Framebuffer>(fbo);

	fb->colorTex = desc.colors_[0];
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, fb->colorTex, 0);
	assert(desc.colors_[1] == 0);

	if (desc.depthStencil_ != 0) {
		fb->depthTex = desc.depthStencil_;
		glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, fb->depthTex, 0);
	}

	const auto &colorDesc = renderTargets[desc.colors_[0]];
	fb->width  = colorDesc.width_;
	fb->height = colorDesc.height_;

	return fb;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	GLuint rt = 0;

	glCreateTextures(GL_TEXTURE_2D, 1, &rt);
	glTextureStorage2D(rt, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(rt, GL_TEXTURE_MAX_LEVEL, 0);

	renderTargets.emplace(rt, desc);

	return rt;
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	GLuint sampler = 0;
	glCreateSamplers(1, &sampler);

	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (desc.min == Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (desc.mag == Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S,     (desc.wrapMode == Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T,     (desc.wrapMode == Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

	return sampler;
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
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


void Renderer::deleteBuffer(BufferHandle handle) {
	glDeleteBuffers(1, &handle);
}


void Renderer::deleteSampler(SamplerHandle handle) {
	glDeleteSamplers(1, &handle);
}


void Renderer::deleteTexture(TextureHandle handle) {
	glDeleteTextures(1, &handle);
}


void Renderer::recreateSwapchain(const SwapchainDesc &desc) {
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


void Renderer::presentFrame() {
	SDL_GL_SwapWindow(window);
}


void Renderer::blitFBO(const FramebufferHandle &src, const FramebufferHandle &dest) {
	assert(src);
	assert(dest);
	assert(src->width  == dest->width);
	assert(src->height == dest->height);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest->fbo);
	glBlitFramebuffer(0, 0, src->width, src->height, 0, 0, src->width, src->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


void Renderer::bindFramebuffer(const FramebufferHandle &fbo) {
	assert(fbo);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
}


void Renderer::bindShader(const std::unique_ptr<Shader> &shader) {
	assert(shader);
	assert(shader->program != 0);

	glUseProgram(shader->program);
}


#endif //  RENDERER_OPENGL
