/*
Copyright (c) 2015 Alternative Games Ltd / Turo Lamminen

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


#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL.h>


#ifdef USE_GLEW

#include <GL/glew.h>

#else  // USE_GLEW

#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl3.h>
#include <GLES3/gl2ext.h>

#endif  // USE_GLEW


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef EMSCRIPTEN
#include <tclap/CmdLine.h>
#endif  // EMSCRIPTEN

#include "AreaTex.h"
#include "SearchTex.h"


#define ATTR_POS   0
#define ATTR_COLOR   1
#define ATTR_CUBEPOS 2
#define ATTR_ROT     3


#define TEXUNIT_TEMP 0
#define TEXUNIT_COLOR 1
#define TEXUNIT_AREATEX 2
#define TEXUNIT_SEARCHTEX 3
#define TEXUNIT_EDGES 4
#define TEXUNIT_BLEND 5


extern "C" {


#ifndef USE_GLEW


#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif  // GLAPIENTRY


// prototypes so the code compiles
// these are not supposed to be called so they're not defined
void GLAPIENTRY glBindFragDataLocation(GLuint program,  GLuint colorNumber, const char *name);
void GLAPIENTRY glBindMultiTextureEXT(GLenum texunit, GLuint texture, GLenum target);
void GLAPIENTRY glTextureParameteriEXT(GLuint texture, GLuint target, GLenum pname, GLint param);
void GLAPIENTRY glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void GLAPIENTRY glTextureSubImage2DEXT(GLuint texture, GLuint target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);


#define glBindTextureUnit glBindTextureUnitEmulated
#define glCreateBuffers glCreateBuffersEmulated
#define glCreateSamplers glCreateSamplersEmulated
#define glCreateTextures glCreateTexturesEmulated
#define glCreateVertexArrays glCreateVertexArraysEmulated
#define glEnableVertexArrayAttrib glEnableVertexArrayAttribEmulated
#define glNamedBufferData glNamedBufferDataEmulated
#define glNamedBufferSubData glNamedBufferSubDataEmulated
#define glNamedFramebufferTexture glNamedFramebufferTextureEmulated
#define glTextureParameteri glTextureParameteriEmulated
#define glTextureStorage2D glTextureStorage2DEmulated
#define glTextureSubImage2D glTextureSubImage2DEmulated
#define glVertexArrayElementBuffer glVertexArrayElementBufferEmulated


#endif  // USE_GLEW


void GLAPIENTRY glBindTextureUnitEXTEmulated(GLuint unit, GLuint texture) {
	glBindMultiTextureEXT(GL_TEXTURE0 + unit, GL_TEXTURE_2D, texture);
}


void GLAPIENTRY glBindTextureUnitEmulated(GLuint unit, GLuint texture) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, texture);
}


void GLAPIENTRY glCreateTexturesEXTEmulated(GLenum /* target */, GLsizei n, GLuint *textures) {
	glGenTextures(n, textures);
	for (GLsizei i = 0; i < n; i++) {
		glBindTextureUnit(TEXUNIT_TEMP, textures[i]);
	}
}


void GLAPIENTRY glCreateTexturesEmulated(GLenum target, GLsizei n, GLuint *textures) {
	glActiveTexture(GL_TEXTURE0 + TEXUNIT_TEMP);
	glGenTextures(n, textures);
	for (GLsizei i = 0; i < n; i++) {
		glBindTexture(target, textures[i]);
	}
}


void GLAPIENTRY glTextureStorage2DEXTEmulated(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
	glTextureStorage2DEXT(texture, GL_TEXTURE_2D, levels, internalformat, width, height);
}


void GLAPIENTRY glTextureSubImage2DEXTEmulated(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
	glTextureSubImage2DEXT(texture, GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, type, pixels);
}


void GLAPIENTRY glTextureParameteriEXTEmulated(GLuint texture, GLenum pname, GLint param) {
	glTextureParameteriEXT(texture, GL_TEXTURE_2D, pname, param);
}


void GLAPIENTRY glTextureStorage2DEmulated(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
	const GLenum target = GL_TEXTURE_2D;
	glActiveTexture(GL_TEXTURE0 + TEXUNIT_TEMP);
	glBindTexture(target, texture);
	GLenum format = GL_NONE;
	switch (internalformat) {
	case GL_RG8:
		format = GL_RG;
		break;

	case GL_R8:
		format = GL_RED;
		break;

	case GL_RGB8:
		format = GL_RGB;
		break;

	case GL_RGBA8:
		format = GL_RGBA;
		break;

	case GL_DEPTH_COMPONENT16:
		format =  GL_DEPTH_COMPONENT;
		break;

	default:
		assert(false);  // No matching internalformat
		break;
	}
	for (GLsizei i = 0; i < levels; i++) {
		glTexImage2D(target, i, internalformat, width, height, 0, format, GL_BYTE, NULL);
		width = std::max(1.0, floor(width / 2));
		height = std::max(1.0, floor(height / 2));
	}
}


void GLAPIENTRY glTextureSubImage2DEmulated(GLuint texture, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {
	const GLenum target = GL_TEXTURE_2D;
	glActiveTexture(GL_TEXTURE0 + TEXUNIT_TEMP);
	glBindTexture(target, texture);
	glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}


void GLAPIENTRY glTextureParameteriEmulated(GLuint texture, GLenum pname, GLint param) {
	const GLenum target = GL_TEXTURE_2D;
	glActiveTexture(GL_TEXTURE0 + TEXUNIT_TEMP);
	glBindTexture(target, texture);
	glTexParameteri(target, pname, param);
}


void GLAPIENTRY glNamedFramebufferTextureEmulated(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, level);
}


void GLAPIENTRY glCreateSamplersEmulated(GLsizei n, GLuint *samplers) {
	glGenSamplers(n, samplers);
	for (GLsizei i = 0; i < n; i++) {
		glBindSampler(TEXUNIT_TEMP, samplers[i]);
	}
}


void GLAPIENTRY glCreateBuffersEmulated(GLsizei n, GLuint *buffers) {
	glGenBuffers(n, buffers);
	for (GLsizei i = 0; i < n; i++) {
		glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
	}
}


void GLAPIENTRY glNamedBufferDataEmulated(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage) {
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, size, data, usage);	// TODO: better wrapper so we can set "target" parameter
}


void GLAPIENTRY glCreateVertexArraysEmulated(GLsizei n, GLuint *vaos) {
	glGenVertexArrays(n, vaos);
	for (GLsizei i = 0; i < n; i++) {
		glBindVertexArray(vaos[i]);
	}
}

void GLAPIENTRY glVertexArrayElementBufferEmulated(GLuint vao, GLuint ibo) {
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}


void GLAPIENTRY glEnableVertexArrayAttribEmulated(GLuint vao, GLuint index) {
	glBindVertexArray(vao);
	glEnableVertexAttribArray(index);
}


void GLAPIENTRY glNamedBufferSubDataEmulated(GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid *data) {
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}


}  // extern "C"


#ifdef EMSCRIPTEN


#include <emscripten.h>


// a collection of emscripten hacks to avoid polluting main code with them


#define SDL_WINDOW_FULLSCREEN_DESKTOP SDL_WINDOW_FULLSCREEN


Uint64 SDL_GetPerformanceFrequency() {
	return 1000;
}


Uint64 SDL_GetPerformanceCounter() {
	return SDL_GetTicks();
}


struct SDL_Window {
};


SDL_Window* SDL_CreateWindow(const char* /* title */,
                             int         /* x */,
                             int         /* y */,
                             int         w,
                             int         h,
                             Uint32      flags_)
{
	Uint32 flags = SDL_DOUBLEBUF;

	if ((flags_ & SDL_WINDOW_RESIZABLE) != 0) {
		flags |= SDL_RESIZABLE;
	}
	if ((flags_ & SDL_WINDOW_FULLSCREEN) != 0) {
		flags |= SDL_FULLSCREEN;
	}
	if ((flags_ & SDL_WINDOW_OPENGL) != 0) {
		flags |= SDL_OPENGL;
	}

	SDL_SetVideoMode(w, h, 32, flags);

	return new SDL_Window();
}


