/*
Copyright (c) 2015-2021 Alternative Games Ltd / Turo Lamminen

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


#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <vector>

#include <boost/variant/get.hpp>

#include <spirv_glsl.hpp>

#include "Renderer.h"
#include "utils/Utils.h"
#include "RendererInternal.h"

#include <glm/gtc/type_ptr.hpp> // glm::value_ptr


namespace renderer {


struct GLValueName {
	GLenum      value;
	const char  *name;
};


#define GLVALUE(x) { x, "" #x }

static const GLValueName interestingValues[] = {
	  GLVALUE(GL_MAX_COLOR_TEXTURE_SAMPLES)
	, GLVALUE(GL_MAX_DEPTH_TEXTURE_SAMPLES)
	, GLVALUE(GL_MAX_INTEGER_SAMPLES)
	, GLVALUE(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT)
	, GLVALUE(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT)
};


#undef GLVALUE


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */);


static void pushString(std::vector<char> &v, const std::string &str) {
	v.insert(v.end(), str.begin(), str.end());
}


static std::vector<char> spirv2glsl(const std::string &name, const ShaderMacros &macros, spirv_cross::CompilerGLSL &glsl) {
	std::string src_ = glsl.compile();

	std::vector<char> result;
	{
		size_t size = src_.size() + 3 + name.size() + 1;
		std::vector<std::string> sorted;
		sorted.reserve(macros.size());
		for (const auto &macro : macros) {
			std::string str = macro.first;
			if (!macro.second.empty()) {
				str += "=";
				str += macro.second;
			}
			size += 3 + str.size() + 1;
			sorted.emplace_back(std::move(str));
		}

		std::sort(sorted.begin(), sorted.end());

		result.reserve(size);

		pushString(result, "// ");
		pushString(result, name);
		result.push_back('\n');

		for (const auto &s : sorted) {
			pushString(result, "// ");
			pushString(result, s);
			result.push_back('\n');
		}
	}

	result.insert(result.end(), src_.begin(), src_.end());
	return result;
}


static GLuint createShader(GLenum type, const std::string &name, const ShaderMacros &macros, spirv_cross::CompilerGLSL &glsl) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);

	std::vector<char> src = spirv2glsl(name, macros, glsl);

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
				LOG("shader \"{}\" info log:\n{}\ninfo log end", name, &infoLog[0]);
				logFlush();
			}
		}
	}

	if (status != GL_TRUE) {
		glDeleteShader(shader);
		throw std::runtime_error("shader compile failed");
	}

	return shader;
}


static GLenum blendFunc(BlendFunc b) {
	switch (b) {
	case BlendFunc::Zero:
		return GL_ZERO;

	case BlendFunc::One:
		return GL_ONE;

	case BlendFunc::Constant:
		return GL_CONSTANT_ALPHA;

	case BlendFunc::SrcAlpha:
		return GL_SRC_ALPHA;

	case BlendFunc::OneMinusSrcAlpha:
		return GL_ONE_MINUS_SRC_ALPHA;

	}

	HEDLEY_UNREACHABLE();
	return GL_NONE;
}


static GLenum glTexFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();

	case Format::R8:
		return GL_R8;

	case Format::RG8:
		return GL_RG8;

	case Format::RGB8:
		return GL_RGB8;

	case Format::RGBA8:
		return GL_RGBA8;

	case Format::sRGBA8:
		return GL_SRGB8_ALPHA8;

	case Format::RG16Float:
		return GL_RG16F;

	case Format::RGBA16Float:
		return GL_RGBA16F;

	case Format::RGBA32Float:
		return GL_RGBA32F;

	case Format::Depth16:
		return GL_DEPTH_COMPONENT16;

	case Format::Depth16S8:
		return GL_DEPTH24_STENCIL8;

	case Format::Depth24S8:
		return GL_DEPTH24_STENCIL8;

	case Format::Depth24X8:
		return GL_DEPTH_COMPONENT24;

	case Format::Depth32Float:
		return GL_DEPTH_COMPONENT32F;

	}

	HEDLEY_UNREACHABLE();
}