SDL_GLContext SDL_GL_CreateContext(SDL_Window* window) {
	return reinterpret_cast<SDL_GLContext>(window);
}


int SDL_GetNumVideoDisplays() {
	return 0;
}


int SDL_GetNumDisplayModes(int /* displayIndex */) {
	return 0;
}


int SDL_GetDisplayMode(int /* displayIndex */, int /* modeIndex */, SDL_DisplayMode* /* mode */) {
	return 0;
}


#endif  // EMSCRIPTEN


#ifdef _MSC_VER
#define fileno _fileno
#define __builtin_unreachable() assert(false)
#endif


// should be ifdeffed out on compilers which already have it (eg. VS2013)
// http://isocpp.org/files/papers/N3656.txt
#ifndef _MSC_VER
namespace std {

    template<class T> struct _Unique_if {
        typedef unique_ptr<T> _Single_object;
    };

    template<class T> struct _Unique_if<T[]> {
        typedef unique_ptr<T[]> _Unknown_bound;
    };

    template<class T, size_t N> struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
        typename _Unique_if<T>::_Single_object
        make_unique(Args&&... args) {
            return unique_ptr<T>(new T(std::forward<Args>(args)...));
        }

    template<class T>
        typename _Unique_if<T>::_Unknown_bound
        make_unique(size_t n) {
            typedef typename remove_extent<T>::type U;
            return unique_ptr<T>(new U[n]());
        }

    template<class T, class... Args>
        typename _Unique_if<T>::_Known_bound
        make_unique(Args&&...) = delete;


}  // namespace std
#endif


struct FILEDeleter {
	void operator()(FILE *f) { fclose(f); }
};


union Color {
	uint32_t val;
	struct {
		uint8_t r, g, b, a;
	};
};


static const Color white = { 0xFFFFFFFF };


std::vector<char> readTextFile(std::string filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (!file) {
		// TODO: better exception
		throw std::runtime_error("file not found " + filename);
	}

	int fd = fileno(file.get());
	if (fd < 0) {
		// TODO: better exception
		throw std::runtime_error("no fd");
	}

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	int retval = fstat(fd, &statbuf);
	if (retval < 0) {
		// TODO: better exception
		throw std::runtime_error("fstat failed");
	}

	uint64_t filesize = static_cast<uint64_t>(statbuf.st_size);
	// ensure NUL -termination
	std::vector<char> buf(filesize + 1, '\0');

	size_t ret = fread(&buf[0], 1, filesize, file.get());
	if (ret != filesize)
	{
		// TODO: better exception
		throw std::runtime_error("fread failed");
	}

	return buf;
}


std::vector<char> readFile(std::string filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (!file) {
		// TODO: better exception
		throw std::runtime_error("file not found " + filename);
	}

	int fd = fileno(file.get());
	if (fd < 0) {
		// TODO: better exception
		throw std::runtime_error("no fd");
	}

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	int retval = fstat(fd, &statbuf);
	if (retval < 0) {
		// TODO: better exception
		throw std::runtime_error("fstat failed");
	}

	uint64_t filesize = static_cast<uint64_t>(statbuf.st_size);
	std::vector<char> buf(filesize, '\0');

	size_t ret = fread(&buf[0], 1, filesize, file.get());
	if (ret != filesize)
	{
		// TODO: better exception
		throw std::runtime_error("fread failed");
	}

	return buf;
}


static std::vector<char> processShaderIncludes(std::vector<char> shaderSource) {
	std::vector<char> output(shaderSource);

	auto includePos = output.begin();

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

	return output;
}


static GLuint createShader(GLenum type, const std::string &name, const std::vector<char> &rawSrc) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
	auto src = processShaderIncludes(rawSrc);

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


class Shader;
class ShaderBuilder;


class VertexShader {
	GLuint shader;

	VertexShader() = delete;

	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	friend class Shader;

public:

	VertexShader(const std::string &name, const ShaderBuilder &builder);

	~VertexShader();
};


class FragmentShader {
	GLuint shader;
	bool glES;

	FragmentShader() = delete;

	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&) = delete;
	FragmentShader &operator=(FragmentShader &&) = delete;

	friend class Shader;

public:

	FragmentShader(const std::string &name, const ShaderBuilder &builder);

	~FragmentShader();
};


class ShaderBuilder {
	std::vector<char> source;

	ShaderBuilder &operator=(const ShaderBuilder &) = delete;

	ShaderBuilder(ShaderBuilder &&) = delete;
	ShaderBuilder &operator=(ShaderBuilder &&) = delete;

	friend class VertexShader;
	friend class FragmentShader;

	bool glES;

public:
	explicit ShaderBuilder(bool glES_);
	ShaderBuilder(const ShaderBuilder &other);

	VertexShader compileVertex();
	FragmentShader compileFragment();

	void pushLine(const std::string &line);
	void pushVertexAttr(const std::string &attr);
	void pushVertexVarying(const std::string &var);
	void pushFragmentVarying(const std::string &var);
	void pushFragmentOutputDecl();
	void pushFragmentOutput(const std::string &expr);
	void pushFile(const std::string &filename);
};


ShaderBuilder::ShaderBuilder(bool glES_)
: glES(glES_)
{
	source.reserve(512);

	if (glES) {
		pushLine("#version 100");

		// FIXME: is this universally available on WebGL implementations?
		pushLine("#extension GL_EXT_shader_texture_lod : enable");

		pushLine("precision highp float;");

		pushLine("#define round(x) floor((x) + 0.5)");
		pushLine("#define textureLod texture2DLodEXT");
		pushLine("#define texture2DLod texture2DLodEXT");
	} else {
		pushLine("#version 130");

#ifdef USE_GLEW

		if (GLEW_ARB_gpu_shader5) {
			pushLine("#extension GL_ARB_gpu_shader5 : enable");
		}
		if (GLEW_ARB_texture_gather) {
			pushLine("#extension GL_ARB_texture_gather : enable");
		}

#endif  // USE_GLEW

	}
}


ShaderBuilder::ShaderBuilder(const ShaderBuilder &other)
: glES(other.glES)
{
	source = other.source;
}


void ShaderBuilder::pushLine(const std::string &line) {
	source.reserve(source.size() + line.size() + 1);
	source.insert(source.end(), line.begin(), line.end());
	source.push_back('\n');
}


void ShaderBuilder::pushVertexAttr(const std::string &attr) {
	if (glES) {
		pushLine("attribute " + attr);
	} else {
		pushLine("in " + attr);
	}

}


void ShaderBuilder::pushVertexVarying(const std::string &var) {
	if (glES) {
		pushLine("varying " + var);
	} else {
		pushLine("out " + var);
	}
}


void ShaderBuilder::pushFragmentVarying(const std::string &var) {
	if (glES) {
		pushLine("varying " + var);
	} else {
		pushLine("in " + var);
	}
}


void ShaderBuilder::pushFragmentOutput(const std::string &expr) {
	if (glES) {
		pushLine("    gl_FragColor = " + expr);
	} else {
		pushLine("    outColor = " + expr);
	}
}


void ShaderBuilder::pushFragmentOutputDecl() {
	if (!glES) {
		pushLine("out vec4 outColor;");
	}
}


void ShaderBuilder::pushFile(const std::string &filename) {
	// TODO: grab file here, don't use #include
	// which we'll just end up parsing back later
	pushLine("#include \"" + filename + "\"");
}


class Shader {
    GLuint program;

	GLint screenSizeLoc;

	Shader() = delete;
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&) = delete;
	Shader &operator=(Shader &&) = delete;

public:
	Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader);

	~Shader();

	GLint getUniformLocation(const char *name);

	GLint getScreenSizeLocation() const;

	void bind();
};


VertexShader::VertexShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
{
	shader = createShader(GL_VERTEX_SHADER, name, builder.source);
}


VertexShader::~VertexShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


FragmentShader::FragmentShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
, glES(builder.glES)
{
	shader = createShader(GL_FRAGMENT_SHADER, name, builder.source);
}


FragmentShader::~FragmentShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


Shader::Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader)
: program(0)
, screenSizeLoc(0)
{
	program = glCreateProgram();
	glBindAttribLocation(program, ATTR_POS, "position");
	glBindAttribLocation(program, ATTR_COLOR, "color");
	glBindAttribLocation(program, ATTR_CUBEPOS, "cubePos");
	glBindAttribLocation(program, ATTR_ROT, "rotationQuat");

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

	if (!fragmentShader.glES) {
		glBindFragDataLocation(program, 0, "outColor");
	}

	GLint colorLoc = getUniformLocation("colorTex");
	glUniform1i(colorLoc, TEXUNIT_COLOR);

	GLint areaTexLoc = getUniformLocation("areaTex");
	if (areaTexLoc >= 0) {
		glUniform1i(areaTexLoc, TEXUNIT_AREATEX);
	}

	GLint searchTexLoc = getUniformLocation("searchTex");
	if (searchTexLoc >= 0) {
		glUniform1i(searchTexLoc, TEXUNIT_SEARCHTEX);
	}

	GLint edgesTexLoc = getUniformLocation("edgesTex");
	if (edgesTexLoc >= 0) {
		glUniform1i(edgesTexLoc, TEXUNIT_EDGES);
	}

	GLint blendTexLoc = getUniformLocation("blendTex");
	if (blendTexLoc >= 0) {
		glUniform1i(blendTexLoc, TEXUNIT_BLEND);
	}

	screenSizeLoc = getUniformLocation("screenSize");
}


Shader::~Shader() {
	if (program != 0) {
		glDeleteProgram(program);
		program = 0;
	}
}


GLint Shader::getUniformLocation(const char *name) {
	assert(program != 0);
	assert(name != NULL);

	return glGetUniformLocation(program, name);
}


GLint Shader::getScreenSizeLocation() const {
	return screenSizeLoc;
}


void Shader::bind() {
	assert(program != 0);

	glUseProgram(program);
}


class SMAADemo;


class Framebuffer {
	// TODO: need a proper Render object to control the others
	friend class SMAADemo;

	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;

	Framebuffer() = delete;
	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&) = delete;
	Framebuffer &operator=(Framebuffer &&) = delete;

public:

	explicit Framebuffer(GLuint fbo_)
	: fbo(fbo_)
	, colorTex(0)
	, depthTex(0)
	, width(0)
	, height(0)
	{
	}

	~Framebuffer();

	void bind();

	void blitTo(Framebuffer &target);
};


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


void Framebuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}


void Framebuffer::blitTo(Framebuffer &target) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.fbo);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


namespace DSAMode {

enum DSAMode {
	  None
	, EXT
	, ARB
};


}  // namespace DSAMode


namespace AAMethod {

enum AAMethod {
	  FXAA
	, SMAA
	, LAST = SMAA
};


const char *name(AAMethod m) {
	switch (m) {
	case FXAA:
		return "FXAA";
		break;

	case SMAA:
		return "SMAA";
		break;
	}

	__builtin_unreachable();
}


}  // namespace AAMethod


static const char *smaaDebugModeStr(unsigned int mode) {
	switch (mode) {
	case 0:
		return "none";

	case 1:
		return "edges";

	case 2:
		return "blend";
	}

	__builtin_unreachable();
}


// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

static uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}


class RandomGen {
	pcg32_random_t rng;

	RandomGen(const RandomGen &) = delete;
	RandomGen &operator=(const RandomGen &) = delete;
	RandomGen(RandomGen &&) = delete;
	RandomGen &operator=(RandomGen &&) = delete;

public:

	explicit RandomGen(uint64_t seed)
	{
		rng.state = seed;
		rng.inc = 1;
		// spin it once for proper initialization
		pcg32_random_r(&rng);
	}


	float randFloat() {
		uint32_t u = randU32();
		// because 24 bits mantissa
		u &= 0x00FFFFFFU;
		return float(u) / 0x00FFFFFFU;
	}


	uint32_t randU32() {
		return pcg32_random_r(&rng);
	}
};


static const char *fxaaQualityLevels[] =
{ "10", "15", "20", "29", "39" };


static const unsigned int maxFXAAQuality = sizeof(fxaaQualityLevels) / sizeof(fxaaQualityLevels[0]);


static const char *smaaQualityLevels[] =
{ "LOW", "MEDIUM", "HIGH", "ULTRA" };


static const unsigned int maxSMAAQuality = sizeof(smaaQualityLevels) / sizeof(smaaQualityLevels[0]);


class SMAADemo {
	unsigned int windowWidth, windowHeight;
	unsigned int resizeWidth, resizeHeight;
	bool vsync;
	bool fullscreen;
	SDL_Window *window;
	SDL_GLContext context;
	bool glES;
	bool glDebug;
	unsigned int glMajor;
	unsigned int glMinor;
	bool smaaSupported;
	bool useInstancing;
	bool useVAO;
	bool useSamplerObjects;
	DSAMode::DSAMode dsaMode;

	std::unique_ptr<Shader> cubeInstanceShader;
	std::unique_ptr<Shader> cubeShader;
	std::unique_ptr<Shader> imageShader;

	// TODO: create helper classes for these
	GLuint cubeVAO;
	GLuint cubeVBO, cubeIBO;
	GLuint fullscreenVAO;
	GLuint fullscreenVBO;
	GLuint instanceVBO;

	GLuint linearSampler;
	GLuint nearestSampler;

	unsigned int cubePower;

	std::unique_ptr<Framebuffer> builtinFBO;
	std::unique_ptr<Framebuffer> renderFBO;
	std::unique_ptr<Framebuffer> edgesFBO;
	std::unique_ptr<Framebuffer> blendFBO;

	bool antialiasing;
	AAMethod::AAMethod aaMethod;
	std::unique_ptr<Shader> fxaaShader;
	std::unique_ptr<Shader> smaaEdgeShader;
	std::unique_ptr<Shader> smaaBlendWeightShader;
	std::unique_ptr<Shader> smaaNeighborShader;
	GLuint areaTex;
	GLuint searchTex;

	bool rotateCamera;
	float cameraRotation;
	uint64_t lastTime;
	uint64_t freq;
	uint64_t rotationTime;
	unsigned int debugMode;
	unsigned int colorMode;
	bool rightShift, leftShift;
	RandomGen random;
	unsigned int fxaaQuality;
	unsigned int smaaQuality;
	bool keepGoing;
	// 0 for cubes
	// 1.. for images
	unsigned int activeScene;


	struct Image {
		std::string filename;
		GLuint tex;
	};

	std::vector<Image> images;


	struct Cube {
		glm::vec3 pos;
		glm::quat orient;
		Color col;


		Cube(float x_, float y_, float z_, glm::quat orient_, Color col_)
		: pos(x_, y_, z_), orient(orient_), col(col_)
		{
		}
	};

	std::vector<Cube> cubes;


	struct InstanceData {
		float x, y, z;
		float qx, qy, qz;
		Color col;


		InstanceData(glm::quat rot, glm::vec3 pos, Color col_)
			: x(pos.x), y(pos.y), z(pos.z)
			, qx(rot.x), qy(rot.y), qz(rot.z)
			, col(col_)
		{
			// shader assumes this and uses it to calculate w from other components
			assert(rot.w >= 0.0);
		}
	};

	std::vector<InstanceData> instances;

	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) = delete;
	SMAADemo &operator=(SMAADemo &&) = delete;