static GLenum glTexBaseFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		HEDLEY_UNREACHABLE();

	case Format::R8:
		return GL_RED;

	case Format::RG8:
	case Format::RG16Float:
		return GL_RG;

	case Format::RGB8:
		return GL_RGB;

	case Format::RGBA8:
	case Format::RGBA16Float:
	case Format::RGBA32Float:
		return GL_RGBA;

	case Format::sRGBA8:
		return GL_RGBA;

	case Format::Depth16:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	case Format::Depth16S8:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	case Format::Depth24S8:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	case Format::Depth24X8:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	case Format::Depth32Float:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	}

	HEDLEY_UNREACHABLE();
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
		LOG("GL error from {} type {}: ({}) {}", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		LOG("GL warning from {} type {}: ({}) {}", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_LOW_ARB:
		LOG("GL debug from {} type {}: ({}) {}", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_NOTIFICATION:
		if (type != GL_DEBUG_TYPE_PUSH_GROUP && type != GL_DEBUG_TYPE_POP_GROUP) {
			LOG("GL notice from {} type {}: ({}) {}", errorSource(source), errorType(type), id, message);
		}
		break;

	default:
		LOG("GL error of unknown severity {:#04x} from {} type {}: ({}) {}", severity, errorSource(source), errorType(type), id, message);
		break;
	}
}



void mergeShaderResources(ShaderResources &first, const ShaderResources &second) {
	for (unsigned int i = 0; i < second.ubos.size(); i++) {
		DSIndex idx = second.ubos.at(i);
		if (i < first.ubos.size()) {
			DSIndex other = first.ubos.at(i);
			if (idx != other) {
				LOG("ERROR: mismatch when merging shader UBOs, {} is ({}, {}) when expecting ({}, {})", i, idx.set, idx.binding, other.set, other.binding);
				throw std::runtime_error("resource mismatch");
			}
		} else {
			first.ubos.push_back(idx);
		}
	}

	for (unsigned int i = 0; i < second.ssbos.size(); i++) {
		DSIndex idx = second.ssbos.at(i);
		if (i < first.ssbos.size()) {
			DSIndex other = first.ssbos.at(i);
			if (idx != other) {
				LOG("ERROR: mismatch when merging shader SSBOs, {} is ({}, {}) when expecting ({}, {})", i, idx.set, idx.binding, other.set, other.binding);
				throw std::runtime_error("resource mismatch");
			}
		} else {
			first.ssbos.push_back(idx);
		}
	}

	for (unsigned int i = 0; i < second.textures.size(); i++) {
		DSIndex idx = second.textures.at(i);
		if (i < first.textures.size()) {
			DSIndex other = first.textures.at(i);
			if (idx != other) {
				LOG("ERROR: mismatch when merging shader textures, {} is ({}, {}) when expecting ({}, {})", i, idx.set, idx.binding, other.set, other.binding);
				throw std::runtime_error("resource mismatch");
			}
		} else {
			first.textures.push_back(idx);
		}
	}

	for (unsigned int i = 0; i < second.samplers.size(); i++) {
		DSIndex idx = second.samplers.at(i);
		if (i < first.samplers.size()) {
			DSIndex other = first.samplers.at(i);
			if (idx != other) {
				LOG("ERROR: mismatch when merging shader textures, {} is ({}, {}) when expecting ({}, {})", i, idx.set, idx.binding, other.set, other.binding);
				throw std::runtime_error("resource mismatch");
			}
		} else {
			first.samplers.push_back(idx);
		}
	}
}


RendererImpl::RendererImpl(const RendererDesc &desc)
: RendererBase(desc)
, window(nullptr)
, context(nullptr)
, ringBuffer(0)
, persistentMapInUse(false)
, persistentMapping(nullptr)
, decriptorSetsDirty(true)
, debug(desc.debug)
, tracing(desc.tracing)
, vao(0)
, idxBuf16Bit(false)
, indexBufByteOffset(0)
{

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	// TODO: highdpi
	// TODO: check errors

	unsigned int glMajor = 4;
	unsigned int glMinor = 5;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
	bool wantKHRDebug = debug || tracing;
	if (wantKHRDebug) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	}
	if (desc.robustness) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG);
	}

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	LOG("Number of displays detected: {}", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int retval = SDL_GetDesktopDisplayMode(i, &mode);
		if (retval == 0) {
			LOG("Desktop mode for display {}: {}x{}, refresh {} Hz", i, mode.w, mode.h, mode.refresh_rate);
			currentRefreshRate = mode.refresh_rate;
		} else {
			LOG("Failed to get desktop display mode for display {}", i);
		}

		int numModes = SDL_GetNumDisplayModes(i);
		LOG("Number of display modes for display {} : {}", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			LOG("Display mode {} : width {}, height {}, BPP {}, refresh {} Hz", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
			maxRefreshRate = std::max(static_cast<unsigned int>(mode.refresh_rate), maxRefreshRate);
		}
	}

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (desc.swapchain.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow(desc.applicationName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	if (!window) {
		LOG("SDL_CreateWindow failed: {}", SDL_GetError());
		throw std::runtime_error("SDL_CreateWindow failed");
	}

	context = SDL_GL_CreateContext(window);

	{
		int value = -1;
		SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &value);
		LOG("sRGB framebuffer: {}", value);
		features.sRGBFramebuffer = value;
	}

	bool vsync = false;
	int retval = 0;
	switch (desc.swapchain.vsync) {
	case VSync::LateSwapTear:
		retval = SDL_GL_SetSwapInterval(-1);
		if (retval != 0) {
			LOG("Failed to set late swap tearing vsync: {}", SDL_GetError());
		} else {
			vsync = true;
			break;
		}
		// fallthrough

	case VSync::On:
		retval = SDL_GL_SetSwapInterval(1);
		if (retval != 0) {
			LOG("Failed to set vsync: {}", SDL_GetError());
		} else {
			vsync = true;
		}
		break;

	case VSync::Off:
		// nothing here
		break;

	}

	LOG("VSync is {}", vsync ? "on" : "off");

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	glewExperimental = true;
	glewInit();

	// TODO: check extensions
	// at least direct state access, texture storage

	if (GLEW_VERSION_4_3 || GLEW_ARB_shader_storage_buffer_object) {
		features.SSBOSupported = true;
		LOG("Shader storage buffer supported");
	} else {
		features.SSBOSupported = false;
		LOG("Shader storage buffer not supported");
	}

	if (!GLEW_ARB_direct_state_access) {
		LOG("ARB_direct_state_access not found");
		throw std::runtime_error("ARB_direct_state_access not found");
	}

	if (!GLEW_ARB_buffer_storage) {
		LOG("ARB_buffer_storage not found");
		throw std::runtime_error("ARB_buffer_storage not found");
	}

	if (!GLEW_ARB_clip_control) {
		LOG("ARB_clip_control not found");
		throw std::runtime_error("ARB_clip_control not found");
	}

	if (!(GLEW_ARB_texture_view || GLEW_VERSION_4_3)) {
		LOG("ARB_texture_view not found");
		throw std::runtime_error("ARB_texture_view not found");
	}

	if (!GLEW_ARB_texture_storage_multisample) {
		LOG("ARB_texture_storage_multisample not found");
		throw std::runtime_error("ARB_texture_storage_multisample not found");
	}

	if (wantKHRDebug) {
		if (!GLEW_KHR_debug) {
			LOG("KHR_debug not found");
			throw std::runtime_error("KHR_debug not found");
		} else {
			LOG("KHR_debug found");
			if (debug) {
				glDebugMessageCallback(glDebugCallback, NULL);
				glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			}
		}
	}

	LOG("GL vendor: \"{}\"", glGetString(GL_VENDOR));
	LOG("GL renderer: \"{}\"", glGetString(GL_RENDERER));
	LOG("GL version: \"{}\"", glGetString(GL_VERSION));
	LOG("GLSL version: \"{}\"", glGetString(GL_SHADING_LANGUAGE_VERSION));

	LOG("Interesting GL values:");
	glValues.reserve(sizeof(interestingValues) / sizeof(interestingValues[0]));
	for (const auto &v : interestingValues) {
		GLint temp = -1;
		glGetIntegerv(v.value, &temp);
		LOG("{}: {}", v.name, temp);
		glValues.emplace(v.value, temp);
	}

	uboAlign   = glValues[GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT];
	ssboAlign  = glValues[GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT];

	features.maxMSAASamples = std::min(glValues[GL_MAX_COLOR_TEXTURE_SAMPLES], glValues[GL_MAX_DEPTH_TEXTURE_SAMPLES]);

	// TODO: use GL_UPPER_LEFT to match Vulkan
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	if (!recreateSwapchain()) {
		LOG("initial swapchain create failed");
		throw std::runtime_error("initial swapchain create failed");
	}

	recreateRingBuffer(desc.ephemeralRingBufSize);

	// swap once to get better traces
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
}


void RendererImpl::recreateRingBuffer(unsigned int newSize) {
	assert(newSize > 0);

	// if buffer already exists, free it after it's no longer in use
	if (ringBuffer) {
		assert(ringBufSize       != 0);

		if (persistentMapInUse) {
			glUnmapNamedBuffer(ringBuffer);
			persistentMapping = nullptr;
		}

		auto result = buffers.add();
		frames.at(currentFrameIdx).ephemeralBuffers.push_back(result.second);

		Buffer &buffer = result.first;

		buffer.buffer          = ringBuffer;
		ringBuffer             = 0;

		buffer.ringBufferAlloc = false;
		buffer.offset          = 0;

		buffer.type            = BufferType::Everything;

		buffer.size            = ringBufSize;
		ringBufSize            = 0;

		ringBufPtr             = 0;
	}

	// set up ring buffer
	glCreateBuffers(1, &ringBuffer);
	// TODO: proper error checking
	assert(ringBuffer != 0);
	assert(ringBufSize               == 0);
	assert(ringBufPtr                == 0);
	assert(persistentMapping         == nullptr);
	unsigned int bufferFlags = 0;
	// if tracing is on, disable persistent buffer because apitrace can't trace it
	persistentMapInUse = !tracing;
	ringBufSize        = newSize;

	if (!persistentMapInUse) {
		// need GL_DYNAMIC_STORAGE_BIT since we intend to glBufferSubData it
		bufferFlags |= GL_DYNAMIC_STORAGE_BIT;
	} else {
		// TODO: do we need GL_DYNAMIC_STORAGE_BIT?
		// spec seems to say only for glBufferSubData, not persistent mapping
		bufferFlags |= GL_MAP_WRITE_BIT;
		bufferFlags |= GL_MAP_PERSISTENT_BIT;
		bufferFlags |= GL_MAP_COHERENT_BIT;
	}

	// when tracing add read bit so qapitrace can see buffer contents
	if (tracing) {
		bufferFlags |= GL_MAP_READ_BIT;
	}

	glNamedBufferStorage(ringBuffer, ringBufSize, nullptr, bufferFlags);
	if (persistentMapInUse) {
		persistentMapping = reinterpret_cast<char *>(glMapNamedBufferRange(ringBuffer, 0, ringBufSize, bufferFlags));
	}
}


RendererImpl::~RendererImpl() {
	assert(ringBuffer != 0);

	// wait for all pending frames to finish
	waitForDeviceIdle();

	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		assert(!f.outstanding);
		deleteFrameInternal(f);
	}
	frames.clear();


	if (persistentMapInUse) {
		glUnmapNamedBuffer(ringBuffer);
		persistentMapping = nullptr;
	} else {
		assert(persistentMapping == nullptr);
	}

	glDeleteBuffers(1, &ringBuffer);
	ringBuffer = 0;

	framebuffers.clearWith([](Framebuffer &fb) {
		assert(fb.fbo != 0);
		assert(fb.numSamples > 0);
		glDeleteFramebuffers(1, &fb.fbo);
		fb.fbo = 0;
		fb.numSamples = 0;
	} );

	renderPasses.clearWith([](RenderPass &) {
	} );

	renderTargets.clearWith([this](RenderTarget &rt) {
		assert(rt.texture);

		if (rt.helperFBO != 0) {
			glDeleteFramebuffers(1, &rt.helperFBO);
			rt.helperFBO = 0;
		}

		{
			auto &tex = this->textures.get(rt.texture);
			assert(tex.renderTarget);
			tex.renderTarget = false;
			glDeleteTextures(1, &tex.tex);
			tex.tex = 0;
		}

		this->textures.remove(rt.texture);
		rt.texture = TextureHandle();

		if (rt.additionalView) {
			auto &view = this->textures.get(rt.additionalView);
			assert(view.renderTarget);
			view.renderTarget = false;
			assert(view.tex != 0);
			glDeleteTextures(1, &view.tex);
			view.tex = 0;
			this->textures.remove(rt.additionalView);
			rt.additionalView = TextureHandle();
		}
	} );


	pipelines.clearWith([](Pipeline &p) {
		assert(p.shader != 0);
		glDeleteProgram(p.shader);
		p.shader = 0;
	} );

	vertexShaders.clearWith([](VertexShader &) {
	} );

	fragmentShaders.clearWith([](FragmentShader &) {
	} );

	textures.clearWith([](Texture &tex) {
		assert(!tex.renderTarget);
		assert(tex.tex != 0);
		assert(tex.target != GL_NONE);

		glDeleteTextures(1, &tex.tex);
		tex.tex = 0;
		tex.target = GL_NONE;
		tex.format = Format::Invalid;
	} );

	samplers.clearWith([](Sampler &sampler) {
		assert(sampler.sampler != 0);

		glDeleteSamplers(1, &sampler.sampler);
		sampler.sampler = 0;
	} );

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);

	SDL_Quit();
}