public:

	SMAADemo();

	~SMAADemo();

	void parseCommandLine(int argc, char *argv[]);

	void initRender();

	void createFramebuffers();

	void applyVSync();

	void applyFullscreen();

	void buildCubeShader();

	void buildImageShader();

	void buildFXAAShader();

	void buildSMAAShaders();

	void createCubes();

	void colorCubes();

	void setCubeVBO();

	void setFullscreenVBO();

	void mainLoopIteration();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void render();
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, vsync(true)
, fullscreen(false)
, window(NULL)
, context(NULL)
#ifdef EMSCRIPTEN
, glES(true)
#else  // EMSCRIPTEN
, glES(false)
#endif  // EMSCRIPTEN
, glDebug(false)
, glMajor(3)
, glMinor(1)
, smaaSupported(true)
, useInstancing(true)
, useVAO(false)
, useSamplerObjects(false)
, dsaMode(DSAMode::ARB)
, cubeVAO(0)
, cubeVBO(0)
, cubeIBO(0)
, fullscreenVAO(0)
, fullscreenVBO(0)
, instanceVBO(0)
, linearSampler(0)
, nearestSampler(0)
, cubePower(3)
, antialiasing(true)
, aaMethod(AAMethod::SMAA)
, areaTex(0)
, searchTex(0)
, rotateCamera(false)
, cameraRotation(0.0f)
, lastTime(0)
, freq(0)
, rotationTime(0)
, debugMode(0)
, colorMode(0)
, rightShift(false)
, leftShift(false)
, random(1)
, fxaaQuality(maxFXAAQuality - 1)
, smaaQuality(maxSMAAQuality - 1)
, keepGoing(true)
, activeScene(0)
{
	resizeWidth = windowWidth;
	resizeHeight = windowHeight;

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	freq = SDL_GetPerformanceFrequency();
	lastTime = SDL_GetPerformanceCounter();

	// TODO: detect screens, log interesting display parameters etc
	// TODO: initialize random using external source
}


SMAADemo::~SMAADemo() {
	if (context != NULL) {
		SDL_GL_DeleteContext(context);
		context = NULL;
	}

	if (window != NULL) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	if (useVAO) {
		glDeleteVertexArrays(1, &cubeVAO);
		glDeleteVertexArrays(1, &fullscreenVAO);
	}
	
	glDeleteBuffers(1, &cubeVBO);
	glDeleteBuffers(1, &cubeIBO);
	glDeleteBuffers(1, &fullscreenVBO);
	glDeleteBuffers(1, &instanceVBO);

	if (useSamplerObjects) {
		glDeleteSamplers(1, &linearSampler);
		glDeleteSamplers(1, &nearestSampler);
	}

	glDeleteTextures(1, &areaTex);
	glDeleteTextures(1, &searchTex);

	SDL_Quit();
}


struct Vertex {
	float x, y, z;
};


const float coord = sqrtf(3.0f) / 2.0f;


static const Vertex vertices[] =
{
	  { -coord , -coord, -coord }
	, { -coord ,  coord, -coord }
	, {  coord , -coord, -coord }
	, {  coord ,  coord, -coord }
	, { -coord , -coord,  coord }
	, { -coord ,  coord,  coord }
	, {  coord , -coord,  coord }
	, {  coord ,  coord,  coord }
};


static const float fullscreenVertices[] =
{
	  -1.0f, -1.0f
	,  3.0f, -1.0f
	, -1.0f,  3.0f
};


static const uint32_t indices[] =
{
	// top
	  1, 3, 5
	, 5, 3, 7

	// front
	, 0, 2, 1
	, 1, 2, 3

	// back
	, 7, 6, 5
	, 5, 6, 4

	// left
	, 0, 1, 4
	, 4, 1, 5

	// right
	, 2, 6, 3
	, 3, 6, 7

	// bottom
	, 2, 0, 6
	, 6, 0, 4
};


#define VBO_OFFSETOF(st, member) reinterpret_cast<GLvoid *>(offsetof(st, member))


void SMAADemo::buildCubeShader() {
	ShaderBuilder s(glES);

	ShaderBuilder vert(s);
	vert.pushLine("uniform mat4 viewProj;");
	vert.pushVertexAttr("vec3 rotationQuat;");
	vert.pushVertexAttr("vec3 cubePos;");
	vert.pushVertexAttr("vec3 color;");
	vert.pushVertexAttr("vec3 position;");
	vert.pushVertexVarying("vec3 colorFrag;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    // our quaternions are normalized and have w > 0.0");
	vert.pushLine("    float qw = sqrt(1.0 - dot(rotationQuat, rotationQuat));");
	vert.pushLine("    // rotate");
	vert.pushLine("    // this is quaternion multiplication from glm");
	vert.pushLine("    vec3 v = position;");
	vert.pushLine("    vec3 uv = cross(rotationQuat, v);");
	vert.pushLine("    vec3 uuv = cross(rotationQuat, uv);");
	vert.pushLine("    uv *= (2.0 * qw);");
	vert.pushLine("    uuv *= 2.0;");
	vert.pushLine("    vec3 rotatedPos = v + uv + uuv;");
	vert.pushLine("");
	vert.pushLine("    gl_Position = viewProj * vec4(rotatedPos + cubePos, 1.0);");
	vert.pushLine("    colorFrag = color;");
	vert.pushLine("}");

	VertexShader vShader("cube.vert", vert);

	// fragment
	ShaderBuilder frag(s);

	frag.pushFragmentVarying("vec3 colorFrag;");
	frag.pushFragmentOutputDecl();
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushLine("    vec4 temp;");
	frag.pushLine("    temp.xyz = colorFrag;");
	frag.pushLine("    temp.w = dot(colorFrag, vec3(0.299, 0.587, 0.114));");
	frag.pushFragmentOutput("temp;");
	frag.pushLine("}");

	FragmentShader fShader("cube.frag", frag);

	cubeInstanceShader = std::make_unique<Shader>(vShader, fShader);

	ShaderBuilder vert2(s);
	vert2.pushLine("uniform mat4 viewProj;");
	vert2.pushLine("uniform vec3 rotationQuat;");
	vert2.pushLine("uniform vec3 cubePos;");
	vert2.pushLine("uniform vec3 color;");
	vert2.pushVertexAttr("vec3 position;");
	vert2.pushVertexVarying("vec3 colorFrag;");
	vert2.pushLine("void main(void)");
	vert2.pushLine("{");
	vert2.pushLine("    // our quaternions are normalized and have w > 0.0");
	vert2.pushLine("    float qw = sqrt(1.0 - dot(rotationQuat, rotationQuat));");
	vert2.pushLine("    // rotate");
	vert2.pushLine("    // this is quaternion multiplication from glm");
	vert2.pushLine("    vec3 v = position;");
	vert2.pushLine("    vec3 uv = cross(rotationQuat, v);");
	vert2.pushLine("    vec3 uuv = cross(rotationQuat, uv);");
	vert2.pushLine("    uv *= (2.0 * qw);");
	vert2.pushLine("    uuv *= 2.0;");
	vert2.pushLine("    vec3 rotatedPos = v + uv + uuv;");
	vert2.pushLine("");
	vert2.pushLine("    gl_Position = viewProj * vec4(rotatedPos + cubePos, 1.0);");
	vert2.pushLine("    colorFrag = color;");
	vert2.pushLine("}");

	VertexShader vShader2("cube2.vert", vert2);
	cubeShader = std::make_unique<Shader>(vShader2, fShader);
}


void SMAADemo::buildImageShader() {
	ShaderBuilder s(glES);

	ShaderBuilder vert(s);
	vert.pushVertexAttr("vec2 pos;");
	vert.pushVertexVarying("vec2 texcoord;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    texcoord = pos * vec2(0.5, -0.5) + vec2(0.5, 0.5);");
	vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
	vert.pushLine("}");

	VertexShader vShader("image.vert", vert);

	// fragment
	ShaderBuilder frag(s);
	frag.pushLine("uniform sampler2D colorTex;");
	frag.pushFragmentVarying("vec2 texcoord;");
	frag.pushFragmentOutputDecl();
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushFragmentOutput("texture2D(colorTex, texcoord);");
	frag.pushLine("}");

	FragmentShader fShader("image.frag", frag);

	imageShader = std::make_unique<Shader>(vShader, fShader);
}