bool Renderer::isRenderTargetFormatSupported(Format format) const {
	GLenum target         = GL_TEXTURE_2D;
	GLenum internalFormat = glTexFormat(format);
	int params            = 0;

	glGetInternalformativ(target, internalFormat, GL_INTERNALFORMAT_SUPPORTED, sizeof(int), &params);
	if (params == GL_FALSE) {
		return false;
	}

	params = 0;
	glGetInternalformativ(target, internalFormat, GL_FRAMEBUFFER_RENDERABLE, sizeof(int), &params);
	if (params != GL_FULL_SUPPORT) {
		return false;
	}

	GLenum renderable = isDepthFormat(format) ? GL_DEPTH_RENDERABLE : GL_COLOR_RENDERABLE;

	params = 0;
	glGetInternalformativ(target, internalFormat, renderable, sizeof(int), &params);
	if (params == GL_FALSE) {
		return false;
	}

	return true;
}


BufferHandle Renderer::createBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != +BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	unsigned int bufferFlags = 0;
	if (impl->tracing) {
		bufferFlags |= GL_MAP_READ_BIT;
	}

	auto result            = impl->buffers.add();
	Buffer &buffer         = result.first;
	glCreateBuffers(1, &buffer.buffer);
	glNamedBufferStorage(buffer.buffer, size, contents, bufferFlags);
	buffer.ringBufferAlloc = false;
	buffer.offset          = 0;
	buffer.size            = size;
	buffer.type            = type;

	return result.second;
}


BufferHandle Renderer::createEphemeralBuffer(BufferType type, uint32_t size, const void *contents) {
	assert(type != +BufferType::Invalid);
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: use appropriate alignment
	// TODO: need buffer usage flags for that
	unsigned int beginPtr = impl->ringBufferAllocate(size, std::max(impl->uboAlign, impl->ssboAlign));

	if (impl->persistentMapInUse) {
		memcpy(impl->persistentMapping + beginPtr, contents, size);
	} else {
		glNamedBufferSubData(impl->ringBuffer, beginPtr, size, contents);
	}

	auto result    = impl->buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer          = impl->ringBuffer;
	buffer.ringBufferAlloc = true;
	buffer.offset          = beginPtr;
	buffer.size            = size;
	buffer.type            = type;

	impl->frames.at(impl->currentFrameIdx).ephemeralBuffers.push_back(result.second);

	return result.second;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

    std::vector<uint32_t> spirv = compileSpirv(vertexShaderName, macros, ShaderKind::Vertex);

	auto result_ = vertexShaders.add();
	auto &v = result_.first;
	v.name      = vertexShaderName;
	v.spirv     = std::move(spirv);
	v.macros    = macros;

	return result_.second;
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName = name + ".frag";

	std::vector<uint32_t> spirv = compileSpirv(fragmentShaderName, macros, ShaderKind::Fragment);

	auto result_ = fragmentShaders.add();
	auto &f = result_.first;
	f.name      = fragmentShaderName;
	f.spirv     = std::move(spirv);

	return result_.second;
}


struct ResourceInfo {
	DescriptorType  type;
	uint32_t        glIndex;


	ResourceInfo(DescriptorType type_, uint32_t glIndex_)
	: type(type_)
	, glIndex(glIndex_)
	{
	}


	ResourceInfo(const ResourceInfo &)            = default;
	ResourceInfo(ResourceInfo &&) noexcept        = default;


	ResourceInfo &operator=(const ResourceInfo &) = default;
	ResourceInfo &operator=(ResourceInfo &&) noexcept = default;

	~ResourceInfo() {}
};


typedef HashMap<DSIndex, ResourceInfo> ResourceMap;


static void processShaderResources(ShaderResources &shaderResources, const ResourceMap& dsResources, spirv_cross::CompilerGLSL &glsl) {
	shaderResources.uboSizes.resize(shaderResources.ubos.size(), 0);

	// TODO: only in debug mode
	HashSet<DSIndex> bindings;

	auto spvResources = glsl.get_shader_resources();

	for (const auto &ubo : spvResources.uniform_buffers) {
		DSIndex idx;
		idx.set     = glsl.get_decoration(ubo.id, spv::DecorationDescriptorSet);
		idx.binding = glsl.get_decoration(ubo.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			LOG("Duplicate UBO binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate UBO binding");
		}

		auto it = dsResources.find(idx);
		if (it == dsResources.end()) {
            LOG("UBO ({}, {}) not in descriptor sets", idx.set, idx.binding);
			throw std::runtime_error("UBO not in descriptor sets");
		}

		assert(it->second.type == +DescriptorType::UniformBuffer);
		unsigned int openglIDX = it->second.glIndex;
		assert(openglIDX < shaderResources.ubos.size());
		assert(shaderResources.ubos[openglIDX] == idx);

		uint32_t maxOffset = 0;
		LOG("UBO {} index {} ranges:", ubo.id, openglIDX);
		for (auto r : glsl.get_active_buffer_ranges(ubo.id)) {
			LOG("  {}:  {}  {}", r.index, r.offset, r.range);
			maxOffset = std::max(maxOffset, static_cast<uint32_t>(r.offset + r.range));
		}
		LOG(" max offset: {}", maxOffset);
		shaderResources.uboSizes[openglIDX] = maxOffset;

		// opengl doesn't like set decorations, strip them
		glsl.unset_decoration(ubo.id, spv::DecorationDescriptorSet);
		glsl.set_decoration(ubo.id, spv::DecorationBinding, openglIDX);
	}

	for (const auto &ssbo : spvResources.storage_buffers) {
		DSIndex idx;
		idx.set     = glsl.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
		idx.binding = glsl.get_decoration(ssbo.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			LOG("Duplicate SSBO binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate SSBO binding");
		}

		auto it = dsResources.find(idx);
		if (it == dsResources.end()) {
            LOG("SSBO ({}, {}) not in descriptor sets", idx.set, idx.binding);
			throw std::runtime_error("SSBO not in descriptor sets");
		}

		assert(it->second.type == +DescriptorType::StorageBuffer);
		unsigned int openglIDX = it->second.glIndex;
		assert(openglIDX < shaderResources.ssbos.size());
		assert(shaderResources.ssbos[openglIDX] == idx);

		// opengl doesn't like set decorations, strip them
		glsl.unset_decoration(ssbo.id, spv::DecorationDescriptorSet);
		glsl.set_decoration(ssbo.id, spv::DecorationBinding, openglIDX);
	}

	for (const auto &s : spvResources.sampled_images) {
		DSIndex idx;
		idx.set     = glsl.get_decoration(s.id, spv::DecorationDescriptorSet);
		idx.binding = glsl.get_decoration(s.id, spv::DecorationBinding);

		// must be the first time we find this (set, binding) combination
		// if not, there's a bug in the shader
		auto b = bindings.insert(idx);
		if (!b.second) {
			LOG("Duplicate image binding ({}, {})", idx.set, idx.binding);
			throw std::runtime_error("Duplicate image binding");
		}

		auto it = dsResources.find(idx);
		if (it == dsResources.end()) {
            LOG("Sampled image ({}, {}) not in descriptor sets", idx.set, idx.binding);
			throw std::runtime_error("Sampled image not in descriptor sets");
		}

		assert(it->second.type == +DescriptorType::CombinedSampler);
		unsigned int openglIDX = it->second.glIndex;
		assert(openglIDX < shaderResources.textures.size());
		assert(openglIDX < shaderResources.samplers.size());
		assert(shaderResources.textures[openglIDX] == idx);
		assert(shaderResources.samplers[openglIDX] == idx);

		// opengl doesn't like set decorations, strip them
		glsl.unset_decoration(s.id, spv::DecorationDescriptorSet);
		glsl.set_decoration(s.id, spv::DecorationBinding, openglIDX);
	}

	// build combined image samplers
	// TODO: need to store this info
	glsl.build_combined_image_samplers();

	for (const spirv_cross::CombinedImageSampler &c : glsl.get_combined_image_samplers()) {
		assert(shaderResources.textures.size() == shaderResources.samplers.size());
		unsigned int openglIDX = shaderResources.textures.size();

		DSIndex idx;
		idx.set     = glsl.get_decoration(c.image_id, spv::DecorationDescriptorSet);
		idx.binding = glsl.get_decoration(c.image_id, spv::DecorationBinding);
		shaderResources.textures.push_back(idx);

		idx.set     = glsl.get_decoration(c.sampler_id, spv::DecorationDescriptorSet);
		idx.binding = glsl.get_decoration(c.sampler_id, spv::DecorationBinding);
		shaderResources.samplers.push_back(idx);

		// don't clear the set decoration because other combined samplers might need it
		glsl.set_decoration(c.combined_id, spv::DecorationBinding, openglIDX);
	}

	// now clear the set decorations
	for (const spirv_cross::CombinedImageSampler &c : glsl.get_combined_image_samplers()) {
		glsl.unset_decoration(c.image_id,    spv::DecorationDescriptorSet);
		glsl.unset_decoration(c.image_id,    spv::DecorationBinding);
		glsl.unset_decoration(c.sampler_id, spv::DecorationDescriptorSet);
		glsl.unset_decoration(c.sampler_id, spv::DecorationBinding);
	}
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	assert(!desc.vertexShaderName.empty());
	assert(!desc.fragmentShaderName.empty());
	assert(desc.renderPass_);
	assert(!desc.name_.empty());

#ifndef NDEBUG
	const auto &rp = impl->renderPasses.get(desc.renderPass_);
	assert(desc.numSamples_ == rp.numSamples);
#endif //  NDEBUG

	auto vshaderHandle = impl->createVertexShader(desc.vertexShaderName, desc.shaderMacros_);
	const auto &v = impl->vertexShaders.get(vshaderHandle);
	auto fshaderHandle = impl->createFragmentShader(desc.fragmentShaderName, desc.shaderMacros_);
	const auto &f = impl->fragmentShaders.get(fshaderHandle);

	// construct map of descriptor set resources
	ResourceMap      dsResources;
	ShaderResources  shaderResources;
	for (unsigned int i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
		if (desc.descriptorSetLayouts[i]) {
			const auto &layoutDesc = impl->dsLayouts.get(desc.descriptorSetLayouts[i]).descriptors;
			for (unsigned int binding = 0; binding < layoutDesc.size(); binding++) {
				DSIndex idx;
				idx.set     = i;
				idx.binding = binding;
				uint32_t glIndex = 0xFFFFFFFFU;

				auto type = layoutDesc.at(binding).type;
				switch (type) {
				case DescriptorType::UniformBuffer:
					glIndex = shaderResources.ubos.size();
					shaderResources.ubos.push_back(idx);
					break;

				case DescriptorType::StorageBuffer:
					glIndex = shaderResources.ssbos.size();
					shaderResources.ssbos.push_back(idx);
					break;

				case DescriptorType::Sampler:
				case DescriptorType::Texture:
                    // gets assigned later after spirv-cross has built combined samplers
					break;

				case DescriptorType::CombinedSampler:
					glIndex         = shaderResources.textures.size();
					assert(glIndex == shaderResources.samplers.size());

					shaderResources.textures.push_back(idx);
					shaderResources.samplers.push_back(idx);
					break;

				case DescriptorType::End:
					assert(false);
					break;
				}

				dsResources.emplace(idx, ResourceInfo(type, glIndex));
			}
		}
	}

	GLuint vertexShader = 0;
	GLuint fragmentShader = 0;
	{
		spirv_cross::CompilerGLSL::Options glslOptions;
		glslOptions.vertex.fixup_clipspace = false;
		glslOptions.vertex.support_nonzero_base_instance = false;

		spirv_cross::CompilerGLSL glslVert(v.spirv);
		glslVert.set_common_options(glslOptions);
		processShaderResources(shaderResources, dsResources, glslVert);

		spirv_cross::CompilerGLSL glslFrag(f.spirv);
		glslFrag.set_common_options(glslOptions);
		processShaderResources(shaderResources, dsResources, glslFrag);

		vertexShader = createShader(GL_VERTEX_SHADER, v.name, v.macros, glslVert);
		fragmentShader = createShader(GL_FRAGMENT_SHADER, f.name, f.macros, glslFrag);
	}

	// TODO: cache shaders
	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetProgramInfoLog(program, status, NULL, &infoLog[0]);
		LOG("info log: {}", &infoLog[0]);
		logFlush();
		throw std::runtime_error("shader link failed");
	}
	glUseProgram(program);

	auto result = impl->pipelines.add();
	Pipeline &pipeline = result.first;
	pipeline.desc      = desc;
	pipeline.shader    = program;
	pipeline.srcBlend  = blendFunc(desc.sourceBlend_);
	pipeline.destBlend = blendFunc(desc.destinationBlend_);
	pipeline.resources = std::move(shaderResources);

	if (impl->tracing) {
		glObjectLabel(GL_PROGRAM, program, desc.name_.size(), desc.name_.c_str());
	}

	return result.second;
}