void SMAADemo::buildFXAAShader() {
	glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

	ShaderBuilder s(glES);

	s.pushLine("#define FXAA_PC 1");

	if (glES) {
		s.pushLine("#define FXAA_FAST_PIXEL_OFFSET 0");
		s.pushLine("#define ivec2 vec2");
		s.pushLine("#define FXAA_GLSL_120 1");
	} else {
		s.pushLine("#define FXAA_GLSL_130 1");
	}

	// TODO: cache shader based on quality level
	s.pushLine("#define FXAA_QUALITY_PRESET " + std::string(fxaaQualityLevels[fxaaQuality]));

	ShaderBuilder vert(s);
	vert.pushVertexAttr("vec2 pos;");
	vert.pushVertexVarying("vec2 texcoord;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
	vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
	vert.pushLine("}");

	VertexShader vShader("fxaa.vert", vert);

	// fragment
	ShaderBuilder frag(s);
	if (glES) {
		frag.pushLine("#define FXAA_GATHER4_ALPHA 0");
	}
	frag.pushFile("fxaa3_11.h");
	frag.pushLine("uniform sampler2D colorTex;");
	frag.pushLine("uniform vec4 screenSize;");
	frag.pushFragmentVarying("vec2 texcoord;");
	frag.pushFragmentOutputDecl();
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushLine("    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);");
	frag.pushFragmentOutput("FxaaPixelShader(texcoord, zero, colorTex, colorTex, colorTex, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);");
	frag.pushLine("}");

	FragmentShader fShader("fxaa.frag", frag);

	fxaaShader = std::make_unique<Shader>(vShader, fShader);
	glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
}


void SMAADemo::buildSMAAShaders() {
	try {
		ShaderBuilder s(glES);

		s.pushLine("#define SMAA_RT_METRICS screenSize");
		s.pushLine("#define SMAA_GLSL_3 1");
		// TODO: cache shader based on quality level
		s.pushLine("#define SMAA_PRESET_"  + std::string(smaaQualityLevels[smaaQuality]) + " 1");

		s.pushLine("uniform vec4 screenSize;");

		ShaderBuilder commonVert(s);

		commonVert.pushLine("#define SMAA_INCLUDE_PS 0");
		commonVert.pushLine("#define SMAA_INCLUDE_VS 1");

		commonVert.pushFile("smaa.h");

		ShaderBuilder commonFrag(s);

		commonFrag.pushLine("#define SMAA_INCLUDE_PS 1");
		commonFrag.pushLine("#define SMAA_INCLUDE_VS 0");

		commonFrag.pushFile("smaa.h");
		commonFrag.pushFragmentOutputDecl();

		glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec4 offset0;");
			vert.pushVertexVarying("vec4 offset1;");
			vert.pushVertexVarying("vec4 offset2;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    vec4 offsets[3];");
			vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    SMAAEdgeDetectionVS(texcoord, offsets);");
			vert.pushLine("    offset0 = offsets[0];");
			vert.pushLine("    offset1 = offsets[1];");
			vert.pushLine("    offset2 = offsets[2];");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaEdge.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D colorTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec4 offset0;");
			frag.pushFragmentVarying("vec4 offset1;");
			frag.pushFragmentVarying("vec4 offset2;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushLine("    vec4 offsets[3];");
			frag.pushLine("    offsets[0] = offset0;");
			frag.pushLine("    offsets[1] = offset1;");
			frag.pushLine("    offsets[2] = offset2;");
			frag.pushFragmentOutput("vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 0.0);");
			frag.pushLine("}");

			FragmentShader fShader("smaaEdge.frag", frag);

			smaaEdgeShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}

		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec2 pixcoord;");
			vert.pushVertexVarying("vec4 offset0;");
			vert.pushVertexVarying("vec4 offset1;");
			vert.pushVertexVarying("vec4 offset2;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    vec4 offsets[3];");
			vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    pixcoord = vec2(0.0, 0.0);");
			vert.pushLine("    SMAABlendingWeightCalculationVS(texcoord, pixcoord, offsets);");
			vert.pushLine("    offset0 = offsets[0];");
			vert.pushLine("    offset1 = offsets[1];");
			vert.pushLine("    offset2 = offsets[2];");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaBlendWeight.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D edgesTex;");
			frag.pushLine("uniform sampler2D areaTex;");
			frag.pushLine("uniform sampler2D searchTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec2 pixcoord;");
			frag.pushFragmentVarying("vec4 offset0;");
			frag.pushFragmentVarying("vec4 offset1;");
			frag.pushFragmentVarying("vec4 offset2;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushLine("    vec4 offsets[3];");
			frag.pushLine("    offsets[0] = offset0;");
			frag.pushLine("    offsets[1] = offset1;");
			frag.pushLine("    offsets[2] = offset2;");
			frag.pushFragmentOutput("SMAABlendingWeightCalculationPS(texcoord, pixcoord, offsets, edgesTex, areaTex, searchTex, vec4(0.0, 0.0, 0.0, 0.0));");
			frag.pushLine("}");

			FragmentShader fShader("smaaBlendWeight.frag", frag);

			smaaBlendWeightShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}

		{
			ShaderBuilder vert(commonVert);

			vert.pushVertexAttr("vec2 pos;");
			vert.pushVertexVarying("vec2 texcoord;");
			vert.pushVertexVarying("vec4 offset;");
			vert.pushLine("void main(void)");
			vert.pushLine("{");
			vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
			vert.pushLine("    offset = vec4(0.0, 0.0, 0.0, 0.0);");
			vert.pushLine("    SMAANeighborhoodBlendingVS(texcoord, offset);");
			vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
			vert.pushLine("}");

			VertexShader vShader("smaaNeighbor.vert", vert);

			ShaderBuilder frag(commonFrag);

			frag.pushLine("uniform sampler2D blendTex;");
			frag.pushLine("uniform sampler2D colorTex;");
			frag.pushFragmentVarying("vec2 texcoord;");
			frag.pushFragmentVarying("vec4 offset;");
			frag.pushLine("void main(void)");
			frag.pushLine("{");
			frag.pushFragmentOutput("SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);");
			frag.pushLine("}");

			FragmentShader fShader("smaaNeighbor.frag", frag);

			smaaNeighborShader = std::make_unique<Shader>(vShader, fShader);
			glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		}
	} catch (std::exception &e) {
		printf("SMAA shader compile failed: \"%s\"\n", e.what());
		smaaSupported = false;
		aaMethod = AAMethod::FXAA;
	}
}


#ifndef EMSCRIPTEN


void SMAADemo::parseCommandLine(int argc, char *argv[]) {
	try {
		TCLAP::CmdLine cmd("SMAA demo", ' ', "1.0");

		TCLAP::SwitchArg glesSwitch("", "gles", "Use OpenGL ES", cmd, false);
		TCLAP::SwitchArg glDebugSwitch("", "gldebug", "Enable OpenGL debugging", cmd, false);
		TCLAP::SwitchArg noinstancingSwitch("", "noinstancing", "Don't use instanced rendering", cmd, false);
		TCLAP::ValueArg<std::string> dsaSwitch("", "dsa", "Select DSA mode", false, "arb", "arb, ext or none", cmd);
		TCLAP::ValueArg<unsigned int> glMajorSwitch("", "glmajor", "OpenGL major version", false, glMajor, "version", cmd);
		TCLAP::ValueArg<unsigned int> glMinorSwitch("", "glminor", "OpenGL minor version", false, glMinor, "version", cmd);
		TCLAP::ValueArg<unsigned int> windowWidthSwitch("", "width", "Window width", false, windowWidth, "width", cmd);
		TCLAP::ValueArg<unsigned int> windowHeightSwitch("", "height", "Window height", false, windowHeight, "height", cmd);
		TCLAP::UnlabeledMultiArg<std::string> imagesArg("images", "image files", false, "image file", cmd, true, nullptr);

		cmd.parse(argc, argv);

		glES = glesSwitch.getValue();
		glDebug = glDebugSwitch.getValue();
		useInstancing = !noinstancingSwitch.getValue();
		std::string dsaString = dsaSwitch.getValue();
		if (dsaString == "ext") {
			dsaMode = DSAMode::EXT;
		} else if (dsaString == "none") {
			dsaMode = DSAMode::None;
		}
		glMajor = glMajorSwitch.getValue();
		glMinor = glMinorSwitch.getValue();
		windowWidth = windowWidthSwitch.getValue();
		windowHeight = windowHeightSwitch.getValue();
		resizeWidth = windowWidth;
		resizeHeight = windowHeight;

		const auto &imageFiles = imagesArg.getValue();
		images.reserve(imageFiles.size());
		for (const auto &filename : imageFiles) {
			images.push_back(Image());
			auto &img = images.back();
			img.tex = 0;
			img.filename = filename;
		}

	} catch (TCLAP::ArgException &e) {
		printf("parseCommandLine exception: %s for arg %s\n", e.error().c_str(), e.argId().c_str());
	} catch (...) {
		printf("parseCommandLine: unknown exception\n");
	}
}


#endif  // EMSCRIPTEN


#ifdef USE_GLEW


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


#endif  // USE_GLEW


void SMAADemo::initRender() {
	assert(window == NULL);
	assert(context == NULL);

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)
#ifndef EMSCRIPTEN

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinor);
	if (glES) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	} else {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		if (glDebug) {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		}
	}