static const GLenum drawBuffers[MAX_COLOR_RENDERTARGETS] = {
	  GL_COLOR_ATTACHMENT0
	, GL_COLOR_ATTACHMENT1
};


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	assert(!desc.name_.empty());
	assert(desc.renderPass_);

#ifndef NDEBUG
	auto &renderPass = impl->renderPasses.get(desc.renderPass_);
#endif  // NDEBUG

	auto result = impl->framebuffers.add();
	Framebuffer &fb = result.first;
	glCreateFramebuffers(1, &fb.fbo);

	unsigned int width UNUSED = 0, height UNUSED = 0;

	unsigned int numColorAttachments = 0;
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (!desc.colors_[i]) {
			continue;
		}
		numColorAttachments++;

		const auto &colorRT = impl->renderTargets.get(desc.colors_[i]);

		if (width == 0) {
			assert(height == 0);
			width  = colorRT.width;
			height = colorRT.height;
		} else {
			assert(width  == colorRT.width);
			assert(height == colorRT.height);
		}

		assert(colorRT.width  > 0);
		assert(colorRT.height > 0);
		assert(colorRT.numSamples > 0);
		assert(colorRT.numSamples <= static_cast<unsigned int>(impl->glValues[GL_MAX_COLOR_TEXTURE_SAMPLES]));
		assert(colorRT.numSamples == renderPass.numSamples);
		assert(colorRT.texture);
		assert(colorRT.format == renderPass.desc.colorRTs_[i].format);
		fb.renderPass = desc.renderPass_;
		fb.numSamples = colorRT.numSamples;
		fb.colors[i]  = desc.colors_[i];
		if (issRGBFormat(colorRT.format)) {
			fb.sRGB   = true;
		}
		fb.width      = colorRT.width;
		fb.height     = colorRT.height;

		const auto &colorRTtex = impl->textures.get(colorRT.texture);
		assert(colorRTtex.renderTarget);
		assert(colorRTtex.tex != 0);

		glNamedFramebufferTexture(fb.fbo, GL_COLOR_ATTACHMENT0 + i, colorRTtex.tex, 0);
	}

	glNamedFramebufferDrawBuffers(fb.fbo, numColorAttachments, drawBuffers);

	if (desc.depthStencil_) {
		const auto &depthRT = impl->renderTargets.get(desc.depthStencil_);
		assert(depthRT.format == renderPass.desc.depthStencilFormat_);
		assert(depthRT.width  == width);
		assert(depthRT.height == height);
		assert(depthRT.texture);
		assert(depthRT.numSamples > 0);
		assert(depthRT.numSamples <= static_cast<unsigned int>(impl->glValues[GL_MAX_DEPTH_TEXTURE_SAMPLES]));
		assert(depthRT.numSamples == renderPass.numSamples);

		const auto &depthRTtex = impl->textures.get(depthRT.texture);
		assert(depthRTtex.renderTarget);
		assert(depthRTtex.tex != 0);
		fb.depthStencil = desc.depthStencil_;
		glNamedFramebufferTexture(fb.fbo, GL_DEPTH_ATTACHMENT, depthRTtex.tex, 0);
	} else {
		assert(renderPass.desc.depthStencilFormat_ == +Format::Invalid);
	}

	assert(impl->isRenderPassCompatible(renderPass, fb));

	GLenum status = glCheckNamedFramebufferStatus(fb.fbo, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		LOG("Framebuffer \"{}\" is not complete: {:#04x}", desc.name_, status);
		logFlush();
		throw std::runtime_error("Framebuffer is not complete");
	}

	if (impl->tracing) {
		glObjectLabel(GL_FRAMEBUFFER, fb.fbo, desc.name_.size(), desc.name_.c_str());
	}

	return result.second;
}