#endif  // EMSCRIPTEN

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

	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);

	context = SDL_GL_CreateContext(window);

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	applyVSync();

#ifdef USE_GLEW
	glewExperimental = true;
	glewInit();

	// TODO: check extensions
	// at least direct state access, texture storage

	if (GLEW_ARB_direct_state_access && dsaMode == DSAMode::ARB) {
		printf("ARB_direct_state_access found\n");
	} else if (GLEW_EXT_direct_state_access && dsaMode != DSAMode::None) {
		printf("EXT_direct_state_access found\n");
		glCreateTextures = glCreateTexturesEXTEmulated;
		glTextureStorage2D = glTextureStorage2DEXTEmulated;
		glTextureSubImage2D = glTextureSubImage2DEXTEmulated;
		glTextureParameteri = glTextureParameteriEXTEmulated;
		glNamedFramebufferTexture = glNamedFramebufferTextureEXT;
		glCreateSamplers = glCreateSamplersEmulated;
		glNamedBufferData = glNamedBufferDataEmulated;
		glCreateBuffers = glCreateBuffersEmulated;
		glCreateVertexArrays = glCreateVertexArraysEmulated;
		glVertexArrayElementBuffer = glVertexArrayElementBufferEmulated;
		glEnableVertexArrayAttrib = glEnableVertexArrayAttribEXT;
		glNamedBufferSubData = glNamedBufferSubDataEXT;
		glBindTextureUnit = glBindTextureUnitEXTEmulated;
	} else {
		printf("No direct state access\n");
		glCreateTextures = glCreateTexturesEmulated;
		glTextureStorage2D = glTextureStorage2DEmulated;
		glTextureSubImage2D = glTextureSubImage2DEmulated;
		glTextureParameteri = glTextureParameteriEmulated;
		glNamedFramebufferTexture = glNamedFramebufferTextureEmulated;
		glCreateSamplers = glCreateSamplersEmulated;
		glNamedBufferData = glNamedBufferDataEmulated;
		glCreateBuffers = glCreateBuffersEmulated;
		glCreateVertexArrays = glCreateVertexArraysEmulated;
		glVertexArrayElementBuffer = glVertexArrayElementBufferEmulated;
		glEnableVertexArrayAttrib = glEnableVertexArrayAttribEmulated;
		glNamedBufferSubData = glNamedBufferSubDataEmulated;
		glBindTextureUnit = glBindTextureUnitEmulated;
	}

	if (glDebug) {
		if (GLEW_KHR_debug) {
			printf("KHR_debug found\n");

			glDebugMessageCallback(glDebugCallback, NULL);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		} else {
			printf("KHR_debug not found\n");
		}
	}

	if (GLEW_VERSION_3_0 || GLEW_ARB_vertex_array_object) {
		printf("Vertex array objects enabled\n");
		useVAO = true;
	}

	if (GLEW_VERSION_3_3 || GLEW_ARB_sampler_objects) {
		printf("Sampler objects enabled\n");
		useSamplerObjects = true;
	}

#endif  // USE_GLEW

	printf("GL vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("GL renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("GL version: \"%s\"\n", glGetString(GL_VERSION));
	printf("GLSL version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// swap once to get better traces
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	SDL_GL_SwapWindow(window);

	buildCubeShader();
	buildImageShader();
	buildSMAAShaders();
	buildFXAAShader();

	if (useSamplerObjects) {
		glCreateSamplers(1, &linearSampler);
		glSamplerParameteri(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glCreateSamplers(1, &nearestSampler);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	glCreateBuffers(1, &cubeVBO);
	glNamedBufferData(cubeVBO, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

	glCreateBuffers(1, &cubeIBO);
	glNamedBufferData(cubeIBO, sizeof(indices), &indices[0], GL_STATIC_DRAW);

	glCreateBuffers(1, &instanceVBO);
	glNamedBufferData(instanceVBO, sizeof(InstanceData), NULL, GL_STREAM_DRAW);

	if (useVAO) {
		glCreateVertexArrays(1, &cubeVAO);
		glVertexArrayElementBuffer(cubeVAO, cubeIBO);

		glBindVertexArray(cubeVAO);

		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

		glEnableVertexArrayAttrib(cubeVAO, ATTR_POS);

		if (useInstancing) {
			glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
			glVertexAttribPointer(ATTR_CUBEPOS, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, x));
			glVertexAttribDivisor(ATTR_CUBEPOS, 1);

			glVertexAttribPointer(ATTR_ROT, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, qx));
			glVertexAttribDivisor(ATTR_ROT, 1);

			glVertexAttribPointer(ATTR_COLOR, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, col));
			glVertexAttribDivisor(ATTR_COLOR, 1);

			glEnableVertexArrayAttrib(cubeVAO, ATTR_CUBEPOS);
			glEnableVertexArrayAttrib(cubeVAO, ATTR_ROT);
			glEnableVertexArrayAttrib(cubeVAO, ATTR_COLOR);
		}
	}

	glCreateBuffers(1, &fullscreenVBO);
	glNamedBufferData(fullscreenVBO, sizeof(fullscreenVertices), &fullscreenVertices[0], GL_STATIC_DRAW);

	if (useVAO) {
		glCreateVertexArrays(1, &fullscreenVAO);
		glBindVertexArray(fullscreenVAO);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
		glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);

		glEnableVertexArrayAttrib(fullscreenVAO, ATTR_POS);
	}

	const bool flipSMAATextures = true;

	glCreateTextures(GL_TEXTURE_2D, 1, &areaTex);
	glBindTextureUnit(TEXUNIT_AREATEX, areaTex);
	glTextureStorage2D(areaTex, 1, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT);
	glTextureParameteri(areaTex, GL_TEXTURE_MAX_LEVEL, 0);

	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(AREATEX_SIZE);
		for (unsigned int y = 0; y < AREATEX_HEIGHT; y++) {
			unsigned int srcY = AREATEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * AREATEX_PITCH], areaTexBytes + srcY * AREATEX_PITCH, AREATEX_PITCH);
		}
		glTextureSubImage2D(areaTex, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	} else {
		glTextureSubImage2D(areaTex, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);
	}
	glBindSampler(TEXUNIT_AREATEX, linearSampler);

	glCreateTextures(GL_TEXTURE_2D, 1, &searchTex);
	glBindTextureUnit(TEXUNIT_SEARCHTEX, searchTex);
	glTextureStorage2D(searchTex, 1, GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT);
	glTextureParameteri(searchTex, GL_TEXTURE_MAX_LEVEL, 0);
	if (flipSMAATextures) {
		std::vector<unsigned char> tempBuffer(SEARCHTEX_SIZE);
		for (unsigned int y = 0; y < SEARCHTEX_HEIGHT; y++) {
			unsigned int srcY = SEARCHTEX_HEIGHT - 1 - y;
			//unsigned int srcY = y;
			memcpy(&tempBuffer[y * SEARCHTEX_PITCH], searchTexBytes + srcY * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
		}
		glTextureSubImage2D(searchTex, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	} else {
		glTextureSubImage2D(searchTex, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);
	}

	glBindSampler(TEXUNIT_SEARCHTEX, linearSampler);

	builtinFBO = std::make_unique<Framebuffer>(0);
	builtinFBO->width = windowWidth;
	builtinFBO->height = windowHeight;

	createFramebuffers();

	for (auto &img : images) {
		const auto filename = img.filename.c_str();
		int width = 0, height = 0;
		unsigned char *imageData = stbi_load(filename, &width, &height, NULL, 3);
		printf(" %p  %dx%d\n", imageData, width, height);

		glCreateTextures(GL_TEXTURE_2D, 1, &img.tex);
		glTextureStorage2D(img.tex, 1, GL_RGB8, width, height);
		glTextureParameteri(img.tex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureSubImage2D(img.tex, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imageData);

		stbi_image_free(imageData);
	}

	// default scene to last image or cubes if none
	activeScene = images.size();
}


void SMAADemo::setCubeVBO() {
	if (useVAO) {
		glBindVertexArray(cubeVAO);
	} else {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);

		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

		glEnableVertexAttribArray(ATTR_POS);

		if (useInstancing) {
			glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
			glVertexAttribPointer(ATTR_CUBEPOS, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, x));
			glVertexAttribDivisor(ATTR_CUBEPOS, 1);

			glVertexAttribPointer(ATTR_ROT, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, qx));
			glVertexAttribDivisor(ATTR_ROT, 1);

			glVertexAttribPointer(ATTR_COLOR, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, col));
			glVertexAttribDivisor(ATTR_COLOR, 1);

			glEnableVertexAttribArray(ATTR_CUBEPOS);
			glEnableVertexAttribArray(ATTR_ROT);
			glEnableVertexAttribArray(ATTR_COLOR);
		} else {
			glDisableVertexAttribArray(ATTR_CUBEPOS);
			glDisableVertexAttribArray(ATTR_ROT);
			glDisableVertexAttribArray(ATTR_COLOR);
		}
	}
}