RenderPassHandle Renderer::createRenderPass(const RenderPassDesc &desc) {
	assert(!desc.name_.empty());

	GLbitfield clearMask = 0;
	if (desc.clearDepthAttachment) {
		clearMask |= GL_DEPTH_BUFFER_BIT;
	}

	auto result = impl->renderPasses.add();
	RenderPass &pass = result.first;
	pass.desc = desc;
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		switch (desc.colorRTs_[i].passBegin) {
		case PassBegin::DontCare:
			assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);
			break;

		case PassBegin::Keep:
			assert(desc.colorRTs_[i].initialLayout != +Layout::Undefined);
			break;

		case PassBegin::Clear:
			assert(desc.colorRTs_[i].initialLayout == +Layout::Undefined);
			pass.colorClearValues[i] = desc.colorRTs_[i].clearValue;
			break;

		}
	}
	pass.depthClearValue = desc.depthClearValue;
	pass.clearMask       = clearMask;
	pass.numSamples      = desc.numSamples_;

	return result.second;
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != +Format::Invalid);
	assert(isPow2(desc.numSamples_));
	assert(!desc.name_.empty());

	GLuint id = 0;
	GLenum target;
	if (desc.numSamples_ > 1) {
		target = GL_TEXTURE_2D_MULTISAMPLE;
		glCreateTextures(target, 1, &id);
		glTextureStorage2DMultisample(id, desc.numSamples_, glTexFormat(desc.format_), desc.width_, desc.height_, true);
	} else {
		target = GL_TEXTURE_2D;
		glCreateTextures(target, 1, &id);
		glTextureStorage2D(id, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	}
	glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, 0);
	if (impl->tracing) {
		glObjectLabel(GL_TEXTURE, id, desc.name_.size(), desc.name_.c_str());
	}

	auto textureResult = impl->textures.add();
	Texture &tex = textureResult.first;
	tex.tex           = id;
	tex.width         = desc.width_;
	tex.height        = desc.height_;
	tex.renderTarget  = true;
	tex.target        = target;
	tex.format        = desc.format_;

	auto result = impl->renderTargets.add();
	RenderTarget &rt = result.first;
	rt.width  = desc.width_;
	rt.height = desc.height_;
	rt.format = desc.format_;
	rt.numSamples = desc.numSamples_;
	// TODO: std::move?
	rt.texture = textureResult.second;

	if (desc.additionalViewFormat_ != +Format::Invalid) {
		GLuint viewId = 0;
		glGenTextures(1, &viewId);
		glTextureView(viewId, tex.target, id, glTexFormat(desc.additionalViewFormat_), 0, 1, 0, 1);

		auto viewResult   = impl->textures.add();
		Texture &view     = viewResult.first;
		view.tex          = viewId;
		view.width        = desc.width_;
		view.height       = desc.height_;
		view.renderTarget = true;
		view.target       = target;
		view.format       = desc.additionalViewFormat_;
		rt.additionalView = viewResult.second;
	}

	return result.second;
}


void RendererImpl::createRTHelperFBO(RenderTarget &rt) {
	assert(!rt.helperFBO);

	const auto &texture = textures.get(rt.texture);
	assert(texture.renderTarget);
	assert(texture.width       == rt.width);
	assert(texture.height      == rt.height);
	assert(texture.tex         != 0);
	assert(texture.target      == GL_TEXTURE_2D
	    || texture.target      == GL_TEXTURE_2D_MULTISAMPLE);

	glCreateFramebuffers(1, &rt.helperFBO);
	assert(rt.helperFBO   != 0);
	glNamedFramebufferTexture(rt.helperFBO, GL_COLOR_ATTACHMENT0, texture.tex, 0);
	glNamedFramebufferDrawBuffers(rt.helperFBO, 1, drawBuffers);
	GLenum status = glCheckNamedFramebufferStatus(rt.helperFBO, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		LOG("helper FBO for RT is not complete: {:#04x}", status);
		logFlush();
		throw std::runtime_error("helper FBO for RT is not complete");
	}
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	auto result = impl->samplers.add();
	Sampler &sampler = result.first;
	glCreateSamplers(1, &sampler.sampler);

	glSamplerParameteri(sampler.sampler, GL_TEXTURE_MIN_FILTER, (desc.min == +FilterMode::Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_MAG_FILTER, (desc.mag == +FilterMode::Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_WRAP_S,     (desc.wrapMode == +WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_WRAP_T,     (desc.wrapMode == +WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

	if (impl->tracing) {
		glObjectLabel(GL_SAMPLER, sampler.sampler, desc.name_.size(), desc.name_.c_str());
	}

	return result.second;
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	GLuint texture = 0;
	GLenum target = GL_TEXTURE_2D;
	glCreateTextures(target, 1, &texture);
	glTextureStorage2D(texture, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, desc.numMips_ - 1);
	unsigned int w = desc.width_, h = desc.height_;

	for (unsigned int i = 0; i < desc.numMips_; i++) {
		assert(desc.mipData_[i].data != nullptr);
		assert(desc.mipData_[i].size != 0);
		glTextureSubImage2D(texture, i, 0, 0, w, h, glTexBaseFormat(desc.format_), GL_UNSIGNED_BYTE, desc.mipData_[i].data);

		w = std::max(w / 2, 1u);
		h = std::max(h / 2, 1u);
	}

	auto result  = impl->textures.add();
	Texture &tex = result.first;
	tex.tex    = texture;
	tex.width  = desc.width_;
	tex.height = desc.height_;
	tex.target = target;
	tex.format = desc.format_;
	assert(!tex.renderTarget);

	if (impl->tracing) {
		glObjectLabel(GL_TEXTURE, texture, desc.name_.size(), desc.name_.c_str());
	}

	return result.second;
}


DSLayoutHandle Renderer::createDescriptorSetLayout(const DescriptorLayout *layout) {
	auto result = impl->dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;

	while (layout->type != +DescriptorType::End) {
		dsLayout.descriptors.push_back(*layout);
		layout++;
	}
	assert(layout->offset == 0);

	return result.second;
}


TextureHandle Renderer::getRenderTargetView(RenderTargetHandle handle, Format f) {
	const auto &rt = impl->renderTargets.get(handle);

	TextureHandle result;
	if (f == rt.format) {
		result = rt.texture;

#ifndef NDEBUG
		const auto &tex = impl->textures.get(result);
		assert(tex.renderTarget);
#endif //  NDEBUG
	} else {
		result = rt.additionalView;

#ifndef NDEBUG
		const auto &tex = impl->textures.get(result);
		assert(tex.renderTarget);
		assert(tex.format == f);
#endif //  NDEBUG
	}

	return result;
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->buffers.removeWith(handle, [](struct Buffer &b) {
		assert(b.buffer != 0);
		glDeleteBuffers(1, &b.buffer);
		b.buffer = 0;

		assert(b.size != 0);
		b.size   = 0;

		assert(!b.ringBufferAlloc);
		assert(b.type != +BufferType::Invalid);
		b.type   = BufferType::Invalid;
	} );
}


void Renderer::deleteFramebuffer(FramebufferHandle handle) {
	impl->framebuffers.removeWith(handle, [](Framebuffer &fb) {
		assert(fb.fbo != 0);
		assert(fb.numSamples > 0);
		glDeleteFramebuffers(1, &fb.fbo);
		fb.fbo = 0;
		fb.numSamples = 0;
	} );
}


void Renderer::deletePipeline(PipelineHandle handle) {
	impl->pipelines.removeWith(handle, [](Pipeline &p) {
		assert(p.shader != 0);
		glDeleteProgram(p.shader);
		p.shader = 0;
	} );
}


void Renderer::deleteRenderPass(RenderPassHandle handle) {
	impl->renderPasses.removeWith(handle, [](RenderPass &) {
	} );
}


void Renderer::deleteRenderTarget(RenderTargetHandle &handle) {
	impl->renderTargets.removeWith(handle, [this](RenderTarget &rt) {
		assert(rt.texture);
		assert(rt.numSamples > 0);

		rt.numSamples = 0;
		if (rt.helperFBO != 0) {
			glDeleteFramebuffers(1, &rt.helperFBO);
			rt.helperFBO = 0;
		}

		{
			auto &tex = impl->textures.get(rt.texture);
			assert(tex.renderTarget);
			assert(tex.target != GL_NONE);
			tex.renderTarget = false;
			assert(tex.tex != 0);
			glDeleteTextures(1, &tex.tex);
			tex.tex = 0;
			tex.target = GL_NONE;
			tex.format = Format::Invalid;
		}
		impl->textures.remove(rt.texture);
		rt.texture = TextureHandle();

		if (rt.additionalView) {
			auto &view = impl->textures.get(rt.additionalView);
			assert(view.renderTarget);
			assert(view.target != GL_NONE);
			view.renderTarget = false;
			assert(view.tex != 0);
			glDeleteTextures(1, &view.tex);
			view.tex = 0;
			view.target = GL_NONE;
			view.format = Format::Invalid;
			impl->textures.remove(rt.additionalView);
			rt.additionalView = TextureHandle();
		}
	} );
}


void Renderer::deleteSampler(SamplerHandle handle) {
	impl->samplers.removeWith(handle, [](Sampler &sampler) {
		assert(sampler.sampler != 0);

		glDeleteSamplers(1, &sampler.sampler);
		sampler.sampler = 0;
	} );
}


void Renderer::deleteTexture(TextureHandle handle) {
	impl->textures.removeWith(handle, [](Texture &tex) {
		assert(!tex.renderTarget);
		assert(tex.tex != 0);
		assert(tex.target != GL_NONE);

		glDeleteTextures(1, &tex.tex);
		tex.tex = 0;
		tex.target = GL_NONE;
		tex.format = Format::Invalid;
	} );
}


void Renderer::setSwapchainDesc(const SwapchainDesc &desc) {
	bool changed = false;

	if (impl->swapchainDesc.fullscreen != desc.fullscreen) {
		changed = true;
		if (desc.fullscreen) {
			// TODO: check return val?
			SDL_SetWindowFullscreen(impl->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			LOG("Fullscreen");
		} else {
			SDL_SetWindowFullscreen(impl->window, 0);
			LOG("Windowed");
		}
	}

	if (impl->swapchainDesc.vsync != desc.vsync) {
		changed = true;
		int retval = 0;
		switch (desc.vsync) {
		case VSync::LateSwapTear:
			// enable vsync, using late swap tearing if possible
			retval = SDL_GL_SetSwapInterval(-1);
			if (retval == 0) {
				LOG("VSync is on");
			} else {
				break;
			}
			// fallthrough

		case VSync::On:
			// TODO: check return val
			SDL_GL_SetSwapInterval(1);
			LOG("VSync is on");
			break;

		case VSync::Off:
			// TODO: check return val
			SDL_GL_SetSwapInterval(0);
			LOG("VSync is off");
			break;
		}
	}

	if (impl->swapchainDesc.numFrames != desc.numFrames) {
		changed = true;
	}

	if (impl->swapchainDesc.width     != desc.width) {
		changed = true;
	}

	if (impl->swapchainDesc.height    != desc.height) {
		changed = true;
	}

	if (changed) {
		impl->wantedSwapchain = desc;
		impl->swapchainDirty  = true;
	}
}


glm::uvec2 Renderer::getDrawableSize() const {
	int w = -1, h = -1;
	SDL_GL_GetDrawableSize(impl->window, &w, &h);
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	return glm::uvec2(w, h);
}


bool RendererImpl::recreateSwapchain() {
	assert(swapchainDirty);

	int w = -1, h = -1;
	SDL_GL_GetDrawableSize(window, &w, &h);
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("drawable size is negative");
	}

	swapchainDesc.width  = w;
	swapchainDesc.height = h;

	unsigned int numImages = wantedSwapchain.numFrames;
	numImages = std::max(numImages, 1U);

	LOG("Want {} images, using {} images", wantedSwapchain.numFrames, numImages);

	swapchainDesc.fullscreen = wantedSwapchain.fullscreen;
	swapchainDesc.numFrames  = numImages;
	swapchainDesc.vsync      = wantedSwapchain.vsync;

	if (frames.size() != numImages) {
		if (numImages < frames.size()) {
			waitForDeviceIdle();

			// decreasing, delete old and resize
			for (unsigned int i = numImages; i < frames.size(); i++) {
				auto &f = frames.at(i);
				assert(!f.outstanding);

				// delete contents of Frame
				deleteFrameInternal(f);
			}
			frames.resize(numImages);
		} else {
			// increasing, resize and initialize new
			frames.resize(numImages);

			// TODO: put some stuff here
		}
	}

	swapchainDirty = false;

	return true;
}


MemoryStats Renderer::getMemStats() const {
	MemoryStats stats;
	return stats;
}


void RendererImpl::waitForDeviceIdle() {
	for (unsigned int i = 0; i < frames.size(); i++) {
		auto &f = frames.at(i);
		if (f.outstanding) {
			// try to wait
			waitForFrame(i);
			assert(!f.outstanding);
		}
	}
}


bool Renderer::beginFrame() {
#ifndef NDEBUG
	assert(!impl->inFrame);
#endif //  NDEBUG

	if (impl->swapchainDirty) {
		// return false when recreateSwapchain fails and let caller deal with it
		if (!impl->recreateSwapchain()) {
			assert(impl->swapchainDirty);
			return false;
		}
		assert(!impl->swapchainDirty);
	}

	impl->currentFrameIdx        = impl->frameNum % impl->frames.size();
	assert(impl->currentFrameIdx < impl->frames.size());
	auto &frame                  = impl->frames.at(impl->currentFrameIdx);

	// frames are a ringbuffer
	// if the frame we want to reuse is still pending on the GPU, wait for it
	if (frame.outstanding) {
		impl->waitForFrame(impl->currentFrameIdx);
	}
	assert(!frame.outstanding);

#ifndef NDEBUG

	impl->inFrame       = true;
	impl->inRenderPass  = false;
	impl->validPipeline = false;
	impl->pipelineDrawn = true;

#endif //  NDEBUG

	impl->currentPipeline        = PipelineHandle();
	impl->descriptors.clear();

	// TODO: reset all relevant state in case some 3rd-party program fucked them up
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthMask(GL_TRUE);

	if (impl->features.sRGBFramebuffer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	} else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// TODO: only clear depth/stencil if we have it
	// TODO: set color/etc write masks if necessary
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	return true;
}


void Renderer::presentFrame(RenderTargetHandle image) {
#ifndef NDEBUG
	assert(impl->inFrame);
	impl->inFrame = false;
#endif //  NDEBUG

	auto &frame = impl->frames.at(impl->currentFrameIdx);

	auto &rt = impl->renderTargets.get(image);
	assert(rt.currentLayout == +Layout::TransferSrc);

	unsigned int width  = rt.width;
	unsigned int height = rt.height;

	// TODO: only if enabled
	glDisable(GL_SCISSOR_TEST);
	if (impl->features.sRGBFramebuffer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	} else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}


	// TODO: necessary? should do linear blit?
	assert(width  == impl->swapchainDesc.width);
	assert(height == impl->swapchainDesc.height);

	assert(width > 0);
	assert(height > 0);

	if (rt.helperFBO == 0) {
		impl->createRTHelperFBO(rt);
	}
	assert(rt.helperFBO != 0);

	glBlitNamedFramebuffer(rt.helperFBO, 0
	                     , 0, 0, width, height
	                     , 0, 0, width, height
	                     , GL_COLOR_BUFFER_BIT, GL_NEAREST);

	SDL_GL_SwapWindow(impl->window);

	frame.fence        = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	frame.usedRingBufPtr = impl->ringBufPtr;
	frame.outstanding  = true;
	frame.lastFrameNum = impl->frameNum;

	impl->frameNum++;
}


void RendererImpl::waitForFrame(unsigned int frameIdx) {
	assert(frameIdx < frames.size());

	Frame &frame = frames.at(frameIdx);
	assert(frame.outstanding);

	// wait for the fence
	assert(frame.fence);
	GLenum result = glClientWaitSync(frame.fence, GL_SYNC_FLUSH_COMMANDS_BIT, frameTimeoutNanos);
	switch (result) {
	case GL_ALREADY_SIGNALED:
	case GL_CONDITION_SATISFIED:
		// nothing
		break;

	default:
		// TODO: do something better
		LOG("glClientWaitSync failed: {:#04x}", result);
		throw std::runtime_error("glClientWaitSync failed");
	}

	glDeleteSync(frame.fence);
	frame.fence = nullptr;

	for (auto handle : frame.ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		if (buffer.ringBufferAlloc) {
			buffer.buffer          = 0;
			buffer.ringBufferAlloc = false;
		} else {
			glDeleteBuffers(1, &buffer.buffer);
			buffer.buffer = 0;
		}

		assert(buffer.size   >  0);
		buffer.size = 0;
		buffer.offset = 0;
		assert(buffer.type != +BufferType::Invalid);
		buffer.type   = BufferType::Invalid;

		buffers.remove(handle);
	}
	frame.ephemeralBuffers.clear();
	frame.outstanding = false;
	lastSyncedFrame = std::max(lastSyncedFrame, frame.lastFrameNum);
	lastSyncedRingBufPtr = std::max(lastSyncedRingBufPtr, frame.usedRingBufPtr);
}


void RendererImpl::deleteFrameInternal(Frame &f UNUSED) {
	assert(!f.outstanding);
}


void Renderer::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(!impl->inRenderPass);
	impl->inRenderPass  = true;
	impl->validPipeline = false;
#endif //  NDEBUG

	assert(fbHandle);
	const auto &fb = impl->framebuffers.get(fbHandle);
	assert(fb.fbo != 0);

	assert(rpHandle);
	const auto &rp = impl->renderPasses.get(rpHandle);

	if (impl->tracing) {
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, -1, rp.desc.name_.c_str());
	}

	// make sure renderpass and framebuffer match
	// OpenGL doesn't care but Vulkan does
	assert(fb.renderPass == rpHandle || impl->isRenderPassCompatible(rp, fb));

	assert(fb.fbo != 0);
	assert(fb.width > 0);
	assert(fb.height > 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
	if (fb.sRGB) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	} else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

	if (fb.numSamples > 1) {
		glEnable(GL_MULTISAMPLE);
	} else {
		glDisable(GL_MULTISAMPLE);
	}

	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (rp.desc.colorRTs_[i].passBegin == +PassBegin::Clear) {
			glClearBufferfv(GL_COLOR, i, glm::value_ptr(rp.desc.colorRTs_[i].clearValue));
		}

	}

	if (rp.clearMask) {
		// TODO: stencil
		if ((rp.clearMask & GL_DEPTH_BUFFER_BIT) != 0) {
			glClearBufferfv(GL_DEPTH, 0, &rp.depthClearValue);
		}
	}

	impl->currentRenderPass  = rpHandle;
	impl->currentFramebuffer = fbHandle;
}


void Renderer::endRenderPass() {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(impl->inRenderPass);
	impl->inRenderPass = false;
#endif  // NDEBUG

	if (impl->tracing) {
		glPopDebugGroup();
	}

	const auto &pass = impl->renderPasses.get(impl->currentRenderPass);
	const auto &fb   = impl->framebuffers.get(impl->currentFramebuffer);

	// TODO: track depthstencil layout too
	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (fb.colors[i]) {
			auto &rt = impl->renderTargets.get(fb.colors[i]);
			rt.currentLayout = pass.desc.colorRTs_[i].finalLayout;
		}
	}

	impl->currentRenderPass  = RenderPassHandle();
	impl->currentFramebuffer = FramebufferHandle();
}