void SMAADemo::setFullscreenVBO() {
	if (useVAO) {
		glBindVertexArray(fullscreenVAO);
	} else {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
		glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);

		glEnableVertexAttribArray(ATTR_POS);
		glDisableVertexAttribArray(ATTR_CUBEPOS);
		glDisableVertexAttribArray(ATTR_ROT);
		glDisableVertexAttribArray(ATTR_COLOR);
	}
}


void SMAADemo::createFramebuffers()	{
	renderFBO.reset();
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	renderFBO = std::make_unique<Framebuffer>(fbo);
	renderFBO->width = windowWidth;
	renderFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	GLuint tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	renderFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_COLOR, renderFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	renderFBO->depthTex = tex;
	glBindTextureUnit(TEXUNIT_TEMP, renderFBO->depthTex);
	glTextureStorage2D(tex, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, tex, 0);

	// SMAA edges texture and FBO
	edgesFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	edgesFBO = std::make_unique<Framebuffer>(fbo);
	edgesFBO->width = windowWidth;
	edgesFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	edgesFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_EDGES, edgesFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glBindSampler(TEXUNIT_EDGES, linearSampler);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	// SMAA blending weights texture and FBO
	blendFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	blendFBO = std::make_unique<Framebuffer>(fbo);
	blendFBO->width = windowWidth;
	blendFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	blendFBO->colorTex = tex;
	glBindTextureUnit(TEXUNIT_BLEND, blendFBO->colorTex);
	glTextureStorage2D(tex, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glBindSampler(TEXUNIT_BLEND, linearSampler);
	glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
}


void SMAADemo::applyVSync() {
	if (vsync) {
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


void SMAADemo::applyFullscreen() {
#ifndef EMSCRIPTEN
	// emscripten doesn't allow program-initiated fullscreen without exterme trickery

	if (fullscreen) {
		// TODO: check return val?
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		printf("Fullscreen\n");
	} else {
		SDL_SetWindowFullscreen(window, 0);
		printf("Windowed\n");
	}
#endif  // EMSCRIPTEN
}


void SMAADemo::createCubes() {
	// cubes on a side is some power of 2
	const unsigned int cubesSide = pow(2, cubePower);

	// cube of cubes, n^3 cubes total
	const unsigned int numCubes = pow(cubesSide, 3);

	const float cubeDiameter = sqrtf(3.0f);
	const float cubeDistance = cubeDiameter + 1.0f;

	const float bigCubeSide = cubeDistance * cubesSide;

	cubes.clear();
	cubes.reserve(numCubes);

	for (unsigned int x = 0; x < cubesSide; x++) {
		for (unsigned int y = 0; y < cubesSide; y++) {
			for (unsigned int z = 0; z < cubesSide; z++) {
				float qx = random.randFloat();
				float qy = random.randFloat();
				float qz = random.randFloat();
				float qw = random.randFloat();
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				cubes.emplace_back((x * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (z * cubeDistance) - (bigCubeSide / 2.0f)
				                 , glm::quat(qx, qy, qz, qw)
				                 , white);
			}
		}
	}

	// reallocate instance data buffer
	glNamedBufferData(instanceVBO, sizeof(InstanceData) * numCubes, NULL, GL_STREAM_DRAW);

	colorCubes();
}


void SMAADemo::colorCubes() {
	if (colorMode == 0) {
		for (auto &cube : cubes) {
			Color col;
			// random RGB, alpha = 1.0
			// FIXME: we're abusing little-endianness, make it portable
			col.val = random.randU32() | 0xFF000000;
			cube.col = col;
		}
	} else {
		for (auto &cube : cubes) {
			Color col;
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			// TODO: use the same luma as shader

			float y = 0.5f;
			const float c_red = 0.299
				, c_green = 0.587
			, c_blue = 0.114;
			float cb = random.randFloat();
			float cr = random.randFloat();

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			col.r = 255 * r;
			col.g = 255 * g;
			col.b = 255 * b;
			col.a = 0xFF;
			cube.col = col;
		}
	}
}


static void printHelp() {
	printf(" a     - toggle antialiasing on/off\n");
	printf(" c     - re-color cubes\n");
	printf(" d     - cycle through debug visualizations\n");
	printf(" f     - toggle fullscreen\n");
	printf(" h     - print help\n");
	printf(" m     - change antialiasing method\n");
	printf(" q     - cycle through AA quality levels\n");
	printf(" v     - toggle vsync\n");
	printf(" SPACE - toggle camera rotation\n");
	printf(" ESC   - quit\n");
}


void SMAADemo::mainLoopIteration() {
		// TODO: timing
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		while (SDL_PollEvent(&event)) {
			int sceneIncrement = 1;
			switch (event.type) {
			case SDL_QUIT:
				keepGoing = false;
				break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE:
					keepGoing = false;
					break;

				case SDL_SCANCODE_LSHIFT:
					leftShift = true;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = true;
					break;

				case SDL_SCANCODE_SPACE:
					rotateCamera = !rotateCamera;
					printf("camera rotation is %s\n", rotateCamera ? "on" : "off");
					break;

				case SDL_SCANCODE_A:
					antialiasing = !antialiasing;
					printf("antialiasing set to %s\n", antialiasing ? "on" : "off");
					break;

				case SDL_SCANCODE_C:
					if (rightShift || leftShift) {
						colorMode = (colorMode + 1) % 2;
						printf("color mode set to %s\n", colorMode ? "YCbCr" : "RGB");
					}
					colorCubes();
					break;

				case SDL_SCANCODE_D:
					if (antialiasing && aaMethod == AAMethod::SMAA) {
						if (leftShift || rightShift) {
							debugMode = (debugMode + 3 - 1) % 3;
						} else {
							debugMode = (debugMode + 1) % 3;
						}
						printf("Debug mode set to %s\n", smaaDebugModeStr(debugMode));
					}
					break;

				case SDL_SCANCODE_H:
					printHelp();
					break;

				case SDL_SCANCODE_M:
					aaMethod = AAMethod::AAMethod((int(aaMethod) + 1) % (int(AAMethod::LAST) + 1));
					if (aaMethod == AAMethod::SMAA && !smaaSupported) {
						// Skip to next method
						aaMethod = AAMethod::AAMethod((int(aaMethod) + 1) % (int(AAMethod::LAST) + 1));
					}
					printf("aa method set to %s\n", AAMethod::name(aaMethod));
					break;

				case SDL_SCANCODE_Q:
					switch (aaMethod) {
					case AAMethod::FXAA:
						if (leftShift || rightShift) {
							fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
						} else {
							fxaaQuality = fxaaQuality + 1;
						}
						fxaaQuality = fxaaQuality % maxFXAAQuality;
						buildFXAAShader();
						printf("FXAA quality set to %s (%u)\n", fxaaQualityLevels[fxaaQuality], fxaaQuality);
						break;

					case AAMethod::SMAA:
						if (leftShift || rightShift) {
							smaaQuality = smaaQuality + maxSMAAQuality - 1;
						} else {
							smaaQuality = smaaQuality + 1;
						}
						smaaQuality = smaaQuality % maxSMAAQuality;
						buildSMAAShaders();
						printf("SMAA quality set to %s (%u)\n", smaaQualityLevels[smaaQuality], smaaQuality);
						break;

					}
					break;

				case SDL_SCANCODE_V:
					vsync = !vsync;
					applyVSync();
					break;

				case SDL_SCANCODE_F:
					fullscreen = !fullscreen;
					applyFullscreen();
					break;

				case SDL_SCANCODE_LEFT:
					sceneIncrement = -1;
					// fallthrough
				case SDL_SCANCODE_RIGHT:
					{
						// all images + cubes scene
						unsigned int numScenes = images.size() + 1;
						activeScene = (activeScene + sceneIncrement + numScenes) % numScenes;
					}
					break;

				default:
					break;
				}
				break;

			case SDL_KEYUP:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_LSHIFT:
					leftShift = false;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = false;
					break;

				default:
					break;
				}
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					resizeWidth = event.window.data1;
					resizeHeight = event.window.data2;
					break;
				default:
					break;
				}
			}
		}

		render();
}


void SMAADemo::render() {
	if (resizeWidth != windowWidth || resizeHeight != windowHeight) {
		windowWidth = resizeWidth;
		windowHeight = resizeHeight;
		createFramebuffers();

		glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

		glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}

	uint64_t ticks = SDL_GetPerformanceCounter();
	uint64_t elapsed = ticks - lastTime;

	lastTime = ticks;

	// TODO: reset all relevant state in case some 3rd-party program fucked them up

	glViewport(0, 0, windowWidth, windowHeight);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	builtinFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	renderFBO->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (activeScene == 0) {
		if (rotateCamera) {
			rotationTime += elapsed;

			const uint64_t rotationPeriod = 30 * freq;
			rotationTime = rotationTime % rotationPeriod;
			cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
		}
		glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f)), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, 0.1f, 100.0f);
		glm::mat4 viewProj = proj * view;

		if (useInstancing) {
			cubeInstanceShader->bind();
			GLint viewProjLoc = cubeInstanceShader->getUniformLocation("viewProj");
			glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

			instances.clear();
			instances.reserve(cubes.size());
			for (const auto &cube : cubes) {
				instances.emplace_back(cube.orient, cube.pos, cube.col);
			}

			setCubeVBO();
			glNamedBufferSubData(instanceVBO, 0, sizeof(InstanceData) * instances.size(), &instances[0]);

			glDrawElementsInstanced(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL, cubes.size());
		} else {
			cubeShader->bind();
			GLint viewProjLoc = cubeShader->getUniformLocation("viewProj");
			glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

			GLint cubePosLoc = cubeShader->getUniformLocation("cubePos");
			GLint rotationQuatLoc = cubeShader->getUniformLocation("rotationQuat");
			GLint colorLoc = cubeShader->getUniformLocation("color");

			setCubeVBO();

			for (const auto &cube : cubes) {
				glUniform3fv(cubePosLoc, 1, glm::value_ptr(cube.pos));
				glUniform3fv(rotationQuatLoc, 1, glm::value_ptr(cube.orient));
				glm::vec3 colorF;
				colorF.x = float(cube.col.r) / 255.0f;
				colorF.y = float(cube.col.g) / 255.0f;
				colorF.z = float(cube.col.b) / 255.0f;
				glUniform3fv(colorLoc, 1, glm::value_ptr(colorF));
				glDrawElements(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL);
			}

		}
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		assert(activeScene - 1 < images.size());
		const auto &image = images[activeScene - 1];
		imageShader->bind();
		glBindTextureUnit(TEXUNIT_COLOR, image.tex);
		glBindSampler(TEXUNIT_COLOR, nearestSampler);
		setFullscreenVBO();
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindTextureUnit(TEXUNIT_COLOR, renderFBO->colorTex);
		glBindSampler(TEXUNIT_COLOR, linearSampler);
	}

	setFullscreenVBO();

	if (antialiasing) {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		switch (aaMethod) {
		case AAMethod::FXAA:
			builtinFBO->bind();
			fxaaShader->bind();
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;

		case AAMethod::SMAA:
			smaaEdgeShader->bind();

			if (debugMode == 1) {
				// detect edges only
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				edgesFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			smaaBlendWeightShader->bind();
			if (debugMode == 2) {
				// show blending weights
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				blendFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			// full effect
			smaaNeighborShader->bind();
			builtinFBO->bind();
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;
		}

	} else {
		renderFBO->blitTo(*builtinFBO);
	}

	SDL_GL_SwapWindow(window);
}


#ifdef EMSCRIPTEN


static void mainLoopWrapper(void *smaaDemo_) {
	SMAADemo *demo = reinterpret_cast<SMAADemo *>(smaaDemo_);
	demo->mainLoopIteration();
}


#endif  // EMSCRIPTEN


int main(int argc, char *argv[]) {
	try {
	auto demo = std::make_unique<SMAADemo>();

#ifdef EMSCRIPTEN

	(void) argc;
	(void) argv;

#else  // EMSRIPTEN

	demo->parseCommandLine(argc, argv);

#endif  // EMSCRIPTEN

	demo->initRender();
	demo->createCubes();
	printHelp();

#ifdef EMSCRIPTEN

	emscripten_set_main_loop_arg(mainLoopWrapper, demo.release(), 0, 1);

#else  // EMSCRIPTEN

	while (demo->shouldKeepGoing()) {
		demo->mainLoopIteration();
	}

#endif  // EMSCRIPTEN

	} catch (std::exception &e) {
		printf("caught std::exception \"%s\"\n", e.what());
		// so native dumps core
		throw;
	} catch (...) {
		printf("unknown exception\n");
		// so native dumps core
		throw;
	}
	return 0;
}