void Renderer::layoutTransition(RenderTargetHandle image, Layout src UNUSED, Layout dest) {
	assert(image);
	assert(dest != +Layout::Undefined);
	assert(src != dest);

	auto &rt = impl->renderTargets.get(image);
	assert(src == +Layout::Undefined || rt.currentLayout == src);
	rt.currentLayout = dest;
}


void Renderer::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(impl->inFrame);
	glViewport(x, y, width, height);
}


void Renderer::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
#ifndef NDEBUG
	assert(impl->validPipeline);

	const auto &p = impl->pipelines.get(impl->currentPipeline);
	assert(p.desc.scissorTest_);
	impl->scissorSet = true;
#endif  // NDEBUG

	// flip y from Vulkan convention to OpenGL convention
	// TODO: should use current FB height
	glScissor(x, impl->swapchainDesc.height - (y + height), width, height);
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
#ifndef NDEBUG
	assert(impl->inFrame);
	assert(pipeline);
	assert(impl->inRenderPass);
	assert(impl->pipelineDrawn);
	impl->pipelineDrawn = false;
	impl->validPipeline = true;
	impl->scissorSet = false;
#endif  // NDEBUG

	impl->decriptorSetsDirty = true;

	const auto &p = impl->pipelines.get(pipeline);

	// TODO: shadow state, set only necessary
	glUseProgram(p.shader);
	if (p.desc.depthWrite_) {
		glDepthMask(GL_TRUE);
	} else {
		glDepthMask(GL_FALSE);
	}

	if (p.desc.depthTest_) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}

	if (p.desc.cullFaces_) {
		glEnable(GL_CULL_FACE);
	} else {
		glDisable(GL_CULL_FACE);
	}

	if (p.desc.scissorTest_) {
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}

	if (p.desc.blending_) {
		glEnable(GL_BLEND);
		// TODO: get from Pipeline
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(p.srcBlend, p.destBlend);
		if (p.srcBlend == GL_CONSTANT_ALPHA || p.destBlend == GL_CONSTANT_ALPHA) {
			// TODO: get from Pipeline
			glBlendColor(0.5f, 0.5f, 0.5f, 0.5f);
		}
	} else {
		glDisable(GL_BLEND);
	}

	uint32_t oldMask = impl->currentPipeline ? (impl->pipelines.get(impl->currentPipeline).desc.vertexAttribMask) : 0;
	uint32_t newMask = p.desc.vertexAttribMask;

	// enable/disable changed attributes
	uint32_t vattrChanged = oldMask ^ newMask;
	forEachSetBit(vattrChanged, [newMask] (uint32_t bit, uint32_t mask) {
		if (newMask & mask) {
			glEnableVertexAttribArray(bit);
		} else {
			glDisableVertexAttribArray(bit);
		}
	});

	// set format on new attributes
	const auto &attribs = p.desc.vertexAttribs;
	forEachSetBit(newMask, [attribs] (uint32_t bit, uint32_t /* mask */ ) {
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
	});

	impl->currentPipeline = pipeline;
}


void Renderer::bindIndexBuffer(BufferHandle handle, bool bit16) {
	assert(impl->inFrame);
	assert(impl->validPipeline);

	const Buffer &buffer = impl->buffers.get(handle);
	assert(buffer.size > 0);
	assert(buffer.type == +BufferType::Index);
	if (buffer.ringBufferAlloc) {
		assert(buffer.buffer == impl->ringBuffer);
		assert(buffer.offset + buffer.size < impl->ringBufSize);
	} else {
		assert(buffer.buffer != 0);
		assert(buffer.offset == 0);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.buffer);
	impl->indexBufByteOffset = buffer.offset;
	impl->idxBuf16Bit        = bit16;
}


void Renderer::bindVertexBuffer(unsigned int binding, BufferHandle handle) {
	assert(impl->inFrame);
	assert(impl->validPipeline);

	const Buffer &buffer = impl->buffers.get(handle);
	assert(buffer.size >  0);
	assert(buffer.type == +BufferType::Vertex);
	if (buffer.ringBufferAlloc) {
		// this is not strictly correct since we might have reallocated the ringbuf bigger
		// but it should never fail, at worst it will not spot some errors immediately after realloc
		// which are rare events anyway
		assert(buffer.offset + buffer.size < impl->ringBufSize);
	} else {
		assert(buffer.buffer != 0);
		assert(buffer.offset == 0);
	}
	const auto &p = impl->pipelines.get(impl->currentPipeline);
	glBindVertexBuffer(binding, buffer.buffer, buffer.offset, p.desc.vertexBuffers[binding].stride);
}


void Renderer::bindDescriptorSet(unsigned int index, DSLayoutHandle layoutHandle, const void *data_) {
#ifndef NDEBUG
	assert(impl->validPipeline);
	const auto &p = impl->pipelines.get(impl->currentPipeline);
	assert(p.desc.descriptorSetLayouts[index] == layoutHandle);
#endif  // NDEBUG

	impl->decriptorSetsDirty = true;

	// TODO: get shader bindings from current pipeline, use index
	const DescriptorSetLayout &layout = impl->dsLayouts.get(layoutHandle);

	const char *data = reinterpret_cast<const char *>(data_);
	unsigned int descIndex = 0;
	for (const auto &l : layout.descriptors) {
		DSIndex idx;
		idx.set     = index;
		idx.binding = descIndex;

		switch (l.type) {
		case DescriptorType::End:
			// can't happen because createDesciptorSetLayout doesn't let it
			HEDLEY_UNREACHABLE();
			break;

		case DescriptorType::UniformBuffer: {
			// this is part of the struct, we know it's correctly aligned and right type
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			const Buffer &buffer = impl->buffers.get(handle);
			assert(buffer.size > 0);
			assert(buffer.type == +BufferType::Uniform);
			if (buffer.ringBufferAlloc) {
				assert(buffer.buffer == impl->ringBuffer);
				assert(buffer.offset + buffer.size < impl->ringBufSize);
			} else {
				assert(buffer.buffer != 0);
				assert(buffer.offset == 0);
			}
			impl->descriptors[idx] = handle;
		} break;

		case DescriptorType::StorageBuffer: {
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			const Buffer &buffer = impl->buffers.get(handle);
			assert(buffer.size  > 0);
			assert(buffer.type == +BufferType::Storage);
			if (buffer.ringBufferAlloc) {
				assert(buffer.buffer == impl->ringBuffer);
				assert(buffer.offset + buffer.size < impl->ringBufSize);
			} else {
				assert(buffer.buffer != 0);
				assert(buffer.offset == 0);
			}
			impl->descriptors[idx] = handle;
		} break;

		case DescriptorType::Sampler: {
			SamplerHandle handle = *reinterpret_cast<const SamplerHandle *>(data + l.offset);

#ifndef NDEBUG
			const auto &sampler = impl->samplers.get(handle);
			assert(sampler.sampler);
#endif  // NDEBUG

			impl->descriptors[idx] = handle;
		} break;

		case DescriptorType::Texture: {
			TextureHandle texHandle = *reinterpret_cast<const TextureHandle *>(data + l.offset);
			impl->descriptors[idx] = texHandle;
		} break;

		case DescriptorType::CombinedSampler: {
			const CSampler &combined = *reinterpret_cast<const CSampler *>(data + l.offset);

#ifndef NDEBUG
			const Texture &tex = impl->textures.get(combined.tex);
			assert(tex.tex);

			const auto &sampler = impl->samplers.get(combined.sampler);
			assert(sampler.sampler);
#endif  // NDEBUG

			impl->descriptors[idx] = combined;
		} break;

		}

		descIndex++;
	}
}


bool RendererImpl::isRenderPassCompatible(const RenderPass &pass, const Framebuffer &fb) {
	if (pass.numSamples != fb.numSamples) {
		return false;
	}

	if (fb.depthStencil) {
		const auto &depthRT = renderTargets.get(fb.depthStencil);

		if (pass.desc.depthStencilFormat_ != depthRT.format) {
			return false;
		}
	} else {
		if (pass.desc.depthStencilFormat_ != +Format::Invalid) {
			return false;
		}
	}

	for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
		if (fb.colors[i]) {
			const auto &colorRT = renderTargets.get(fb.colors[i]);

			if (pass.desc.colorRTs_[i].format != colorRT.format) {
				return false;
			}
		} else {
			if (pass.desc.colorRTs_[i].format != +Format::Invalid) {
				return false;
			}
		}
	}

	return true;
}


void RendererImpl::rebindDescriptorSets() {
	assert(decriptorSetsDirty);

	const auto &pipeline  = pipelines.get(currentPipeline);
	const auto &resources = pipeline.resources;

	// TODO: only change what is necessary
	for (unsigned int i = 0; i < resources.ubos.size(); i++) {
		const auto &r = resources.ubos.at(i);
		const auto &d = descriptors.at(r);
		const Buffer &buffer = buffers.get(boost::get<BufferHandle>(d));
		assert(resources.uboSizes[i] <= buffer.size);
		glBindBufferRange(GL_UNIFORM_BUFFER, i, buffer.buffer, buffer.offset, buffer.size);
	}

	for (unsigned int i = 0; i < resources.ssbos.size(); i++) {
		const auto &r = resources.ssbos.at(i);
		const auto &d = descriptors.at(r);
		const Buffer &buffer = buffers.get(boost::get<BufferHandle>(d));
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, buffer.buffer, buffer.offset, buffer.size);
	}

	for (unsigned int i = 0; i < resources.textures.size(); i++) {
		const auto &r = resources.textures.at(i);
		const auto &d = descriptors.at(r);

		// TODO: find a better way than magic numbers
		// std::variant has holds_alternative
		switch (d.which()) {
		case 1: {
			const CSampler &combined = boost::get<CSampler>(d);
			const Texture &tex  = textures.get(combined.tex);
			glBindTextureUnit(i, tex.tex);
		} break;

		case 3: {
			const TextureHandle &handle = boost::get<TextureHandle>(d);
			const Texture &tex  = textures.get(handle);
			glBindTextureUnit(i, tex.tex);
		} break;

		default:
			HEDLEY_UNREACHABLE();
			break;
		}
	}

	for (unsigned int i = 0; i < resources.samplers.size(); i++) {
		const auto &r = resources.samplers.at(i);
		const auto &d = descriptors.at(r);
		// TODO: find a better way than magic numbers
		// std::variant has holds_alternative
		switch (d.which()) {
		case 1: {
			const CSampler &combined = boost::get<CSampler>(d);
			const auto &sampler = samplers.get(combined.sampler);
			glBindSampler(i, sampler.sampler);
		} break;

		case 2: {
			const SamplerHandle &handle = boost::get<SamplerHandle>(d);
			const auto &sampler = samplers.get(handle);
			glBindSampler(i, sampler.sampler);
		} break;

		default:
			HEDLEY_UNREACHABLE();
			break;
		}
	}

	decriptorSetsDirty = false;
}


void Renderer::blit(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);

	// TODO: check they're both color targets
	// or implement depth blit

	auto &srcRT = impl->renderTargets.get(source);
	assert(srcRT.numSamples  == 1);
	assert(srcRT.width       >  0);
	assert(srcRT.height      >  0);
	assert(srcRT.currentLayout == +Layout::TransferSrc);
	assert(srcRT.texture);
	if (srcRT.helperFBO == 0) {
		impl->createRTHelperFBO(srcRT);
	}
	assert(srcRT.helperFBO != 0);

	auto &destRT = impl->renderTargets.get(target);
	assert(destRT.numSamples == 1);
	assert(destRT.width      >  0);
	assert(destRT.height     >  0);
	assert(destRT.currentLayout == +Layout::TransferDst);
	assert(destRT.texture);
	if (destRT.helperFBO == 0) {
		impl->createRTHelperFBO(destRT);
	}
	assert(destRT.helperFBO != 0);

	assert(srcRT.helperFBO   != destRT.helperFBO);
	assert(srcRT.width       == destRT.width);
	assert(srcRT.height      == destRT.height);

	glBlitNamedFramebuffer(srcRT.helperFBO, destRT.helperFBO
	                     , 0, 0, srcRT.width, srcRT.height
	                     , 0, 0, destRT.width, destRT.height
	                     , GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


void Renderer::resolveMSAA(RenderTargetHandle source, RenderTargetHandle target) {
	assert(source);
	assert(target);

	assert(!impl->inRenderPass);

	// TODO: check they're both color targets

	auto &srcRT = impl->renderTargets.get(source);
	assert(srcRT.numSamples  >  1);
	assert(srcRT.width       >  0);
	assert(srcRT.height      >  0);
	assert(srcRT.currentLayout == +Layout::TransferSrc);
	assert(srcRT.texture);
	if (srcRT.helperFBO == 0) {
		impl->createRTHelperFBO(srcRT);
	}
	assert(srcRT.helperFBO != 0);

	auto &destRT = impl->renderTargets.get(target);
	assert(destRT.numSamples == 1);
	assert(destRT.width      >  0);
	assert(destRT.height     >  0);
	assert(destRT.currentLayout == +Layout::TransferDst);
	assert(destRT.texture);
	if (destRT.helperFBO == 0) {
		impl->createRTHelperFBO(destRT);
	}
	assert(destRT.helperFBO != 0);

	assert(srcRT.helperFBO   != destRT.helperFBO);
	assert(srcRT.width       == destRT.width);
	assert(srcRT.height      == destRT.height);

	glBlitNamedFramebuffer(srcRT.helperFBO, destRT.helperFBO
	                     , 0, 0, srcRT.width, srcRT.height
	                     , 0, 0, destRT.width, destRT.height
	                     , GL_COLOR_BUFFER_BIT, GL_LINEAR);
}


void Renderer::draw(unsigned int firstVertex, unsigned int vertexCount) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	const auto &p = impl->pipelines.get(impl->currentPipeline);
	assert(!p.desc.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
#endif //  NDEBUG

	if (impl->decriptorSetsDirty) {
		impl->rebindDescriptorSets();
	}
	assert(!impl->decriptorSetsDirty);

	// TODO: get primitive from current pipeline
	glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(instanceCount > 0);
	assert(vertexCount > 0);
	const auto &p = impl->pipelines.get(impl->currentPipeline);
	assert(!p.desc.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
#endif //  NDEBUG

	if (impl->decriptorSetsDirty) {
		impl->rebindDescriptorSets();
	}
	assert(!impl->decriptorSetsDirty);

	// TODO: get primitive from current pipeline
	GLenum format = impl->idxBuf16Bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT ;
	auto ptr = reinterpret_cast<const void *>(impl->indexBufByteOffset);
	if (instanceCount == 1) {
		glDrawElements(GL_TRIANGLES, vertexCount, format, ptr);
	} else {
		glDrawElementsInstanced(GL_TRIANGLES, vertexCount, format, ptr, instanceCount);
	}
}


void Renderer::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int minIndex, unsigned int maxIndex) {
#ifndef NDEBUG
	assert(impl->inRenderPass);
	assert(impl->validPipeline);
	assert(vertexCount > 0);
	const auto &p = impl->pipelines.get(impl->currentPipeline);
	assert(!p.desc.scissorTest_ || impl->scissorSet);
	impl->pipelineDrawn = true;
#endif //  NDEBUG

	if (impl->decriptorSetsDirty) {
		impl->rebindDescriptorSets();
	}
	assert(!impl->decriptorSetsDirty);

	GLenum format        = impl->idxBuf16Bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	unsigned int idxSize = impl->idxBuf16Bit ? 2                 : 4 ;
	auto ptr = reinterpret_cast<const char *>(firstIndex * idxSize + impl->indexBufByteOffset);
	// TODO: get primitive from current pipeline
	glDrawRangeElements(GL_TRIANGLES, minIndex, maxIndex, vertexCount, format, ptr);
}


} // namespace renderer


#endif //  RENDERER_OPENGL
